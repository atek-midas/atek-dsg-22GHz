"""
DSG 22.6 GHz - Python control API

Every remote command supported by the DSG firmware is available as a method of
the DSG class, so the device can be fully controlled from a Python script
without the desktop UI. The desktop UI (gui.py) uses this same class.

Example:

    from dsg_api import DSG

    with DSG("COM5") as dsg:
        print(dsg.identify())
        dsg.set_display_menu("CW")
        dsg.set_frequency(2.4, "GHz")
        dsg.set_filter(False)
        dsg.set_power_level(0)
        dsg.set_rf_output(True)
        print(dsg.read_telemetry())

Requires: pyserial
"""

import json
import time

import serial


class DSG:
    # Units accepted by frequency commands
    FREQ_UNITS = ("Hz", "KHz", "MHz", "GHz")

    # ADC channels used for telemetry
    ADC_VOLTAGE = 2
    ADC_CURRENT = 3
    ADC_TEMPERATURE = 4

    # PLL register that holds the lock-detect readback (rb_LD)
    PLL_LOCK_REGISTER = 74

    def __init__(self, port, baudrate=115200, timeout=0.1, message_handler=None):
        """Open the serial port and connect to the device.

        message_handler: optional function called with every device line that is
        not the reply to a command (sweep progress, "SWEEP:DONE", error messages
        such as "-222,Data out of range").
        """
        self.port = port
        self.message_handler = message_handler
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(1.0)
        self.write("*IDN?")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    # -------------------------------------------------------------------------
    # Connection and low-level access
    # -------------------------------------------------------------------------
    def is_open(self):
        """Check whether the serial connection is currently open."""
        return self.ser is not None and self.ser.is_open

    def close(self):
        """Close the serial port connection."""
        if self.is_open():
            self.ser.close()

    def write(self, cmd):
        """Send a raw SCPI command without waiting for a reply."""
        if self.is_open():
            try:
                self.ser.write((cmd + "\n").encode())
            except Exception:
                pass

    def in_waiting(self):
        """Return the number of data bytes waiting in the serial input buffer."""
        if self.is_open():
            return self.ser.in_waiting
        return 0

    def read_line(self):
        """Read a single line from the serial port and return it as a string."""
        if self.is_open():
            try:
                return self.ser.readline().decode(errors="ignore").strip()
            except Exception:
                pass
        return ""

    def _handle_message(self, line):
        if self.message_handler is not None:
            self.message_handler(line)

    def command(self, cmd, expected="0", timeout=0.5):
        """Send a SCPI command and wait for its acknowledgment.

        Returns True when the expected reply arrives within the timeout. Any other
        line received meanwhile (e.g. an error message) is passed to the
        message_handler.
        """
        self.write(cmd)
        return self._wait_for_reply(expected, timeout)

    def _wait_for_reply(self, expected, timeout):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self.in_waiting() > 0:
                resp = self.read_line()
                if resp == expected:
                    return True
                if resp:
                    self._handle_message(resp)
            time.sleep(0.005)

        # Give the device a little more time if it did not acknowledge in time.
        time.sleep(0.05)
        return False

    def query(self, cmd, timeout=0.5):
        """Send a SCPI query (e.g. "*IDN?") and return the first reply line, or None."""
        if not self.is_open():
            return None
        try:
            self.ser.reset_input_buffer()
            self.write(cmd)
            t0 = time.time()
            while time.time() - t0 < timeout:
                if self.in_waiting() > 0:
                    resp = self.read_line()
                    if resp:
                        return resp
                time.sleep(0.005)
        except Exception:
            pass
        return None

    # -------------------------------------------------------------------------
    # Identification
    # -------------------------------------------------------------------------
    def identify(self):
        """Return the device identification string (*IDN?).

        Format: "DSG,DSG-22.6GHz,<serial number>,<firmware version>"
        """
        return self.query("*IDN?")

    def get_firmware_version(self):
        """Return the firmware version, e.g. "1.3.0" ("dev" for local builds)."""
        return self.query(":SYST:VERS?")

    def get_firmware_info(self):
        """Return the firmware version, git commit and build date as a dict.

        Example: {"version": "1.3.0", "commit": "409169d",
                  "build_date": "Oct 12 2026 21:35:24"}
        The build date is the compile time (UTC for release builds).
        """
        info = {"version": self.get_firmware_version(), "commit": None, "build_date": None}
        build = self.query(":SYST:BUILD?")
        if build:
            commit, _, build_date = build.partition(" ")
            info["commit"] = commit
            info["build_date"] = build_date
        return info

    # -------------------------------------------------------------------------
    # Display
    # -------------------------------------------------------------------------
    def set_display_menu(self, menu):
        """Switch the device screen: "CW" (main) or "SWP" (sweep)."""
        return self.command(f":DISP:MENU {menu}")

    def get_display_menu(self):
        """Return the current screen: "CW", "SWP" or "UNKNOWN"."""
        return self.query(":DISP:MENU?")

    # -------------------------------------------------------------------------
    # CW (continuous wave) output
    # -------------------------------------------------------------------------
    def set_frequency(self, value, unit="MHz"):
        """Set the CW output frequency, e.g. set_frequency(2.4, "GHz")."""
        return self.command(f":FREQ {value}{unit}")

    def get_frequency(self):
        """Return the current CW frequency as reported by the device."""
        return self.query(":FREQ?")

    def set_power_level(self, dbm):
        """Set the calibrated target output power in dBm.

        Set the filter first: the available power depends on the filter state.
        The power actually applied can be read back with get_power_level() or
        get_settings().
        """
        return self.command(f":POW:LEV {dbm}")

    def get_power_level(self):
        """Return the target power applied by the device (dBm)."""
        return self.query(":POW:LEV?")

    def set_filter(self, on):
        """Enable (True, 2-18 GHz) or disable (False, 0.15-22.6 GHz) the filter."""
        return self.command(f":FILT {'ON' if on else 'OFF'}", expected="1" if on else "0")

    def get_filter(self):
        """Return True if the filter is enabled, False if disabled, None if unknown."""
        resp = self.query(":FILT?")
        if resp in ("0", "1"):
            return resp == "1"
        return None

    def set_rf_output(self, on):
        """Turn the RF output on (True) or off (False)."""
        return self.command(f":OUTP {'ON' if on else 'OFF'}", expected="1" if on else "0")

    def get_rf_output(self):
        """Return True if the RF output is on, False if off, None if unknown."""
        resp = self.query(":OUTP?")
        if resp in ("0", "1"):
            return resp == "1"
        return None

    # -------------------------------------------------------------------------
    # Sweep
    # -------------------------------------------------------------------------
    def set_sweep_start(self, value, unit="MHz"):
        """Set the sweep start frequency."""
        return self.command(f":SWEEP:STAR {value}{unit}")

    def set_sweep_stop(self, value, unit="MHz"):
        """Set the sweep stop frequency."""
        return self.command(f":SWEEP:STOP {value}{unit}")

    def set_sweep_step(self, value, unit="MHz"):
        """Set the sweep frequency step."""
        return self.command(f":SWEEP:STEP {value}{unit}")

    def set_sweep_dwell(self, ms):
        """Set the dwell time per sweep point in milliseconds."""
        return self.command(f":SWEEP:DWEL {ms}")

    def set_sweep_count(self, count):
        """Set the number of sweep cycles (0 = run until aborted)."""
        return self.command(f":SWEEP:COUN {count}")

    def set_sweep_power(self, dbm):
        """Set the sweep target power in dBm."""
        return self.command(f":SWEEP:POW {dbm}")

    def set_sweep_type(self, sweep_type):
        """Set the sweep type: "LIN" (linear) or "LOG" (logarithmic)."""
        return self.command(f":SWEEP:TYPE {sweep_type}")

    def start_sweep(self):
        """Start the sweep from the beginning (also enables the RF output).

        While the sweep runs, the device reports each frequency as a
        "Freq(MHz): ..." line and sends "SWEEP:DONE" when a finite count ends;
        these lines go to the message_handler.
        """
        return self.command(":SWEEP:INIT")

    def abort_sweep(self):
        """Abort the running sweep immediately and turn the RF output off.

        Does not wait for a reply, so it can be used as an emergency stop.
        """
        self.write(":SWEEP:ABOR")
        if self.is_open():
            try:
                self.ser.reset_input_buffer()  # Clear any blocked or pending serial input data
            except Exception:
                pass

    # -------------------------------------------------------------------------
    # Telemetry
    # -------------------------------------------------------------------------
    def read_adc(self, channel):
        """Read the raw voltage of an ADC channel."""
        if not self.is_open():
            return 0.0
        try:
            cmd = f":ADC:READ? {channel}\n".encode()
            # Sequential read-and-wait strategy used to obtain a stable ADC response:
            self.ser.write(cmd)
            time.sleep(0.02)
            self.ser.readline()  # Discard the first stale/empty response
            self.ser.write(cmd)
            time.sleep(0.02)
            resp = self.ser.readline().decode(errors="ignore").strip()

            if resp:
                return float(resp)
        except Exception:
            pass
        return 0.0

    def read_adc_averaged(self, channel):
        """One dummy read, then the average of three ADC readings to reduce noise."""
        if not self.is_open():
            return 0.0

        self.read_adc(channel)
        time.sleep(0.01)

        val1 = self.read_adc(channel)
        val2 = self.read_adc(channel)
        val3 = self.read_adc(channel)

        return (val1 + val2 + val3) / 3.0

    def read_current(self):
        """Return the supply current in A."""
        return self.read_adc_averaged(self.ADC_CURRENT)

    def read_voltage(self):
        """Return the supply voltage in V."""
        return self.read_adc_averaged(self.ADC_VOLTAGE) * 2.0

    def read_temperature(self):
        """Return the board temperature in degrees C."""
        return (self.read_adc_averaged(self.ADC_TEMPERATURE) - 0.5) * 100.0

    def is_pll_locked(self):
        """Return True if the PLL reports lock."""
        return self._pll_locked(self.read_pll_register(self.PLL_LOCK_REGISTER))

    @staticmethod
    def _pll_locked(reg_value):
        return ((reg_value >> 14) & 0x03) == 2

    def read_telemetry(self):
        """Read all telemetry values at once.

        Returns a dict with: current_a, voltage_v, power_w, temperature_c,
        pll_locked and pll_raw (the raw lock-detect register value).
        """
        current = self.read_current()
        temperature = self.read_temperature()
        voltage = self.read_voltage()
        pll_raw = self.read_pll_register(self.PLL_LOCK_REGISTER)

        return {
            "current_a": current,
            "voltage_v": voltage,
            "power_w": current * voltage,
            "temperature_c": temperature,
            "pll_locked": self._pll_locked(pll_raw),
            "pll_raw": pll_raw,
        }

    # -------------------------------------------------------------------------
    # Device state
    # -------------------------------------------------------------------------
    def get_settings(self):
        """Return the device's current CW/sweep/display settings as a dict (or None).

        Keys include cw_freq, cw_unit, cw_amp, filt, sw_start, sw_start_u,
        sw_stop, sw_stop_u, sw_step, sw_step_u, sw_dwell, sw_count, sw_amp,
        sw_type, fw_version, fw_commit and fw_build.
        """
        if not self.is_open():
            return None

        try:
            self.ser.reset_input_buffer()
            self.ser.write(b":SYNC?\n")

            timeout_time = time.time() + 0.5

            while time.time() < timeout_time:
                if self.ser.in_waiting > 0:
                    resp = self.ser.readline().decode(errors="ignore").strip()

                    if resp.startswith("{"):
                        return json.loads(resp)

                time.sleep(0.005)

        except Exception:
            pass

        return None

    # -------------------------------------------------------------------------
    # Calibration
    # -------------------------------------------------------------------------
    def set_serial_number(self, serial_number):
        """Set the device serial number (format "SN_XXX")."""
        return self.command(f":CAL:SER {serial_number}")

    def get_serial_number(self):
        """Return the device serial number."""
        return self.query(":CAL:SER?")

    def clear_calibration(self):
        """Clear the normal calibration table in device RAM."""
        return self.command(":CAL:CLEAR")

    def add_calibration_point(self, freq_mhz, att6_on, att3_on, attn3_on,
                              att6_off, att3_off, attn3_off):
        """Append one normal calibration point (frequency + 6 attenuation values)."""
        return self.command(
            f":CAL:DATA {freq_mhz},{att6_on},{att3_on},{attn3_on},"
            f"{att6_off},{att3_off},{attn3_off}"
        )

    def save_calibration(self):
        """Save the normal calibration table and serial number to device flash."""
        return self.command(":CAL:SAVE")

    def clear_high_power_calibration(self):
        """Clear the high-power calibration table in device RAM (normal table is untouched)."""
        return self.command(":HPCAL:CLEAR")

    def add_high_power_calibration_point(self, freq_mhz, att8_on, att9_on, att10_on,
                                         att11_on, att12_on, att13_off, att14_off,
                                         att15_off, att16_off, att17_off, att18_off):
        """Append one high-power calibration point (frequency + 11 attenuation values).

        Use -1 for a target power that is unavailable at this frequency.
        """
        return self.command(
            f":HPCAL:DATA {freq_mhz},{att8_on},{att9_on},{att10_on},{att11_on},"
            f"{att12_on},{att13_off},{att14_off},{att15_off},{att16_off},"
            f"{att17_off},{att18_off}"
        )

    def save_high_power_calibration(self):
        """Save the high-power calibration table to device flash."""
        return self.command(":HPCAL:SAVE")

    # -------------------------------------------------------------------------
    # Advanced / service commands
    # -------------------------------------------------------------------------
    def set_attenuator(self, db):
        """Set the step attenuator directly (0..31 dB), bypassing calibration."""
        return self.command(f":POW:ATT {db}")

    def get_attenuator(self):
        """Return the cached attenuator value."""
        return self.query(":POW:ATT?")

    def set_pll_output_power(self, level, output="AB"):
        """Set the LMX2820 output power (0..7) for output "A", "B" or both ("AB")."""
        name = {"A": "OUTA", "B": "OUTB", "AB": "LO"}[output.upper()]
        ok = self.command(f":POW:{name} {level}")
        if name == "LO":
            # The device replies once per output; wait for the second reply too.
            ok = self._wait_for_reply("0", 0.5) and ok
        return ok

    def read_pll_register(self, register):
        """Read a PLL register and return its value as an integer."""
        if not self.is_open():
            return 0
        try:
            cmd = f":PLL:READ? {register}\n".encode()
            self.ser.write(cmd)
            time.sleep(0.02)
            resp = self.ser.readline().decode(errors="ignore").strip()

            if resp:
                # Convert the incoming hexadecimal string using base 16.
                return int(resp, 16)
        except Exception:
            pass
        return 0

    def write_pll_register(self, register, value):
        """Write a 16-bit value to a PLL register."""
        return self.command(f":PLL:WRITE {register},{value:04X}")

    def read_io_expander(self, register, expander=1):
        """Read an MCP23S17 I/O expander register."""
        resp = self.query(f":IOEXP:READ {expander},{register}")
        try:
            return int(resp)
        except (TypeError, ValueError):
            return None

    def write_io_expander(self, register, value, expander=1):
        """Write an MCP23S17 I/O expander register."""
        return self.command(f":IOEXP:WRITE {expander},{register},{value}")
