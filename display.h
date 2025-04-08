#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h> // TFT ekran kütüphanesini ekliyoruz

extern TFT_eSPI tft;
extern TFT_eSprite sprite;

#define Font1Size 8
#define Font2Size 16
#define Font4Size 26
#define Font6Size 48
#define Font8Size 75

// Fonksiyon bildirimleri
void SetupDisplay();

int FontHeight(int font);
void setWifiStat(bool onOff);
void ConnectionStatus(const char* text, bool clearLine);
void drawFreqA_AmpA(const char* freq, const char* amp, const char* unit);
void drawFreqB_AmpB(const char* freqB, const char* ampB);
void drawChannelA_SetOnOff(bool onOff);
void drawChannelB_SetOnOff(bool onOff);
void updateFreqArea();
void updateAmpArea();
void initTouch();
bool getTouch(int &x, int &y);
void handleTouch();
void GetTouchData();
void drawMainMenu();
void drawFreqMenu();
void drawAmpMenu();
String drawNumericKeyboard();
void closeNumericKeyboard();
void drawButton(int x, int y, int w, int h, const char *label, bool pressed);
void drawDataField(int x, int y, int w, int h, const char *text);
void drawUnderline(int x, int y);
void drawPowerButton(int x, int y);
void drawFilterArea(int x, int y);
void drawCenteredText(const char* text, int x, int y, int width, int height);
bool checkenteredFreqValue();
bool checkenteredAmpValue();
void SetTemp(const char* text);
void SetUSBVoltge(const char* text);

enum WifiStatus {
    WIFI_STATUS_OFF,
    WIFI_STATUS_ON,
    WIFI_STATUS_HOTSPOT
};


void SetFreqOnMainMenu(String value);
void SetFreqUnitOnMainMenu(String value);
void SetAmpOnMainMenu(String value);

void SetWifiStatus(WifiStatus status);
void SetBITStatus(bool value);
void SetFilter();
void SetLock(bool value);
void SetRfOnOff(bool value);
void SaveRfSettingsBtn();
void SetSaveButton();
// Konumlandırma ve stil değişkenleri
extern int XOffset;
extern int YOffset;
extern int LastLine;
extern int OffsetH1;

#endif
