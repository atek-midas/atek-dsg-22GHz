#include <TFT_eSPI.h>  // TFT ekran kütüphanesi
#include "img_logo.h"
#include "display.h"
#include "main.h"
#include <Wire.h>
#include  "Lmx2820.h"
#include <CST816_TouchLib.h>  // CST816S dokunmatik ekran sürücüsü
#include "MCP23S17.h"
#include "ADC78H90.h"

#define PIN_I2C_SDA 18
#define PIN_I2C_SCL 17
#define DISPLAY_X 320
TFT_eSPI tft = TFT_eSPI();
using namespace MDO;
CST816Touch_SWMode oTouchController;  //the main touch screen controller class from the library used by this example.






MenuState currentMenu = NONE;
 
String enteredFreqValue = "";
String prev_enteredFreqValue = "";

String enteredUnitValue = "";
String prev_enteredUnitValue = "";

String enteredAmpValue = "";
String prev_enteredAmpValue = "";

String freqValue = "22.6 GHz";
String ampValue = "0 dBm";


void setupTDisplayS3() {
  //since this controller was the original base for this library
  //the majority of the defaults are already good
  if (!oTouchController.begin(Wire)) {  //this will initialize & use the TouchScreenEventCache, instead of a provided Observer
    Serial.println("Touch screen initialization failed..");
    while (true) {
      delay(100);
    }
  }
  oTouchController.setSwapXY(true);
  oTouchController.setInvertY(true, 170);  // // comment when tft.setRotation(3) used
  //no GestureFactory needed for this setup of controller/firmware

  //no DoubleClickFactory here, just for demo to show it's optional
  oTouchController.enableDoubleClickFactory_Quick();    //quickest option (no delay), for a double click however it will give a Touch event and a Gesture
  oTouchController.enableDoubleClickFactory_Elegant();  //most elegant option, however this does buffer (read: delay) touch events slightly
}


int Xpos147 = 0;// Button 1,4 and 7
int Xpos258 = 63;// Button 2,5 and 8
int Xpos369 = 123;// Button 3,6 and 9
int XposEBD0 = 190; // Button Enter, Backspace, Dot and 0
int XposXKMG = 253; // Button X, KHz, MHz and GHz

int ButtonWidth = 56;
int ButtonHeight = 36;

int YposEX = 0;
int Ypos123BK = 43; // Button1 , Button2 , Button3, Backspace , KHz
int Ypos456DM = 86; // // Button4 , Button5 , Button6, Dot , MHz (or Minus)
int Ypos7890G = 127; // // Button7 , Button8 , Button9, Button0 , GHz


bool CurrentLockStatus = false;
bool CurrentBITStatus = true;
bool CurrentRFStatus = false;
WifiStatus CurrentWifiStatus = WIFI_STATUS_OFF;
String CurrentTempValue = "";
String CurrentUSBVoltageValue = "";
String CurrentConnectionStatus = "";
void GetTouchData(int x, int y) {

/*
  Serial.print("Touch: (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(")");
*/


  if (currentMenu == MAIN_MENU) {
    if (x > 3 && x < 80 && y > 3 && y < 45) {  // Frequency Butonu
      Serial.println("Frequency Button Pressed");
      prev_enteredFreqValue = enteredFreqValue;
      prev_enteredUnitValue = enteredUnitValue;
      drawFreqMenu();
    } else if (x > 3 && x < 80 && y > 53 && y < 95) {  // Amplitude Butonu
      Serial.println("Amplitude Button Pressed");
      prev_enteredAmpValue = enteredAmpValue;
      drawAmpMenu();
    }else if (x > 3 && x < 80 && y > 100 && y < 142) {  // Filter Butonu


      Serial.println("Filter Button Pressed");
      FilterStatus = !FilterStatus;
      SetFilter();
      
    }else if (x > 264 && x < 312 && y > 42 && y < 90) {  // RfOnOFF Butonu


        if (CurrentRFStatus)
        {
          Serial.println("RF Out OFF Button Pressed");
          SetRfOnOff(false);
        }
        else
        {
          SetRfOnOff(true);
          checkenteredFreqValue();
          Serial.println("RF Out ON Button Pressed");
        }

    }else if (x > 268 && x < 308 && y > 95 && y < 135) {  // Save Butonu

          Serial.println("Rf Settings Saved");
          SaveRfSettingsBtn();
 

    }
    
  
  }
   // **Frekans Girişi Sayfası (Freq.png)**
  else if (currentMenu == FREQ_MENU) {
    // **Sayı Butonları**
    if (x > Xpos147 && x < Xpos147 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 1"); enteredFreqValue += "1"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 4"); enteredFreqValue += "4"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 7"); enteredFreqValue += "7"; }
    } else if (x > Xpos258 && x < Xpos258 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 2"); enteredFreqValue += "2"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 5"); enteredFreqValue += "5"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 8"); enteredFreqValue += "8"; }
    } else if (x > Xpos369 && x < Xpos369 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 3"); enteredFreqValue += "3"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 6"); enteredFreqValue += "6"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 9"); enteredFreqValue += "9"; }
    }

    // **Enter, Backspace, Dot, 0 Butonları**
    else if (x > XposEBD0 && x < XposEBD0 + ButtonWidth) {
      if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 0"); enteredFreqValue += "0"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { 
        Serial.println("Pressed: .");
        if (enteredFreqValue.indexOf('.') == -1) enteredFreqValue += "."; 
      }
      else if (y > YposEX && y < YposEX + ButtonHeight) { Serial.println("Pressed: Enter");   enteredFreqValue = enteredFreqValue;if ( checkenteredFreqValue()) drawMainMenu(); else return; }
      else if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { 
        Serial.println("Pressed: Backspace");
        if (!enteredFreqValue.isEmpty()) enteredFreqValue.remove(enteredFreqValue.length() - 1);
      }
    }

    // **X, KHz, MHz, GHz Butonları**
    else if (x > XposXKMG && x < XposXKMG + ButtonWidth) {
      if (y > YposEX && y < YposEX + ButtonHeight) { Serial.println("Pressed: X (Cancel)"); enteredFreqValue = prev_enteredFreqValue; enteredUnitValue  = prev_enteredUnitValue; drawMainMenu(); }
      else if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: KHz"); enteredUnitValue = "KHz"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: MHz"); enteredUnitValue = "MHz"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: GHz"); enteredUnitValue = "GHz"; }
    }
   
    updateFreqArea(); 

  }
  // **Amplitüd Girişi Sayfası (Amp.png)**
  else if (currentMenu == AMP_MENU) {
    // **Sayı Butonları**
    if (x > Xpos147 && x < Xpos147 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 1"); enteredAmpValue += "1"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 4"); enteredAmpValue += "4"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 7"); enteredAmpValue += "7"; }
    } else if (x > Xpos258 && x < Xpos258 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 2"); enteredAmpValue += "2"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 5"); enteredAmpValue += "5"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 8"); enteredAmpValue += "8"; }
    } else if (x > Xpos369 && x < Xpos369 + ButtonWidth) {
      if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: 3"); enteredAmpValue += "3"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: 6"); enteredAmpValue += "6"; }
      else if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 9"); enteredAmpValue += "9"; }
    }

    // **Enter, Backspace, Dot, 0 , - , Butonları**
    else if (x > XposEBD0 && x < XposEBD0 + ButtonWidth) {
      if (y > Ypos7890G && y < Ypos7890G + ButtonHeight) { Serial.println("Pressed: 0"); enteredAmpValue += "0"; }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Pressed: .");    if (enteredAmpValue.indexOf('.') == -1) enteredAmpValue += ".";   }
      else if (y > YposEX && y < YposEX + ButtonHeight) { Serial.println("Pressed: Enter"); enteredAmpValue = enteredAmpValue; if ( checkenteredAmpValue()) drawMainMenu(); else return; }
      else if (y > Ypos123BK && y < Ypos123BK + ButtonHeight) { Serial.println("Pressed: Backspace");      if (!enteredAmpValue.isEmpty()) enteredAmpValue.remove(enteredAmpValue.length() - 1);  }
    }

 

    // **X , Minus(-) Butonu**
    else if (x > XposXKMG && x < XposXKMG + ButtonWidth) {
      if (y > YposEX && y < YposEX + ButtonHeight) { Serial.println("Pressed: X (Cancel)"); enteredAmpValue = prev_enteredAmpValue; drawMainMenu(); }
      else if (y > Ypos456DM && y < Ypos456DM + ButtonHeight) { Serial.println("Minus Pressed: .");    if (enteredAmpValue.length() < 1) enteredAmpValue += "-";   }
    }

   updateAmpArea();
    
  }

}
 
 bool checkenteredFreqValue() {
    // enteredFreqValue'yi uzunluk kontrolü için geçici bir değişkene aktar
    String tempFreq = enteredFreqValue;
    //tempFreq.replace(".", "");  // Noktayı kaldır

    // Frekans birimine göre çarpan belirle
    double freqValue = tempFreq.toDouble();
    if (enteredUnitValue == "KHz") {
        freqValue *= 1000.0;
    } else if (enteredUnitValue == "MHz") {
        freqValue *= 1000000.0;
    } else if (enteredUnitValue == "GHz") {
        freqValue *= 1000000000.0;
    }

    // Sınır kontrolü
    if (freqValue < MIN_FREQ || freqValue > MAX_FREQ) {
 
      tft.fillRect(10, 4, 161, 35, TFT_WHITE); // Eski değeri temizle
      tft.setTextColor(TFT_RED, TFT_WHITE);
      tft.setCursor(10, 32);
      tft.setFreeFont(&FreeSansBold18pt7b);
      tft.print(enteredFreqValue);
      return false;

    }
    else 
    {
      //Serial.print("Freq Value : ");
      //Serial.print(enteredFreqValue);
      //Serial.println(" " + enteredUnitValue);

      if (FilterStatus)
      {
        SetFilterBand(freqValue / 1000000.0);
      }
 
      CurrentLockStatus = Lmx2820SetFreqinMHz(freqValue / 1000000.0  , 10000000);
      ApplyFrequency(freqValue); 
      return true;

    }
}


bool checkenteredAmpValue() {
    String tempAmp = enteredAmpValue;
    double AmpValue = tempAmp.toDouble();

    // Sınır kontrolü
    if (AmpValue < MIN_RFPOWER || AmpValue > MAX_RFPOWER) {
 
      tft.fillRect(10, 4, 108, 35, TFT_WHITE); // Clear old value
      tft.setTextColor(TFT_RED, TFT_WHITE);
      tft.setCursor(10, 32);
      tft.setFreeFont(&FreeSansBold18pt7b);
      tft.print(enteredAmpValue);

      return false;

    } else 
    {
      ApplyAmplitude((uint8_t)AmpValue); 
      return true;
    }
}

void drawUnderline(int x, int y) {
  tft.drawLine(x + 10, y + ButtonHeight*0.9, x + ButtonWidth-2, y + ButtonHeight*0.9, tft.color565(50, 50, 50));
  tft.drawLine(x + 10, y+1 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+1 +  ButtonHeight*0.9, tft.color565(50, 50, 50));
  tft.drawLine(x + 10, y+2 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+2 + ButtonHeight*0.9, tft.color565(50, 50, 50));
  tft.drawLine(x + 10, y+3 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+3 + ButtonHeight*0.9, tft.color565(50, 50, 50));
}

void clearUnderline(int x, int y) {
  tft.drawLine(x + 10, y + ButtonHeight*0.9, x + ButtonWidth-2, y + ButtonHeight*0.9, tft.color565(225, 213, 231));
  tft.drawLine(x + 10, y+1 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+1 +  ButtonHeight*0.9,  tft.color565(225, 213, 231));
  tft.drawLine(x + 10, y+2 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+2 + ButtonHeight*0.9,  tft.color565(225, 213, 231));
  tft.drawLine(x + 10, y+3 + ButtonHeight*0.9, x  + ButtonWidth-2 , y+3 + ButtonHeight*0.9,  tft.color565(225, 213, 231));
}

void updateFreqArea() {
  if (currentMenu == FREQ_MENU) 
   {
    //drawFreqMenu();

    String tempValue = enteredFreqValue; 
    tempValue.replace(".", ""); 

    if (tempValue.length() > 8) {
        if (!enteredFreqValue.isEmpty()) enteredFreqValue.remove(enteredFreqValue.length() - 1); // remove last charcter if length is bigger than 8
    }

    tft.fillRect(10, 4, 161, 35, TFT_WHITE); // Eski değeri temizle
    tft.setTextColor(tft.color565(50, 50, 50), TFT_WHITE);
    tft.setCursor(10, 32);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.print(enteredFreqValue);


    clearUnderline(XposXKMG, Ypos123BK);
    clearUnderline(XposXKMG, Ypos456DM);
    clearUnderline(XposXKMG, Ypos7890G);

    if (enteredUnitValue == "KHz")  
    {
        drawUnderline(XposXKMG, Ypos123BK);
    }
    else if (enteredUnitValue == "MHz")  
    {
        drawUnderline(XposXKMG, Ypos456DM);
    }
    else if (enteredUnitValue == "GHz")  
    {
        drawUnderline(XposXKMG, Ypos7890G);
    }

}
}
void updateAmpArea() {
   if (currentMenu == AMP_MENU) 
   {

    String tempValue = enteredAmpValue; 
    tempValue.replace(".", ""); 

    if (tempValue.length() > 3) {
        if (!enteredAmpValue.isEmpty()) enteredAmpValue.remove(enteredAmpValue.length() - 1); // remove last charcter if length is bigger than 8
    }

    tft.fillRect(10, 4, 108, 35, TFT_WHITE); // Clear old value
    tft.setTextColor(tft.color565(50, 50, 50), TFT_WHITE);
    tft.setCursor(10, 32);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.print(enteredAmpValue);
   }
}

void SetFreqOnMainMenu(String value)
{
  tft.fillRect(82, 6, 164, 34, TFT_WHITE); // Clear old values
  enteredFreqValue = value;// Re set entered value incase it can be set from WEB interface
  tft.setTextColor(tft.color565(50, 50, 50), TFT_WHITE);
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setCursor(85, 34);
  tft.print(enteredFreqValue);
}

void SetFreqUnitOnMainMenu(String value)
{
  tft.fillRect(255, 6, 60, 30,  tft.color565(151, 186, 218)); // Clear old values
  enteredUnitValue = value;// Re set entered value incase it can be set from WEB interface
  tft.setTextColor(tft.color565(50, 50, 50), TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(255, 30);
  tft.print(enteredUnitValue);
}


void SetAmpUnitOnMainMenu()
{
 
  tft.setTextColor(tft.color565(50, 50, 50), TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(190, 85);
  tft.print("dBm");
}

void SetAmpOnMainMenu(String value)
{
   tft.fillRect(82, 56, 95, 30,  TFT_WHITE); // Clear old values
  enteredAmpValue = value; // Re set entered value incase it can be set from WEB interface
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setCursor(85, 83);
  tft.print(enteredAmpValue);
}

void SetAmp(String value)
{
  enteredAmpValue = value;
}
void SetFreqUnit(String value)
{
  enteredUnitValue = value;
}

void SetFreq(String value)
{
  enteredFreqValue = value;
}
void drawMainMenu() {

  // if (currentMenu != MAIN_MENU) 
    {
      currentMenu = MAIN_MENU;
      tft.pushImage(0, 0, 320, 170, (uint16_t*)MainMenu);
    }
 

  SetFreqOnMainMenu(enteredFreqValue);
  SetFreqUnitOnMainMenu(enteredUnitValue);
  SetAmpOnMainMenu(enteredAmpValue);
  SetAmpUnitOnMainMenu();


  SetFilter();
  SetRfOnOff(CurrentRFStatus);
  SetSaveButton();
  SetBITStatus(CurrentBITStatus);
  SetLock(CurrentLockStatus);
  SetWifiStatus(CurrentWifiStatus);
  
  SetTemp(CurrentTempValue.c_str());
  SetUSBVoltge(CurrentUSBVoltageValue.c_str());
  ConnectionStatus(CurrentConnectionStatus.c_str(),true);


}

void SetSaveButton()
{
  tft.pushImage(268, 95, 40, 40, (uint16_t*)Save);
}
void drawFreqMenu() {
  currentMenu = FREQ_MENU;
  tft.pushImage(0, 0, 320, 170, (uint16_t*)FreqSet);
  updateFreqArea();
}
void drawAmpMenu() {
  currentMenu = AMP_MENU;
  tft.pushImage(0, 0, 320, 170, (uint16_t*)AmpSet);
  updateAmpArea();
}

void initTouch() {

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);  //For reliable communication, it is recommended to use a *maximum* communication rate of 400Kbps

  setupTDisplayS3();

  MDO::CST816Touch::device_type_t eDeviceType;
  if (oTouchController.getDeviceType(eDeviceType)) {
    Serial.print("Found device of type: ");
    Serial.println(CST816Touch::deviceTypeToString(eDeviceType));
  }

  Serial.println("Touch screen initialization done");
}

void handleTouch() {
  oTouchController.control();
  TouchScreenEventCache* pTouchCache = TouchScreenEventCache::getInstance();
  if (pTouchCache->hadTouch()) {
    int x = 0;
    int y = 0;
    pTouchCache->getLastTouchPosition(x, y);  //this 'consumes' the touch from the event cache
    //x = DISPLAY_X - x; // un-comment when tft.setRotation(3) used 
    GetTouchData(x, y);
  }
  if (pTouchCache->hadGesture()) {
    TouchScreenController::gesture_t gesture = TouchScreenController::gesture_t::GESTURE_NONE;
    int x = 0;
    int y = 0;
    pTouchCache->getLastGesture(gesture, x, y);  //this 'consumes' the gesture from the event cache
    //x = DISPLAY_X - x; // un-comment when tft.setRotation(3) used
    Serial.print("Gesture: ");
    Serial.print(TouchScreenController::gestureIdToString(gesture));
    if (gesture == TouchScreenController::gesture_t::GESTURE_DOUBLE_CLICK)  {
      //Serial.print(String(", at position (") + x + ", " + y + ")");
      GetTouchData(x, y);
    }

    if (gesture == TouchScreenController::gesture_t::GESTURE_LONG_PRESS) {
      //Serial.print(String(", at position (") + x + ", " + y + ")");
      GetTouchData(x, y);
    }

    if (gesture == TouchScreenController::gesture_t::GESTURE_TOUCH_BUTTON) {
        if (currentMenu != INFO_MENU) {
          drawInfoScreen();  
        } else {
          drawMainMenu();    
        }
    }

 
    Serial.println("");
  }
}


 
 

 
int LastLine = 150;
int OffsetH1 = 5;

int FontHeight(int font) {
  switch (font) {
    case 1:
      return Font1Size;
    case 2:
      return Font2Size;
    case 4:
      return Font4Size;
    case 6:
      return Font6Size;
    case 8:
      return Font8Size;
    default:
      return 0;  // Geçersiz font numarası için 0 döndür
  }
}


int screenWidth;
int fontHeight;
void SetupDisplay() {
  tft.init();
  tft.setRotation(1);

  tft.pushImage(0, 0, 320, 170, (uint16_t*)img_logo);
  delay(1000);

/*
  tft.fillScreen(TFT_BLUE);
  tft.fillRect(0, 151, 319, 19, TFT_BLACK);
  // Draw borders
  tft.drawLine(0, 0, 319, 0, TFT_DARKGREY);
  tft.drawLine(0, 0, 0, 169, TFT_DARKGREY);
  tft.drawLine(319, 0, 319, 169, TFT_DARKGREY);
  tft.drawLine(0, 169, 319, 169, TFT_DARKGREY);

  //tft.drawLine(0, 60, 319, 60, TFT_DARKGREY);
  int LastLine = 150;
  tft.drawLine(0, LastLine, 319, LastLine, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
*/

  // Set the font
  tft.setFreeFont(&FreeSans12pt7b);
  fontHeight = tft.fontHeight();  // Get font height
  screenWidth = tft.width();      // Get screen width
}
 

int XOffset = 3;
int YOffset = 3;

void ConnectionStatus(const char* text, bool clearLine) {

  CurrentConnectionStatus = String(text);
  //Serial.println(text); 
 
    if (currentMenu == MAIN_MENU) 
    {

      static int cursorX = 191;  // **Mevcut cursor pozisyonunu takip eden değişken**
      tft.setTextColor(TFT_WHITE);
      tft.setFreeFont(&FreeSans12pt7b);
      tft.setTextSize(1);
      if (clearLine) {
        // **Satırı temizle ve cursoru başa al**
        tft.fillRect(191, 151, 107, 18, tft.color565(51, 51, 51));
        cursorX = 191;  // **Cursoru sıfırla**
      }
      tft.setCursor(cursorX, 151, 2);  // **Kaldığı yerden devam et**
      tft.print(text);     // **Metni yazdır**
    
    
      cursorX = tft.getCursorX(); // Save current Cursor pozition
      delay(500);
  }
}




bool getTouch(int& x, int& y) {
  TouchScreenEventCache* pTouchCache = TouchScreenEventCache::getInstance();
  if (pTouchCache->hadTouch()) {
    pTouchCache->getLastTouchPosition(x, y);
    Serial.printf("Touched at: %d, %d\n", x, y);
    return true;
  }
  return false;
}

/*
void drawFreqA_AmpA(const char* freq, const char* amp, const char* unit) {
  int fontHeight = tft.fontHeight();
  int rectWidth = 100;               // Dikdörtgen genişliği (ayarlanabilir)
  int rectHeight = fontHeight + 10;  // Dikdörtgen yüksekliği (ayarlanabilir)

  // Freq Alanı
  int freqX = XOffset;
  int freqY = YOffset + fontHeight * 0;
  drawArea(freqX, freqY, rectWidth, rectHeight, "Freq", freq, unit);

  // Amp Alanı
  int ampX = XOffset;
  int ampY = YOffset + fontHeight * 1 + OffsetH1;
  drawArea(ampX, ampY, rectWidth, rectHeight, "Amp", amp, "dBm");

  // Filter Alanı
  int filterX = XOffset;
  int filterY = YOffset + fontHeight * 2 + OffsetH1 * 2;
  int filterWidth = 180;  // Filter alanı daha geniş
  drawArea(filterX, filterY, filterWidth, rectHeight, "Filter", "2-18 GHz", "");
}
*/

void drawFreqB_AmpB(const char* freqB, const char* ampB) {
  // Not implemented
}

void drawChannelA_SetOnOff(bool onOff) {
  int fontHeight = tft.fontHeight();
  String onText = " ON";
  int onWidth = tft.textWidth(onText);  // Get width of "ON"
  // Önceki metni silmek için dikdörtgen çiz
  tft.fillRect(screenWidth - onWidth, fontHeight * 1 + YOffset, onWidth, fontHeight, TFT_BLUE);  // Arka plan rengini kullanın
  tft.setTextColor(TFT_GREEN);                                                                   // Set text color to green
  tft.drawString(onText, screenWidth - onWidth - XOffset, fontHeight * 1 + YOffset);
}

void drawChannelB_SetOnOff(bool onOff) {
  // Not implemented
}


void SetLock(bool value)
{
  CurrentLockStatus = value;
  if (value)
  {
    tft.pushImage(21, 151, 18, 18, (uint16_t*)Locked);
  }
  else
  {
    tft.pushImage(21, 151, 18, 18, (uint16_t*)UnLocked); 
  }
}

void SetFilter()
{

    String tempFreq = enteredFreqValue;

    // Frekans birimine göre çarpan belirle
    double freqValue = tempFreq.toDouble();
    if (enteredUnitValue == "KHz") {
        freqValue *= 1000.0;
    } else if (enteredUnitValue == "MHz") {
        freqValue *= 1000000.0;
    } else if (enteredUnitValue == "GHz") {
        freqValue *= 1000000000.0;
    }


  SetFilterStat(FilterStatus);

    if (FilterStatus)
    {
      tft.pushImage(80, 100, 40, 35, (uint16_t*)FilterON);
      //
      tft.fillRect(127, 100, 120, 31,  tft.color565(46, 116, 181)); // Clear old value
      tft.setTextColor(tft.color565(50, 50, 50), tft.color565(46, 116, 181));
      tft.setCursor(127, 124);
      tft.setFreeFont(&FreeSansBold9pt7b);
      tft.print("2-18 GHz");
      //
      SetFilterBand(freqValue / 1000000.0); 
    }
    else
    {
      tft.pushImage(80, 100, 40, 35, (uint16_t*)FilterOFF); 
      //
      tft.fillRect(127, 100, 120, 31,  tft.color565(46, 116, 181)); // Clear old value
      tft.setTextColor(tft.color565(50, 50, 50), tft.color565(46, 116, 181));
      tft.setCursor(127, 124);
      tft.setFreeFont(&FreeSansBold9pt7b);
      tft.print("0.3-22.6 GHz");
    }

  

}
void SetBITStatus(bool value)
{
  CurrentBITStatus = value;
  if (value)
  {
    tft.pushImage(168, 151, 18, 18, (uint16_t*)BIT_PASS);
  }
  else
  {
    tft.pushImage(168, 151, 18, 18, (uint16_t*)BIT_FAIL); 
  }
}

void SetRfOnOff(bool value)
{
  CurrentRFStatus = value;
  SetPLL1OnOff(value);
  
  if (value)
  {
    tft.pushImage(264, 42, 48, 48, (uint16_t*)RF_ON);
  }
  else
  {
    tft.pushImage(264, 42, 48, 48, (uint16_t*)RF_OFF); 
  }

}

void SaveRfSettingsBtn()
{
  tft.pushImage(268, 95, 40, 40, (uint16_t*)SaveOK);
  currentFrequency  = enteredFreqValue;
  currentAmplitude = enteredAmpValue;
  currentFreqUnit = enteredUnitValue;

  saveRFSettings();  // Save to crediantials
  delay(500);
  tft.pushImage(268, 95, 40, 40, (uint16_t*)Save);
}

void drawInfoScreen() {
  if (currentMenu != INFO_MENU) {
    tft.fillScreen(TFT_BLUE);  // Tüm ekranı temizle
  }

  currentMenu = INFO_MENU;

  // Gerçek zamanlı değerleri oku
  float temp = Read_Temp();
  float usb_voltage = Read_5V_Voltage();
  float dsg_current = Read_5V_Current();
  bool ld_result = isPLL_Locked();

  // Yazı ayarları
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans12pt7b);

  // Temizleme boyutları
  int lineHeight = 25;
  int textX = 20;
  int width = 280; // genişlik
  int height = 24;

  // Her satırı sil ve yeniden yaz
  tft.fillRect(textX, 20, width, height, TFT_BLUE);
  tft.setCursor(textX, 40);
  tft.printf("5V Current: %.2f A", dsg_current);

  tft.fillRect(textX, 50, width, height, TFT_BLUE);
  tft.setCursor(textX, 70);
  tft.printf("Temperature: %.1f C", temp);

  tft.fillRect(textX, 80, width, height, TFT_BLUE);
  tft.setCursor(textX, 100);
  tft.printf("5V Voltage: %.2f V", usb_voltage);

  tft.fillRect(textX, 110, width, height, TFT_BLUE);
  tft.setCursor(textX, 130);
  tft.printf("LD Result: %s", ld_result ? "LOCKED" : "UNLOCKED");
}




void SetWifiStatus(WifiStatus status) {
  CurrentWifiStatus = status;

    if (currentMenu == MAIN_MENU) 
   {
      switch (status) {
          case WIFI_STATUS_ON:
              tft.pushImage(300, 151, 18, 18, (uint16_t*)WifiOn);
              break;
          case WIFI_STATUS_OFF:
              tft.pushImage(300, 151, 18, 18, (uint16_t*)WifiOff);
              break;
          case WIFI_STATUS_HOTSPOT:
              tft.pushImage(300, 151, 18, 18, (uint16_t*)WifiHotspot);
              break;
          default:
              break;
      }
   }
}



void SetTemp(const char* text)
{
  CurrentTempValue = String(text);
  tft.fillRect(50, 151, 32, 18, tft.color565(51, 51, 51));
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setCursor(50, 151, 2);
  tft.print(String(text) + "C");   
}



void SetUSBVoltge(const char* text)
{
  CurrentUSBVoltageValue = String(text); 
  tft.fillRect(120, 151, 29, 18, tft.color565(51, 51, 51));
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setCursor(120, 151, 2);
  tft.print(String(text) + "V");   

}


// Apply frequency without affecting UI logic
void ApplyFrequency(double fHz) {
    double fMHz = fHz / 1e6;

    if (FilterStatus) {
        SetFilterBand(fMHz);
    }

    // PLL set
    bool lock = Lmx2820SetFreqinMHz(fMHz, 10000000);

    // Update state only
    if (fHz >= 1e9) {
        enteredFreqValue = String(fHz / 1e9, 3);
        enteredUnitValue = "GHz";
    } else if (fHz >= 1e6) {
        enteredFreqValue = String(fHz / 1e6, 3);
        enteredUnitValue = "MHz";
    } else if (fHz >= 1e3) {
        enteredFreqValue = String(fHz / 1e3, 3);
        enteredUnitValue = "KHz";
    } else {
        enteredFreqValue = String(fHz, 0);
        enteredUnitValue = "Hz";
    }
    SetFreqUnitOnMainMenu(enteredUnitValue);
    SetFreqOnMainMenu(enteredFreqValue);
    SetLock(lock);

}

// Apply amplitude without affecting UI logic
void ApplyAmplitude(double  dBm) {
    enteredAmpValue = String(dBm, 1);
    SetAmpUnitOnMainMenu();
    SetAttenuator((uint8_t)dBm);
    SetAmpOnMainMenu(String(dBm));

}

void ApplyFilter(bool enable) {
    FilterStatus = enable;
    SetFilterStat(FilterStatus);
    if (currentMenu == MAIN_MENU) {
        SetFilter();
    }
}