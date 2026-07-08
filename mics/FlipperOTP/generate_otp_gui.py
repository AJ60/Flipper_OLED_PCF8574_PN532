import struct
import time
import os
import sys
import random
import string
import subprocess
import threading
import tkinter as tk
from tkinter import messagebox, filedialog
from tkinter import ttk
import webbrowser

class Tooltip:
    def __init__(self, widget, text):
        self.widget = widget
        self.text = text
        self.tipwindow = None
        self.id = None
        self.x = self.y = 0
        self.widget.bind("<Enter>", self.enter)
        self.widget.bind("<Leave>", self.leave)

    def enter(self, event=None):
        self.schedule()

    def leave(self, event=None):
        self.unschedule()
        self.hidetip()

    def schedule(self):
        self.unschedule()
        self.id = self.widget.after(400, self.showtip)

    def unschedule(self):
        id_ = self.id
        self.id = None
        if id_:
            self.widget.after_cancel(id_)

    def showtip(self, event=None):
        x = y = 0
        x, y, cx, cy = self.widget.bbox("insert")
        x += self.widget.winfo_rootx() + 20
        y += self.widget.winfo_rooty() + 25
        self.tipwindow = tw = tk.Toplevel(self.widget)
        tw.wm_overrideredirect(1)
        tw.wm_geometry(f"+{x}+{y}")
        
        # OLED/Mica style tooltip window
        label = tk.Label(tw, text=self.text, justify='left',
                         background="#0c101b", foreground="#82daff",
                         relief='solid', borderwidth=1,
                         highlightthickness=1, highlightbackground="#ff6c00",
                         font=("Segoe UI", 9, "bold"), padx=10, pady=5)
        label.pack(ipadx=1)

    def hidetip(self):
        tw = self.tipwindow
        self.tipwindow = None
        if tw:
            tw.destroy()

class CustomMessagebox(tk.Toplevel):
    def __init__(self, parent, title, message, box_type="info", choices=None):
        super().__init__(parent)
        self.parent = parent
        self.result = None
        self.choices = choices or ["OK"]
        
        # Window settings
        self.title(title)
        self.resizable(False, False)
        self.configure(bg=BG_COLOR)
        self.wm_overrideredirect(1)  # Borderless custom window
        
        # Calculate center position relative to parent window
        parent.update_idletasks()
        parent_x = parent.winfo_rootx()
        parent_y = parent.winfo_rooty()
        parent_w = parent.winfo_width()
        parent_h = parent.winfo_height()
        
        # Estimate visual wrapped lines (about 48 chars per line at 370px wrap width)
        import math
        visual_lines = 0
        for line in message.split('\n'):
            line_len = len(line.strip())
            if line_len == 0:
                visual_lines += 1
            else:
                visual_lines += math.ceil(line_len / 48)

        w = 420
        # ~20px per visual line + 130px for titlebar/buttons/padding
        h = min(550, max(190, visual_lines * 20 + 130))
            
        x = parent_x + (parent_w - w) // 2
        y = parent_y + (parent_h - h) // 2
        self.geometry(f"{w}x{h}+{x}+{y}")
        
        # Apply Windows 11 Acrylic & Immersive Dark Titlebar effects
        try:
            import ctypes
            hwnd = ctypes.windll.user32.GetParent(self.winfo_id())
            # Enable Acrylic System Backdrop
            ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 38, ctypes.byref(ctypes.c_int(3)), 4)
            # Immersive Dark Mode
            ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(ctypes.c_int(1)), 4)
            self.attributes("-alpha", 0.94)
        except Exception:
            pass

        # Outer glow frame (Orange for warnings/errors, Blue for info)
        outer_color = ACCENT_COLOR if box_type in ["error", "warning"] else TEXT_COLOR
        main_frame = tk.Frame(self, bg=CONTAINER_COLOR, bd=1, relief="solid", highlightthickness=2, highlightbackground=outer_color)
        main_frame.pack(fill="both", expand=True, padx=2, pady=2)
        
        # Title bar frame
        title_frame = tk.Frame(main_frame, bg=CONTAINER_COLOR)
        title_frame.pack(fill="x", padx=15, pady=(15, 5))
        
        # Header title
        icon_symbol = "⚠️" if box_type in ["warning", "error"] else "ℹ️"
        header_lbl = tk.Label(title_frame, text=f"{icon_symbol}  {title.upper()}", font=("Segoe UI", 11, "bold"), fg=outer_color, bg=CONTAINER_COLOR)
        header_lbl.pack(side="left")
        
        # Close button [X] in the top-right corner
        close_btn = tk.Button(
            title_frame, 
            text="✕", 
            font=("Segoe UI", 10, "bold"), 
            bg=CONTAINER_COLOR, 
            fg=TEXT_MUTED, 
            activebackground=CONTAINER_COLOR, 
            activeforeground=outer_color, 
            relief="flat", 
            bd=0, 
            cursor="hand2", 
            command=lambda: self.on_click("CANCEL")
        )
        close_btn.pack(side="right")
        close_btn.bind("<Enter>", lambda e: close_btn.config(fg=outer_color))
        close_btn.bind("<Leave>", lambda e: close_btn.config(fg=TEXT_MUTED))
        
        # Pack buttons area first (side="bottom") so it is guaranteed to have space
        btn_frame = tk.Frame(main_frame, bg=CONTAINER_COLOR)
        btn_frame.pack(fill="x", side="bottom", pady=15, padx=15)

        # Message text (packs in remaining space)
        msg_text = tk.Label(main_frame, text=message, font=("Segoe UI", 9, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR, justify="left", wraplength=370)
        msg_text.pack(fill="both", expand=True, padx=15, pady=5)
        
        # Add buttons dynamically
        for choice in self.choices:
            btn = tk.Button(
                btn_frame, 
                text=choice, 
                font=("Segoe UI", 9, "bold"), 
                bg="#18233c" if choice not in ["YES", "OK"] else ACCENT_COLOR, 
                fg=TEXT_COLOR if choice not in ["YES", "OK"] else "#ffffff",
                activebackground="#28395f" if choice not in ["YES", "OK"] else "#ff8533",
                activeforeground=TEXT_COLOR if choice not in ["YES", "OK"] else "#ffffff",
                relief="solid", 
                bd=1, 
                highlightthickness=1, 
                highlightbackground="#344870" if choice not in ["YES", "OK"] else "#ff9e59",
                cursor="hand2", 
                padx=10, 
                pady=2,
                command=lambda c=choice: self.on_click(c)
            )
            
            # Hover animations
            if choice not in ["YES", "OK"]:
                btn.bind("<Enter>", lambda e, b=btn: b.config(bg="#28395f"))
                btn.bind("<Leave>", lambda e, b=btn: b.config(bg="#18233c"))
            else:
                btn.bind("<Enter>", lambda e, b=btn: b.config(bg="#ff8533"))
                btn.bind("<Leave>", lambda e, b=btn: b.config(bg=ACCENT_COLOR))
                
            btn.pack(side="right", padx=(5, 0), fill="x", expand=True)

        self.grab_set()  # Lock focus to messagebox
        self.focus_set()
        self.wait_window()

    def on_click(self, choice):
        self.result = choice
        self.destroy()

    @staticmethod
    def show_info(parent, title, message):
        CustomMessagebox(parent, title, message, box_type="info", choices=["OK"])

    @staticmethod
    def show_error(parent, title, message):
        CustomMessagebox(parent, title, message, box_type="error", choices=["OK"])

    @staticmethod
    def ask_yes_no(parent, title, message):
        box = CustomMessagebox(parent, title, message, box_type="warning", choices=["NO", "YES"])
        return box.result == "YES"

# Premium Frosted Glass (Acrylic/Mica) colors with Flipper Orange accents
BG_COLOR = "#000000"          # Deep background
CONTAINER_COLOR = "#0c101b"   # Obsidian glass base
TEXT_COLOR = "#00d2ff"        # Glowing ice-blue
TEXT_MUTED = "#82daff"        # Ice-blue secondary
ACCENT_COLOR = "#ff6c00"      # Flipper Orange
BUTTON_ACTIVE = "#ff9e59"

def clean_cli_log(raw_log):
    """Strip garbled progress bar lines from STM32_Programmer_CLI output.
    The CLI prints block chars (█) for progress which decode as 'ы' on cp1251 Windows."""
    import re
    cleaned = []
    for line in raw_log.split('\n'):
        # Skip lines that are mostly repeated non-ASCII junk (progress bars)
        stripped = line.strip()
        if not stripped:
            continue
        # Count non-ASCII characters
        non_ascii = sum(1 for c in stripped if ord(c) > 127)
        if len(stripped) > 0 and non_ascii / len(stripped) > 0.5:
            continue  # Skip garbled progress bar lines
        # Also skip lines that are just +/= characters (progress indicators)
        if re.fullmatch(r'[+=%\s\-|#]+', stripped):
            continue
        cleaned.append(line)
    return '\n'.join(cleaned)

# Standard install paths for STM32CubeProgrammer CLI on Windows
ST_PROG_CLI_PATHS = [
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    r"C:\Program Files\x86\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
]

def generate_random_name():
    first_char = random.choice(string.ascii_uppercase)
    rest_chars = ''.join(random.choices(string.ascii_lowercase + string.digits, k=7))
    return first_char + rest_chars

class OTPGeneratorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("DIY Flipper Zero — OTP Tool")
        self.root.geometry("455x640")
        self.root.configure(bg=BG_COLOR)
        self.root.resizable(False, False)
        
        # Set window icon with high-clarity on Windows taskbar
        try:
            # 1. Tell Windows this is a distinct application to prevent generic python taskbar icon grouping/scaling
            import ctypes
            myappid = 'diyflipper.otptool.1.0'
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(myappid)
        except Exception:
            pass

        try:
            if getattr(sys, 'frozen', False):
                base_path = sys._MEIPASS
            else:
                base_path = os.path.dirname(os.path.abspath(__file__))
            icon_path = os.path.join(base_path, "flipper.ico")
            if os.path.exists(icon_path):
                # Set iconbitmap for the titlebar
                self.root.iconbitmap(icon_path)
                # Set iconphoto to feed high-res sizes (32x32, 48x48, etc.) to the OS taskbar
                from PIL import Image, ImageTk
                img = Image.open(icon_path)
                photo = ImageTk.PhotoImage(img)
                self.root.iconphoto(True, photo)
                # Save reference to prevent garbage collection
                self._icon_photo = photo
        except Exception:
            pass
        
        # Apply Windows 11 Acrylic Backdrop & Immersive Dark Title Bar Effects
        self.root.update()  # Force creation of window window handle (HWND)
        try:
            import ctypes
            hwnd = ctypes.windll.user32.GetParent(self.root.winfo_id())
            
            # 1. Enable Immersive Dark Mode for window title bar (attribute 20)
            DWMWA_USE_IMMERSIVE_DARK_MODE = 20
            use_dark_mode = ctypes.c_int(1)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, 
                DWMWA_USE_IMMERSIVE_DARK_MODE, 
                ctypes.byref(use_dark_mode), 
                ctypes.sizeof(use_dark_mode)
            )
            
            # 2. Enable System Acrylic Backdrop blur (attribute 38, value 3)
            DWMWA_SYSTEMBACKDROP_TYPE = 38
            backdrop_type = ctypes.c_int(3) # 3 = Acrylic (Frosted Glass)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, 
                DWMWA_SYSTEMBACKDROP_TYPE, 
                ctypes.byref(backdrop_type), 
                ctypes.sizeof(backdrop_type)
            )
            
            # 3. Set window opacity slightly lower to let the Acrylic blur show through
            self.root.attributes("-alpha", 0.88)
        except Exception:
            pass
        
        # Style Comboboxes to match OLED look and remove default Windows gray selection
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TCombobox", 
                        fieldbackground=BG_COLOR, 
                        background=CONTAINER_COLOR, 
                        foreground=TEXT_COLOR,
                        darkcolor="#1b2a47",      # Sleek dark glass border (idle)
                        lightcolor="#1b2a47",
                        bordercolor="#1b2a47",
                        arrowcolor=ACCENT_COLOR)
        
        style.map('TCombobox',
                  fieldbackground=[('readonly', BG_COLOR), ('focus', BG_COLOR)],
                  foreground=[('readonly', TEXT_COLOR), ('focus', TEXT_COLOR)],
                  selectbackground=[('readonly', BG_COLOR), ('focus', BG_COLOR)],
                  selectforeground=[('readonly', TEXT_COLOR), ('focus', TEXT_COLOR)],
                  background=[('readonly', CONTAINER_COLOR), ('focus', CONTAINER_COLOR), ('active', CONTAINER_COLOR)],
                  arrowcolor=[('readonly', ACCENT_COLOR), ('focus', ACCENT_COLOR), ('active', ACCENT_COLOR)],
                  bordercolor=[('focus', TEXT_COLOR), ('active', TEXT_COLOR)],
                  darkcolor=[('focus', TEXT_COLOR), ('active', TEXT_COLOR)],
                  lightcolor=[('focus', TEXT_COLOR), ('active', TEXT_COLOR)])
                  
        # Apply style to the dropdown popup listbox globally to override Windows theme
        self.root.option_add("*TCombobox*Listbox.background", BG_COLOR)
        self.root.option_add("*TCombobox*Listbox.foreground", TEXT_COLOR)
        self.root.option_add("*TCombobox*Listbox.selectBackground", ACCENT_COLOR)
        self.root.option_add("*TCombobox*Listbox.selectForeground", BG_COLOR)
        self.root.option_add("*TCombobox*Listbox.font", ("Segoe UI", 10))
        self.root.option_add("*TCombobox*Listbox.borderWidth", "0")
        self.root.option_add("*TCombobox*Listbox.highlightThickness", "0")
        self.root.option_add("*TCombobox*Listbox.relief", "flat")
        
        self.root.option_add("*Listbox.background", BG_COLOR)
        self.root.option_add("*Listbox.foreground", TEXT_COLOR)
        self.root.option_add("*Listbox.selectBackground", ACCENT_COLOR)
        self.root.option_add("*Listbox.selectForeground", BG_COLOR)
        self.root.option_add("*Listbox.font", ("Segoe UI", 10))
        self.root.option_add("*Listbox.borderWidth", "0")
        self.root.option_add("*Listbox.highlightThickness", "0")
        self.root.option_add("*Listbox.relief", "flat")
        
        self.cli_path = self.find_stm32_programmer_cli()
        self.create_widgets()
        
        # Start DFU background checking
        self.check_dfu_status()
        
    def check_dfu_status(self):
        # Run the CLI probe in a background thread, then poll result from main thread
        self._dfu_result = None  # None = pending, True/False = result

        def probe():
            connected = False
            if self.cli_path:
                try:
                    result = subprocess.run(
                        [self.cli_path, "-l", "usb"],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        text=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=3
                    )
                    for line in result.stdout.split('\n'):
                        if "Total number of available STM32 device" in line:
                            parts = line.split(':')
                            if len(parts) > 1 and int(parts[1].strip()) > 0:
                                connected = True
                            break
                except Exception:
                    pass
            self._dfu_result = connected

        threading.Thread(target=probe, daemon=True).start()
        # Poll for the result from the main thread
        self._poll_dfu_result()

    def _poll_dfu_result(self):
        if self._dfu_result is not None:
            # Result is ready — update the label
            if self._dfu_result:
                self.status_label.config(text="🟢 DFU Status: Connected (Ready)", fg="#00d2ff")
            else:
                self.status_label.config(text="🔴 DFU Status: Disconnected", fg="#ff4500")
            # Schedule next full check in 3 seconds
            self.root.after(3000, self.check_dfu_status)
        else:
            # Still waiting for the thread — poll again in 200ms
            self.root.after(200, self._poll_dfu_result)
        
    def find_stm32_programmer_cli(self):
        # 1. Search in standard paths
        for path in ST_PROG_CLI_PATHS:
            if os.path.isfile(path):
                return path
        # 2. Check if in system PATH
        try:
            subprocess.run(["STM32_Programmer_CLI.exe", "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return "STM32_Programmer_CLI.exe"
        except FileNotFoundError:
            return None

    def create_widgets(self):
        # Header Frame
        header_frame = tk.Frame(self.root, bg=BG_COLOR, pady=10)
        header_frame.pack(fill="x")
        
        title_label = tk.Label(header_frame, text="🐬 DIY Flipper OTP & Flasher", font=("Segoe UI", 16, "bold"), fg=ACCENT_COLOR, bg=BG_COLOR)
        title_label.pack()
        
        subtitle_label = tk.Label(header_frame, text="Generate, read, and flash hardware profiles over USB", font=("Segoe UI", 9, "bold"), fg=ACCENT_COLOR, bg=BG_COLOR)
        subtitle_label.pack()

        # DFU Connection Status indicator in the header
        self.status_label = tk.Label(header_frame, text="🔴 DFU Status: Checking...", font=("Segoe UI", 9, "bold"), fg="#ff4500", bg=BG_COLOR)
        self.status_label.pack(pady=(4, 0))
        Tooltip(self.status_label, 
                "How to enter DFU mode (WeAct STM32WB55):\n"
                "1. Disconnect USB cable from the board\n"
                "2. Hold the BOOT0 button on the board\n"
                "3. Connect the USB cable to PC\n"
                "4. Release the BOOT0 button\n"
                "5. Status should change to Connected (Ready)")

        # Outer glow frame for container (simulates neon glow effect)
        container_glow = tk.Frame(self.root, bg="#3b1900", padx=1, pady=1)
        container_glow.pack(fill="both", expand=True, padx=19, pady=5)

        # Main Container (Screen)
        container = tk.Frame(container_glow, bg=CONTAINER_COLOR, padx=20, pady=20, bd=1, relief="solid", highlightthickness=2, highlightbackground=ACCENT_COLOR)
        container.pack(fill="both", expand=True)
        
        # 1. Device Name
        name_lbl = tk.Label(container, text="Device Name (max 8 ASCII characters):", font=("Segoe UI", 10, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR)
        name_lbl.pack(anchor="w", pady=(0, 5))
        self.name_var = tk.StringVar(value=generate_random_name())
        
        # Validation: max 8 ASCII-printable characters only
        def validate_name(new_value):
            if len(new_value) > 8:
                return False
            # Allow only ASCII printable characters (codes 32-126)
            return all(32 <= ord(c) <= 126 for c in new_value)
        
        vcmd_name = (self.root.register(validate_name), '%P')
        # Entry with glass border styling and orange high-contrast selection colors
        self.name_entry = tk.Entry(container, textvariable=self.name_var, font=("Segoe UI", 11, "bold"), bg=BG_COLOR, fg=TEXT_COLOR, insertbackground=TEXT_COLOR, bd=1, relief="solid", highlightthickness=2, highlightbackground="#1b2a47", highlightcolor=TEXT_COLOR, selectbackground=ACCENT_COLOR, selectforeground="#000000", validate="key", validatecommand=vcmd_name)
        self.name_entry.pack(fill="x", pady=(0, 15))
        
        # 2. Board Version
        version_lbl = tk.Label(container, text="Board Version (0–255):", font=("Segoe UI", 10, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR)
        version_lbl.pack(anchor="w", pady=(0, 5))
        self.version_var = tk.StringVar(value="12")
        
        # Validation: digits only, value 0-255
        def validate_version(new_value):
            if new_value == "":
                return True  # Allow clearing the field
            if not new_value.isdigit():
                return False
            if len(new_value) > 3:
                return False  # Max 3 digits (255)
            if len(new_value) > 1 and new_value[0] == '0':
                return False  # No leading zeros
            return int(new_value) <= 255
        
        vcmd_ver = (self.root.register(validate_version), '%P')
        self.version_entry = tk.Entry(container, textvariable=self.version_var, font=("Segoe UI", 11, "bold"), bg=BG_COLOR, fg=TEXT_COLOR, insertbackground=TEXT_COLOR, bd=1, relief="solid", highlightthickness=2, highlightbackground="#1b2a47", highlightcolor=TEXT_COLOR, selectbackground=ACCENT_COLOR, selectforeground="#000000", validate="key", validatecommand=vcmd_ver)
        self.version_entry.pack(fill="x", pady=(0, 15))
        
        # 3. Display Type
        display_lbl = tk.Label(container, text="Display Type:", font=("Segoe UI", 10, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR)
        display_lbl.pack(anchor="w", pady=(0, 5))
        self.display_combobox = ttk.Combobox(container, values=["MGG (Monochrome Glass Grid - Custom OLED)", "ERC (Original Flipper LCD)"], state="readonly")
        self.display_combobox.current(0)
        self.display_combobox.pack(fill="x", pady=(0, 15))
        
        # 4. Color Option
        color_lbl = tk.Label(container, text="Body Color:", font=("Segoe UI", 10, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR)
        color_lbl.pack(anchor="w", pady=(0, 5))
        self.color_combobox = ttk.Combobox(container, values=["Black (0x01)", "White (0x02)", "Transparent (0x03)"], state="readonly")
        self.color_combobox.current(0)
        self.color_combobox.pack(fill="x", pady=(0, 15))
        
        # 5. Region Option
        region_lbl = tk.Label(container, text="Sub-GHz Region:", font=("Segoe UI", 10, "bold"), fg=TEXT_MUTED, bg=CONTAINER_COLOR)
        region_lbl.pack(anchor="w", pady=(0, 5))
        self.region_combobox = ttk.Combobox(container, values=["Europe / Russia (0x01)", "USA / Canada / Australia (0x02)", "Japan (0x03)", "World (0x04)"], state="readonly")
        self.region_combobox.current(0)
        self.region_combobox.pack(fill="x", pady=(0, 15))
        
        # Action Row 1: File Operations (Load / Save)
        file_frame = tk.Frame(container, bg=CONTAINER_COLOR)
        file_frame.pack(fill="x", pady=(10, 4))
        
        load_btn = tk.Button(
            file_frame, 
            text="Load .bin", 
            font=("Segoe UI", 9, "bold"), 
            bg="#18233c", 
            fg=TEXT_COLOR, 
            activebackground="#28395f", 
            activeforeground=TEXT_COLOR, 
            relief="solid", 
            bd=1, 
            highlightthickness=1, 
            highlightbackground="#344870", 
            cursor="hand2", 
            command=self.load_otp_from_file
        )
        load_btn.pack(side="left", fill="x", expand=True, padx=(0, 4))
        
        save_btn = tk.Button(
            file_frame, 
            text="Save .bin", 
            font=("Segoe UI", 9, "bold"), 
            bg="#18233c", 
            fg=TEXT_COLOR, 
            activebackground="#28395f", 
            activeforeground=TEXT_COLOR, 
            relief="solid", 
            bd=1, 
            highlightthickness=1, 
            highlightbackground="#344870", 
            cursor="hand2", 
            command=self.save_otp_dialog
        )
        save_btn.pack(side="left", fill="x", expand=True)

        # Action Row 2: DFU Device Operations (Read / Flash)
        device_frame = tk.Frame(container, bg=CONTAINER_COLOR)
        device_frame.pack(fill="x", pady=(4, 0))

        self.read_btn = tk.Button(
            device_frame, 
            text="Read (DFU)", 
            font=("Segoe UI", 9, "bold"), 
            bg="#18233c", 
            fg=TEXT_COLOR, 
            activebackground="#28395f", 
            activeforeground=TEXT_COLOR, 
            relief="solid", 
            bd=1, 
            highlightthickness=1, 
            highlightbackground="#344870", 
            cursor="hand2", 
            state="disabled",  # Disabled until DFU is Connected
            command=self.read_otp
        )
        self.read_btn.pack(side="left", fill="x", expand=True, padx=(0, 4))

        self.flash_btn = tk.Button(
            device_frame, 
            text="Flash (DFU)", 
            font=("Segoe UI", 9, "bold"), 
            bg=ACCENT_COLOR, 
            fg="#ffffff", 
            activebackground="#ff8533", 
            activeforeground="#ffffff", 
            relief="solid", 
            bd=1, 
            highlightthickness=1, 
            highlightbackground="#ff9e59", 
            cursor="hand2", 
            state="disabled",  # Disabled until DFU is Connected
            command=self.flash_otp
        )
        self.flash_btn.pack(side="left", fill="x", expand=True)

        # Helper functions for premium hover animations
        def apply_button_hover(widget, bg_hover, fg_hover, bg_normal, fg_normal):
            def on_enter(e):
                if str(widget['state']) != 'disabled':
                    widget.config(bg=bg_hover, fg=fg_hover)
            def on_leave(e):
                if str(widget['state']) != 'disabled':
                    widget.config(bg=bg_normal, fg=fg_normal)
            widget.bind("<Enter>", on_enter)
            widget.bind("<Leave>", on_leave)

        apply_button_hover(load_btn, "#28395f", TEXT_COLOR, "#18233c", TEXT_COLOR)
        apply_button_hover(save_btn, "#28395f", TEXT_COLOR, "#18233c", TEXT_COLOR)
        apply_button_hover(self.read_btn, "#28395f", TEXT_COLOR, "#18233c", TEXT_COLOR)
        apply_button_hover(self.flash_btn, "#ff8533", "#ffffff", ACCENT_COLOR, "#ffffff")

        # Subtle glow animations on text entries hover
        def apply_entry_hover(entry):
            entry.bind("<Enter>", lambda e: entry.config(highlightbackground="#344870") if entry.root.focus_get() != entry else None)
            entry.bind("<Leave>", lambda e: entry.config(highlightbackground="#1b2a47") if entry.root.focus_get() != entry else None)

        apply_entry_hover(self.name_entry)
        apply_entry_hover(self.version_entry)

        # Attach tooltips to each label to make the tool beginner-friendly (English only)
        Tooltip(name_lbl, "Custom name for your Flipper Zero (max 8 characters). Written into OTP permanently!")
        Tooltip(version_lbl, "Hardware board version. Normally 12 for DIY WeAct STM32WB55 boards.")
        Tooltip(display_lbl, "Display driver. Choose 'MGG' for custom I2C SSD1306 OLED screen to function.")
        Tooltip(color_lbl, "Case body color. Changes dolphin animations and shell look in menus.")
        Tooltip(region_lbl, "Frequency bands region. Sets frequency rules for Sub-GHz CC1101 transceiver.")

        # Bottom warning (Disclaimer)
        footer_label = tk.Label(
            self.root, 
            text="⚠️ OTP WRITING IS IRREVERSIBLE! Done at your own risk.", 
            font=("Segoe UI", 8, "bold"), 
            fg="#ff4500", # Glowing neon red-orange warning color
            bg=BG_COLOR, 
            pady=2
        )
        footer_label.pack(side="bottom")

        # GitHub Author Link Label
        author_label = tk.Label(
            self.root,
            text="github.com/artema0g",
            font=("Segoe UI", 8, "underline"),
            fg=TEXT_MUTED,
            bg=BG_COLOR,
            cursor="hand2"
        )
        author_label.pack(side="bottom", pady=(5, 0))
        
        # Link click and hover bindings
        author_label.bind("<Button-1>", lambda e: webbrowser.open("https://github.com/artema0g/oled_flipper"))
        author_label.bind("<Enter>", lambda e: author_label.config(fg=ACCENT_COLOR))
        author_label.bind("<Leave>", lambda e: author_label.config(fg=TEXT_MUTED))

    def get_otp_binary_data(self):
        name = self.name_var.get().strip()
        if not name:
            name = generate_random_name()
            self.name_var.set(name)
            
        if len(name) > 8:
            name = name[:8]
            self.name_var.set(name)
            
        name = name.ljust(8, '\x00')
        name_bytes = name.encode('ascii', errors='ignore')
        
        try:
            version = int(self.version_var.get())
        except ValueError:
            CustomMessagebox.show_error(self.root, "Error", "Board Version must be an integer!")
            return None, None
            
        # Parse display index: MGG is 2, ERC is 1
        display_idx = 2 if "MGG" in self.display_combobox.get() else 1
        
        # Parse color index
        color_text = self.color_combobox.get()
        color_idx = 1 if "Black" in color_text else (2 if "White" in color_text else 3)
        
        # Parse region index
        region_text = self.region_combobox.get()
        region_idx = 1 if "Europe" in region_text else (2 if "USA" in region_text else (3 if "Japan" in region_text else 4))
        
        # Constants
        header_magic = 0xBABE
        header_version = 2
        header_reserved = 0
        header_timestamp = int(time.time())
        
        board_target = 7   # standard firmware target
        board_body = 9     # custom layout body
        board_connect = 6  # custom layout connection
        
        board_reserved2_0 = 0
        board_reserved2_1 = 0
        
        board_reserved3_0 = 0
        board_reserved3_1 = 0
        
        # Pack V2 binary
        otp_data = struct.pack(
            "<HBBI BBBBBBH BBHI 8s",
            header_magic,
            header_version,
            header_reserved,
            header_timestamp,
            
            version,
            board_target,
            board_body,
            board_connect,
            display_idx,
            board_reserved2_0,
            board_reserved2_1,
            
            color_idx,
            region_idx,
            board_reserved3_0,
            board_reserved3_1,
            
            name_bytes
        )
        return otp_data, name.strip(chr(0))

    def load_otp_from_file(self):
        file_path = filedialog.askopenfilename(
            parent=self.root,
            title="Select OTP Binary File",
            filetypes=[("Binary Files", "*.bin"), ("All Files", "*.*")]
        )
        if not file_path:
            return

        try:
            with open(file_path, "rb") as f:
                data = f.read()

            if len(data) < 32:
                CustomMessagebox.show_error(self.root, "Load Failed", "Selected file is too small to be a valid OTP profile (must be at least 32 bytes).")
                return

            # Unpack structure
            # Header Magic (uint16), Header Version (uint8), Reserved (uint8), Timestamp (uint32)
            # Board Version (uint8), Target (uint8), Body (uint8), Connect (uint8), Display (uint8), Res2_0 (uint8), Res2_1 (uint16)
            # Color (uint8), Region (uint8), Res3_0 (uint16), Res3_1 (uint32)
            # Name (char[8])
            header_magic, header_ver, _, timestamp, \
            b_ver, b_target, b_body, b_connect, b_display, _, _, \
            b_color, b_region, _, _, \
            name_bytes = struct.unpack("<HBBI BBBBBBH BBHI 8s", data)

            if header_magic != 0xBABE:
                confirm = CustomMessagebox.ask_yes_no(
                    self.root,
                    "Invalid Magic",
                    f"Warning: Header magic {hex(header_magic)} is not 0xBABE.\n"
                    "This might not be a valid Flipper OTP file.\n\n"
                    "Do you want to load it anyway?"
                )
                if not confirm:
                    return

            # Set Device Name
            name_str = name_bytes.decode('ascii', errors='ignore').strip('\x00')
            self.name_var.set(name_str)

            # Set Board Version
            self.version_var.set(str(b_ver))

            # Set Display Type
            if b_display == 2:
                self.display_combobox.set("MGG (Monochrome Glass Grid - Custom OLED)")
            elif b_display == 1:
                self.display_combobox.set("ERC (Original Flipper LCD)")
            else:
                self.display_combobox.set(f"Unknown (0x{b_display:02X})")

            # Set Body Color
            if b_color == 1:
                self.color_combobox.set("Black (0x01)")
            elif b_color == 2:
                self.color_combobox.set("White (0x02)")
            elif b_color == 3:
                self.color_combobox.set("Transparent (0x03)")
            else:
                self.color_combobox.set(f"Unknown (0x{b_color:02X})")

            # Set Sub-GHz Region
            if b_region == 1:
                self.region_combobox.set("Europe / Russia (0x01)")
            elif b_region == 2:
                self.region_combobox.set("USA / Canada / Australia (0x02)")
            elif b_region == 3:
                self.region_combobox.set("Japan (0x03)")
            elif b_region == 4:
                self.region_combobox.set("World (0x04)")
            else:
                self.region_combobox.set(f"Unknown (0x{b_region:02X})")

            CustomMessagebox.show_info(
                self.root,
                "Profile Loaded",
                f"Successfully loaded profile from '{os.path.basename(file_path)}'!\n\n"
                f"Name: {name_str}\n"
                f"Version: {b_ver}"
            )

        except Exception as e:
            CustomMessagebox.show_error(self.root, "Load Error", f"Failed to parse binary file:\n{str(e)}")

    def save_otp_dialog(self):
        otp_data, clean_name = self.get_otp_binary_data()
        if otp_data is None:
            return
            
        file_path = filedialog.asksaveasfilename(
            defaultextension=".bin",
            filetypes=[("Binary Files", "*.bin"), ("All Files", "*.*")],
            initialfile=f"{clean_name.lower()}_otp.bin",
            title="Save OTP Binary File"
        )
        
        if file_path:
            try:
                with open(file_path, "wb") as f:
                    f.write(otp_data)
                CustomMessagebox.show_info(self.root, "Success", f"Successfully generated & saved OTP file:\n{os.path.basename(file_path)}")
            except Exception as e:
                CustomMessagebox.show_error(self.root, "Error", f"Failed to save file:\n{str(e)}")

    def ensure_cli_path(self):
        if not self.cli_path:
            # Let user find it manually
            CustomMessagebox.show_info(self.root, "CLI Not Found", "STM32_Programmer_CLI.exe was not found in standard paths.\nPlease browse and select it manually.")
            selected_path = filedialog.askopenfilename(
                title="Locate STM32_Programmer_CLI.exe",
                filetypes=[("Executables", "*.exe"), ("All Files", "*.*")],
                initialfile="STM32_Programmer_CLI.exe"
            )
            if selected_path and os.path.basename(selected_path) == "STM32_Programmer_CLI.exe":
                self.cli_path = selected_path
            else:
                return False
        return True

    def flash_otp(self):
        if not self.ensure_cli_path():
            CustomMessagebox.show_error(self.root, "Abort", "Flashing aborted. STM32_Programmer_CLI.exe is required for flashing.")
            return

        # Confirm the write operation (One-Time programmable warning with At Your Own Risk disclaimer!)
        confirm = CustomMessagebox.ask_yes_no(self.root, 
            "⚠️ CRITICAL WARNING",
            "OTP (One-Time Programmable) memory can ONLY be written ONCE!\n"
            "If you write incorrect data, you will NOT be able to change or erase it.\n\n"
            "Proceeding is done entirely AT YOUR OWN RISK.\n"
            "Do you want to flash the OTP profile now?"
        )
        if not confirm:
            return

        # Pack data
        otp_data, clean_name = self.get_otp_binary_data()
        if otp_data is None:
            return

        # Save to a temporary file for flashing
        temp_file = os.path.join(os.environ.get("TEMP", "."), "temp_otp_flipper.bin")
        try:
            with open(temp_file, "wb") as f:
                f.write(otp_data)
        except Exception as e:
            CustomMessagebox.show_error(self.root, "Error", f"Failed to write temp file:\n{str(e)}")
            return

        self.flash_btn.config(state="disabled", text="Flashing...")
        self.root.update()

        try:
            cmd = [
                self.cli_path,
                "-c", "port=USB1",
                "-w", temp_file, "0x1FFF7000",
                "-v" # Verify after write
            ]
            
            # Run ST CLI and capture output
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
            
            # Cleanup temp file
            if os.path.exists(temp_file):
                os.remove(temp_file)
                
            if result.returncode == 0 and "File download complete" in result.stdout:
                CustomMessagebox.show_info(self.root, "Success!", f"Successfully flashed OTP profile '{clean_name}' to address 0x1FFF7000!\n\nYou can now reboot the device.")
            else:
                err_log = clean_cli_log(result.stdout + "\n" + result.stderr)
                if "Connection to target not established" in err_log or "No STM32 device in DFU mode" in err_log:
                    CustomMessagebox.show_error(self.root, "Connection Error", "No STM32 DFU device detected.\n\nPlease ensure you hold the BOOT0 button while connecting the USB cable to the PC, then try again.")
                else:
                    CustomMessagebox.show_error(self.root, "Flashing Failed", f"STM32_Programmer_CLI exited with error code {result.returncode}.\n\nLog Details:\n{err_log}")
                    
        except Exception as e:
            CustomMessagebox.show_error(self.root, "Process Error", f"Failed to start flash utility:\n{str(e)}")
        finally:
            self.flash_btn.config(state="normal" if self._dfu_result else "disabled", text="Flash (DFU)")

    def read_otp(self):
        if not self.ensure_cli_path():
            return

        self.read_btn.config(state="disabled", text="Reading...")
        self.root.update()

        temp_file = os.path.join(os.environ.get("TEMP", "."), "temp_otp_read.bin")
        
        try:
            # Command: -c port=USB1 -r 0x1FFF7000 32 temp_file
            cmd = [
                self.cli_path,
                "-c", "port=USB1",
                "-r", "0x1FFF7000", "32", temp_file
            ]
            
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
            err_log = clean_cli_log(result.stdout + "\n" + result.stderr)

            if result.returncode != 0 or not os.path.isfile(temp_file):
                if "Connection to target not established" in err_log or "No STM32 device in DFU mode" in err_log:
                    CustomMessagebox.show_error(self.root, "Connection Error", "No STM32 DFU device detected.\n\nPlease ensure you hold the BOOT0 button while connecting the USB cable to the PC, then try again.")
                else:
                    CustomMessagebox.show_error(self.root, "Reading Failed", f"Failed to read OTP memory.\n\nLog Details:\n{err_log}")
                return

            # Read binary file
            with open(temp_file, "rb") as f:
                data = f.read(32)

            if os.path.exists(temp_file):
                os.remove(temp_file)

            if len(data) < 32:
                CustomMessagebox.show_error(self.root, "Error", "Read incomplete data from device memory.")
                return

            # Unpack structure
            # Header Magic (uint16), Header Version (uint8), Reserved (uint8), Timestamp (uint32)
            # Board Version (uint8), Target (uint8), Body (uint8), Connect (uint8), Display (uint8), Res2_0 (uint8), Res2_1 (uint16)
            # Color (uint8), Region (uint8), Res3_0 (uint16), Res3_1 (uint32)
            # Name (char[8])
            header_magic, header_ver, _, timestamp, \
            b_ver, b_target, b_body, b_connect, b_display, _, _, \
            b_color, b_region, _, _, \
            name_bytes = struct.unpack("<HBBI BBBBBBH BBHI 8s", data)

            # Check if OTP is clean (all 0xFF)
            if header_magic == 0xFFFF and timestamp == 0xFFFFFFFF:
                CustomMessagebox.show_info(self.root, "OTP Status", "🎉 OTP memory is completely EMPTY (0xFF).\n\nYou can safely write your custom profile now!")
                return

            # Decode name
            name_str = name_bytes.decode('ascii', errors='ignore').strip('\x00')
            if not name_str.strip():
                name_str = "[Not Set]"

            # Map helper values
            display_str = "MGG (Monochrome Glass Grid)" if b_display == 2 else ("ERC" if b_display == 1 else f"Unknown ({b_display})")
            color_str = "Black" if b_color == 1 else ("White" if b_color == 2 else ("Transparent" if b_color == 3 else f"Unknown ({b_color})"))
            region_str = "Europe / Russia" if b_region == 1 else ("USA / Canada / Australia" if b_region == 2 else ("Japan" if b_region == 3 else ("World" if b_region == 4 else f"Unknown ({b_region})")))
            
            time_str = "Unknown"
            if timestamp != 0xFFFFFFFF:
                try:
                    time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(timestamp))
                except Exception:
                    pass

            info_msg = (
                f"📝 Current Device OTP Configuration:\n"
                f"--------------------------------------------------\n"
                f"• Device Name:     {name_str}\n"
                f"• Board Version:   {b_ver}\n"
                f"• Display Type:    {display_str}\n"
                f"• Body Color:      {color_str}\n"
                f"• Region:          {region_str}\n"
                f"• Provision Date:  {time_str}\n"
                f"--------------------------------------------------\n"
                f"• Magic:           {hex(header_magic)} (Version {header_ver})\n"
                f"• MCU Target:      {b_target} | Body: {b_body} | Connect: {b_connect}\n\n"
                f"💡 Note: Display Type 'MGG' is required for the SSD1306 OLED screen to function."
            )
            CustomMessagebox.show_info(self.root, "Device OTP Info", info_msg)

        except Exception as e:
            CustomMessagebox.show_error(self.root, "Process Error", f"An error occurred while reading OTP:\n{str(e)}")
        finally:
            self.read_btn.config(state="normal" if self._dfu_result else "disabled", text="Read (DFU)")

if __name__ == "__main__":
    root = tk.Tk()
    app = OTPGeneratorApp(root)
    root.mainloop()
