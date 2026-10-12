# Third-Party Software Notices

The ATEK DSG desktop application uses third-party open-source software
components. These components are developed and maintained by their respective
copyright holders and are not owned or developed by ATEK MIDAS.

Each third-party component remains subject to its own license terms and
copyright notices.

This notice lists the direct third-party dependencies currently used by the
ATEK DSG desktop application. A packaged release may also contain transitive
dependencies required by these components; their original license notices
must be retained or distributed where required.

## CustomTkinter

CustomTkinter is a modern Python UI library based on Tkinter.

License:
MIT License

CustomTkinter is used by the DSG desktop UI to implement the graphical user
interface.

Website:
https://github.com/TomSchimansky/CustomTkinter

---

## pySerial

pySerial is a Python library providing serial port communication for Python
applications.

Copyright:
Chris Liechti and contributors

License:
BSD-3-Clause

pySerial is used by the DSG desktop UI and DSG communication layer for serial
communication with the device.

Website:
https://github.com/pyserial/pyserial

---

## esptool

esptool is a Python-based utility for flashing, provisioning, and interacting
with Espressif SoCs.

Copyright:
Espressif Systems and contributors

License:
GNU General Public License v2.0 or later (GPL-2.0-or-later)

esptool is invoked by the DSG desktop UI for firmware update functionality.

Website:
https://github.com/espressif/esptool

---

## Python Standard Library / Tkinter

The DSG desktop UI uses Python standard-library modules including Tkinter,
sys, os, re, time, queue, datetime, threading, subprocess, webbrowser and
collections.

These modules are part of the Python distribution rather than separate ATEK
MIDAS components. If a packaged release includes a Python runtime, Tcl/Tk, or
other runtime components, the applicable upstream license notices should also
be distributed as required.

---

## ATEK DSG Desktop UI Licensing

The original ATEK MIDAS source code of the DSG desktop user interface is
licensed under the MIT License.

Third-party software is not relicensed by the ATEK DSG MIT License and remains
subject to the licenses listed above and any applicable licenses of bundled
transitive dependencies.
