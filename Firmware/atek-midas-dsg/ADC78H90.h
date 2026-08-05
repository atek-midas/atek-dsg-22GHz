#ifndef ADC78H90_H
#define ADC78H90_H

#include <Arduino.h>
#include "main.h"

// --- SPI Pin Definitions for ADC ---
#define ADC_CS          2    // Chip Select Pin for ADC
#define SCLK_           12   // SPI Clock Pin
#define MOSI_           11   // SPI MOSI Pin
#define MISO_           13   // SPI MISO Pin

// --- Function Declarations ---
void InitADC();
float ADC_Read (uint8_t Chnl);
float ADC_Read_Average(uint8_t Chnl, uint8_t AvgCount, float *Min, float *Max, float *Avg);

float Read_5V_Voltage();
float Read_5V_Current();
float Read_Temp();
float Read_4V_Current();

#endif