"""
DSG 22.6 GHz - dsg_api usage example

Turns on a 2.4 GHz CW signal at 0 dBm for a few seconds, prints the device
information and telemetry, then turns the RF output off again.

Usage:
    python api_usage_example.py          (uses PORT below)
    python api_usage_example.py COM5     (uses the given port)
"""

import sys
import time

from dsg_api import DSG

PORT = "COM4"           # serial port of the DSG (Windows: COMx, Linux: /dev/ttyACMx)
RF_ON_SECONDS = 5       # how long the RF output stays on


def print_telemetry(dsg):
    t = dsg.read_telemetry()
    print(f"  Current     : {t['current_a']:.2f} A")
    print(f"  Voltage     : {t['voltage_v']:.2f} V")
    print(f"  Power       : {t['power_w']:.2f} W")
    print(f"  Temperature : {t['temperature_c']:.1f} C")
    print(f"  PLL         : {'LOCKED' if t['pll_locked'] else 'UNLOCKED'}")


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else PORT
    print(f"Connecting to {port}...")

    # "with" closes the serial port at the end, even if an error occurs.
    # message_handler prints device messages such as error replies.
    with DSG(port, message_handler=lambda msg: print(f"  Device message: {msg}")) as dsg:
        print(f"Device        : {dsg.identify()}")
        print(f"Serial number : {dsg.get_serial_number()}")
        fw = dsg.get_firmware_info()
        print(f"Firmware      : {fw['version']} (commit {fw['commit']}, built {fw['build_date']})")

        print("\nSetting CW output: 2.4 GHz, filter OFF, 0 dBm")
        dsg.set_display_menu("CW")
        dsg.set_frequency(2.4, "GHz")
        dsg.set_filter(False)       # set the filter before the power level
        dsg.set_power_level(0)

        settings = dsg.get_settings()
        if settings:
            print(f"  Applied: {settings.get('cw_freq')} {settings.get('cw_unit')}, "
                  f"{settings.get('cw_amp')} dBm")

        print(f"\nRF output ON for {RF_ON_SECONDS} seconds")
        dsg.set_rf_output(True)
        time.sleep(1)               # let the output settle before reading telemetry
        print_telemetry(dsg)
        time.sleep(max(0, RF_ON_SECONDS - 1))

        dsg.set_rf_output(False)
        print("\nRF output OFF")

    print("Disconnected.")


if __name__ == "__main__":
    main()
