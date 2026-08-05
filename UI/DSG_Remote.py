import serial
import time


class DSG_Remote:
    def __init__(self, port, baudrate=115200, timeout=0.1):
        """Initialize the driver class and connect to the serial port."""
        self.port = port
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(1.0)
        self._write("*IDN?")

    def is_open(self):
        """Check whether the serial connection is currently open."""
        return self.ser is not None and self.ser.is_open

    def close(self):
        """Close the serial port connection."""
        if self.is_open():
            self.ser.close()

    def _write(self, cmd):
        """Send a string or SCPI command to the device."""
        if self.is_open():
            try:
                self.ser.write((cmd + "\n").encode())
            except Exception:
                pass

    def ADCRead(self, channel):
        """Read the ADC value from the specified channel and return it as a float."""
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
            resp = self.ser.readline().decode(errors='ignore').strip()

            if resp:
                return float(resp)
        except Exception:
            pass
        return 0.0

    def PLLRead(self, register):
        """Read the specified PLL register and return the value as an integer."""
        if not self.is_open():
            return 0
        try:
            cmd = f":PLL:READ? {register}\n".encode()
            self.ser.write(cmd)
            time.sleep(0.02)
            resp = self.ser.readline().decode(errors='ignore').strip()

            if resp:
                # Convert the incoming hexadecimal string using base 16.
                return int(resp, 16)
        except Exception:
            pass
        return 0

    def in_waiting(self):
        """Return the number of data bytes waiting in the serial input buffer."""
        if self.is_open():
            return self.ser.in_waiting
        return 0

    def read_line(self):
        """Read a single line from the serial port and return it as a string."""
        if self.is_open():
            try:
                return self.ser.readline().decode(errors='ignore').strip()
            except Exception:
                pass
        return ""

    def SweepAbortUrgent(self):
        """Send the emergency stop command to abort the active sweep immediately."""
        self._write(":SWEEP:ABOR")
        if self.is_open():
            try:
                self.ser.reset_input_buffer()  # Clear any blocked or pending serial input data
            except Exception:
                pass

    def GetSyncSettings(self):
        """Read the device's current live settings as a JSON object."""
        if not self.is_open():
            return None
        try:
            self.ser.reset_input_buffer()
            cmd = b":SYNC?\n"
            self.ser.write(cmd)
            time.sleep(0.05)
            # Read the expected JSON-formatted response line.
            resp = self.ser.readline().decode(errors='ignore').strip()
            if resp and resp.startswith("{"):
                import json
                return json.loads(resp)
        except Exception:
            pass
        return None
