import sys
import webbrowser
import os
import re
import time
import queue
import datetime
import threading
import subprocess
import tkinter as tk
from collections import deque
from tkinter import filedialog

import customtkinter as ctk
import serial.tools.list_ports

from dsg_api import DSG

try:
    from assets import ASSETS
except ImportError:
    ASSETS = {}

# --- THEME AND COLORS ---
COLOR_BG = "#1e1e2e"
COLOR_PANEL = "#313244"
COLOR_TEXT = "#cdd6f4"
COLOR_ACCENT = "#89b4fa"
COLOR_SUCCESS = "#a6e3a1"
COLOR_ERROR = "#f38ba8"
COLOR_WARNING = "#f9e2af"

COLOR_BORDER = "#45475a"
COLOR_INPUT = "#11111b"
COLOR_ON_ACCENT = "#11111b"  # dark text used on top of bright accent buttons
COLOR_HOVER = "#3b3d54"

FONT_FAMILY = "Segoe UI"
MONO_FAMILY = "Consolas"

UNITS = ["KHz", "MHz", "GHz"]
FILTER_ITEMS = ["Filter: OFF (0.15-22.6 GHz)", "Filter: ON (2-18 GHz)"]
APP_VERSION = "1.0.0"  # replaced with the git tag by the release workflow
ESPTOOL_FLAG = "--run-esptool"  # packaged executable: run esptool instead of the GUI
REPO_URL = "https://github.com/atek-midas/atek-dsg-22GHz"

# -----------------------------------------------------------------------------
# Small styling helpers
# -----------------------------------------------------------------------------
def font(size=14, bold=False, family=FONT_FAMILY):
    return ctk.CTkFont(family=family, size=size, weight="bold" if bold else "normal")


def lighten(hex_color, ratio=0.12):
    """Blend a #rrggbb color towards white (used for hover shades)."""
    rgb = [int(hex_color[i:i + 2], 16) for i in (1, 3, 5)]
    return "#%02x%02x%02x" % tuple(round(c + (255 - c) * ratio) for c in rgb)

def find_local_file(filename):
    """Look for a file next to the executable/script and one directory above."""
    if getattr(sys, "frozen", False):
        base = os.path.dirname(sys.executable)
    else:
        base = os.path.dirname(os.path.abspath(__file__))

    for folder in (base, os.path.dirname(base)):
        path = os.path.join(folder, filename)
        if os.path.isfile(path):
            return path

    return None


def set_window_icon(window):
    """Use dsg.ico as the window/taskbar icon (Windows). Bundled into the packaged executable."""
    if sys.platform != "win32":
        return
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    icon = os.path.join(base, "dsg.ico")
    if os.path.isfile(icon):
        try:
            window.iconbitmap(icon)
        except tk.TclError:
            pass


def open_file_or_repo_page(filename):
    """Open the local file if available; otherwise open it from GitHub."""
    path = find_local_file(filename)

    try:
        if path is None:
            webbrowser.open(f"{REPO_URL}/blob/main/{filename}")
            return

        if sys.platform == "win32":
            os.startfile(path)
        elif sys.platform == "darwin":
            subprocess.Popen(["open", path])
        else:
            subprocess.Popen(["xdg-open", path])

    except Exception:
        webbrowser.open(f"{REPO_URL}/blob/main/{filename}")

def make_button(parent, text, command=None, *, fg=COLOR_PANEL, text_color=COLOR_TEXT,
                border_color=COLOR_BORDER, border_width=1, size=14, height=38,
                hover=None, hover_border=True, **kw):
    """Create a consistently styled button."""
    btn = ctk.CTkButton(
        parent, text=text, command=command, height=height, corner_radius=4,
        fg_color=fg, text_color=text_color, border_width=border_width,
        border_color=border_color, hover_color=hover or (COLOR_HOVER if fg == COLOR_PANEL else lighten(fg, 0.15)),
        font=font(size, True), **kw
    )
    if hover_border:
        # Standard buttons highlight their border with the accent color on hover.
        btn.bind("<Enter>", lambda e: btn.configure(border_color=COLOR_ACCENT), add="+")
        btn.bind("<Leave>", lambda e: btn.configure(border_color=border_color), add="+")
    return btn


# -----------------------------------------------------------------------------
# Themed message dialog (replacement for QMessageBox)
# -----------------------------------------------------------------------------
class AboutWindow(ctk.CTkToplevel):
    def __init__(self, master):
        super().__init__(master, fg_color=COLOR_PANEL)

        self.title("About")
        # CTkToplevel sets its own default icon shortly after creation; override it afterwards.
        self.after(250, lambda: set_window_icon(self))
        self.resizable(False, False)
        self.transient(master)

        width = 500
        height = 410

        master.update_idletasks()
        x = master.winfo_rootx() + (master.winfo_width() - width) // 2
        y = master.winfo_rooty() + (master.winfo_height() - height) // 3

        self.geometry(f"{width}x{height}+{max(x, 0)}+{max(y, 0)}")

        self._logo_image = None

        try:
            logo_data = ASSETS.get("logo_new-300x86.png")

            if logo_data:
                self._logo_image = tk.PhotoImage(data=logo_data)

                tk.Label(
                    self,
                    image=self._logo_image,
                    bg=COLOR_PANEL,
                    borderwidth=0
                ).pack(pady=(25, 10))
            else:
                raise ValueError

        except Exception:
            ctk.CTkLabel(
                self,
                text="ATEK MIDAS",
                font=font(26, True),
                text_color=COLOR_ACCENT
            ).pack(pady=(25, 10))

        ctk.CTkLabel(
            self,
            text="DSG 22.6 GHz Control Panel",
            font=font(18, True),
            text_color=COLOR_TEXT
        ).pack(pady=(5, 3))

        ctk.CTkLabel(
            self,
            text=f"Version {APP_VERSION}",
            font=font(13),
            text_color="#a6adc8"
        ).pack(pady=(0, 15))

        ctk.CTkLabel(
            self,
            text="© 2026 ATEK MIDAS",
            font=font(13),
            text_color=COLOR_TEXT
        ).pack()

        ctk.CTkLabel(
            self,
            text=(
                "Source code licensed under the MIT License.\n"
                "Third-party components remain under their own licenses."
            ),
            font=font(12),
            text_color="#a6adc8",
            justify="center"
        ).pack(pady=(6, 10))

        repo_link = ctk.CTkLabel(
            self,
            text=REPO_URL,
            font=font(11),
            text_color=COLOR_ACCENT,
            cursor="hand2"
        )
        repo_link.pack(pady=(0, 18))
        repo_link.bind("<Button-1>", lambda _e: webbrowser.open(REPO_URL))

        btn_row = ctk.CTkFrame(self, fg_color="transparent")
        btn_row.pack(pady=(0, 20))

        make_button(
            btn_row,
            "License",
            lambda: open_file_or_repo_page("LICENSE"),
            width=100,
            height=32
        ).pack(side="left", padx=5)

        make_button(
            btn_row,
            "Third-Party Notices",
            lambda: open_file_or_repo_page("../THIRD-PARTY-NOTICES.md"),
            width=170,
            height=32
        ).pack(side="left", padx=5)

        make_button(
            btn_row,
            "Close",
            self.destroy,
            width=90,
            height=32,
            fg=COLOR_ACCENT,
            text_color=COLOR_ON_ACCENT,
            border_width=0,
            hover_border=False
        ).pack(side="left", padx=5)

        self.after(100, self._bring_to_front)

    def _bring_to_front(self):
        try:
            self.lift()
            self.focus_force()
        except Exception:
            pass


class MessageDialog(ctk.CTkToplevel):
    KINDS = {
        "info": ("i", COLOR_ACCENT),
        "warning": ("!", COLOR_WARNING),
        "error": ("✕", COLOR_ERROR),
        "question": ("?", COLOR_ACCENT),
    }

    def __init__(self, master, title, message, kind="info", buttons=("OK",)):
        super().__init__(master, fg_color=COLOR_BG)
        self.title(title)
        self.resizable(False, False)
        self.result = None
        self._buttons = buttons

        symbol, color = self.KINDS.get(kind, self.KINDS["info"])

        body = ctk.CTkFrame(self, fg_color="transparent")
        body.pack(fill="both", expand=True, padx=26, pady=(24, 12))

        ctk.CTkLabel(
            body,
            text=symbol,
            width=44,
            height=44,
            corner_radius=22,
            fg_color=color,
            text_color=COLOR_ON_ACCENT,
            font=font(22, True)
        ).grid(row=0, column=0, sticky="n")

        ctk.CTkLabel(
            body,
            text=message,
            wraplength=380,
            justify="left",
            anchor="w",
            text_color=COLOR_TEXT,
            font=font(14, True)
        ).grid(row=0, column=1, sticky="w", padx=(18, 0))

        row = ctk.CTkFrame(self, fg_color="transparent")
        row.pack(fill="x", padx=26, pady=(8, 22))
        row.grid_columnconfigure(0, weight=1)

        for i, label in enumerate(buttons):
            primary = i == 0

            btn = make_button(
                row,
                label,
                lambda l=label: self._choose(l),
                width=96,
                height=34,
                fg=COLOR_ACCENT if primary else COLOR_PANEL,
                text_color=COLOR_ON_ACCENT if primary else COLOR_TEXT,
                border_width=0 if primary else 1,
                hover_border=not primary
            )
            btn.grid(row=0, column=i + 1, padx=(8, 0))

        self.protocol("WM_DELETE_WINDOW", self._cancel)
        self.bind("<Return>", lambda e: self._choose(buttons[0]))
        self.bind("<Escape>", lambda e: self._cancel())
        self.transient(master)
        self.after(60, self._show)

    def _show(self):
        self.update_idletasks()

        try:
            m = self.master
            x = m.winfo_rootx() + (m.winfo_width() - self.winfo_reqwidth()) // 2
            y = m.winfo_rooty() + (m.winfo_height() - self.winfo_reqheight()) // 2
            tk.Toplevel.geometry(self, f"+{max(x, 0)}+{max(y, 0)}")
        except Exception:
            pass

        self.lift()
        self.focus_force()

        try:
            self.grab_set()
        except Exception:
            pass

    def _choose(self, label):
        self.result = label
        self.destroy()

    def _cancel(self):
        self.result = self._buttons[-1] if len(self._buttons) > 1 else self._buttons[0]
        self.destroy()

    def show(self):
        self.master.wait_window(self)
        return self.result


# -----------------------------------------------------------------------------
# Custom input widgets (CustomTkinter has no spin box / combo equivalents)
# -----------------------------------------------------------------------------
class NumberField(ctk.CTkFrame):
    """Numeric input with optional +/- buttons (replacement for QDoubleSpinBox / QSpinBox)."""

    def __init__(self, master, *, value=0.0, minimum=0.0, maximum=100.0, decimals=2,
                 step=1.0, buttons=True, integer=False, special_text=None, command=None):
        super().__init__(master, fg_color=COLOR_INPUT, border_width=1,
                         border_color=COLOR_BORDER, corner_radius=3)
        self._min = minimum
        self._max = maximum
        self._decimals = decimals
        self._step = step
        self._integer = integer
        self._special = special_text
        self._command = command
        self._value = self._clamp(value)

        self.grid_columnconfigure(0, weight=1)
        self._var = tk.StringVar(value=self._format(self._value))
        self._entry = ctk.CTkEntry(self, textvariable=self._var, width=60, height=30, border_width=0,
                                   fg_color=COLOR_INPUT, text_color="white", font=font(14))
        self._entry.grid(row=0, column=0, sticky="ew", padx=(4, 0 if buttons else 4), pady=1)
        self._entry.bind("<Return>", self._commit, add="+")
        self._entry.bind("<FocusOut>", self._commit, add="+")
        self._entry.bind("<KeyPress>", self._on_keypress, add="+")
        self._entry.bind("<KeyRelease>", self._track, add="+")
        self._entry.bind("<FocusIn>", self._on_focus_in, add="+")
        self._entry.bind("<Up>", lambda e: self._bump(+1), add="+")
        self._entry.bind("<Down>", lambda e: self._bump(-1), add="+")

        if buttons:
            for col, (sym, direction) in enumerate((("−", -1), ("+", +1)), start=1):
                b = ctk.CTkButton(self, text=sym, width=26, height=26, corner_radius=3,
                                  fg_color=COLOR_PANEL, hover_color=COLOR_BORDER,
                                  text_color=COLOR_TEXT, font=font(15, True),
                                  command=lambda d=direction: self._bump(d))
                b.grid(row=0, column=col, padx=(2, 3 if col == 2 else 0), pady=2)

    # -- internal helpers ----------------------------------------------------
    def _clamp(self, v):
        v = max(self._min, min(self._max, v))
        return int(round(v)) if self._integer else round(v, self._decimals)

    def _format(self, v):
        if self._special is not None and v == self._min:
            return self._special

        if self._integer:
            return str(int(v))

        # Do not force trailing zeros in the GUI (300.000 -> 300, 22.600 -> 22.6).
        text = f"{v:.{self._decimals}f}".rstrip("0").rstrip(".")
        return text if text else "0"

    def _parse(self, text):
        t = text.strip()
        if self._special is not None and t == self._special:
            return self._min
        return float(t)

    def _notify(self, v):
        if v != self._value:
            self._value = v
            if self._command:
                self._command()

    def _on_keypress(self, event):
        """Match the web UI numeric-entry rules as closely as possible."""
        # Keep navigation/editing shortcuts working.
        if event.keysym in {
            "BackSpace", "Delete", "Left", "Right", "Home", "End", "Tab",
            "Return", "KP_Enter", "Up", "Down"
        }:
            return None

        # Ctrl/Cmd shortcuts such as copy/paste/select-all.
        if event.state & 0x4:
            return None

        char = event.char
        if not char:
            return None

        if char.isdigit():
            return None

        # Decimal fields use '.' only. Pressing ',' inserts '.' instead.
        if char == ",":
            if self._integer:
                return "break"
            if "." not in self._var.get():
                pos = self._entry.index("insert")
                current = self._var.get()
                self._var.set(current[:pos] + "." + current[pos:])
                self._entry.icursor(pos + 1)
            return "break"

        if char == ".":
            if self._integer or "." in self._var.get():
                return "break"
            return None

        # Only fields whose minimum is negative may contain a leading minus sign.
        if char == "-":
            if self._min >= 0 or "-" in self._var.get() or self._entry.index("insert") != 0:
                return "break"
            return None

        # '+', scientific notation and all other characters are not valid here.
        return "break"

    def _track(self, _event=None):
        """Validate typed/pasted text live and keep '.' as the decimal separator."""
        raw = self._var.get()

        # Allow an empty field temporarily while the user edits it. _commit() restores
        # the previous valid value if the field is left empty.
        if raw == "":
            return

        if self._integer:
            pattern = r"^-?\d+$" if self._min < 0 else r"^\d+$"
            if not re.fullmatch(pattern, raw):
                self._var.set(self._format(self._value))
                self._entry.icursor("end")
                return
            text = raw
        else:
            text = raw.replace(",", ".")
            pattern = r"^-?\d*\.?\d*$" if self._min < 0 else r"^\d*\.?\d*$"
            if not re.fullmatch(pattern, text):
                self._var.set(self._format(self._value))
                self._entry.icursor("end")
                return
            if text != raw:
                cursor_pos = self._entry.index("insert")
                self._var.set(text)
                self._entry.icursor(min(cursor_pos, len(text)))

        # Intermediate forms such as '-', '.', or '-.' are allowed while typing.
        if text in {"-", ".", "-."}:
            return

        try:
            self._notify(self._clamp(self._parse(text)))
        except ValueError:
            pass

    def _commit(self, _event=None):
        v = self.value()
        self._var.set(self._format(v))
        self._notify(v)

    def _on_focus_in(self, _event=None):
        if self._special is not None and self._var.get() == self._special:
            self.after(10, lambda: self._entry.select_range(0, "end"))

    def _bump(self, direction):
        self.set_value(self.value() + direction * self._step)

    # -- public API ----------------------------------------------------------
    def value(self):
        try:
            return self._clamp(self._parse(self._var.get()))
        except ValueError:
            return self._value

    def set_value(self, v):
        v = self._clamp(v)
        self._var.set(self._format(v))
        self._notify(v)

    def set_single_step(self, step):
        self._step = step


class Dropdown(ctk.CTkFrame):
    """Read-only drop-down list (replacement for a non-editable QComboBox)."""

    def __init__(self, master, values, *, value=None, command=None, width=140):
        super().__init__(master, fg_color=COLOR_INPUT, border_width=1,
                         border_color=COLOR_BORDER, corner_radius=3)
        self._values = list(values)
        self._command = command
        self._menu = ctk.CTkOptionMenu(
            self, values=self._values, command=self._on_select, width=width, height=30,
            corner_radius=3, dynamic_resizing=False, anchor="w",
            fg_color=COLOR_INPUT, button_color=COLOR_PANEL, button_hover_color=COLOR_BORDER,
            text_color="white", text_color_disabled="#7f849c",
            dropdown_fg_color=COLOR_PANEL, dropdown_hover_color=COLOR_BORDER,
            dropdown_text_color=COLOR_TEXT, font=font(14), dropdown_font=font(14))
        self._menu.pack(fill="x", padx=1, pady=1)
        self._menu.set(value if value in self._values else (self._values[0] if self._values else ""))

    def _on_select(self, value):
        if self._command:
            self._command(value)

    def get(self):
        return self._menu.get()

    def set(self, value):
        if value in self._values:
            self._menu.set(value)

    def index(self):
        try:
            return self._values.index(self.get())
        except ValueError:
            return -1

    def set_index(self, i):
        if 0 <= i < len(self._values):
            self._menu.set(self._values[i])

    def set_values(self, values):
        self._values = list(values)
        self._menu.configure(values=self._values)
        self._menu.set(self._values[0] if self._values else "")

    def set_enabled(self, enabled):
        self._menu.configure(state="normal" if enabled else "disabled")


class Tabs(ctk.CTkFrame):
    """Tab container: a row of tab buttons above a bordered content pane (replacement for QTabWidget)."""

    def __init__(self, master):
        super().__init__(master, fg_color="transparent")
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        self._bar = ctk.CTkFrame(self, fg_color="transparent")
        self._bar.grid(row=0, column=0, sticky="w")
        self._pane = ctk.CTkFrame(self, fg_color=COLOR_BG, border_width=1,
                                  border_color=COLOR_BORDER, corner_radius=0)
        self._pane.grid(row=1, column=0, sticky="nsew")
        self._pane.grid_columnconfigure(0, weight=1)
        self._pane.grid_rowconfigure(0, weight=1)
        self._buttons = {}
        self._frames = {}
        self._current = None

    def add(self, name):
        btn = ctk.CTkButton(self._bar, text=name, height=40, corner_radius=4, border_width=1,
                            border_color=COLOR_BORDER, font=font(14, True),
                            command=lambda n=name: self.set(n))
        btn.pack(side="left", padx=(0, 2), ipadx=8)
        frame = ctk.CTkFrame(self._pane, fg_color="transparent", corner_radius=0)
        frame.grid(row=0, column=0, sticky="nsew", padx=1, pady=1)
        self._buttons[name] = btn
        self._frames[name] = frame
        self.set(name if self._current is None else self._current)
        return frame

    def set(self, name):
        self._current = name
        for n, btn in self._buttons.items():
            selected = (n == name)
            btn.configure(fg_color=COLOR_ACCENT if selected else COLOR_PANEL,
                          hover_color=COLOR_ACCENT if selected else COLOR_HOVER,
                          text_color=COLOR_ON_ACCENT if selected else COLOR_TEXT)
            if selected:
                # All tab pages share one grid cell, so the pane keeps a constant
                # height (the tallest page) and never jumps when switching tabs.
                self._frames[n].tkraise()

    def get(self):
        return self._current


class GroupBox(ctk.CTkFrame):
    """Titled panel (replacement for QGroupBox). Put child widgets into `.body`."""

    def __init__(self, master, title):
        super().__init__(master, fg_color=COLOR_BG, border_width=1,
                         border_color=COLOR_BORDER, corner_radius=6)
        ctk.CTkLabel(self, text=title, text_color=COLOR_ACCENT,
                     font=font(14, True)).pack(anchor="w", padx=16, pady=(10, 0))
        self.body = ctk.CTkFrame(self, fg_color="transparent")
        self.body.pack(fill="both", expand=True, padx=14, pady=(8, 14))


# -----------------------------------------------------------------------------
# Background communication thread
# Handles communication with the DSG device without blocking the GUI.
# It runs queued dsg_api.DSG method calls, reads device messages, and posts
# events (logs, telemetry, connection status, UI sync) to a thread-safe queue
# that the GUI polls from the Tk main loop. All device commands live in
# dsg_api.py; this class only decides when to run them.
# -----------------------------------------------------------------------------
class ScpiWorker:
    def __init__(self, events: "queue.Queue"):
        self.events = events
        self.port = ""
        self.running = False
        self.is_sweeping = False
        self.cmd_queue = deque()
        self._thread = None
        self._sync_requested = False

    # -- event helpers (replacements for Qt signals) ---------------------------
    def _emit(self, kind, payload=None):
        self.events.put((kind, payload))

    def connect_device(self, port):
        if self._thread is not None and self._thread.is_alive():
            return
        self.port = port
        self._thread = threading.Thread(target=self.run, daemon=True)
        self._thread.start()

    def disconnect_device(self):
        self.running = False
        if self._thread is not None:
            self._thread.join()

    def call(self, method, *args):
        """Queue a dsg_api.DSG method call, e.g. call("set_frequency", 2.4, "GHz")."""
        self.cmd_queue.append((method, args))

    def request_sync(self):
        self._sync_requested = True

    def abort_sweep_urgent(self):
        self.is_sweeping = False
        if hasattr(self, 'dsg') and self.dsg.is_open():
            self.dsg.abort_sweep()

    def process_rx(self, resp):
        if not resp or resp == "0":
            return
        if "TIMING" in resp or "DEBUG" in resp:
            return
        if "Freq(MHz):" in resp:
            freq_str = resp.split("Freq(MHz):")[-1].strip()
            self._emit("log", f"〰 {freq_str}")
            return
        if resp.strip() == "SWEEP:DONE":
            # The device stopped the sweep on its own (configured Count
            # reached), not because the user pressed Stop. Let the GUI know
            # so it can reset the toggle button - otherwise it would keep
            # showing "STOP SWEEP" even though the sweep already finished.
            self.is_sweeping = False
            self._emit("sweep_auto_stopped")
            return
        if self.is_sweeping:
            return
        if resp.startswith("-"):
            self._emit("log", f"⚠ Device Message: {resp}")

    def run(self):
        try:
            # Lines that are not command replies (errors, sweep progress) go to process_rx.
            self.dsg = DSG(self.port, message_handler=self.process_rx)
            self.running = True
            self._emit("connection", True)
            self._emit("log", f"[SYS] Successfully connected to port {self.port}.")
            # --- REQUEST CURRENT DEVICE SCREEN SETTINGS AND SEND THEM TO THE GUI ---
            time.sleep(0.2)
            settings = self.dsg.get_settings()
            if settings:
                self._emit("sync", settings)
            # ------------------------------------------------

            last_telemetry_time = time.time()

            # Main worker loop: process queued commands, update telemetry, and read device messages.
            while self.running:
                while self.cmd_queue:
                    method, args = self.cmd_queue.popleft()
                    # Each DSG method waits for the device acknowledgment itself.
                    getattr(self.dsg, method)(*args)

                if self._sync_requested and not self.cmd_queue:
                    self._sync_requested = False

                    try:
                        settings = self.dsg.get_settings()

                        if settings:
                            self._emit("sync", settings)

                    except Exception as e:
                        self._emit("log", f"[ERR] State sync failed: {e}")

                # Periodic telemetry polling is disabled during sweep mode to avoid disturbing timing.
                if not self.is_sweeping and time.time() - last_telemetry_time > 1.5 and not self.cmd_queue:
                    try:
                        self._emit("telemetry", self.dsg.read_telemetry())
                    except Exception:
                        pass
                    last_telemetry_time = time.time()

                lines_read = 0
                while self.dsg.in_waiting() > 0 and lines_read < 100:
                    resp = self.dsg.read_line()
                    self.process_rx(resp)
                    lines_read += 1

                time.sleep(0.01)

        except Exception as e:
            self._emit("log", f"[ERR] Connection Error: {str(e)}")
        finally:
            if hasattr(self, 'dsg'):
                self.dsg.close()
            # Make sure the state flag is cleared even if the loop ended by an
            # exception (e.g. USB cable unplugged), so CONNECT works again.
            self.running = False
            self._emit("connection", False)


# -----------------------------------------------------------------------------
# Main GUI window
# Builds the CustomTkinter user interface and connects user actions to the
# communication worker. This class does not talk directly to the serial port; it
# sends commands through ScpiWorker to keep the interface responsive.
# -----------------------------------------------------------------------------
class DSGMainWindow(ctk.CTk):
    def __init__(self):
        super().__init__(fg_color=COLOR_BG)
        self.title("DSG 22.6 GHz - Control Panel")
        set_window_icon(self)
        self.geometry("1200x780")
        self.minsize(1100, 700)

        self.events = queue.Queue()
        self.worker = ScpiWorker(self.events)
        self._handlers = {
            "log": self.append_log,
            "connection": self.on_connection_changed,
            "telemetry": self.update_sensors,
            "sync": self.sync_ui_with_device,
            "sweep_auto_stopped": self.on_sweep_auto_stopped,
            "fw_line": lambda line: self.append_log(f"[FW] {line}"),
            "fw_done": self._firmware_flash_finished,
        }

        self.rf_state = False
        self.firmware_process = None
        self.about_window = None
        self._poll_id = None
        self.setup_ui()

        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self._poll_events()

    # -------------------------------------------------------------------------
    # Thread -> GUI event pump (replaces Qt signals/slots)
    # -------------------------------------------------------------------------
    def _poll_events(self):
        try:
            while True:
                kind, payload = self.events.get_nowait()
                handler = self._handlers.get(kind)
                if handler is None:
                    continue
                try:
                    handler() if payload is None else handler(payload)
                except Exception as e:  # never let one bad event stop the pump
                    print(f"[GUI] event '{kind}' failed: {e}", file=sys.stderr)
        except queue.Empty:
            pass
        self._poll_id = self.after(30, self._poll_events)

    def on_close(self):
        try:
            if self._poll_id is not None:
                self.after_cancel(self._poll_id)
            if self.worker.running:
                self.worker.disconnect_device()
        finally:
            self.destroy()

    # -------------------------------------------------------------------------
    # Message box shortcuts
    # -------------------------------------------------------------------------
    def msg_info(self, title, text):
        MessageDialog(self, title, text, "info").show()

    def msg_warning(self, title, text):
        MessageDialog(self, title, text, "warning").show()

    def msg_error(self, title, text):
        MessageDialog(self, title, text, "error").show()

    def msg_question(self, title, text):
        return MessageDialog(self, title, text, "question", ("Yes", "No")).show() == "Yes"

    def show_about(self):
        if self.about_window is not None and self.about_window.winfo_exists():
            self.about_window.lift()
            self.about_window.focus_force()
            return

        self.about_window = AboutWindow(self)

    def show_target_power_info(self):
        self.msg_info(
            "Target Power Information",
            "The maximum Target Power depends on the selected frequency "
            "and filter state.\n\n"
            "Filter ON:\n"
            "2 GHz - 8 GHz: 12 dBm\n"
            "8 GHz - 16 GHz: 10 dBm\n"
            "16 GHz - 18 GHz: 10 dBm\n\n"
            "Filter OFF:\n"
            "150 MHz - 19 GHz: 18 dBm\n"
            "19 GHz - 21 GHz: 17 dBm\n"
            "21 GHz - 22.6 GHz: 15 dBm\n\n"
            "If the requested power is higher than the maximum available "
            "power, the DSG automatically applies the highest available value. "
            "The actual applied value is then shown in the GUI."
        )

    # -------------------------------------------------------------------------
    # UI construction
    # -------------------------------------------------------------------------
    def setup_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=3)
        self.grid_rowconfigure(2, weight=1)

        # --- CONNECTION BAR ---
        h_conn = ctk.CTkFrame(self, fg_color="transparent")
        h_conn.grid(row=0, column=0, sticky="ew", padx=16, pady=(14, 8))

        ctk.CTkLabel(h_conn, text="Port:", text_color=COLOR_TEXT,
                     font=font(14)).pack(side="left", padx=(0, 8))
        self.combo_port = Dropdown(h_conn, ["No Port Found"], width=150)
        self.combo_port.pack(side="left", padx=(0, 10))

        self.btn_refresh = make_button(h_conn, "REFRESH", self.refresh_ports, width=110, height=36)
        self.btn_refresh.pack(side="left", padx=(0, 10))

        self.btn_connect = make_button(h_conn, "CONNECT", self.toggle_connection, width=130, height=36)
        self.btn_connect.pack(side="left")

        # Calibration button lives in the Settings tab (created here, packed there).
        self.btn_load_cal = None

        # --- BODY: TABS (left) + LIVE DEVICE PANEL (right) ---
        h_body = ctk.CTkFrame(self, fg_color="transparent")
        h_body.grid(row=1, column=0, sticky="nsew", padx=16, pady=(0, 8))
        h_body.grid_columnconfigure(0, weight=5, uniform="body")
        h_body.grid_columnconfigure(1, weight=2, uniform="body")
        h_body.grid_rowconfigure(0, weight=1)

        self.tabs = Tabs(h_body)
        self.tabs.grid(row=0, column=0, sticky="nsew", padx=(0, 8))

        self._build_cw_tab(self.tabs.add("CW (Continuous Wave)"))
        self._build_sweep_tab(self.tabs.add("Sweep"))
        self._build_settings_tab(self.tabs.add("Settings"))
        self.tabs.set("CW (Continuous Wave)")

        self._build_sensor_panel(h_body)

        # --- LOG PANEL ---
        log_frame = ctk.CTkFrame(self, fg_color="transparent")
        log_frame.grid(row=2, column=0, sticky="nsew", padx=16, pady=(0, 8))
        log_frame.grid_columnconfigure(0, weight=1)
        log_frame.grid_rowconfigure(1, weight=1)

        log_header = ctk.CTkFrame(log_frame, fg_color="transparent")
        log_header.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        ctk.CTkLabel(log_header, text="System Logs", text_color=COLOR_ACCENT,
                     font=font(16, True)).pack(side="left")
        make_button(log_header, "CLEAR LOG", self.clear_log, width=96, height=28,
                    size=12).pack(side="right")

        self.txt_log = ctk.CTkTextbox(
            log_frame, fg_color=COLOR_INPUT, text_color=COLOR_TEXT, border_width=1,
            border_color=COLOR_BORDER, corner_radius=4, wrap="word", height=100,
            font=font(12, False, MONO_FAMILY), state="disabled",
            scrollbar_button_color=COLOR_BORDER, scrollbar_button_hover_color=COLOR_ACCENT)
        self.txt_log.grid(row=1, column=0, sticky="nsew")

        # --- STATUS BAR ---
        # Firmware build timestamp, shown bottom-left. Populated once the device
        # connects and sends its :SYNC? data (see sync_ui_with_device); reset to
        # "--" on disconnect.
        status = ctk.CTkFrame(self, fg_color=COLOR_INPUT, corner_radius=0, height=28)
        status.grid(row=3, column=0, sticky="ew")
        self.lbl_fw_version = ctk.CTkLabel(status, text="Firmware: --", text_color=COLOR_TEXT,
                                           font=font(12))
        self.lbl_fw_version.pack(side="left", padx=12, pady=3)

        self.refresh_ports()
        self.update_freq_step()

    @staticmethod
    def _tab_content(tab):
        holder = ctk.CTkFrame(tab, fg_color="transparent")
        holder.pack(fill="both", expand=True, padx=16, pady=(14, 10))
        return holder

    def _label(self, parent, text, **grid):
        lbl = ctk.CTkLabel(parent, text=text, text_color=COLOR_TEXT, font=font(14), anchor="w")
        lbl.grid(**grid)
        return lbl

    # --- TAB 1: CW ---
    def _build_cw_tab(self, tab):
        cw = self._tab_content(tab)
        for col, weight in enumerate((0, 1, 0, 0, 1, 0)):
            cw.grid_columnconfigure(col, weight=weight)
        pad = dict(padx=6, pady=8)

        self.spin_freq = NumberField(cw, value=1000, minimum=0, maximum=999999999, decimals=9)
        self.combo_unit = Dropdown(cw, UNITS, value="MHz", width=80)

        self.spin_freq_step = NumberField(cw, value=100, minimum=0, maximum=999999999,
                                          decimals=9)
        self.combo_step_unit = Dropdown(cw, UNITS, value="MHz", width=80)

        self._cw_freq_unit_last = "MHz"
        self._cw_step_unit_last = "MHz"
        self.spin_freq_step._command = self.update_freq_step
        self.combo_unit._command = self._on_cw_frequency_unit_changed
        self.combo_step_unit._command = self._on_cw_step_unit_changed

        self.spin_att = NumberField(cw, value=4, minimum=-20, maximum=20, integer=True)
        self.combo_filt = Dropdown(cw, FILTER_ITEMS)

        self._label(cw, "CW Frequency:", row=0, column=0, sticky="w", **pad)
        self.spin_freq.grid(row=0, column=1, sticky="ew", **pad)
        self.combo_unit.grid(row=0, column=2, sticky="ew", **pad)
        self._label(cw, "Increment Step:", row=0, column=3, sticky="e", **pad)
        self.spin_freq_step.grid(row=0, column=4, sticky="ew", **pad)
        self.combo_step_unit.grid(row=0, column=5, sticky="ew", **pad)

        cw_power_label = ctk.CTkFrame(cw, fg_color="transparent")
        cw_power_label.grid(row=1, column=0, sticky="w", **pad)

        ctk.CTkLabel(
            cw_power_label,
            text="Target Power (dBm):",
            text_color=COLOR_TEXT,
            font=font(14)
        ).pack(side="left")

        ctk.CTkButton(
            cw_power_label,
            text="i",
            width=24,
            height=24,
            corner_radius=12,
            fg_color=COLOR_ACCENT,
            hover_color=lighten(COLOR_ACCENT, 0.15),
            text_color=COLOR_ON_ACCENT,
            font=font(12, True),
            command=self.show_target_power_info
        ).pack(side="left", padx=(6, 0))
        self.spin_att.grid(row=1, column=1, columnspan=5, sticky="ew", **pad)
        self.combo_filt.grid(row=2, column=0, columnspan=6, sticky="ew", **pad)

        btn_apply_cw = make_button(cw, "APPLY CW SETTINGS", self.apply_cw_settings, height=42,
                                   fg=COLOR_ACCENT, text_color=COLOR_ON_ACCENT, border_width=0,
                                   hover_border=False)
        btn_apply_cw.grid(row=3, column=0, columnspan=6, sticky="ew", padx=6, pady=(14, 8))

        self.btn_rf = make_button(cw, "RF OUTPUT: OFF", self.toggle_rf, size=18, height=58,
                                  border_width=2, hover_border=False)
        self.btn_rf.grid(row=4, column=0, columnspan=6, sticky="ew", padx=6, pady=8)
        self._style_rf("OFF", COLOR_ERROR)
        cw.grid_rowconfigure(5, weight=1)

    # --- TAB 2: SWEEP ---
    def _build_sweep_tab(self, tab):
        sw = self._tab_content(tab)
        for col, weight in enumerate((0, 1, 0, 1)):
            sw.grid_columnconfigure(col, weight=weight)
        pad = dict(padx=6, pady=3)

        self.sw_start = NumberField(sw, value=300, minimum=0, maximum=999999999, decimals=9, buttons=False)
        self.sw_start_u = Dropdown(sw, UNITS, value="MHz", width=90)
        self.sw_stop = NumberField(sw, value=22600, minimum=0, maximum=999999999, decimals=9, buttons=False)
        self.sw_stop_u = Dropdown(sw, UNITS, value="MHz", width=90)

        self._sw_start_unit_last = "MHz"
        self._sw_stop_unit_last = "MHz"

        def preset_row(parent, items, target):
            frame = ctk.CTkFrame(parent, fg_color="transparent")
            for text, val, unit in items:
                make_button(frame, text, lambda v=val, u=unit: self.set_preset(target, v, u),
                            fg=COLOR_BORDER, size=12, height=28, width=80,
                            hover=lighten(COLOR_BORDER, 0.15)).pack(side="left", padx=(0, 5))
            return frame

        h_start_pre = preset_row(sw, [("150 MHz", 150.0, "MHz"), ("1 GHz", 1.0, "GHz"),
                                      ("5 GHz", 5.0, "GHz")], "start")
        h_stop_pre = preset_row(sw, [("5 GHz", 5.0, "GHz"), ("10 GHz", 10.0, "GHz"),
                                     ("22.6 GHz", 22.6, "GHz")], "stop")

        self.sw_step = NumberField(sw, value=100, minimum=0, maximum=999999999, decimals=9, buttons=False)
        self.sw_step_u = Dropdown(sw, UNITS, value="MHz", width=90)
        self._sw_step_unit_last = "MHz"

        self.sw_dwell = NumberField(sw, value=1, minimum=1, maximum=1000000,
                                    integer=True, buttons=False)
        self.sw_count = NumberField(sw, value=0, minimum=0, maximum=999999,
                                    integer=True, buttons=False)  # 0 = run forever
        self.sw_pow = NumberField(sw, value=4, minimum=-20, maximum=20, integer=True)
        self.sw_type = Dropdown(sw, ["Linear (LIN)", "Logarithmic (LOG)"], width=300)

        self.sw_start_u._command = lambda new_unit: self._on_sweep_unit_changed(
            self.sw_start, "_sw_start_unit_last", new_unit)
        self.sw_stop_u._command = lambda new_unit: self._on_sweep_unit_changed(
            self.sw_stop, "_sw_stop_unit_last", new_unit)
        self.sw_step_u._command = lambda new_unit: self._on_sweep_unit_changed(
            self.sw_step, "_sw_step_unit_last", new_unit)

        # Sweep uses the same physical filter state as CW. Keep both selectors synchronized.
        self.sw_filt = Dropdown(sw, FILTER_ITEMS, width=300)
        self.sw_filt.set_index(self.combo_filt.index())
        self.combo_filt._command = lambda _v: self.sync_filter_from_cw(self.combo_filt.index())
        self.sw_filt._command = lambda _v: self.sync_filter_from_sweep(self.sw_filt.index())

        self._label(sw, "Start:", row=0, column=0, sticky="w", **pad)
        self.sw_start.grid(row=0, column=1, sticky="ew", **pad)
        self.sw_start_u.grid(row=0, column=2, sticky="ew", **pad)
        h_start_pre.grid(row=0, column=3, sticky="w", **pad)

        self._label(sw, "Stop:", row=1, column=0, sticky="w", **pad)
        self.sw_stop.grid(row=1, column=1, sticky="ew", **pad)
        self.sw_stop_u.grid(row=1, column=2, sticky="ew", **pad)
        h_stop_pre.grid(row=1, column=3, sticky="w", **pad)

        self._label(sw, "Step:", row=2, column=0, sticky="w", **pad)
        self.sw_step.grid(row=2, column=1, sticky="ew", **pad)
        self.sw_step_u.grid(row=2, column=2, sticky="ew", **pad)
        self._label(sw, "Dwell (ms):", row=3, column=0, sticky="w", **pad)
        self.sw_dwell.grid(row=3, column=1, columnspan=2, sticky="ew", **pad)
        sw_power_label = ctk.CTkFrame(sw, fg_color="transparent")
        sw_power_label.grid(row=4, column=0, sticky="w", **pad)

        ctk.CTkLabel(
            sw_power_label,
            text="Target Power (dBm):",
            text_color=COLOR_TEXT,
            font=font(14)
        ).pack(side="left")

        ctk.CTkButton(
            sw_power_label,
            text="i",
            width=24,
            height=24,
            corner_radius=12,
            fg_color=COLOR_ACCENT,
            hover_color=lighten(COLOR_ACCENT, 0.15),
            text_color=COLOR_ON_ACCENT,
            font=font(12, True),
            command=self.show_target_power_info
        ).pack(side="left", padx=(6, 0))
        self.sw_pow.grid(row=4, column=1, columnspan=2, sticky="ew", **pad)
        self._label(sw, "Type:", row=5, column=0, sticky="w", **pad)
        self.sw_type.grid(row=5, column=1, columnspan=2, sticky="w", **pad)
        self._label(sw, "Filter:", row=6, column=0, sticky="w", **pad)
        self.sw_filt.grid(row=6, column=1, columnspan=2, sticky="w", **pad)
        self._label(sw, "Sweep Count (0 = ∞):", row=7, column=0, sticky="w", **pad)
        self.sw_count.grid(row=7, column=1, columnspan=2, sticky="ew", **pad)

        btn_apply_sw = make_button(sw, "LOAD SWEEP SETTINGS", self.apply_sweep_settings, height=38,
                                   fg=COLOR_ACCENT, text_color=COLOR_ON_ACCENT, border_width=0,
                                   hover_border=False)
        btn_apply_sw.grid(row=8, column=0, columnspan=4, sticky="ew", padx=6, pady=(10, 4))

        self.btn_sweep_toggle = make_button(sw, "▶ START SWEEP", self.toggle_sweep, size=16, height=44,
                                            border_width=0, hover_border=False)
        self.btn_sweep_toggle.grid(row=9, column=0, columnspan=4, sticky="ew", padx=6, pady=4)
        self._style_sweep_button(False)
        sw.grid_rowconfigure(10, weight=1)

    # --- TAB 3: SETTINGS ---
    def _build_settings_tab(self, tab):
        holder = self._tab_content(tab)

        group_cal = GroupBox(holder, "Calibration")
        group_cal.pack(fill="x", pady=(0, 15))
        self.btn_load_cal = make_button(
            group_cal.body, "LOAD CALIBRATION", self.load_calibration_csv, height=44,
            fg=COLOR_WARNING, text_color=COLOR_ON_ACCENT, border_width=0, hover_border=False)
        self.btn_load_cal.pack(fill="x")

        group_firmware = GroupBox(holder, "Firmware")
        group_firmware.pack(fill="x")
        self.btn_firmware_update = make_button(
            group_firmware.body, "⬆ FIRMWARE UPDATE", self.flash_firmware, height=44,
            fg=COLOR_ACCENT, text_color=COLOR_ON_ACCENT, border_width=0, hover_border=False)
        self.btn_firmware_update.pack(fill="x")

        group_about = GroupBox(holder, "About")
        group_about.pack(fill="x", pady=(15, 0))

        self.btn_about = make_button(
            group_about.body,
            "ABOUT / LICENSES",
            self.show_about,
            height=44
        )
        self.btn_about.pack(fill="x")

    # --- LIVE DEVICE TELEMETRY PANEL ---
    # Current, voltage, power, temperature, and PLL lock state.
    def _build_sensor_panel(self, parent):
        group_sensor = GroupBox(parent, "Device Screen (Live)")
        group_sensor.grid(row=0, column=1, sticky="nsew", padx=(8, 0))

        def sensor_label(text):
            lbl = ctk.CTkLabel(group_sensor.body, text=text, height=46, corner_radius=4,
                               fg_color=COLOR_INPUT, text_color=COLOR_WARNING,
                               font=font(17, True))
            lbl.pack(fill="x", pady=(0, 14))
            return lbl

        self.lbl_curr = sensor_label("Current: --.- A")
        self.lbl_volt = sensor_label("Voltage: --.- V")
        self.lbl_power = sensor_label("Power: --.- W")
        self.lbl_temp = sensor_label("Temperature: --.- C")
        self.lbl_pll = sensor_label("LD Result: UNKNOWN")

    # -------------------------------------------------------------------------
    # Style helpers for state-dependent buttons
    # -------------------------------------------------------------------------
    def _style_rf(self, label, color):
        self.btn_rf.configure(text=f"RF OUTPUT: {label}", fg_color=COLOR_PANEL, hover_color=COLOR_HOVER,
                              border_color=color, text_color=color, border_width=2)

    def _style_sweep_button(self, running):
        color = COLOR_ERROR if running else COLOR_SUCCESS
        self.btn_sweep_toggle.configure(
            text="⏹ STOP SWEEP" if running else "▶ START SWEEP",
            fg_color=color, hover_color=lighten(color, 0.15), text_color=COLOR_ON_ACCENT)

    def _set_pll_state(self, text, color):
        self.lbl_pll.configure(text=text, text_color=color)

    # -------------------------------------------------------------------------
    # UI logic
    # -------------------------------------------------------------------------
    def sync_filter_from_cw(self, index):
        """Keep the Sweep filter selector synchronized with the CW filter selector."""
        if self.sw_filt.index() != index:
            self.sw_filt.set_index(index)

    def sync_filter_from_sweep(self, index):
        """Keep the CW filter selector synchronized with the Sweep filter selector."""
        if self.combo_filt.index() != index:
            self.combo_filt.set_index(index)

    def set_preset(self, target, val, unit):
        if target == "start":
            self.sw_start.set_value(val)
            self.sw_start_u.set(unit)
            self._sw_start_unit_last = unit
        elif target == "stop":
            self.sw_stop.set_value(val)
            self.sw_stop_u.set(unit)
            self._sw_stop_unit_last = unit

    @staticmethod
    def _to_hz(value, unit):
        unit_lower = unit.lower()
        if unit_lower == "khz":
            return value * 1e3
        if unit_lower == "mhz":
            return value * 1e6
        if unit_lower == "ghz":
            return value * 1e9
        return value

    @staticmethod
    def _from_hz(hz, unit):
        unit_lower = unit.lower()
        if unit_lower == "khz":
            return hz / 1e3
        if unit_lower == "mhz":
            return hz / 1e6
        if unit_lower == "ghz":
            return hz / 1e9
        return hz

    def _convert_unit_field(self, field, old_unit, new_unit):
        if old_unit == new_unit:
            return
        hz = self._to_hz(field.value(), old_unit)
        field.set_value(self._from_hz(hz, new_unit))

    def _on_cw_frequency_unit_changed(self, new_unit):
        self._convert_unit_field(self.spin_freq, self._cw_freq_unit_last, new_unit)
        self._cw_freq_unit_last = new_unit
        self.update_freq_step()

    def _on_cw_step_unit_changed(self, new_unit):
        self._convert_unit_field(self.spin_freq_step, self._cw_step_unit_last, new_unit)
        self._cw_step_unit_last = new_unit
        self.update_freq_step()

    def _on_sweep_unit_changed(self, field, state_attr, new_unit):
        old_unit = getattr(self, state_attr)
        self._convert_unit_field(field, old_unit, new_unit)
        setattr(self, state_attr, new_unit)

    def update_freq_step(self):
        step_hz = self._to_hz(self.spin_freq_step.value(), self.combo_step_unit.get())
        final_step = self._from_hz(step_hz, self.combo_unit.get())
        self.spin_freq.set_single_step(final_step)

    def validate_frequency(self, value, unit):
        mhz_val = value
        unit_lower = unit.lower()
        if unit_lower == "khz":
            mhz_val = value / 1000.0
        elif unit_lower == "ghz":
            mhz_val = value * 1000.0

        if mhz_val < 150.0:
            self.msg_warning("Invalid Value",
                             "Below 150 MHz. Please enter a value between 150 MHz and 22.6 GHz.")
            return False
        elif mhz_val > 22600.0:
            self.msg_warning("Invalid Value",
                             "You entered a value above 22.6 GHz. Please enter a value between 150 MHz and 22.6 GHz.")
            return False
        return True

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            self.combo_port.set_values(ports)
            self.append_log("[SYS] COM ports refreshed.")
        else:
            self.combo_port.set_values(["No Port Found"])
            self.append_log("[SYS] No COM ports found!")

    def toggle_connection(self):
        if not self.worker.running:
            if self.combo_port.get() != "No Port Found":
                self.worker.connect_device(self.combo_port.get())
        else:
            self.worker.disconnect_device()

    def on_connection_changed(self, connected):
        if connected:
            self.btn_connect.configure(text="DISCONNECT", fg_color=COLOR_ERROR,
                                       hover_color=lighten(COLOR_ERROR, 0.15), text_color=COLOR_ON_ACCENT)
        else:
            self.btn_connect.configure(text="CONNECT", fg_color=COLOR_ACCENT,
                                       hover_color=lighten(COLOR_ACCENT, 0.15), text_color=COLOR_ON_ACCENT)
        self.btn_refresh.configure(state="disabled" if connected else "normal")
        self.combo_port.set_enabled(not connected)

        if not connected:
            self.lbl_curr.configure(text="Current: --.- A")
            self.lbl_volt.configure(text="Voltage: --.- V")
            self.lbl_power.configure(text="Power: --.- W")
            self.lbl_temp.configure(text="Temperature: --.- C")
            self._set_pll_state("LD Result: UNKNOWN", COLOR_WARNING)
            self.lbl_fw_version.configure(text="Firmware: --")
            self.sw_filt.set_enabled(True)

            self.rf_state = False
            self._style_rf("OFF", COLOR_ERROR)
            if self.worker.is_sweeping:
                self.worker.is_sweeping = False
                self._style_sweep_button(False)

    def apply_cw_settings(self):
        if not self.worker.running: return

        if not self.validate_frequency(self.spin_freq.value(), self.combo_unit.get()):
            return

        self.worker.call("set_display_menu", "CW")
        self.worker.call("set_frequency", self.spin_freq.value(), self.combo_unit.get())

        # Send the filter setting first so the power calculation logic can use the correct state.
        self.worker.call("set_filter", self.combo_filt.index() == 1)
        self.worker.call("set_power_level", self.spin_att.value())

        # Read back the actual Target Power applied by the DSG.
        self.worker.request_sync()

        self.append_log(">>> CW Settings applied.")

    def toggle_rf(self):
        if not self.worker.running: return

        self.rf_state = not self.rf_state

        if self.rf_state:
            self.worker.call("set_rf_output", True)
            self.worker.call("set_frequency", self.spin_freq.value(), self.combo_unit.get())
        else:
            self.worker.call("set_rf_output", False)

        self._style_rf(
            "ON" if self.rf_state else "OFF",
            COLOR_SUCCESS if self.rf_state else COLOR_ERROR
        )

    def apply_sweep_settings(self):
        if not self.worker.running: return

        if not self.validate_frequency(self.sw_start.value(), self.sw_start_u.get()):
            return
        if not self.validate_frequency(self.sw_stop.value(), self.sw_stop_u.get()):
            return

        start_hz = self._to_hz(self.sw_start.value(), self.sw_start_u.get())
        stop_hz = self._to_hz(self.sw_stop.value(), self.sw_stop_u.get())

        if start_hz > stop_hz:
            self.msg_warning(
                "Invalid Value",
                "Start frequency cannot be greater than Stop frequency."
            )
            return

        if start_hz == stop_hz:
            self.msg_warning(
                "Invalid Value",
                "Start frequency cannot be equal to Stop frequency."
            )
            return

        self.worker.call("set_display_menu", "SWP")
        self.worker.call("set_filter", self.sw_filt.index() == 1)

        self.worker.call("set_sweep_start", self.sw_start.value(), self.sw_start_u.get())
        self.worker.call("set_sweep_stop", self.sw_stop.value(), self.sw_stop_u.get())
        self.worker.call("set_sweep_step", self.sw_step.value(), self.sw_step_u.get())
        self.worker.call("set_sweep_dwell", self.sw_dwell.value())
        self.worker.call("set_sweep_count", self.sw_count.value())
        self.worker.call("set_sweep_power", self.sw_pow.value())
        t_type = "LIN" if self.sw_type.index() == 0 else "LOG"
        self.worker.call("set_sweep_type", t_type)

        # Read back the actual Target Power allowed for the complete Sweep range.
        self.worker.request_sync()

        self.append_log(">>> Sweep Settings loaded.")

    def toggle_sweep(self):
        if not self.worker.running: return
        if not self.worker.is_sweeping:
            self.start_sweep()
        else:
            self.stop_sweep()

    def start_sweep(self):
        self.worker.is_sweeping = True
        self.sw_filt.set_enabled(False)
        self.worker.call("set_display_menu", "SWP")
        self.worker.call("start_sweep")

        self._style_sweep_button(True)
        self._style_rf("SWEEP MODE", COLOR_WARNING)
        self.rf_state = True
        self.append_log(">>> Sweep Started.")

    def stop_sweep(self):
        self.worker.abort_sweep_urgent()
        self._reset_sweep_ui(">>> Sweep Stopped.")

    def on_sweep_auto_stopped(self):
        # Called when the device stopped the sweep on its own (the configured
        # Sweep Count was reached) rather than the user pressing Stop. The
        # worker has already set is_sweeping = False and the device has
        # already turned the sweep/PLL off, so we only need to bring the UI
        # (button labels/colors) back in sync - no abort command needed.
        self._reset_sweep_ui(">>> Sweep finished (reached configured Count).")

    def _reset_sweep_ui(self, log_text):
        self.sw_filt.set_enabled(True)
        self._style_sweep_button(False)

        self.rf_state = False
        self._style_rf("OFF", COLOR_ERROR)
        self.append_log(log_text)

    # -------------------------------------------------------------------------
    # GUI telemetry update
    # Converts raw ADC values into user-facing current, voltage, power,
    # temperature, and PLL lock status values.
    # -------------------------------------------------------------------------
    def update_sensors(self, data):
        try:
            current_val = None
            voltage_val = None

            if "current_a" in data:
                current_val = data["current_a"]
                self.lbl_curr.configure(text=f"Current: {current_val:.2f} A")

            if "voltage_v" in data:
                voltage_val = data["voltage_v"]
                self.lbl_volt.configure(text=f"Voltage: {voltage_val:.2f} V")

            if current_val is not None and voltage_val is not None:
                self.lbl_power.configure(text=f"Power: {data['power_w']:.2f} W")

            if "temperature_c" in data:
                self.lbl_temp.configure(text=f"Temperature: {data['temperature_c']:.1f} C")

            if "pll_locked" in data:
                if data["pll_locked"]:
                    self._set_pll_state("LD Result: LOCKED", COLOR_SUCCESS)
                else:
                    self._set_pll_state("LD Result: UNLOCKED", COLOR_ERROR)
        except Exception:
            pass

    def append_log(self, msg):
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        self.txt_log.configure(state="normal")
        self.txt_log.insert("end", f"[{ts}] {msg}\n")
        self.txt_log.configure(state="disabled")
        self.txt_log.see("end")

    def clear_log(self):
        self.txt_log.configure(state="normal")
        self.txt_log.delete("1.0", "end")
        self.txt_log.configure(state="disabled")

    # -------------------------------------------------------------------------
    # Calibration CSV loader
    # Reads calibration rows from a CSV file and transfers them to the device in
    # small paced batches to avoid overloading the serial communication buffer.
    # -------------------------------------------------------------------------
    def load_calibration_csv(self):
        filepath = filedialog.askopenfilename(
            parent=self,
            title="Select Calibration CSV File",
            filetypes=[("CSV Files", "*.csv"), ("All Files", "*")]
        )

        if not filepath:
            return

        filename = os.path.basename(filepath)

        # ---------------------------------------------------------
        # File name must start with SN_XXX_
        # ---------------------------------------------------------
        serial_match = re.match(r"^(SN_\d{3})_", filename, re.IGNORECASE)

        if not serial_match:
            self.msg_warning(
                "Invalid Calibration File Name",
                "Calibration file name must start with SN_XXX_ format.\n\n"
                "Examples:\n"
                "SN_010_Calib_Optimized_FILT_T-F.csv\n"
                "SN_010_HighPowerCalib_FILT_T-F.csv"
            )
            self.append_log(f"[ERR] Invalid calibration file name: {filename}")
            return

        device_serial = serial_match.group(1).upper()
        filename_lower = filename.lower()

        # ---------------------------------------------------------
        # Decide calibration type from FILE NAME.
        # ---------------------------------------------------------
        if "highpowercalib" in filename_lower:
            calibration_type = "HIGH_POWER"
        elif "calib_optimized" in filename_lower:
            calibration_type = "NORMAL"
        else:
            self.msg_warning(
                "Unknown Calibration Type",
                "Calibration file type could not be identified from the file name.\n\n"
                "Expected names containing:\n"
                "Calib_Optimized\n"
                "or\n"
                "HighPowerCalib"
            )
            self.append_log(f"[ERR] Unknown calibration file type: {filename}")
            return

        try:
            with open(filepath, 'r', encoding='utf-8-sig', errors='ignore') as f:
                lines = f.readlines()

            if len(lines) < 2:
                self.append_log("[ERR] CSV file is empty or invalid!")
                return

            self.append_log(f"[CAL] Device Serial Number: {device_serial}")

            if calibration_type == "NORMAL":
                self._upload_normal_calibration(lines, device_serial)
            elif calibration_type == "HIGH_POWER":
                self._upload_high_power_calibration(lines, device_serial)

        except Exception as e:
            self.append_log(f"[ERR] CSV upload error: {str(e)}")

    # =====================================================
    # NORMAL CALIBRATION
    # =====================================================
    @staticmethod
    def _parse_normal_val(val_str, is_filton, freq):
        v = val_str.strip().replace(',', '.')

        if not v or v.lower() == 'nan':
            return -1

        try:
            val_float = float(v)

            if is_filton and not (2000 <= freq <= 18000):
                return -1

            return int(round(val_float))

        except ValueError:
            return -1

    def _upload_normal_calibration(self, lines, device_serial):
        self.append_log("[CAL] Loading NORMAL calibration...")

        self.worker.call("set_serial_number", device_serial)
        self.worker.call("clear_calibration")

        success_count = 0

        for i in range(1, len(lines)):
            line = lines[i].strip()

            if not line:
                continue

            cols = line.split(';')

            # Existing normal calibration CSV requires
            # at least 18 columns.
            if len(cols) < 13:
                continue

            try:
                freq = int(float(cols[0].strip().replace(',', '.')))

                att6_on = self._parse_normal_val(cols[1], True, freq)
                att3_on = self._parse_normal_val(cols[3], True, freq)
                attn3_on = self._parse_normal_val(cols[5], True, freq)

                # Old normal calibration CSV format
                if len(cols) >= 18:
                    att6_off = self._parse_normal_val(cols[13], False, freq)
                    att3_off = self._parse_normal_val(cols[15], False, freq)
                    attn3_off = self._parse_normal_val(cols[17], False, freq)

                # New normal calibration CSV format
                else:
                    att6_off = self._parse_normal_val(cols[7], False, freq)
                    att3_off = self._parse_normal_val(cols[9], False, freq)
                    attn3_off = self._parse_normal_val(cols[11], False, freq)

                self.worker.call(
                    "add_calibration_point",
                    freq, att6_on, att3_on, attn3_on, att6_off, att3_off, attn3_off
                )

                # --- SERIAL BUFFER PROTECTION DELAY ---
                if i % 10 == 0:
                    time.sleep(0.05)

                success_count += 1

            except Exception:
                continue

        time.sleep(0.1)

        self.worker.call("save_calibration")

        self.append_log(
            f"[CAL] NORMAL calibration transfer complete. "
            f"{success_count} rows uploaded."
        )

        self.msg_info(
            "Normal Calibration Upload Complete",
            f"Device Serial Number: {device_serial}\n\n"
            f"{success_count} normal calibration points "
            f"were transferred and saved."
        )

    # =====================================================
    # HIGH POWER CALIBRATION
    # =====================================================
    # Empty High Power cells are intentionally stored as -1.
    # -1 means this target power is unavailable at this frequency.
    @staticmethod
    def _parse_hp_val(val_str):
        v = val_str.strip().replace(',', '.')

        if not v or v.lower() == 'nan':
            return -1

        try:
            return int(round(float(v)))
        except ValueError:
            return -1

    def _upload_high_power_calibration(self, lines, device_serial):
        self.append_log("[HPCAL] Loading HIGH POWER calibration...")

        # Keep device serial synchronized.
        self.worker.call("set_serial_number", device_serial)

        # Clears ONLY the High Power RAM table.
        # Normal calibration is untouched.
        self.worker.call("clear_high_power_calibration")

        success_count = 0

        for i in range(1, len(lines)):
            line = lines[i].strip()

            if not line:
                continue

            cols = line.split(';')

            # Merged High Power CSV format:
            # 0  Freq_MHz
            # 1  Att8_ON
            # 2  Att9_ON
            # 3  Att10_ON
            # 4  Att11_ON
            # 5  Att12_ON
            # 6  Att13_OFF
            # 7  Att14_OFF
            # 8  Att15_OFF
            # 9  Att16_OFF
            # 10 Att17_OFF
            # 11 Att18_OFF
            if len(cols) < 12:
                continue

            try:
                freq = int(float(cols[0].strip().replace(',', '.')))

                att8_on = self._parse_hp_val(cols[1])
                att9_on = self._parse_hp_val(cols[2])
                att10_on = self._parse_hp_val(cols[3])
                att11_on = self._parse_hp_val(cols[4])
                att12_on = self._parse_hp_val(cols[5])

                att13_off = self._parse_hp_val(cols[6])
                att14_off = self._parse_hp_val(cols[7])
                att15_off = self._parse_hp_val(cols[8])
                att16_off = self._parse_hp_val(cols[9])
                att17_off = self._parse_hp_val(cols[10])
                att18_off = self._parse_hp_val(cols[11])

                self.worker.call(
                    "add_high_power_calibration_point",
                    freq, att8_on, att9_on, att10_on, att11_on, att12_on,
                    att13_off, att14_off, att15_off, att16_off, att17_off, att18_off
                )

                # Same serial-buffer protection used by normal calibration.
                if i % 10 == 0:
                    time.sleep(0.05)

                success_count += 1

            except Exception:
                continue

        time.sleep(0.1)

        self.worker.call("save_high_power_calibration")

        self.append_log(
            f"[HPCAL] HIGH POWER calibration transfer complete. "
            f"{success_count} rows uploaded."
        )

        self.msg_info(
            "High Power Calibration Upload Complete",
            f"Device Serial Number: {device_serial}\n\n"
            f"{success_count} High Power calibration points "
            f"were transferred and saved."
        )

    # -------------------------------------------------------------------------
    # Firmware update (esptool run as a child process, output streamed to the log)
    # -------------------------------------------------------------------------
    def flash_firmware(self):
        """Uploads the selected .bin file to the device via esptool (no Arduino IDE required)."""
        port = self.combo_port.get()
        if not port:
            self.msg_warning("No Port Selected", "Please select a COM port first.")
            return

        # If the serial port is already open (connected), release it before flashing.
        if self.worker.running:
            self.append_log("[FW] Closing existing connection before flashing...")
            self.toggle_connection()

        filepath = filedialog.askopenfilename(
            parent=self,
            title="Select Firmware (.bin) File",
            filetypes=[("Binary Files", "*.bin"), ("All Files", "*")]
        )
        if not filepath:
            return

        if not self.msg_question(
                "Firmware Update",
                f"Selected file:\n{filepath}\n\nPort: {port}\n\nStart the flashing process?"):
            return

        self.btn_firmware_update.configure(state="disabled")
        self.append_log(f"[FW] Uploading firmware: {filepath}")
        self.append_log(f"[FW] Port: {port}")

        args = [
            "--chip", "esp32s3",
            "--port", port,
            "--baud", "460800",
            "write_flash", "0x0", filepath
        ]

        # In the packaged executable, sys.executable is the app itself: it re-runs
        # itself in esptool mode (see ESPTOOL_FLAG) instead of "python -m esptool".
        if getattr(sys, "frozen", False):
            cmd = [sys.executable, ESPTOOL_FLAG] + args
        else:
            cmd = [sys.executable, "-m", "esptool"] + args

        try:
            self.firmware_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
            )
        except OSError as e:
            self.btn_firmware_update.configure(state="normal")
            self.append_log(f"[FW] Could not start esptool: {e}")
            self.firmware_process = None
            return

        threading.Thread(target=self._pump_firmware_output,
                         args=(self.firmware_process,), daemon=True).start()

    def _pump_firmware_output(self, proc):
        """Runs in a helper thread: forwards esptool output lines to the GUI, then the exit code."""
        try:
            for raw in iter(lambda: proc.stdout.read1(4096), b""):
                for line in raw.decode(errors="ignore").splitlines():
                    if line.strip():
                        self.events.put(("fw_line", line.strip()))
        finally:
            self.events.put(("fw_done", proc.wait()))

    def _firmware_flash_finished(self, exit_code):
        self.btn_firmware_update.configure(state="normal")
        if exit_code == 0:
            self.append_log("[FW] Firmware update complete.")
            self.msg_info("Complete", "Firmware updated successfully.")
        else:
            self.append_log(f"[FW] Firmware update failed (exit code: {exit_code}).")
            self.msg_error("Error", "Firmware update failed. Check System Logs for details.")
        self.firmware_process = None

    def sync_ui_with_device(self, data):
        """Populates all UI fields using the JSON packet received from the device."""
        try:
            self.spin_freq.set_value(float(data.get("cw_freq", 1000)))
            cw_unit = data.get("cw_unit", "MHz")
            self.combo_unit.set(cw_unit)
            self._cw_freq_unit_last = cw_unit
            self.spin_att.set_value(float(data.get("cw_amp", 0)))
            filter_index = 1 if int(data.get("filt", 0)) else 0
            self.combo_filt.set_index(filter_index)
            self.sw_filt.set_index(filter_index)

            self.sw_start.set_value(float(data.get("sw_start", 300)))
            sw_start_unit = data.get("sw_start_u", "MHz")
            self.sw_start_u.set(sw_start_unit)
            self._sw_start_unit_last = sw_start_unit

            self.sw_stop.set_value(float(data.get("sw_stop", 22600)))
            sw_stop_unit = data.get("sw_stop_u", "MHz")
            self.sw_stop_u.set(sw_stop_unit)
            self._sw_stop_unit_last = sw_stop_unit

            self.sw_step.set_value(float(data.get("sw_step", 100)))
            sw_step_unit = data.get("sw_step_u", "MHz")
            self.sw_step_u.set(sw_step_unit)
            self._sw_step_unit_last = sw_step_unit
            self.sw_dwell.set_value(float(data.get("sw_dwell", 1)))
            self.sw_count.set_value(int(float(data.get("sw_count", 0))))
            self.sw_pow.set_value(float(data.get("sw_amp", 10)))

            t_type = data.get("sw_type", "Lin")
            self.sw_type.set_index(0 if t_type == "Lin" else 1)

            # Unit combos were changed programmatically; keep conversion state and CW step size consistent.
            self._cw_step_unit_last = self.combo_step_unit.get()
            self.update_freq_step()

            fw_version = data.get("fw_version")
            fw_build = data.get("fw_build")
            if fw_version:
                fw_text = f"Firmware: v{fw_version}" if fw_version != "dev" else "Firmware: dev"
                fw_text += f" ({data.get('fw_commit', 'unknown')}, {fw_build})"
                self.lbl_fw_version.configure(text=fw_text)
            elif fw_build:
                # Older firmware that only reports its build date
                self.lbl_fw_version.configure(text=f"Firmware: {fw_build}")

            self.append_log("[SYS] State Synchronized with Device Screen.")
        except Exception as e:
            self.append_log(f"[ERR] Sync parsing error: {e}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == ESPTOOL_FLAG:
        import esptool
        try:
            esptool.main(sys.argv[2:])
        except Exception as e:
            print(f"esptool error: {e}")
            sys.exit(1)
        sys.exit(0)

    ctk.set_appearance_mode("dark")
    app = DSGMainWindow()
    app.mainloop()