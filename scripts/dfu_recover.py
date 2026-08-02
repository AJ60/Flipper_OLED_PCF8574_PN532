#!/usr/bin/env python3
import os
import sys
import subprocess
import glob
import logging
from flipper.app import App

# Standard STM32 DFU USB ID
STM32_DFU_VID = 0x0483
STM32_DFU_PID = 0xDF11

class DFURecover(App):
    def init(self):
        self.parser.add_argument(
            "-f", "--file",
            type=str,
            default=None,
            help="Path to the full .bin or .dfu firmware file. If not specified, the script will search in dist/f7-C/",
        )
        self.parser.add_argument(
            "--tool",
            choices=["auto", "cube", "dfu-util"],
            default="auto",
            help="Which DFU programmer tool to use (cube = STM32_Programmer_CLI, dfu-util = dfu-util)",
        )
        self.parser.set_defaults(func=self.recover)

    def _find_firmware(self):
        if self.args.file:
            if os.path.exists(self.args.file):
                return os.path.abspath(self.args.file)
            self.logger.error(f"Specified file not found: {self.args.file}")
            return None

        # Look in dist/f7-C/
        dist_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "f7-C")
        if not os.path.isdir(dist_dir):
            self.logger.error(f"Build dist directory not found: {dist_dir}. Please compile the firmware first using fbt.")
            return None

        # Prefer .dfu files, fall back to .bin
        dfu_files = glob.glob(os.path.join(dist_dir, "flipper-z-f7-full-*.dfu"))
        if dfu_files:
            return dfu_files[0]

        bin_files = glob.glob(os.path.join(dist_dir, "flipper-z-f7-full-*.bin"))
        if bin_files:
            return bin_files[0]

        self.logger.error("No full firmware binary (.dfu or .bin) found in dist/f7-C/. Run 'fbt firmware_all' first.")
        return None

    def _find_cube_programmer(self):
        # 1. Check if in PATH
        try:
            subprocess.run(["STM32_Programmer_CLI", "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return "STM32_Programmer_CLI"
        except FileNotFoundError:
            pass

        # 2. Check standard Windows installation paths
        standard_paths = [
            r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
            r"C:\Program Files\x86\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        ]
        for path in standard_paths:
            if os.path.exists(path):
                return path

        return None

    def _find_dfu_util(self):
        try:
            subprocess.run(["dfu-util", "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return "dfu-util"
        except FileNotFoundError:
            pass
        return None

    def _is_dfu_connected_windows(self):
        # We can run wmic or powershell to check if the USB device is present
        try:
            cmd = ["powershell", "-Command", "Get-PnpDevice -Present | Where-Object { $_.InstanceId -like '*USB*' -and ($_.Name -like '*STM32*Bootloader*' -or $_.Name -like '*DFU*') }"]
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if "Bootloader" in result.stdout or "DFU" in result.stdout:
                return True
        except Exception:
            pass
        return False

    def recover(self):
        self.logger.info("Starting DIY Flipper Zero USB DFU recovery...")

        # 1. Find the firmware file
        fw_file = self._find_firmware()
        if not fw_file:
            return 1
        self.logger.info(f"Using firmware file: {fw_file}")

        # 2. Select the programmer tool
        tool_to_use = None
        cube_path = self._find_cube_programmer()
        dfu_util_path = self._find_dfu_util()

        if self.args.tool == "cube":
            if not cube_path:
                self.logger.error("STM32_Programmer_CLI not found. Please install STM32CubeProgrammer.")
                return 2
            tool_to_use = "cube"
        elif self.args.tool == "dfu-util":
            if not dfu_util_path:
                self.logger.error("dfu-util not found. Please install it or make sure it is in your PATH.")
                return 2
            tool_to_use = "dfu-util"
        else:  # auto
            if cube_path:
                tool_to_use = "cube"
                self.logger.info("Auto-detected STM32_Programmer_CLI.")
            elif dfu_util_path:
                tool_to_use = "dfu-util"
                self.logger.info("Auto-detected dfu-util.")
            else:
                self.logger.error("No suitable DFU programmer tool found!")
                self.logger.error("Please install STM32CubeProgrammer (recommended on Windows) or dfu-util.")
                return 2

        # 3. Prompt user to connect in DFU mode
        self.logger.warning("=" * 60)
        self.logger.warning("HOW TO ENTER USB DFU RECOVERY MODE:")
        self.logger.warning("1. Disconnect the USB cable from your Flipper Zero.")
        self.logger.warning("2. Connect the BOOT0 pin to 3.3V (VCC) - hold the boot button if your DIY board has one.")
        self.logger.warning("3. Reconnect the USB cable to your computer.")
        self.logger.warning("4. Release the BOOT0 button (if applicable).")
        self.logger.warning("=" * 60)

        connected = False
        if os.name == "nt":
            self.logger.info("Checking for connected DFU devices...")
            if self._is_dfu_connected_windows():
                self.logger.info("STM32 DFU device detected!")
                connected = True
            else:
                self.logger.warning("STM32 DFU device NOT detected yet.")
        
        if not connected:
            input("Press Enter when the device is connected in DFU mode to proceed...")

        # 4. Perform flashing
        self.logger.info("Initiating recovery flash...")
        success = False

        if tool_to_use == "cube":
            self.logger.info("Running STM32_Programmer_CLI...")
            # If the file is .dfu, CubeProgrammer handles it. If it is .bin, we must specify address 0x08000000.
            cmd = [cube_path, "-c", "port=usb1"]
            if fw_file.endswith(".bin"):
                cmd += ["-d", fw_file, "0x08000000", "-v"]
            else:
                cmd += ["-d", fw_file, "-v"]
            cmd += ["-s"] # reset and start executing firmware after flash
            
            self.logger.debug(f"Command: {' '.join(cmd)}")
            result = subprocess.run(cmd)
            success = (result.returncode == 0)

        elif tool_to_use == "dfu-util":
            self.logger.info("Running dfu-util...")
            # For dfu-util, it's best to flash raw .bin. If we have a .dfu file, dfu-util can flash it directly.
            # Standard dfu-util command:
            # dfu-util -a 0 -s 0x08000000:leave -D file.bin
            cmd = ["dfu-util", "-a", "0"]
            if fw_file.endswith(".dfu"):
                cmd += ["-D", fw_file]
            else:
                cmd += ["-s", "0x08000000:leave", "-D", fw_file]
            
            self.logger.warning("Note: Windows users might need to swap the device driver to WinUSB using Zadig for dfu-util to work.")
            self.logger.debug(f"Command: {' '.join(cmd)}")
            result = subprocess.run(cmd)
            success = (result.returncode == 0)

        if success:
            self.logger.info("=" * 60)
            self.logger.info("RECOVERY SUCCESSFUL! Your DIY Flipper Zero should now reboot into the firmware.")
            self.logger.info("=" * 60)
            return 0
        else:
            self.logger.error("=" * 60)
            self.logger.error("RECOVERY FAILED! Please check the logs and ensure the device is in DFU mode.")
            if tool_to_use == "dfu-util" and os.name == "nt":
                self.logger.error("If you are on Windows and using dfu-util, did you run Zadig and swap the 'STM32 Bootloader' driver to WinUSB?")
            self.logger.error("=" * 60)
            return 3

if __name__ == "__main__":
    DFURecover()()
