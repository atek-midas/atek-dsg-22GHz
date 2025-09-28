#include "ADC78H90.h"
#include "main.h"
#include <SPI.h>





void InitADC()
{
  SPI.begin(SCLK_, MISO_, MOSI_, ADC_CS);
  pinMode(ADC_CS, OUTPUT);
  digitalWrite(ADC_CS, HIGH); 

	float min=0, max=0, avg=0;

  Read_5V_Voltage();
  Read_5V_Current();
  Read_Temp();

/*
	ADC_Read_Average(0, 10, &min, &max, &avg); // AN1
	Serial.print("ADC AN1: Avg: ");
	char * str = FloatToChar(avg);
	Serial.print(str);
	Serial.print(" Min: ");
	Serial.print(FloatToChar(min));
	Serial.print(" Max: ");
	Serial.print(FloatToChar(max));
	Serial.print("\r\n");

	ADC_Read_Average(1, 10, &min, &max, &avg); // AN2
	Serial.print("ADC AN2: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(2, 10, &min, &max, &avg); // AN3
	Serial.print("ADC AN3: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(3, 10, &min, &max, &avg); // AN4
	Serial.print("ADC AN4: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(4, 10, &min, &max, &avg); // AN5
	Serial.print("ADC AN5: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(5, 10, &min, &max, &avg); // AN6
	Serial.print("ADC AN6: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(6, 10, &min, &max, &avg); // AN7
	Serial.print("ADC AN7: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
	ADC_Read_Average(7, 10, &min, &max, &avg); // AN8
	Serial.print("ADC AN8: Avg: "); Serial.print(DoubleToChar(avg)); Serial.print(" Min: "); Serial.print(DoubleToChar(min)); Serial.print(" Max: "); Serial.print(DoubleToChar(max)); Serial.print("\r\n");
*/
}




float ADC_Read_Average(uint8_t Chnl, uint8_t AvgCount, float *Min, float *Max, float *Avg) {
    float sum = 0;
    *Min = 9999;  
    *Max = 0;    

    float value = ADC_Read(Chnl); // ilk datayi at cunku ilk data bir onceki kullanılan kanala ait
    for (int i = 0; i < AvgCount; i++) {
        value = ADC_Read(Chnl); //  

        // Minimum değeri güncelle
        if (value < *Min) {
            *Min = value;
        }

        // Maksimum değeri güncelle
        if (value > *Max) {
            *Max = value;
        }

        sum += value; // Toplam değeri güncelle
    }

    *Avg = sum / AvgCount; // Ortalamayı hesapla 

    return *Avg; // Ortalamayı dön
}

float ADC_Read (uint8_t Chnl)
{
  Chnl &= 0x07;// mask other bits only first 3 bit is valid as a channel 0 to 7
  Chnl = Chnl << 3;
  uint8_t tx_data[2];
  tx_data[0] =Chnl;
  tx_data[1] =0;
  uint8_t rx_data[2];
  digitalWrite(ADC_CS, LOW);  // pull the pin low
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < 2; i++) {
    rx_data[i] = SPI.transfer(tx_data[i]);   
  }

  SPI.endTransaction();
  digitalWrite(ADC_CS, HIGH);  // pull the pin high
  float data =   (float)( (rx_data[0]<<8)| rx_data[1]);
  data = 3.30 * (data / 4096.0);
  return data;
}

#define GAIN    50.0f
#define RSHUNT  0.02f

float Read_5V_Current()
{
    float min = 0, max = 0, avg = 0;
    ADC_Read_Average(3, 10, &min, &max, &avg);
    float current = avg / (GAIN * RSHUNT); // I = V / (Gain * Rshunt)
    return current; // Amper
}

 
float Read_5V_Voltage()
{
    float min = 0, max = 0, avg = 0;
    ADC_Read_Average(2, 10, &min, &max, &avg);
    return avg * 2.0f; // Direnç bölücü düzeltmesi
    
}
 
float Read_Temp()
{
    float min = 0, max = 0, avg = 0;
    ADC_Read_Average(4, 10, &min, &max, &avg);
    // TMP235: T(°C) = (V - 0.5) * 100
    float temperature = (avg - 0.5f) * 100.0f;
    return temperature;
}