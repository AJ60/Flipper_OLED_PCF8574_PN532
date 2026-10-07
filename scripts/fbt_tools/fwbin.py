import SCons
from SCons.Action import Action
from SCons.Builder import Builder

__OBJCOPY_ARM_BIN = "arm-none-eabi-objcopy"
__NM_ARM_BIN = "arm-none-eabi-nm"


def generate(env):
    # Use the already-resolved tool paths from crosscc tool
    objcopy_bin = env.get("OBJCOPY", "arm-none-eabi-objcopy")
    nm_bin = env.get("NM", "arm-none-eabi-nm")

    env.SetDefault(
        BIN2DFU="${FBT_SCRIPT_DIR}/bin2dfu.py",
        BIN_SIZE_SCRIPT="${FBT_SCRIPT_DIR}/fwsize.py",
        OBJCOPY=objcopy_bin,
        NM=nm_bin,
    )

    if not env["VERBOSE"]:
        env.SetDefault(
            HEXCOMSTR="\tHEX\t${TARGET}",
            BINCOMSTR="\tBIN\t${TARGET}",
            DFUCOMSTR="\tDFU\t${TARGET}",
        )

    env.Append(
        BUILDERS={
            "HEXBuilder": Builder(
                action=Action(
                    [["${OBJCOPY}", "-O", "ihex", "${SOURCE}", "${TARGET}"]],
                    "${HEXCOMSTR}",
                ),
                suffix=".hex",
                src_suffix=".elf",
            ),
            "BINBuilder": Builder(
                action=Action(
                    [["${OBJCOPY}", "-O", "binary", "-S", "${SOURCE}", "${TARGET}"]],
                    "${BINCOMSTR}",
                ),
                suffix=".bin",
                src_suffix=".elf",
            ),
            "DFUBuilder": Builder(
                action=Action(
                    [
                        [
                            "${PYTHON3}",
                            "${BIN2DFU}",
                            "-i",
                            "${SOURCE}",
                            "-o",
                            "${TARGET}",
                            "-a",
                            "${IMAGE_BASE_ADDRESS}",
                            "-l",
                            "Flipper Zero F${TARGET_HW}",
                        ]
                    ],
                    "${DFUCOMSTR}",
                ),
                suffix=".dfu",
                src_suffix=".bin",
            ),
        }
    )


def exists(env):
    try:
        return env["OBJCOPY"]
    except KeyError:
        pass

    # Try the resolved path from crosscc
    if objcopy := env.WhereIs("arm-none-eabi-objcopy"):
        return objcopy

    raise SCons.Errors.StopError("Could not detect objcopy for arm")
