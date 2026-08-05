#include "ADC78H90.h"
#include "main.h"
#include <SPI.h>

void InitADC()
{
  SPI.begin(SCLK_, MISO_, MOSI_, ADC_CS);
  pinMode(ADC_CS, OUTPUT);
  digitalWrite(ADC_CS, HIGH); 
  
  float min=0, max=0, avg=0;

  // Discard the first data reading because it belongs to the previously used channel
  Read_5V_Voltage();
  Read_5V_Current();
  Read_Temp();
}

float ADC_Read_Average(uint8_t Chnl, uint8_t AvgCount, float *Min, float *Max, float *Avg)
{
    float sum = 0;
    *Min = 3.3; // Set to max possible value for 3.3V reference
    *Max = 0.0; // Set to min possible value

    // Discard the first data reading because it belongs to the previously used channel
    ADC_Read(Chnl); 
    delay(2);

    for (int i = 0; i < AvgCount; i++) {
        float value = ADC_Read(Chnl); 

        if (value < *Min) {
            *Min = value;
        }
        if (value > *Max) {
            *Max = value;
        }

        sum += value; // Update the total accumulated value
        delay(1);
    }

    *Avg = sum / AvgCount; // Calculate the final average

    return *Avg; // Return the average value
}

float ADC_Read (uint8_t Chnl)
{
  uint8_t tx_data[2];
  uint8_t rx_data[2];
  
  tx_data[0] = (Chnl & 0x07) << 3; // Mask other bits, only first 3 bits are valid as a channel (0 to 7)
  tx_data[1] = 0;
  
  digitalWrite(ADC_CS, LOW);  // Pull the chip select pin low
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  rx_data[0] = SPI.transfer(tx_data[0]);   
  rx_data[1] = SPI.transfer(tx_data[1]);   

  SPI.endTransaction();
  digitalWrite(ADC_CS, HIGH); // Pull the chip select pin high
  
  // Fix: 12-bit ADC data masking (0x0F) is restored
  uint16_t raw_data = ((rx_data[0] & 0x0F) << 8) | rx_data[1];
  float data = 3.30 * ((float)raw_data / 4096.0);
  return data;
}

#define GAIN    50.0f
#define RSHUNT  0.02f // Ohms

float Read_5V_Current()
{
  float min=0, max=0, avg=0;
  ADC_Read_Average(3, 10, &min, &max, &avg); // AN4
  
  float current = avg / (GAIN * RSHUNT);
  return current;
}

float Read_5V_Voltage()
{
  float min=0, max=0, avg=0;
  ADC_Read_Average(2, 10, &min, &max, &avg); // AN3
  
  // Fix: Voltage divider scaling (Raw ADC voltage * 2.0)
  return avg * 2.0;
}

float Read_Temp()
{
  float min=0, max=0, avg=0;
  ADC_Read_Average(4, 10, &min, &max, &avg); // AN5
  
  // Fix: MCP9700 Temperature sensor formula: Temp = (Vout - 0.5V) * 100
  float temperature = (avg - 0.5) * 100.0;
  return temperature;
}

float Read_4V_Current()
{
  float min=0, max=0, avg=0;
  ADC_Read_Average(1, 10, &min, &max, &avg); // AN2
  
  float current = avg / (GAIN * RSHUNT);
  return current;
}