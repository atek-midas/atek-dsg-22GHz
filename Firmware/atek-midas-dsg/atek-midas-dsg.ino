#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include "display.h"
#include "main.h"
#include "Lmx2820.h"
#include "MCP23S17.h"
#include "ADC78H90.h"
#include "RemoteControl.h"

const char *apSSID = "ATEK_DSG_22GHz";
const char *apPassword = "12345678";

String currentFrequency; 
String currentAmplitude;  
String currentFreqUnit; 
bool FilterStatus; 
bool RFStatus;
bool rfOutputEnabled = false; // RF çıkışının başlangıç durumu

char* FloatToChar(float avg) {
    // Yeterli boyutta bir char dizisi oluştur (örn: 20 karakter)
    static char buffer[20];
    // Float değeri char dizisine dönüştür ve buffer'a yaz
    snprintf(buffer, sizeof(buffer), "%.2f", avg);  // %.2f, virgülden sonra 2 basamak gösterir
    return buffer;
}

char* DoubleToChar(double num) {
    // Yeterli boyutta bir char dizisi oluştur (örn: 30 karakter)
    static char buffer[30];

    // Double değeri char dizisine dönüştür ve buffer'a yaz
    snprintf(buffer, sizeof(buffer), "%.4f", num);  // %.4f, virgülden sonra 4 basamak gösterir

    return buffer;
}

WebServer server(80);

Preferences preferences;

void readRFSettings() {  // Function to read from Preferences
  preferences.begin("RFSettings", false);
  
  currentFrequency = preferences.getString("frequency", "6000");      // Default 6 GHz
  currentAmplitude = preferences.getString("amplitude", "-10.0");     // Default -10.0
  currentFreqUnit = preferences.getString("freqUnit", "MHz");         // Default "MHz"
  FilterStatus = preferences.getBool("FilterStat", false);            // Default OFF
  RFStatus  = preferences.getBool("RFStat", false);                   // Default OFF
  
  // Seri porttan okunan değerleri yazdır
  Serial.println("=== RF Settings ===");
  Serial.print("Frequency: ");
  Serial.println(currentFrequency);
  Serial.print("Amplitude: ");
  Serial.println(currentAmplitude);
  Serial.print("Frequency Unit: ");
  Serial.println(currentFreqUnit);
  Serial.print("Filter Status: ");
  Serial.println(FilterStatus ? "ON" : "OFF");
  Serial.print("RF Status: ");
  Serial.println(RFStatus ? "ON" : "OFF");
  Serial.println("====================");
  
  preferences.end();
}


void saveRFSettings() { // Function to save to Preferences
  preferences.begin("RFSettings", false);
  preferences.putString("frequency", currentFrequency);
  preferences.putString("amplitude", currentAmplitude);
  preferences.putString("freqUnit", currentFreqUnit);
  preferences.putBool("FilterStat", FilterStatus );
  preferences.putBool("RFStat", false ); // Always off at power on
  preferences.end();
}

void toggleRFOutput() {


  rfOutputEnabled = !rfOutputEnabled; // Durumu tersine çevir
  if (rfOutputEnabled) {
    // RF çıkışını aç
    activateRFOutput(); // RF çıkışını açmak için kendi fonksiyonunuzu buraya ekleyin
  } else {
    // RF çıkışını kapat
    deactivateRFOutput(); // RF çıkışını kapatmak için kendi fonksiyonunuzu buraya ekleyin
  }
}

void activateRFOutput() {
  Serial.println("Activating RF Output");
  // Your code to turn the RF output ON (e.g., set a pin HIGH)
}

void deactivateRFOutput() {
  Serial.println("Deactivating RF Output");
  // Your code to turn the RF output OFF (e.g., set a pin LOW)
}

void handleToggleRFOutput() {
  toggleRFOutput();
  server.send(200, "text/plain", String(rfOutputEnabled)); // Yeni durumu gönder
}


String wifiSSID;
String wifiPassword;

void readWiFiCredentials() {
  preferences.begin("WifiSettings", false);
  wifiSSID = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  preferences.end();
  Serial.print("Read SSID: "); Serial.println(wifiSSID); // Debug print
  Serial.print("Read Password: "); Serial.println(wifiPassword); // Debug print
}

 
    char statusMessage[50];  

bool connectToWiFi(int tryCount) {
  if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
    snprintf(statusMessage, sizeof(statusMessage), "Connecting to %s", wifiSSID);
    ConnectionStatus(statusMessage,true);
    for (int i = 1; i <= tryCount; i++) {
      ConnectionStatus(" Try ",false);
      String str = String(i);  
      ConnectionStatus(str.c_str(), false);
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str()); // Sadece bir kez çağırın
      int connectionTryCount = 0;
      while (WiFi.status() != WL_CONNECTED && connectionTryCount < 10) { 
        delay(1500);
        ConnectionStatus(".",false);
        connectionTryCount++;
      }
      Serial.println();

      if (WiFi.status() == WL_CONNECTED) {
        ConnectionStatus("Wi-Fi'ye bağlandı!",true);
        return true;
      } else {
        ConnectionStatus("Wi-Fi Connection Failed!",true);
        Serial.print("Hata kodu: ");
        Serial.println(WiFi.status()); // Hata kodunu yazdırın
        if (i < tryCount) {
          delay(5000);
        }
      }
    }
  } else {
    ConnectionStatus("No WiFi credentials stored.",true);
  }
  return false;
}



void handleSave() {
  wifiSSID = server.arg("ssid");
  wifiPassword = server.arg("password");

  if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
    Serial.print("Saving SSID: ");
    Serial.println(wifiSSID);  // Debug print
    Serial.print("Saving Password: ");
    Serial.println(wifiPassword);  // Debug print
    preferences.begin("WifiSettings", false);
    preferences.putString("ssid", wifiSSID);
    preferences.putString("password", wifiPassword);
    preferences.end();

    Serial.println("Credentials saved. Restarting...");  // Debug print
    server.sendHeader("Location", "/"); // Redirect to root
    server.send(302, "text/html", "Credentials saved. Redirecting..."); // User-friendly message
    delay(1000);
    ESP.restart();

  } else {
    Serial.println("SSID and password cannot be empty!");  // Debug print
    server.send(200, "text/plain", "SSID ve parola boş olamaz!");
  }
}

enum WiFiState {
  WIFI_INIT,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_HOTSPOT,
  WIFI_FAILED
};

WiFiState wifiState = WIFI_INIT;
unsigned long wifiStartTime = 0;
int wifiRetryCount = 0;


void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  RC_Begin();
  RC_SetWrite([](const char* s){ Serial.print(s); });  

  SetupDisplay();
 
  initTouch();

/*
  // Wi-Fi Threat
  xTaskCreatePinnedToCore(
    WiFiTask,      // Task fonksiyonu
    "WiFiTask",    // Task ismi
    8192,          // Stack boyutu (ESP32 için önerilen minimum 8KB)
    NULL,          // Parametre (şu an kullanılmıyor)
    1,             // Öncelik (1 düşük, 5 yüksek)
    NULL,          // Task handle (şu an kullanılmıyor)
    1              // Core ID (0 veya 1 seçilebilir, genelde 1 kullanılır)
  );
*/


  readRFSettings(); 

  drawMainMenu();
  SetTemp("24");
  SetUSBVoltge("5.1");

  SetFreqUnitOnMainMenu(currentFreqUnit);
  SetFreqOnMainMenu(currentFrequency);
  SetAmpOnMainMenu(currentAmplitude);

  pinMode(IO1_CS, OUTPUT);
  digitalWrite(IO1_CS, HIGH); 

  pinMode(ADC_CS, OUTPUT);
  digitalWrite(ADC_CS, HIGH); 

  pinMode(PLL_CS, OUTPUT);
  digitalWrite(PLL_CS, HIGH); 

  
  InitADC();
	Serial.print("\r\n");
  InitADC();

  IO_EXP1_Init();

  ConnectionStatus("Wait...", true);  delay(1000);

  InitPLL();
  
}

 

void WiFiTask(void *parameter) {
  return;
  bool isHotSpot = false;
  readRFSettings(); // Read RF settings from Preferences
  readWiFiCredentials();
  SetWifiStatus(WIFI_STATUS_OFF);
  if (connectToWiFi(3)) {  // 3 deneme ile bağlanmayı dene
    ConnectionStatus("IP address: ", true);
    String ipStr = WiFi.localIP().toString();
    ConnectionStatus(ipStr.c_str(), false);
  } else {


    SetWifiStatus(WIFI_STATUS_HOTSPOT);
    ConnectionStatus("Creating Hotspot.", true);
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(apSSID, apPassword);
  
    String dots = "";
    // Wait until IP assigned
    int retries = 0;
    while (WiFi.softAPIP().toString() != "192.168.4.1" && retries++ < 50) {
        delay(500);  // 200ms bekle
        dots += "*";
        ConnectionStatus(dots.c_str(), true);
    }

    ConnectionStatus("Hotspot IP:", true);
    String ipStr = WiFi.softAPIP().toString();
    ConnectionStatus(ipStr.c_str(), false);
    isHotSpot = true;
  }

  server.on("/", handleRoot); // Use handleRoot to send the HTML
  server.on("/save", handleSave);
  server.on("/setFrequency", handleSetFrequency); 
  server.on("/setAmplitude", handleSetAmplitude); 
  server.on("/toggleRFOutput", handleToggleRFOutput);

  server.on("/setLO1SW1", handleSetLO1SW1);
  server.on("/setRFDSA1", handleSetRFDSA1);
  server.on("/setPLL1CE", handleSetPLL1CE);
  server.on("/setIF1SW1C", handleSetIF1SW1C);

  server.begin();
  ConnectionStatus("Web Server Ready.", true);
  if (isHotSpot)
  {
    SetWifiStatus(WIFI_STATUS_HOTSPOT);
  }
  else
  {
    SetWifiStatus(WIFI_STATUS_ON);
  }
  OTABegin();

  vTaskDelete(NULL); // Task tamamlandığında kendini sonlandır
}

void InitServer()
{
  server.on("/", handleRoot); // Use handleRoot to send the HTML
  server.on("/save", handleSave);
  server.on("/setFrequency", handleSetFrequency); 
  server.on("/setAmplitude", handleSetAmplitude); 
  server.on("/toggleRFOutput", handleToggleRFOutput);
  
server.on("/setLO1SW1", handleSetLO1SW1);
server.on("/setRFDSA1", handleSetRFDSA1);
server.on("/setPLL1CE", handleSetPLL1CE);
server.on("/setIF1SW1C", handleSetIF1SW1C);
  server.begin();

  OTABegin();

}
void OTABegin() {
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

String getHTML() {
  String html = R"=====( 
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ATEK 22 GHz RF Signal Generator</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #f4f4f4;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      padding: 20px;
      background-color: white;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
    }
    .wide-button {
      padding: 10px 20px;
      background-color: #007BFF;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      margin-top: 10px;
      width: 80%;
      box-sizing: border-box;
    }
    input[type=text], select, input[type=number], input[type=password] {
      width: 80%;
      padding: 10px;
      margin: 10px 0;
      box-sizing: border-box;
    }
    label {
      display: block;
      margin-bottom: 5px;
    }
    hr {
      margin: 20px 0;
    }
    .wifi-credentials {
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .wifi-credentials > * {
      margin-bottom: 10px;
      width: 80%;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ATEK 22 GHz RF Signal Generator</h1>

    <hr>
    <h2>RF Control</h2>
    <label for="frequency">Frequency:</label>
    <input type="text" id="frequency" name="frequency" value=")=====" + String(currentFrequency) + R"=====(">
    <select id="freqUnit">
      <option value="KHz" )=====" + String(currentFreqUnit == "KHz" ? "selected" : "") + R"=====(>KHz</option>
      <option value="MHz" )=====" + String(currentFreqUnit == "MHz" ? "selected" : "") + R"=====(>MHz</option>
      <option value="GHz" )=====" + String(currentFreqUnit == "GHz" ? "selected" : "") + R"=====(>GHz</option>
    </select>
    <button class="wide-button" id="setFrequency">Set Frequency</button>

    <label for="amplitude">Amplitude (dBm):</label>
    <input type="text" id="amplitude" name="amplitude" value=")=====" + String(currentAmplitude) + R"=====(">
    <button class="wide-button" id="setAmplitude">Set Amplitude</button>

    <hr>
    <h2>IO Expander Control</h2>

    <label for="lo1sw1">LO1_SW1 (0–7):</label>
    <input type="number" id="lo1sw1" min="0" max="7">
    <button class="wide-button" onclick="setLO1SW1()">Set LO1_SW1</button>

    <label for="rfds">RF_DSA1 (0–31):</label>
    <input type="number" id="rfds" min="0" max="31">
    <button class="wide-button" onclick="setRFDSA1()">Set RF_DSA1</button>

    <label>PLL1_CE:</label>
    <button class="wide-button" onclick="setPLL1CE(true)">ON</button>
    <button class="wide-button" onclick="setPLL1CE(false)">OFF</button>

    <label>IF1_SW1_C:</label>
    <button class="wide-button" onclick="setIF1SW1C(true)">ON</button>
    <button class="wide-button" onclick="setIF1SW1C(false)">OFF</button>

    <hr>
    <div class="wifi-credentials">
      <h2>Wi-Fi Credentials</h2>
      <label for="ssid">SSID:</label>
      <input type="text" id="ssid" name="ssid">
      <label for="password">Password:</label>
      <input type="password" id="password" name="password">
      <button class="wide-button" id="save-credentials">Save</button>
      <button class="wide-button" id="clear-credentials">Clear Credentials</button>
    </div>

    <script>
  document.getElementById('setFrequency').addEventListener('click', () => {
    const button = document.getElementById('setFrequency');
    const originalColor = button.style.backgroundColor;
    const frequency = document.getElementById('frequency').value;
    const unit = document.getElementById('freqUnit').value;
    fetch('/setFrequency', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `frequency=${frequency}&unit=${unit}`
    })
    .then(response => response.text())
    .then(data => {
      button.style.backgroundColor = "green";
      setTimeout(() => { button.style.backgroundColor = originalColor; }, 1000);
    });
  });

  document.getElementById('setAmplitude').addEventListener('click', () => {
    const button = document.getElementById('setAmplitude');
    const originalColor = button.style.backgroundColor;
    const amplitude = document.getElementById('amplitude').value;
    fetch('/setAmplitude', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `amplitude=${amplitude}`
    })
    .then(response => response.text())
    .then(data => {
      button.style.backgroundColor = "green";
      setTimeout(() => { button.style.backgroundColor = originalColor; }, 1000);
    });
  });

  document.getElementById('save-credentials').addEventListener('click', async () => {
    const button = document.getElementById('save-credentials');
    const originalColor = button.style.backgroundColor;
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    const response = await fetch('/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `ssid=${ssid}&password=${password}`
    });
    const result = await response.text();
    button.style.backgroundColor = "green";
    setTimeout(() => { button.style.backgroundColor = originalColor; }, 1000);
  });

  document.getElementById('clear-credentials').addEventListener('click', async () => {
    const button = document.getElementById('clear-credentials');
    const originalColor = button.style.backgroundColor;
    const response = await fetch('/clearWiFi', { method: 'POST' });
    const result = await response.text();
    button.style.backgroundColor = "green";
    setTimeout(() => { button.style.backgroundColor = originalColor; }, 1000);
    alert(result);
  });

  // IO Expander functions
  function setLO1SW1() {
    const val = document.getElementById("lo1sw1").value;
    fetch(`/setLO1SW1?value=${val}`)
      .then(response => response.text())
      .then(alert)
      .catch(console.error);
  }

  function setRFDSA1() {
    const val = document.getElementById("rfds").value;
    fetch(`/setRFDSA1?value=${val}`)
      .then(response => response.text())
      .then(alert)
      .catch(console.error);
  }

  function setPLL1CE(state) {
    fetch(`/setPLL1CE?state=${state ? 1 : 0}`)
      .then(response => response.text())
      .then(alert)
      .catch(console.error);
  }

  function setIF1SW1C(state) {
    fetch(`/setIF1SW1C?state=${state ? 1 : 0}`)
      .then(response => response.text())
      .then(alert)
      .catch(console.error);
  }
    </script>
  </div>
</body>
</html>
)=====";
  return html;
}


void handleSetLO1SW1() {
  if (server.hasArg("value")) {
    uint8_t val = server.arg("value").toInt();  // 0-7 arasında bekleniyor
    SetLO1_SW1(val);
    server.send(200, "text/plain", "LO1_SW1 updated: " + String(val));
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

void handleSetRFDSA1() {
  if (server.hasArg("value")) {
    uint8_t val = server.arg("value").toInt();  // 0–31 arası 5-bit veri
    SetRF_DSA1(val);
    server.send(200, "text/plain", "RF_DSA1 updated: " + String(val));
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

void handleSetPLL1CE() {
  if (server.hasArg("state")) {
    bool state = server.arg("state") == "1";
    SetPLL1_CE(state);
    server.send(200, "text/plain", "PLL1_CE set to: " + String(state));
  } else {
    server.send(400, "text/plain", "Missing state parameter");
  }
}

void handleSetIF1SW1C() {
  if (server.hasArg("state")) {
    bool state = server.arg("state") == "1";
    SetIF1_SW1_C(state);
    server.send(200, "text/plain", "IF1_SW1_C set to: " + String(state));
  } else {
    server.send(400, "text/plain", "Missing state parameter");
  }
}


void handleRoot() {
  server.send(200, "text/html", getHTML()); // Send the formatted HTML
}



void handleSetFrequency() {
    String frequency = server.arg("frequency");
    String unit = server.arg("unit");

    long long tempFrequency = atoll(frequency.c_str()); // long long kullanıyoruz

    currentFrequency = String(tempFrequency);
    currentFreqUnit = unit; 

    Serial.print("Set Frequency: ");
    Serial.print(currentFrequency);
    Serial.print("(");
    Serial.print(currentFreqUnit);
    Serial.println(")");

    SetFreq(currentFrequency); // Update display
    SetFreqUnit(currentFreqUnit); 
    if (checkenteredFreqValue()) 
    {
      drawMainMenu(); 
      updateFreqArea();
    }
    else
    {
      server.send(400, "text/plain", "Invalid frequency! Frequency must be between 48 MHzand 22.6 GHz.");
    }
    return;
 
    server.send(200, "text/plain", "Frequency set!");
 
}

void handleSetAmplitude() {
  String amplitude = server.arg("amplitude");
  currentAmplitude = amplitude;

//if (currentAmplitude >= -30 && currentAmplitude <= 30) { 

//Girilen AMP değeri kontrolu ekle. 

//}

  Serial.print("Set Amplitude: ");
  Serial.print(currentAmplitude);
  Serial.println(" dBm");

  SetAmp(currentAmplitude); // Update display
  drawMainMenu();
  updateAmpArea();
  //saveRFSettings(); // Save to Preferences should be called on Demand
  server.send(200, "text/plain", "Amplitude set!");
  //server.send(400, "text/plain", "Invalid amplitude! Amplitude must be between -30 dBm and +30 dBm.");
}



void manageWiFiConnection() {
  static unsigned long lastCheckTime = 0;
  
  switch (wifiState) {
    case WIFI_INIT:
      
      readWiFiCredentials();
      SetWifiStatus(WIFI_STATUS_OFF);
      if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
        wifiState = WIFI_CONNECTING;
        wifiStartTime = millis();
        wifiRetryCount = 0;
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        ConnectionStatus("Conn to WiFi...", true);
      } else {
        wifiState = WIFI_HOTSPOT;
      }
      break;

    case WIFI_CONNECTING:
      if (millis() - wifiStartTime > 2000) { // 2 saniyede bir kontrol et
        wifiStartTime = millis();
        if (WiFi.status() == WL_CONNECTED) {
          wifiState = WIFI_CONNECTED;
          ConnectionStatus("Connected :)", true);
          String ipStr = WiFi.localIP().toString();
          ConnectionStatus(ipStr.c_str(), true);
          SetWifiStatus(WIFI_STATUS_ON);
          InitServer();
        } else {
          wifiRetryCount++;
          ConnectionStatus("Retrying WiFi...", true);
          if (wifiRetryCount >= 10) { // 10 kez denedikten sonra başarısız say
            wifiState = WIFI_HOTSPOT;
          }
        }
      }
      break;

    case WIFI_CONNECTED:
      // WiFi başarılı, hiçbir şey yapmaya gerek yok
      // Burada ara ara wifi connectionkontrol edilecek ve konnetion kopmus ise WIFI_CONNECTING state ine gidilebilir (eger bu stte uygunise tabiki)
      break;

    case WIFI_HOTSPOT:
    {
      ConnectionStatus("Hotspot...", true);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(apSSID, apPassword);
      
      int retries = 0;
      String dots = "";
      while (WiFi.softAPIP().toString() == "0.0.0.0" && retries++ < 30) {
          dots += ".";
          ConnectionStatus(dots.c_str(), true);
          delay(500);
      }

      // IP bastır
      ConnectionStatus("Hotspot IP:", true);
      ConnectionStatus(WiFi.softAPIP().toString().c_str(), true);
      InitServer();
      SetWifiStatus(WIFI_STATUS_HOTSPOT);

      wifiState = WIFI_FAILED;
      break;
    }
    case WIFI_FAILED:
      // Hotspot açık olduğu için burada ekstra bir işlem yapmaya gerek yok
      break;
  }
}

unsigned long lastUpdateTime = 0;
 

void loop() {

  unsigned long currentTime = millis();
  
  while (Serial.available()) 
  {
    RC_ProcessByte((uint8_t)Serial.read()); 
  }

  ArduinoOTA.handle();
  server.handleClient();
  handleTouch();

  manageWiFiConnection();


  if (currentTime - lastUpdateTime >= 500)
  {
    lastUpdateTime = currentTime;


    if (currentMenu == MAIN_MENU)
    {
      float temp = Read_Temp();
      float usb_voltage = Read_5V_Voltage();
      float dsg_current = Read_5V_Current();

      SetTemp(String(temp, 1).c_str());
      SetUSBVoltge(String(usb_voltage, 1).c_str()); 
    }else if (currentMenu == INFO_MENU)
    {
      drawInfoScreen();
    }


  } 

  
}