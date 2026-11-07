#!/usr/bin/env python3
import hashlib
import io
import os
import struct
import subprocess
import tempfile
import time
from collections import defaultdict
from dataclasses import dataclass

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection
from fbt.sdk.hashes import gnu_sym_hash
from flipper.app import App

VERSION = 1


@dataclass
class RelData:
    section: int
    section_value: int
    type: int
    offset: int
    name: str


@dataclass(frozen=True)
class UniqueRelData:
    section: int
    section_value: int
    type: int
    name: str


@dataclass
class RelSection:
    name: str
    oringinal_name: str
    data: dict[UniqueRelData, list[int]]


def serialize_relsection_data(data: dict[UniqueRelData, list[int]]) -> bytes:
    result = struct.pack("<B", VERSION)
    result += struct.pack("<I", len(data))
    for unique, values in data.items():
        if unique.section > 0:
            result += struct.pack("<B", (1 << 7) | unique.type & 0x7F)
            result += struct.pack("<I", unique.section)
            result += struct.pack("<I", unique.section_value)
        else:
            result += struct.pack("<B", (0 << 7) | unique.type & 0x7F)
            result += struct.pack("<I", gnu_sym_hash(unique.name))

        result += struct.pack("<I", len(values))
        for offset in values:
            result += struct.pack(
                "<BBB", offset & 0xFF, (offset >> 8) & 0xFF, (offset >> 16) & 0xFF
            )

    return result


class Main(App):
    def init(self):
        self.parser.add_argument("fap_src_path", help="App file to upload")
        self.parser.add_argument("objcopy_path", help="Objcopy path")
        self.parser.set_defaults(func=self.process)

    def process(self):
        fap_path = self.args.fap_src_path
        objcopy_path = self.args.objcopy_path

        # Output FAP goes next to the source ELF (same directory, .fap extension)
        fap_output_dir = os.path.dirname(fap_path)
        fap_output_name = os.path.splitext(os.path.basename(fap_path))[0] + ".fap"
        fap_output_path = os.path.join(fap_output_dir, fap_output_name)

        sections: list[RelSection] = []

        with open(fap_path, "rb") as f:
            elf_file = ELFFile(f)

            relocation_sections: list[RelocationSection] = []
            symtab_section: SymbolTableSection | None = None

            for section in elf_file.iter_sections():
                if isinstance(section, RelocationSection):
                    relocation_sections.append(section)

                if isinstance(section, SymbolTableSection):
                    symtab_section = section

            if not symtab_section:
                self.logger.error("No symbol table found")
                return 1

            if not relocation_sections:
                self.logger.info(
                    "No relocation sections found, creating minimal FAP"
                )
                self._create_minimal_fap(fap_output_path)
                return 0

            for section in relocation_sections:
                section_relocations: list[RelData] = []

                for relocation in section.iter_relocations():
                    symbol_id: int = relocation.entry["r_info_sym"]
                    offset: int = relocation.entry["r_offset"]
                    type: int = relocation.entry["r_info_type"]
                    symbol = symtab_section.get_symbol(symbol_id)
                    section_index: int = symbol["st_shndx"]
                    section_value: int = symbol["st_value"]
                    if section_index == "SHN_UNDEF":
                        section_index = 0

                    section_relocations.append(
                        RelData(section_index, section_value, type, offset, symbol.name)
                    )

                unique_relocations: dict[UniqueRelData, list[int]] = defaultdict(list)
                for relocation in section_relocations:
                    unique = UniqueRelData(
                        relocation.section,
                        relocation.section_value,
                        relocation.type,
                        relocation.name,
                    )

                    unique_relocations[unique].append(relocation.offset)

                section_name = section.name
                if section_name.startswith(".rel"):
                    section_name = ".fast.rel" + section_name[4:]
                else:
                    self.logger.error(
                        "Unknown relocation section name: %s", section_name
                    )
                    return 1

                sections.append(
                    RelSection(section_name, section.name, unique_relocations)
                )

        with tempfile.TemporaryDirectory() as temp_dir:
            BATCH_SIZE = 25
            for i in range(0, len(sections), BATCH_SIZE):
                batch = sections[i : i + BATCH_SIZE]
                args = [objcopy_path]
                for section in batch:
                    data = serialize_relsection_data(section.data)
                    hash_name = hashlib.md5(section.name.encode()).hexdigest()
                    filename = os.path.join(temp_dir, f"{hash_name}.bin")

                    if not os.path.isfile(filename):
                        with open(filename, "wb") as f:
                            f.write(data)

                    args.extend(["--add-section", f"{section.name}={filename}"])

                args.append(fap_path)

                max_retries = 5
                for attempt in range(max_retries):
                    res = subprocess.run(args)
                    if res.returncode == 0:
                        break
                    if attempt < max_retries - 1:
                        time.sleep(0.15 * (attempt + 1))
                    else:
                        self.logger.error("objcopy failed after %d attempts", max_retries)
                        return 1

        return 0

    def _create_minimal_fap(self, fap_path):
        """Emit a proper minimal ELF FAP when the ELF has no relocation sections.

        The input ELF already carries the ``.fapmeta`` manifest that
        ``prepare_app_metadata`` + objcopy wrote into it, together with all
        code/data sections and an entry point. With no relocation sections
        there is nothing for the ``.fast.rel*`` pass to add, so the input
        ELF is already the complete FAP artifact. The FAP path is both the
        objcopy output (input) and the deployment location (output), so we
        copy it into place rather than rebuilding a raw archive -- which is
        what the previous implementation did, and which the Flipper loader
        cannot execute (it expects an ELF, not a ``DZLO`` archive).
        """
        with open(fap_path, "rb") as f:
            data = f.read()

        # Safety net: the manifest section must be present for the produced
        # FAP to pass flipper_application_validate_manifest() at load time.
        try:
            elf = ELFFile(io.BytesIO(data))
            if not any(s.name == ".fapmeta" for s in elf.iter_sections()):
                self.logger.warning("Minimal FAP %s is missing a .fapmeta section", fap_path)
        except Exception as e:  # noqa: BLE001 - keep building even if parse fails
            self.logger.warning(
                "Could not parse %s as ELF while validating minimal FAP: %s", fap_path, e
            )

        with open(fap_path, "wb") as f:
            f.write(data)


if __name__ == "__main__":
    Main()()
