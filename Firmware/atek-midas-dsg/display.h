#ifndef DISPLAY_H
#define DISPLAY_H

// Display module interface declarations for TFT screen, touch handling, menu rendering, and UI status updates.

#include <TFT_eSPI.h> // Include the TFT screen library

extern TFT_eSPI tft;
extern TFT_eSprite sprite;
extern bool isSweepRunning;

#define Font1Size 8
#define Font2Size 16
#define Font4Size 26
#define Font6Size 48
#define Font8Size 75

enum WifiStatus {
    WIFI_STATUS_OFF,
    WIFI_STATUS_ON,
    WIFI_STATUS_HOTSPOT
};

enum MenuState {
  MAIN_MENU,
  FREQ_MENU,
  AMP_MENU,
  INFO_MENU,
  SWEEP_MENU,
  START_MENU,
  STOP_MENU,
  STEP_MENU,
  DWELL_MENU,
  SWP_COUNT_MENU,
  NONE
};

extern MenuState currentMenu; 

// Function declarations
void SetupDisplay();

int FontHeight(int font);
void setWifiStat(bool onOff);
void ConnectionStatus(const char* text, bool clearLine);
void drawFreqA_AmpA(const char* freq, const char* amp, const char* unit);
void drawFreqB_AmpB(const char* freqB, const char* ampB);
void drawChannelA_SetOnOff(bool onOff);
void drawChannelB_SetOnOff(bool onOff);
void drawStatusMenu(const char* extRef, const char* pll1, const char* pll2, const char* temp);
void displayInitScreen();
void initTouch();
void handleTouch();

void drawDataField(int x, int y, int w, int h, const char *text);
void drawUnderline(int x, int y);
void drawPowerButton(int x, int y);
void drawFilterArea(int x, int y);
void drawCenteredText(const char* text, int x, int y, int width, int height);
bool checkenteredFreqValue(String FreqVal);
bool checkenteredAmpValue();
void SetTemp(const char* text);
void SetUSBVoltge(const char* text);
void RunSweep();
void SetStartFreqOnSweepMenu(String value);
void SetStopFreqOnSweepMenu(String value);
void SetStepFreqOnSweepMenu(String value);
void SetDwellFreqOnSweepMenu(String value);
void SetAmpAmpOnSweepMenu(String value);
void SetSCountOnSweepMenu(String value);

void SetFreqOnMainMenu(String value);
void SetFreqUnitOnMainMenu(String value);
void SetAmpOnMainMenu(String value);

void SetWifiStatus(WifiStatus status);
void SetBITStatus(bool value);
void SetFilter(bool FilState);
void SetLock(bool value);
void SetRfOnOff(bool value);
void SaveRfSettingsBtn();
void SetSaveButton();
void SetAmp(String value);
void SetFreqUnit(String value);
void SetFreq(String value);
void drawInfoScreen();
void drawStartMenu();
void drawCountMenu();
void drawStopMenu();
void drawDwellMenu();
void drawStepMenu();

void updateFreqAreaOnFreqMenu(String val, String unit);
void updateAmpAreaOnAmpMenu();
void updateStartAreaOnStartMenu(String val, String unit);
void updateStopAreaOnStopMenu(String val, String unit);
void updateStepAreaOnStepMenu(String val, String unit);
void updateDwellAreaOnDwellMenu(String val);
void updateCountAreaOnCountMenu(String val);
void updateAmpAreaOnSwpAmpMenu(String val);


void ApplyFrequency(double fHz);
void ApplyAmplitude(double dBm);
void ApplyFilter(bool enable);
void drawMainMenu();
void drawSweepMenu();
void drawActiveMenu();
void updateDecimalArea();
void drawFreqMenu(MenuState menu);
void drawAmpMenu(MenuState menu);

#endif