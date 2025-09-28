#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#define  MIN_RFPOWER -30
#define  MAX_RFPOWER 31

extern String currentFrequency; 
extern String currentAmplitude;  
extern String currentFreqUnit;  
extern bool FilterStatus; 
extern bool RFStatus;


extern bool rfOutputEnabled; 

char* FloatToChar(float num);
char* DoubleToChar(double num);

void saveRFSettings();
void readRFSettings();

#endif