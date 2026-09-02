#!/usr/bin/env python3
import os
import sys
import json
import time
import glob
import urllib.request
import urllib.error
import zipfile
import tarfile
import tempfile
import threading
import subprocess
import random
import string
import shutil
import tkinter as tk
from tkinter import messagebox, filedialog
from tkinter import ttk
import serial.tools.list_ports as list_ports

# Add scripts directory to path to reuse flipper modules
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from flipper.storage import FlipperStorage, FlipperStorageOperations
from flipper.utils.cdc import resolve_port

# Premium Frosted Glass (Acrylic/Mica) colors with Flipper Orange accents
BG_COLOR = "#05080f"  # Deep cosmic background
CONTAINER_COLOR = "#0c101b"  # Obsidian glass base
TEXT_COLOR = "#00d2ff"  # Glowing ice-blue
TEXT_MUTED = "#82daff"  # Ice-blue secondary
ACCENT_COLOR = "#ff6c00"  # Flipper Orange
BUTTON_ACTIVE = "#ff8533"
LOG_BG = "#03060a"

GITHUB_REPO = "AJ60/Flipper_OLED_PCF8574_PN532"
STM32_DFU_VID = 0x0483
STM32_DFU_PID = 0xDF11


def apply_button_hover(btn, bg_active, fg_active, bg_idle, fg_idle):
    btn.bind(
        "<Enter>", lambda e: btn.config(background=bg_active, foreground=fg_active)
    )
    btn.bind("<Leave>", lambda e: btn.config(background=bg_idle, foreground=fg_idle))


class Tooltip:
    def __init__(self, widget, text):
        self.widget = widget
        self.text = text
        self.tipwindow = None
        self.id = None
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
        label = tk.Label(
            tw,
            text=self.text,
            justify="left",
            background="#0c101b",
            foreground="#82daff",
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground=ACCENT_COLOR,
            font=("Segoe UI", 9, "bold"),
            padx=10,
            pady=5,
        )
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

        self.title(title)
        self.resizable(False, False)
        self.configure(bg=BG_COLOR)
        self.wm_overrideredirect(1)

        parent.update_idletasks()
        parent_x = parent.winfo_rootx()
        parent_y = parent.winfo_rooty()
        parent_w = parent.winfo_width()
        parent_h = parent.winfo_height()

        import math

        visual_lines = 0
        for line in message.split("\n"):
            line_len = len(line.strip())
            if line_len == 0:
                visual_lines += 1
            else:
                visual_lines += math.ceil(line_len / 48)

        w = 420
        h = min(550, max(190, visual_lines * 20 + 130))

        x = parent_x + (parent_w - w) // 2
        y = parent_y + (parent_h - h) // 2
        self.geometry(f"{w}x{h}+{x}+{y}")

        # Outer border
        outer_frame = tk.Frame(
            self,
            bg=CONTAINER_COLOR,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground=ACCENT_COLOR,
        )
        outer_frame.pack(fill="both", expand=True)

        # Title bar
        title_lbl = tk.Label(
            outer_frame,
            text=title.upper(),
            font=("Segoe UI", 10, "bold"),
            fg=ACCENT_COLOR,
            bg=CONTAINER_COLOR,
        )
        title_lbl.pack(fill="x", pady=(10, 5))

        # Message Text
        msg_lbl = tk.Label(
            outer_frame,
            text=message,
            font=("Segoe UI", 10),
            fg=TEXT_COLOR,
            bg=CONTAINER_COLOR,
            justify="center",
            wraplength=380,
        )
        msg_lbl.pack(fill="both", expand=True, padx=20, pady=10)

        # Buttons
        btn_frame = tk.Frame(outer_frame, bg=CONTAINER_COLOR)
        btn_frame.pack(fill="x", side="bottom", pady=15)

        for choice in self.choices:
            btn = tk.Button(
                btn_frame,
                text=choice,
                font=("Segoe UI", 9, "bold"),
                bg=BG_COLOR,
                fg=TEXT_COLOR,
                activebackground=ACCENT_COLOR,
                activeforeground=BG_COLOR,
                relief="solid",
                borderwidth=1,
                highlightthickness=1,
                highlightbackground="#1b2a47",
                padx=15,
                pady=5,
                command=lambda c=choice: self.set_result(c),
            )
            btn.pack(side="left", expand=True, padx=10)
            apply_button_hover(btn, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        self.transient(parent)
        self.grab_set()
        self.parent.wait_window(self)

    def set_result(self, choice):
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
        box = CustomMessagebox(
            parent, title, message, box_type="warning", choices=["NO", "YES"]
        )
        return box.result == "YES"


class DIYFlasherApp:
    def __init__(self, root):
        self.root = root
        self.root.title("🐬 DIY Flipper Zero Flasher & Updater")
        self.root.geometry("820x680")
        self.root.configure(bg=BG_COLOR)

        # Windows 11 dark/acrylic title bar configuration
        self.root.update()
        try:
            import ctypes

            hwnd = ctypes.windll.user32.GetParent(self.root.winfo_id())
            DWMWA_USE_IMMERSIVE_DARK_MODE = 20
            use_dark_mode = ctypes.c_int(1)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd,
                DWMWA_USE_IMMERSIVE_DARK_MODE,
                ctypes.byref(use_dark_mode),
                ctypes.sizeof(use_dark_mode),
            )
            DWMWA_SYSTEMBACKDROP_TYPE = 38
            backdrop_type = ctypes.c_int(3)  # Acrylic
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd,
                DWMWA_SYSTEMBACKDROP_TYPE,
                ctypes.byref(backdrop_type),
                ctypes.sizeof(backdrop_type),
            )
            self.root.attributes("-alpha", 0.90)
        except Exception:
            pass

        # Stylize combobox
        self.style = ttk.Style()
        self.style.theme_use("clam")
        self.style.configure(
            "TCombobox",
            fieldbackground=BG_COLOR,
            background=CONTAINER_COLOR,
            foreground=TEXT_COLOR,
            darkcolor="#1b2a47",
            lightcolor="#1b2a47",
            bordercolor="#1b2a47",
            arrowcolor=ACCENT_COLOR,
        )
        self.style.map(
            "TCombobox",
            fieldbackground=[("readonly", BG_COLOR), ("focus", BG_COLOR)],
            foreground=[("readonly", TEXT_COLOR), ("focus", TEXT_COLOR)],
            selectbackground=[("readonly", BG_COLOR), ("focus", BG_COLOR)],
            selectforeground=[("readonly", TEXT_COLOR), ("focus", TEXT_COLOR)],
            background=[
                ("readonly", CONTAINER_COLOR),
                ("focus", CONTAINER_COLOR),
                ("active", CONTAINER_COLOR),
            ],
            arrowcolor=[
                ("readonly", ACCENT_COLOR),
                ("focus", ACCENT_COLOR),
                ("active", ACCENT_COLOR),
            ],
            bordercolor=[("focus", TEXT_COLOR), ("active", TEXT_COLOR)],
        )

        self.root.option_add("*TCombobox*Listbox.background", BG_COLOR)
        self.root.option_add("*TCombobox*Listbox.foreground", TEXT_COLOR)
        self.root.option_add("*TCombobox*Listbox.selectBackground", ACCENT_COLOR)
        self.root.option_add("*TCombobox*Listbox.selectForeground", BG_COLOR)

        self.cli_path = self.find_stm32_programmer_cli()
        self.dfu_util_path = self.find_dfu_util()

        # State variables
        self.connected_device_mode = "Disconnected"  # Disconnected, DFU, Serial
        self.connected_port = None
        self.releases = []
        self.selected_file_path = None

        self.create_widgets()

        # Start connection checker thread
        self.check_connection_loop()

        # Fetch GitHub releases
        self.fetch_releases_async()

    def create_widgets(self):
        # 1. Header Frame
        header = tk.Frame(self.root, bg=BG_COLOR)
        header.pack(fill="x", padx=20, pady=15)

        title_lbl = tk.Label(
            header,
            text="🐬 DIY FLIPPER ZERO FLASHER",
            font=("Segoe UI", 16, "bold"),
            fg=ACCENT_COLOR,
            bg=BG_COLOR,
        )
        title_lbl.pack(side="left")

        self.status_lbl = tk.Label(
            header,
            text="🔌 Checking device status...",
            font=("Segoe UI", 10, "bold"),
            fg=TEXT_MUTED,
            bg=BG_COLOR,
        )
        self.status_lbl.pack(side="right")

        # 2. Main tabs body
        notebook_frame = tk.Frame(self.root, bg=BG_COLOR)
        notebook_frame.pack(fill="both", expand=True, padx=20)

        # Tabs selectors
        tab_buttons = tk.Frame(notebook_frame, bg=BG_COLOR)
        tab_buttons.pack(fill="x")

        self.tab_container = tk.Frame(
            notebook_frame,
            bg=CONTAINER_COLOR,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
        )
        self.tab_container.pack(fill="both", expand=True)

        # Define tabs
        self.tab1 = tk.Frame(self.tab_container, bg=CONTAINER_COLOR)
        self.tab2 = tk.Frame(self.tab_container, bg=CONTAINER_COLOR)
        self.tab3 = tk.Frame(self.tab_container, bg=CONTAINER_COLOR)

        self.tabs = [self.tab1, self.tab2, self.tab3]
        self.active_tab = 0

        def show_tab(index):
            for i, tab in enumerate(self.tabs):
                if i == index:
                    tab.pack(fill="both", expand=True, padx=15, pady=15)
                    self.tab_btns[i].config(bg=ACCENT_COLOR, fg=BG_COLOR)
                else:
                    tab.pack_forget()
                    self.tab_btns[i].config(bg=BG_COLOR, fg=TEXT_COLOR)
            self.active_tab = index

        self.tab_btns = []
        btn1 = tk.Button(
            tab_buttons,
            text="GITHUB UPDATES",
            font=("Segoe UI", 9, "bold"),
            command=lambda: show_tab(0),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            padx=15,
            pady=5,
        )
        btn1.pack(side="left", padx=(0, 5))
        self.tab_btns.append(btn1)
        apply_button_hover(btn1, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        btn2 = tk.Button(
            tab_buttons,
            text="LOCAL FILE",
            font=("Segoe UI", 9, "bold"),
            command=lambda: show_tab(1),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            padx=15,
            pady=5,
        )
        btn2.pack(side="left", padx=5)
        self.tab_btns.append(btn2)
        apply_button_hover(btn2, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        btn3 = tk.Button(
            tab_buttons,
            text="OTP PROVISIONING",
            font=("Segoe UI", 9, "bold"),
            command=lambda: show_tab(2),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            padx=15,
            pady=5,
        )
        btn3.pack(side="left", padx=5)
        self.tab_btns.append(btn3)
        apply_button_hover(btn3, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        # Initialize Tab 1 (GitHub)
        self.setup_github_tab()

        # Initialize Tab 2 (Local File)
        self.setup_local_tab()

        # Initialize Tab 3 (OTP)
        self.setup_otp_tab()

        show_tab(0)

        # 3. Log Console at the bottom
        log_frame = tk.Frame(self.root, bg=BG_COLOR)
        log_frame.pack(fill="x", padx=20, pady=15)

        log_lbl = tk.Label(
            log_frame,
            text="LOG CONSOLE OUTPUT",
            font=("Segoe UI", 9, "bold"),
            fg=TEXT_MUTED,
            bg=BG_COLOR,
        )
        log_lbl.pack(anchor="w")

        self.log_txt = tk.Text(
            log_frame,
            height=8,
            bg=LOG_BG,
            fg=TEXT_COLOR,
            font=("Consolas", 9),
            relief="solid",
            borderwidth=1,
            highlightbackground="#1b2a47",
        )
        self.log_txt.pack(fill="x")

        # 4. Status Bar/Progress Bar
        progress_frame = tk.Frame(self.root, bg=BG_COLOR)
        progress_frame.pack(fill="x", padx=20, pady=(0, 10))

        self.progress_bar = ttk.Progressbar(progress_frame, mode="determinate")
        self.progress_bar.pack(fill="x", side="left", expand=True)

        # Add custom style for progress bar to be orange
        self.style.configure(
            "TProgressbar", thickness=10, troughcolor=BG_COLOR, background=ACCENT_COLOR
        )

    def log(self, msg, level="INFO"):
        self.log_txt.insert(tk.END, f"[{level}] {msg}\n")
        self.log_txt.see(tk.END)

    def log_clear(self):
        self.log_txt.delete("1.0", tk.END)

    # Tab 1: GitHub Updates Setup
    def setup_github_tab(self):
        tk.Label(
            self.tab1,
            text="SELECT FIRMWARE VERSION FROM GITHUB",
            font=("Segoe UI", 10, "bold"),
            fg=TEXT_COLOR,
            bg=CONTAINER_COLOR,
        ).pack(anchor="w", pady=(0, 5))

        self.gh_combo = ttk.Combobox(self.tab1, state="readonly", font=("Segoe UI", 10))
        self.gh_combo.pack(fill="x", pady=5)
        self.gh_combo.bind("<<ComboboxSelected>>", self.on_release_selected)

        tk.Label(
            self.tab1,
            text="RELEASE DETAILS",
            font=("Segoe UI", 9, "bold"),
            fg=TEXT_MUTED,
            bg=CONTAINER_COLOR,
        ).pack(anchor="w", pady=(10, 2))

        self.release_info_txt = tk.Text(
            self.tab1,
            height=10,
            bg=BG_COLOR,
            fg=TEXT_MUTED,
            font=("Segoe UI", 9),
            wrap="word",
            relief="solid",
            borderwidth=1,
            highlightbackground="#1b2a47",
        )
        self.release_info_txt.pack(fill="both", expand=True, pady=5)

        self.gh_action_btn = tk.Button(
            self.tab1,
            text="DOWNLOAD & INSTALL",
            font=("Segoe UI", 10, "bold"),
            bg=ACCENT_COLOR,
            fg=BG_COLOR,
            activebackground=BUTTON_ACTIVE,
            activeforeground=BG_COLOR,
            relief="flat",
            pady=8,
            command=self.on_github_action,
        )
        self.gh_action_btn.pack(fill="x", pady=(10, 0))
        apply_button_hover(
            self.gh_action_btn, BUTTON_ACTIVE, BG_COLOR, ACCENT_COLOR, BG_COLOR
        )

    # Tab 2: Local File Setup
    def setup_local_tab(self):
        tk.Label(
            self.tab2,
            text="FLASH LOCAL FIRMWARE OR UPDATE PACKAGE",
            font=("Segoe UI", 10, "bold"),
            fg=TEXT_COLOR,
            bg=CONTAINER_COLOR,
        ).pack(anchor="w", pady=(0, 5))

        browse_frame = tk.Frame(self.tab2, bg=CONTAINER_COLOR)
        browse_frame.pack(fill="x", pady=10)

        self.file_entry = tk.Entry(
            browse_frame,
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            font=("Segoe UI", 10),
            relief="solid",
            borderwidth=1,
            highlightbackground="#1b2a47",
        )
        self.file_entry.pack(side="left", fill="x", expand=True, ipady=4, padx=(0, 5))

        browse_btn = tk.Button(
            browse_frame,
            text="BROWSE...",
            font=("Segoe UI", 9, "bold"),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            command=self.browse_local_file,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            padx=15,
            pady=4,
        )
        browse_btn.pack(side="right")
        apply_button_hover(browse_btn, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        # File info panel
        info_frame = tk.Frame(self.tab2, bg=CONTAINER_COLOR)
        info_frame.pack(fill="both", expand=True, pady=10)

        self.local_file_details = tk.Label(
            info_frame,
            text="Please select a file:\n\n• .tgz files are update packages (flashed in Serial/COM mode)\n• .dfu/.bin files are full images (flashed in DFU Recovery mode)",
            font=("Segoe UI", 9),
            fg=TEXT_MUTED,
            bg=BG_COLOR,
            relief="solid",
            borderwidth=1,
            justify="left",
            anchor="nw",
            padx=15,
            pady=15,
        )
        self.local_file_details.pack(fill="both", expand=True)

        self.local_action_btn = tk.Button(
            self.tab2,
            text="INSTALL / FLASH LOCAL FILE",
            font=("Segoe UI", 10, "bold"),
            bg=ACCENT_COLOR,
            fg=BG_COLOR,
            activebackground=BUTTON_ACTIVE,
            activeforeground=BG_COLOR,
            relief="flat",
            pady=8,
            state="disabled",
            command=self.on_local_action,
        )
        self.local_action_btn.pack(fill="x", pady=(10, 0))
        apply_button_hover(
            self.local_action_btn, BUTTON_ACTIVE, BG_COLOR, ACCENT_COLOR, BG_COLOR
        )

    # Tab 3: OTP Setup
    def setup_otp_tab(self):
        tk.Label(
            self.tab3,
            text="GENERATE & PROVISION HARDWARE OTP PROFILE",
            font=("Segoe UI", 10, "bold"),
            fg=TEXT_COLOR,
            bg=CONTAINER_COLOR,
        ).pack(anchor="w", pady=(0, 10))

        # OTP form layout
        form_frame = tk.Frame(self.tab3, bg=CONTAINER_COLOR)
        form_frame.pack(fill="x")

        # Fields configuration
        fields = [
            ("Device Name:", "DeviceName", "Enter 8-char Flipper Name", "Ronaldo"),
            (
                "Board Version:",
                "BoardVersion",
                "Select Board revision number (e.g. 12)",
                "12",
            ),
            (
                "Display Type:",
                "DisplayType",
                "MGG = SSD1306 OLED (DIY), ERC = standard LCD",
                ["MGG", "ERC"],
            ),
            (
                "Body Color:",
                "BodyColor",
                "Color used in animations",
                ["Black", "White", "Transparent"],
            ),
            (
                "Sub-GHz Region:",
                "Region",
                "Radio frequency band locks",
                ["Europe", "USA", "Japan", "World"],
            ),
        ]

        self.otp_vars = {}

        for idx, (label_text, var_name, tooltip_txt, default_val) in enumerate(fields):
            row_frame = tk.Frame(form_frame, bg=CONTAINER_COLOR)
            row_frame.pack(fill="x", pady=6)

            lbl = tk.Label(
                row_frame,
                text=label_text,
                font=("Segoe UI", 10),
                fg=TEXT_MUTED,
                bg=CONTAINER_COLOR,
                width=15,
                anchor="w",
            )
            lbl.pack(side="left")
            Tooltip(lbl, tooltip_txt)

            if isinstance(default_val, list):
                # Combobox
                var = tk.StringVar(value=default_val[0])
                cb = ttk.Combobox(
                    row_frame,
                    textvariable=var,
                    values=default_val,
                    state="readonly",
                    font=("Segoe UI", 10),
                )
                cb.pack(side="left", fill="x", expand=True)
                self.otp_vars[var_name] = var
            else:
                # Entry
                var = tk.StringVar(value=default_val)
                ent = tk.Entry(
                    row_frame,
                    textvariable=var,
                    bg=BG_COLOR,
                    fg=TEXT_COLOR,
                    font=("Segoe UI", 10),
                    relief="solid",
                    borderwidth=1,
                    highlightbackground="#1b2a47",
                )
                ent.pack(side="left", fill="x", expand=True, ipady=2)
                self.otp_vars[var_name] = var

        # Form actions
        actions_frame = tk.Frame(self.tab3, bg=CONTAINER_COLOR)
        actions_frame.pack(fill="x", pady=(20, 0))

        save_btn = tk.Button(
            actions_frame,
            text="1. SAVE .BIN FILE",
            font=("Segoe UI", 9, "bold"),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            command=self.save_otp_bin,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            pady=8,
        )
        save_btn.pack(side="left", fill="x", expand=True, padx=(0, 5))
        apply_button_hover(save_btn, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR)

        self.otp_flash_btn = tk.Button(
            actions_frame,
            text="2. FLASH OTP (DFU)",
            font=("Segoe UI", 9, "bold"),
            bg=ACCENT_COLOR,
            fg=BG_COLOR,
            command=self.flash_otp_dfu,
            relief="flat",
            pady=8,
            state="disabled",
        )
        self.otp_flash_btn.pack(side="left", fill="x", expand=True, padx=5)
        apply_button_hover(
            self.otp_flash_btn, BUTTON_ACTIVE, BG_COLOR, ACCENT_COLOR, BG_COLOR
        )

        self.otp_read_btn = tk.Button(
            actions_frame,
            text="3. READ OTP (DFU)",
            font=("Segoe UI", 9, "bold"),
            bg=BG_COLOR,
            fg=TEXT_COLOR,
            command=self.read_otp_dfu,
            relief="solid",
            borderwidth=1,
            highlightthickness=1,
            highlightbackground="#1b2a47",
            pady=8,
            state="disabled",
        )
        self.otp_read_btn.pack(side="left", fill="x", expand=True, padx=(5, 0))
        apply_button_hover(
            self.otp_read_btn, ACCENT_COLOR, BG_COLOR, BG_COLOR, TEXT_COLOR
        )

    # 5. Find programmer binaries
    def find_stm32_programmer_cli(self):
        for path in ST_PROG_CLI_PATHS:
            if os.path.exists(path):
                return path
        try:
            subprocess.run(
                ["STM32_Programmer_CLI", "--version"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return "STM32_Programmer_CLI"
        except FileNotFoundError:
            pass
        return None

    def find_dfu_util(self):
        try:
            subprocess.run(
                ["dfu-util", "--version"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return "dfu-util"
        except FileNotFoundError:
            pass
        return None

    # Connection detection loop (runs on background thread, updates GUI)
    def check_connection_loop(self):
        def check():
            while True:
                mode = "Disconnected"
                port = None

                # Check DFU mode
                dfu_connected = False
                if self.cli_path:
                    try:
                        res = subprocess.run(
                            [self.cli_path, "-l", "usb"],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW,
                            timeout=1.5,
                        )
                        for line in res.stdout.split("\n"):
                            if "Total number of available STM32 device" in line:
                                parts = line.split(":")
                                if len(parts) > 1 and int(parts[1].strip()) > 0:
                                    dfu_connected = True
                                break
                    except Exception:
                        pass

                if not dfu_connected:
                    # Alternative check via Powershell or listing usb devices
                    try:
                        cmd = [
                            "powershell",
                            "-Command",
                            "Get-PnpDevice -Present | Where-Object { $_.InstanceId -like '*USB*' -and ($_.Name -like '*STM32*Bootloader*' -or $_.Name -like '*DFU*') }",
                        ]
                        res = subprocess.run(
                            cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            text=True,
                            creationflags=subprocess.CREATE_NO_WINDOW,
                            timeout=1.5,
                        )
                        if "Bootloader" in res.stdout or "DFU" in res.stdout:
                            dfu_connected = True
                    except Exception:
                        pass

                if dfu_connected:
                    mode = "DFU Recovery Mode"
                else:
                    # Scan COM Ports
                    flippers = list(list_ports.grep("flip_"))
                    if len(flippers) == 0:
                        # Fallback: scan for any STM32 or general CDC virtual ports
                        all_ports = list_ports.comports()
                        for p in all_ports:
                            desc = p.description.lower()
                            if (
                                "stm" in desc
                                or "virtual com" in desc
                                or "flipper" in desc
                            ):
                                flippers.append(p)
                                break

                    if flippers:
                        mode = "Serial Mode"
                        port = flippers[0].device

                self.root.after(0, self.update_connection_status, mode, port)
                time.sleep(2)

        threading.Thread(target=check, daemon=True).start()

    def update_connection_status(self, mode, port):
        self.connected_device_mode = mode
        self.connected_port = port

        if mode == "DFU Recovery Mode":
            self.status_lbl.config(
                text="🟢 STATUS: DFU RECOVERY MODE (BOOT0)", fg="#00d2ff"
            )
            self.otp_flash_btn.config(state="normal")
            self.otp_read_btn.config(state="normal")
        elif mode == "Serial Mode":
            self.status_lbl.config(text=f"🟢 STATUS: CONNECTED ON {port}", fg="#ff6c00")
            self.otp_flash_btn.config(state="disabled")
            self.otp_read_btn.config(state="disabled")
        else:
            self.status_lbl.config(text="🔴 STATUS: DISCONNECTED", fg="#ff4500")
            self.otp_flash_btn.config(state="disabled")
            self.otp_read_btn.config(state="disabled")

        # Re-evaluate local action button state
        self.update_action_buttons()

    def update_action_buttons(self):
        # GitHub action state
        if not self.releases or self.gh_combo.get() == "":
            self.gh_action_btn.config(state="disabled")
        else:
            self.gh_action_btn.config(state="normal")

        # Local file action state
        if not self.selected_file_path:
            self.local_action_btn.config(
                state="disabled", text="INSTALL / FLASH LOCAL FILE"
            )
            return

        fn = self.selected_file_path.lower()
        if fn.endswith(".tgz"):
            if self.connected_device_mode == "Serial Mode":
                self.local_action_btn.config(
                    state="normal", text="INSTALL OTA PACKAGE (SERIAL)"
                )
            else:
                self.local_action_btn.config(
                    state="disabled", text="TGZ REQUIRES SERIAL MODE"
                )
        elif fn.endswith(".dfu") or fn.endswith(".bin"):
            if self.connected_device_mode == "DFU Recovery Mode":
                self.local_action_btn.config(
                    state="normal", text="FLASH FIRMWARE (DFU)"
                )
            else:
                self.local_action_btn.config(
                    state="disabled", text="DFU/BIN REQUIRES DFU MODE"
                )
        else:
            self.local_action_btn.config(
                state="disabled", text="UNSUPPORTED FILE FORMAT"
            )

    # 6. Fetch Releases from GitHub
    def fetch_releases_async(self):
        def fetch():
            self.log("Fetching releases from GitHub...")
            url = f"https://api.github.com/repos/{GITHUB_REPO}/releases"
            req = urllib.request.Request(
                url, headers={"User-Agent": "DIY-Flipper-Flasher"}
            )
            try:
                with urllib.request.urlopen(req, timeout=5) as response:
                    data = json.loads(response.read().decode())
                    self.releases = data
                    self.root.after(0, self.on_releases_fetched)
            except Exception as e:
                self.log(f"Failed to fetch releases: {str(e)}", "ERROR")

        threading.Thread(target=fetch, daemon=True).start()

    def on_releases_fetched(self):
        if not self.releases:
            self.log("No releases found on GitHub.", "WARNING")
            return

        self.log(f"Fetched {len(self.releases)} releases from GitHub successfully!")
        tags = [release["tag_name"] for release in self.releases]
        self.gh_combo.config(values=tags)
        self.gh_combo.current(0)
        self.on_release_selected(None)

    def on_release_selected(self, event):
        idx = self.gh_combo.current()
        if idx < 0 or idx >= len(self.releases):
            return

        release = self.releases[idx]
        body = release.get("body", "No description provided.")

        self.release_info_txt.delete("1.0", tk.END)
        self.release_info_txt.insert(tk.END, f"TITLE: {release['name']}\n")
        self.release_info_txt.insert(tk.END, f"TAG: {release['tag_name']}\n")
        self.release_info_txt.insert(tk.END, f"DATE: {release['published_at']}\n")
        self.release_info_txt.insert(tk.END, "-" * 50 + "\n\n")
        self.release_info_txt.insert(tk.END, body)

        self.update_action_buttons()

    # 7. Local File Actions
    def browse_local_file(self):
        filepath = filedialog.askopenfilename(
            title="Select Flipper Firmware File",
            filetypes=[
                ("All Files (*.*)", "*.*"),
                ("Supported files", "*.tgz;*.dfu;*.bin;*.zip;*.tar.gz"),
                ("Update packages (.tgz, .zip)", "*.tgz;*.zip;*.tar.gz"),
                ("DFU Recovery files (.dfu, .bin)", "*.dfu;*.bin"),
            ],
        )
        if not filepath:
            return

        self.selected_file_path = filepath
        self.file_entry.delete(0, tk.END)
        self.file_entry.insert(0, filepath)

        # Calculate file size
        size_mb = os.path.getsize(filepath) / (1024 * 1024)
        fn = os.path.basename(filepath)

        info = f"File: {fn}\nSize: {size_mb:.2f} MB\n\n"
        if fn.lower().endswith(".tgz"):
            info += "Type: OTA self-update package.\nRequired Mode: Serial (COM) Mode."
        elif fn.lower().endswith(".dfu"):
            info += "Type: DfuSe Firmware Image.\nRequired Mode: DFU Recovery Mode."
        elif fn.lower().endswith(".bin"):
            info += "Type: Raw binary image.\nRequired Mode: DFU Recovery Mode."

        self.local_file_details.config(text=info)
        self.update_action_buttons()

    def on_local_action(self):
        if not self.selected_file_path:
            return

        fn = self.selected_file_path
        if fn.lower().endswith(".tgz"):
            self.run_serial_ota_install(fn)
        elif fn.lower().endswith(".dfu") or fn.lower().endswith(".bin"):
            self.run_dfu_flash(fn)

    # 8. GitHub Action implementation
    def on_github_action(self):
        idx = self.gh_combo.current()
        if idx < 0 or idx >= len(self.releases):
            return

        release = self.releases[idx]
        assets = release.get("assets", [])

        # Check current device mode to decide which asset to download
        target_extension = (
            ".tgz" if self.connected_device_mode == "Serial Mode" else ".dfu"
        )
        if self.connected_device_mode == "Disconnected":
            CustomMessagebox.show_error(
                self.root,
                "Device Disconnected",
                "Please connect your DIY Flipper Zero in either Serial or DFU Recovery mode first.",
            )
            return

        matching_assets = [
            a for a in assets if a["name"].lower().endswith(target_extension)
        ]
        if not matching_assets and target_extension == ".dfu":
            # Fallback to .bin if .dfu is not available
            matching_assets = [a for a in assets if a["name"].lower().endswith(".bin")]

        if not matching_assets:
            CustomMessagebox.show_error(
                self.root,
                "No Matching Asset",
                f"Could not find a suitable {target_extension} file in this GitHub release.",
            )
            return

        asset = matching_assets[0]
        download_url = asset["browser_download_url"]
        filename = asset["name"]

        # Confirm write if in DFU Mode (it is destructive)
        if self.connected_device_mode == "DFU Recovery Mode":
            if not CustomMessagebox.ask_yes_no(
                self.root,
                "Confirm Recovery Flash",
                "Are you sure you want to write this firmware? This will overwrite the entire memory of the device.",
            ):
                return

        self.gh_action_btn.config(state="disabled", text="Downloading...")
        self.log(f"Downloading {filename} from GitHub...")

        def download_and_install_thread():
            temp_dir = tempfile.gettempdir()
            target_path = os.path.join(temp_dir, filename)

            try:
                # Download with progress bar reporting
                def report(blocknum, blocksize, totalsize):
                    readsofar = blocknum * blocksize
                    if totalsize > 0:
                        percent = min(100, int(readsofar * 100 / totalsize))
                        self.root.after(0, self.update_progress, percent)

                urllib.request.urlretrieve(download_url, target_path, report)
                self.log("Download complete!")

                # Initiate flashing
                if target_path.lower().endswith(".tgz"):
                    self.run_serial_ota_install(target_path)
                else:
                    self.run_dfu_flash(target_path)

            except Exception as e:
                self.log(f"Download/Flash failed: {str(e)}", "ERROR")
                self.root.after(0, self.reset_progress)

            finally:
                self.root.after(
                    0,
                    lambda: self.gh_action_btn.config(
                        state="normal", text="DOWNLOAD & INSTALL"
                    ),
                )

        threading.Thread(target=download_and_install_thread, daemon=True).start()

    def update_progress(self, val):
        self.progress_bar.config(value=val)

    def reset_progress(self):
        self.progress_bar.config(value=0)

    # 9. Flashing Methods
    # A. DFU Flashing
    def run_dfu_flash(self, filepath):
        if self.connected_device_mode != "DFU Recovery Mode":
            CustomMessagebox.show_error(
                self.root,
                "Not in DFU Mode",
                "The device must be in DFU mode to write .dfu or .bin firmware.",
            )
            return

        self.log_clear()
        self.log(f"Initiating DFU flash for: {filepath}")

        tool_path = self.cli_path if self.cli_path else self.dfu_util_path
        if not tool_path:
            CustomMessagebox.show_error(
                self.root,
                "No Flash Utility Found",
                "Could not locate STM32_Programmer_CLI or dfu-util on your PC.\nPlease install STM32CubeProgrammer.",
            )
            return

        self.local_action_btn.config(state="disabled", text="Flashing...")
        self.progress_bar.config(mode="indeterminate")
        self.progress_bar.start(10)

        def flash_thread():
            success = False
            try:
                if "STM32_Programmer_CLI" in tool_path:
                    self.log("Using STM32_Programmer_CLI...")
                    cmd = [tool_path, "-c", "port=usb1"]
                    if filepath.lower().endswith(".bin"):
                        cmd += ["-d", filepath, "0x08000000", "-v"]
                    else:
                        cmd += ["-d", filepath, "-v"]
                    cmd += ["-s"]  # run target

                    self.log(f"Running command: {' '.join(cmd)}")
                    proc = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                    )
                    for line in proc.stdout:
                        clean_line = clean_cli_log(line)
                        if clean_line.strip():
                            self.log(clean_line.strip(), "CLI")
                    proc.wait()
                    success = proc.returncode == 0

                else:
                    self.log("Using dfu-util...")
                    cmd = [tool_path, "-a", "0"]
                    if filepath.lower().endswith(".dfu"):
                        cmd += ["-D", filepath]
                    else:
                        cmd += ["-s", "0x08000000:leave", "-D", filepath]

                    self.log(f"Running command: {' '.join(cmd)}")
                    proc = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                    )
                    for line in proc.stdout:
                        self.log(line.strip(), "DFU-UTIL")
                    proc.wait()
                    success = proc.returncode == 0

            except Exception as e:
                self.log(f"Flash subprocess crashed: {str(e)}", "ERROR")

            self.root.after(0, self.on_flash_completed, success)

        threading.Thread(target=flash_thread, daemon=True).start()

    def on_flash_completed(self, success):
        self.progress_bar.stop()
        self.progress_bar.config(mode="determinate", value=0)
        self.update_action_buttons()

        if success:
            self.log("FLASH SUCCESSFUL! The device should now boot up.", "SUCCESS")
            CustomMessagebox.show_info(
                self.root,
                "Flash Success",
                "Firmware flashed successfully!\n\nYour DIY Flipper Zero will now reboot.",
            )
        else:
            self.log("Flash failed! Please check DFU connections and logs.", "ERROR")
            CustomMessagebox.show_error(
                self.root,
                "Flash Failed",
                "Flashing process encountered errors. Check logs for details.",
            )

    # B. Serial OTA update (tgz update pack)
    def run_serial_ota_install(self, tgz_path):
        if self.connected_device_mode != "Serial Mode" or not self.connected_port:
            CustomMessagebox.show_error(
                self.root,
                "Not in Serial Mode",
                "The device must be running and connected to a COM port to install .tgz updates.",
            )
            return

        self.log_clear()
        self.log(f"Extracting OTA package: {tgz_path}")

        self.local_action_btn.config(state="disabled", text="Installing...")
        self.progress_bar.config(mode="indeterminate")
        self.progress_bar.start(10)

        def ota_thread():
            success = False
            temp_extract_dir = tempfile.mkdtemp()

            try:
                # 1. Unpack tgz
                self.log("Unpacking tarball...")
                with tarfile.open(tgz_path, "r:gz") as tar:
                    # Sanitize tarball members to prevent directory traversal (CVE-2007-4559)
                    resolved_temp_dir = os.path.abspath(temp_extract_dir)
                    for member in tar.getmembers():
                        dest_path = os.path.abspath(os.path.join(temp_extract_dir, member.name))
                        if not dest_path.startswith(resolved_temp_dir + os.sep) and dest_path != resolved_temp_dir:
                            raise RuntimeError(f"Illegal path traversal detected in tar member: {member.name}")
                    if hasattr(tarfile, "data_filter"):
                        tar.extractall(path=temp_extract_dir, filter="data")
                    else:
                        tar.extractall(path=temp_extract_dir)

                # Locate directory with update.fuf (sometimes nested)
                update_fuf_path = None
                for root_dir, dirs, files in os.walk(temp_extract_dir):
                    if "update.fuf" in files:
                        update_fuf_path = os.path.join(root_dir, "update.fuf")
                        break

                if not update_fuf_path:
                    self.log(
                        "Could not find update.fuf manifest inside package!", "ERROR"
                    )
                    shutil.rmtree(temp_extract_dir)
                    self.root.after(0, self.on_ota_completed, False)
                    return

                pkg_dir = os.path.dirname(update_fuf_path)
                pkg_name = os.path.basename(pkg_dir)
                self.log(f"Located update manifest inside '{pkg_name}' folder")

                # 2. Upload directory recursively using Flipper CDC
                self.log(
                    f"Uploading files to Flipper SD card via {self.connected_port}..."
                )

                update_root = "/ext/update"
                flipper_update_path = f"{update_root}/{pkg_name}"

                with FlipperStorage(self.connected_port) as storage:
                    storage_ops = FlipperStorageOperations(storage)
                    storage_ops.mkpath(update_root)
                    storage_ops.mkpath(flipper_update_path)

                    # Log file transfers
                    self.log("Copying update files (this might take a minute)...")
                    storage_ops.recursive_send(flipper_update_path, pkg_dir)

                    # 3. Close active app
                    self.log("Closing active Flipper apps...")
                    for _ in range(10):
                        storage.send_and_wait_eol("loader close\r")
                        res = storage.read.until(storage.CLI_EOL)
                        if b"was closed" in res:
                            storage.read.until(storage.CLI_EOL)
                            time.sleep(0.2)
                        elif res.startswith(b"No application"):
                            storage.read.until(storage.CLI_EOL)
                            break

                    # 4. Trigger OTA install command
                    self.log("Sending installation command...")
                    storage.send_and_wait_eol(
                        f"update install {flipper_update_path}/update.fuf\r"
                    )
                    res = storage.read.until(storage.CLI_EOL)
                    if b"Verifying" in res:
                        res = storage.read.until(storage.CLI_EOL)
                        if res.startswith(b"OK"):
                            success = True
                            self.log(
                                "OTA Command acknowledged! Flipper will now reboot and install."
                            )
                        else:
                            self.log(
                                f"OTA Install error: {res.decode('ascii')}", "ERROR"
                            )
                    else:
                        self.log(
                            f"Unexpected response to update command: {res.decode('ascii')}",
                            "ERROR",
                        )

            except Exception as e:
                self.log(f"OTA update failed: {str(e)}", "ERROR")

            finally:
                # Cleanup temp directory
                try:
                    shutil.rmtree(temp_extract_dir)
                except Exception:
                    pass

            self.root.after(0, self.on_ota_completed, success)

        threading.Thread(target=ota_thread, daemon=True).start()

    def on_ota_completed(self, success):
        self.progress_bar.stop()
        self.progress_bar.config(mode="determinate", value=0)
        self.update_action_buttons()

        if success:
            self.log("OTA UPDATE INITIATED SUCCESSFUL!", "SUCCESS")
            CustomMessagebox.show_info(
                self.root,
                "OTA Update Started",
                "Update files copied successfully!\n\nYour DIY Flipper Zero has rebooted to perform the update.",
            )
        else:
            self.log("OTA Update failed. Check connections and logs.", "ERROR")
            CustomMessagebox.show_error(
                self.root,
                "Update Failed",
                "OTA Update failed. See log output for details.",
            )

    # 10. OTP Panel Methods
    def save_otp_bin(self):
        filepath = filedialog.asksaveasfilename(
            title="Save OTP Profile .bin File",
            defaultextension=".bin",
            filetypes=[("Binary files", "*.bin")],
        )
        if not filepath:
            return

        try:
            data = self.generate_otp_bytes()
            with open(filepath, "wb") as f:
                f.write(data)
            CustomMessagebox.show_info(
                self.root,
                "Saved",
                f"OTP Profile binary saved successfully to:\n{filepath}",
            )
        except Exception as e:
            CustomMessagebox.show_error(
                self.root, "Save Failed", f"Failed to generate and save OTP: {str(e)}"
            )

    def generate_otp_bytes(self):
        # Generate OTP v2 Struct (32 bytes)
        name = self.otp_vars["DeviceName"].get().strip()
        if len(name) > 8:
            raise ValueError("Device Name must be 8 characters or less.")
        # Null-pad name to 8 bytes
        name_bytes = name.encode("ascii").ljust(8, b"\x00")

        board_ver = int(self.otp_vars["BoardVersion"].get())

        # MGG = 2, ERC = 1
        disp = 2 if self.otp_vars["DisplayType"].get() == "MGG" else 1

        # Black=1, White=2, Transparent=3
        color_map = {"Black": 1, "White": 2, "Transparent": 3}
        color = color_map.get(self.otp_vars["BodyColor"].get(), 1)

        # EU=1, US=2, JP=3, World=4
        region_map = {"Europe": 1, "USA": 2, "Japan": 3, "World": 4}
        region = region_map.get(self.otp_vars["Region"].get(), 4)

        timestamp = int(time.time())

        # Struct layout:
        # 0x00: Header magic 0xBABE (2 bytes)
        # 0x02: Header version 2 (1 byte)
        # 0x03: Reserved (1 byte)
        # 0x04: Timestamp (4 bytes)
        # 0x08: Board Version (1 byte)
        # 0x09: Board Target = 7 (1 byte)
        # 0x0A: Board Body = 9 (1 byte)
        # 0x0B: Board Connect = 6 (1 byte)
        # 0x0C: Display Type (1 byte)
        # 0x0D: Reserved (1 byte)
        # 0x0E: Reserved (2 bytes)
        # 0x10: Body Color (1 byte)
        # 0x11: Region (1 byte)
        # 0x12: Reserved (2 bytes)
        # 0x14: Reserved (4 bytes)
        # 0x18: Device Name (8 bytes)

        import struct

        data = struct.pack(
            "<HBBI BBBBBBH BBHI 8s",
            0xBABE,  # Header Magic
            2,  # Header Version
            0,  # Reserved
            timestamp,
            board_ver,
            7,  # Board Target (Flipper)
            9,  # Board Body
            6,  # Board Connect
            disp,  # Display Type
            0,  # Reserved 2.0 (B)
            0,  # Reserved 2.1 (H)
            color,  # Body Color
            region,  # Region
            0,  # Reserved 3.0 (H)
            0,  # Reserved 3.1 (I)
            name_bytes,  # Device Name
        )
        return data

    def flash_otp_dfu(self):
        if self.connected_device_mode != "DFU Recovery Mode":
            CustomMessagebox.show_error(
                self.root,
                "Not in DFU Mode",
                "The device must be in DFU mode to flash OTP.",
            )
            return

        if not CustomMessagebox.ask_yes_no(
            self.root,
            "Irreversible Write Warning",
            "WARNING: OTP memory can only be written ONCE. You cannot erase or modify it later!\n\n"
            + f"Are you sure you want to write OTP profile '{self.otp_vars['DeviceName'].get()}'?",
        ):
            return

        self.log_clear()
        self.log("Preparing to flash OTP...")

        try:
            otp_data = self.generate_otp_bytes()
            temp_dir = tempfile.gettempdir()
            otp_file = os.path.join(temp_dir, "flipper_otp.bin")
            with open(otp_file, "wb") as f:
                f.write(otp_data)

            self.log(f"Saved temporary OTP binary to {otp_file}")

            tool_path = self.cli_path
            if not tool_path:
                CustomMessagebox.show_error(
                    self.root,
                    "CLI Not Found",
                    "STM32_Programmer_CLI is required for OTP flashing (dfu-util is not supported for OTP).",
                )
                return

            self.otp_flash_btn.config(state="disabled")
            self.progress_bar.config(mode="indeterminate")
            self.progress_bar.start(10)

            def flash_thread():
                success = False
                try:
                    # Flash OTP to 0x1FFF7000
                    cmd = [
                        tool_path,
                        "-c",
                        "port=usb1",
                        "-d",
                        otp_file,
                        "0x1FFF7000",
                        "-v",
                    ]
                    self.log(f"Running command: {' '.join(cmd)}")
                    proc = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                    )
                    for line in proc.stdout:
                        clean_line = clean_cli_log(line)
                        if clean_line.strip():
                            self.log(clean_line.strip(), "CLI")
                    proc.wait()
                    success = proc.returncode == 0
                except Exception as e:
                    self.log(f"OTP flashing failed: {str(e)}", "ERROR")

                self.root.after(0, self.on_otp_flashed, success)

            threading.Thread(target=flash_thread, daemon=True).start()

        except Exception as e:
            CustomMessagebox.show_error(
                self.root, "Error", f"Could not prepare OTP data: {str(e)}"
            )

    def on_otp_flashed(self, success):
        self.progress_bar.stop()
        self.progress_bar.config(mode="determinate", value=0)
        self.otp_flash_btn.config(state="normal")

        if success:
            self.log("OTP FLASHED SUCCESSFUL!", "SUCCESS")
            CustomMessagebox.show_info(
                self.root,
                "Success!",
                f"Successfully flashed OTP profile to address 0x1FFF7000!\n\nYou can now reboot the device.",
            )
        else:
            self.log("OTP Flash failed. Check connections and logs.", "ERROR")
            CustomMessagebox.show_error(
                self.root,
                "OTP Flash Failed",
                "Flashing OTP failed. See logs for details.",
            )

    def read_otp_dfu(self):
        if self.connected_device_mode != "DFU Recovery Mode":
            CustomMessagebox.show_error(
                self.root,
                "Not in DFU Mode",
                "The device must be in DFU mode to read OTP.",
            )
            return

        self.log_clear()
        self.log("Reading OTP from device...")

        tool_path = self.cli_path
        if not tool_path:
            CustomMessagebox.show_error(
                self.root,
                "CLI Not Found",
                "STM32_Programmer_CLI is required for OTP reading.",
            )
            return

        temp_dir = tempfile.gettempdir()
        read_file = os.path.join(temp_dir, "read_otp.bin")
        if os.path.exists(read_file):
            try:
                os.remove(read_file)
            except Exception:
                pass

        self.otp_read_btn.config(state="disabled")
        self.progress_bar.config(mode="indeterminate")
        self.progress_bar.start(10)

        def read_thread():
            success = False
            try:
                # Read 32 bytes from 0x1FFF7000
                cmd = [
                    tool_path,
                    "-c",
                    "port=usb1",
                    "-r32",
                    "0x1FFF7000",
                    "32",
                    read_file,
                ]
                self.log(f"Running command: {' '.join(cmd)}")
                proc = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
                for line in proc.stdout:
                    clean_line = clean_cli_log(line)
                    if clean_line.strip():
                        self.log(clean_line.strip(), "CLI")
                proc.wait()
                success = proc.returncode == 0 and os.path.exists(read_file)
            except Exception as e:
                self.log(f"Reading OTP failed: {str(e)}", "ERROR")

            self.root.after(0, self.on_otp_read, success, read_file)

        threading.Thread(target=read_thread, daemon=True).start()

    def on_otp_read(self, success, filepath):
        self.progress_bar.stop()
        self.progress_bar.config(mode="determinate", value=0)
        self.otp_read_btn.config(state="normal")

        if not success:
            self.log("Failed to read OTP from device.", "ERROR")
            CustomMessagebox.show_error(
                self.root, "Read Failed", "Could not read OTP from device. Check logs."
            )
            return

        try:
            with open(filepath, "rb") as f:
                data = f.read()

            if len(data) < 32:
                raise ValueError(f"Expected 32 bytes, got {len(data)} bytes")

            # Parse OTP structure
            import struct

            (
                magic,
                ver,
                res1,
                timestamp,
                board_ver,
                target,
                body,
                connect,
                disp,
                res2,
                res3,
                color,
                region,
                res4,
                res5,
                name_bytes,
            ) = struct.unpack("<HBBIIBBBBBBHBBH8s", data[:32])

            # Clean name
            name = name_bytes.split(b"\x00")[0].decode("ascii", errors="replace")

            self.log("=" * 50)
            self.log("SUCCESSFULLY READ OTP DATA:")
            self.log(f"Device Name: {name}")
            self.log(f"Header Magic: 0x{magic:X}")
            self.log(f"Header Version: {ver}")
            self.log(
                f"Timestamp: {timestamp} ({time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(timestamp))})"
            )
            self.log(f"Board Version: {board_ver}")
            self.log(f"Board Target: {target}")
            self.log(
                f"Display Type: {'MGG (SSD1306 OLED)' if disp == 2 else 'ERC (Standard LCD)'} (Value: {disp})"
            )

            color_map = {1: "Black", 2: "White", 3: "Transparent"}
            self.log(f"Body Color: {color_map.get(color, 'Unknown')} (Value: {color})")

            region_map = {1: "Europe", 2: "USA", 3: "Japan", 4: "World"}
            self.log(
                f"Sub-GHz Region: {region_map.get(region, 'Unknown')} (Value: {region})"
            )
            self.log("=" * 50)

            # Load into UI variables
            self.otp_vars["DeviceName"].set(name)
            self.otp_vars["BoardVersion"].set(str(board_ver))
            self.otp_vars["DisplayType"].set("MGG" if disp == 2 else "ERC")

            rev_color_map = {1: "Black", 2: "White", 3: "Transparent"}
            self.otp_vars["BodyColor"].set(rev_color_map.get(color, "Black"))

            rev_region_map = {1: "Europe", 2: "USA", 3: "Japan", 4: "World"}
            self.otp_vars["Region"].set(rev_region_map.get(region, "World"))

            CustomMessagebox.show_info(
                self.root,
                "OTP Data Loaded",
                f"Successfully read OTP for '{name}' and loaded it into the form.",
            )

        except Exception as e:
            self.log(f"Failed to parse OTP data: {str(e)}", "ERROR")
            CustomMessagebox.show_error(
                self.root, "Parsing Error", f"Failed to parse read OTP data: {str(e)}"
            )


# Clean logs helper


# Helper cleaning log function
def clean_cli_log(raw_log):
    import re

    cleaned = []
    for line in raw_log.split("\n"):
        stripped = line.strip()
        if not stripped:
            continue
        non_ascii = sum(1 for c in stripped if ord(c) > 127)
        if len(stripped) > 0 and non_ascii / len(stripped) > 0.5:
            continue
        if re.fullmatch(r"[+=%\s\-|#]+", stripped):
            continue
        cleaned.append(line)
    return "\n".join(cleaned)


ST_PROG_CLI_PATHS = [
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    r"C:\Program Files\x86\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
]

if __name__ == "__main__":
    root = tk.Tk()
    app = DIYFlasherApp(root)
    root.mainloop()
