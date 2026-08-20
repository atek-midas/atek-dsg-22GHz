## Firmware

The DSG firmware is written for the ESP32-S3 using the Arduino IDE.

### 1. Install the Arduino IDE

Download and install the [Arduino IDE](https://www.arduino.cc/en/software) (developed and tested with **version 2.3.10**).

### 2. Install the Required Boards

Open **Tools → Board → Boards Manager**, and install the following board packages:

| Board Package | Version | Description |
|---|---|---|
| Arduino AVR Boards *(by Arduino)* | 1.8.8 | Includes boards such as Arduino UNO, Arduino Duemilanove/Diecimila, and Arduino Mega ADK. |
| Arduino ESP32 Boards *(by Arduino)* | 2.0.18 | Includes the Arduino Nano ESP32 board. |
| esp32 *(by Espressif Systems)* | 2.0.17 | Includes ESP32 boards such as the ESP32 Wrover Module, IntoRobot Fig, Adafruit QT Py ESP32, and BPI-Leaf-S3 — this package provides the **ESP32S3 Dev Module** board used by the DSG. |

### 3. Install the Required Libraries

Open **Tools → Manage Libraries…**, and install the following libraries:

| Library | Author | Version | Description |
|---|---|---|---|
| CST816S | fbiego | 1.3.0 | Capacitive touch screen library — an Arduino library for the CST816S capacitive touch screen IC. |
| CST816_TouchLib | MDO | 2.2 | A CST816 touch and gesture library, tested using the LilyGO T-Display ESP32-S3 and T-Display S3 AMOLED boards. |
| TFT_eSPI | Bodmer | 2.5.43 | TFT graphics library for Arduino processors, with performance optimisation for RP2040, STM32, and other platforms. |

### 4. Configure the Board Settings

Open **Tools** in the Arduino IDE menu and set the following options:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240MHz (WiFi) |
| Core Debug Level | None |
| USB DFU On Boot | Disabled |
| Erase All Flash Before Sketch Upload | Disabled |
| Events Run On | Core 1 |
| Flash Mode | QIO 80MHz |
| Flash Size | 4MB (32Mb) |
| JTAG Adapter | Disabled |
| Arduino Runs On | Core 1 |
| USB Firmware MSC On Boot | Disabled |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
| PSRAM | Disabled |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600 |
| Port | *(select the COM port your DSG is connected to — see step 6)* |

### 5. Configure the TFT_eSPI Library

The `TFT_eSPI` library must be manually configured for this board's display before compiling.

Navigate to:

```
Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h
```

Open `User_Setup_Select.h` with your code editor of choice, and make sure the LilyGo T-Display S3 configuration is enabled:

```cpp
#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
```

Make sure the default setup is disabled:

```cpp
//#include <User_Setup.h>
```

Only the setup required by the T-Display S3 should be enabled — other display setup files should remain commented out.

Once this is configured, the project is ready to be compiled from the Arduino IDE.

### 6. Upload the Firmware

1. Connect the DSG device to your PC using a **USB-C cable**.
2. In **Tools → Port**, select the COM port that appears for the connected device.
3. In **Tools → Board**, make sure **ESP32S3 Dev Module** is selected.
4. Double-check all the settings from steps 4–5 are correct.
5. Click **Upload**.

The firmware will now be uploaded to the DSG device.