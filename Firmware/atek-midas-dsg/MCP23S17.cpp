
#include "MCP23S17.h"
#include "main.h"
#include <SPI.h>

void IO_EXP1_Init ()
{
  SPI.begin(SCLK_, MISO_, MOSI_, IO1_CS);
  pinMode(IO1_CS, OUTPUT);
  digitalWrite(IO1_CS, HIGH); 

	// IO_EXP1
	IO_EXP1_write(IODIRA,0xFF); // Set all pins to INPUT
	uint8_t dumy1FF = IO_EXP1_read(IODIRA);
	IO_EXP1_write(IODIRA,0x09);// Set all pins to OUTPUT (Except LO1_LD and DT_RFIN)
	uint8_t dumy100 = IO_EXP1_read(IODIRA);

	IO_EXP1_write(IODIRB,0xFF); // Set all pins to INPUT
	uint8_t dumy2FF = IO_EXP1_read(IODIRB);
	IO_EXP1_write(IODIRB,0x00);// Set all pins to OUTPUT
	uint8_t dumy200 = IO_EXP1_read(IODIRB);

	IO_EXP1_write(GPIOA_,0x00); // Set all pins LOW
 	IO_EXP1_write(GPIOB_,0x00); // Set all pins LOW
	if (dumy1FF==0xFF && dumy100 ==0x09 && dumy2FF==0xFF && dumy200 ==0x00)
	{
		Serial.println("IO_EXP1 Initialization Successful.\r\n");
	}
	else
	{
		Serial.println("IO_EXP1 Initialization Failed!!!\r\n");

    if (dumy1FF != 0xFF) Serial.printf("IODIRA expected 0xFF, got 0x%02X\r\n", dumy1FF);
    if (dumy100 != 0x09) Serial.printf("IODIRA expected 0x09, got 0x%02X\r\n", dumy100);
    if (dumy2FF != 0xFF) Serial.printf("IODIRB expected 0xFF, got 0x%02X\r\n", dumy2FF);
    if (dumy200 != 0x00) Serial.printf("IODIRB expected 0x00, got 0x%02X\r\n", dumy200);

	}

  SetIOExpander();

}



void IO_EXP1_DumpAllRegisters()
{
    // Register adresleri
    uint8_t registers[] = {IODIRA, IODIRB, IPOLA, IPOLB, GPINTENA, GPINTENB, DEFVALA, DEFVALB,
                           INTCONA, INTCONB, IOCONA, IOCONB, GPPUA, GPPUB, INTFA, INTFB,
                           INTCAPA, INTCAPB, GPIOA_, GPIOB_, OLATA, OLATB};

    char message[50];

    // Her bir registerı oku ve seri porta yazdır
    for (int i = 0; i < sizeof(registers); i++)
    {
        uint8_t regValue = IO_EXP1_read(registers[i]);  // Register değerini oku
        sprintf(message, "Register 0x%02X: 0x%02X\r\n", registers[i], regValue);  // Değeri formatla
        Serial.println(message);  // Seri porta yaz
    }

    Serial.println("IO_EXP1 Register Dump Completed.\r\n");
}

// PORTA Pins
uint8_t DT_RFIN     = 0;   // GPA0 In
uint8_t PLL_PSYNC   = 0;   // GPA1 Out
uint8_t PLL1_CE     = 1;   // GPA2 Out
uint8_t LO1_LD      = 0;   // GPA3 In
uint8_t RF_DSA1_P5  = 0;   // GPA4 Out
uint8_t RF_DSA1_P4  = 0;   // GPA5 Out
uint8_t RF_DSA1_P3  = 0;   // GPA6 Out
uint8_t RF_DSA1_P2  = 0;   // GPA7 Out

// PORTB Pins
uint8_t RF_DSA1_P1    = 0;   // GPB0 Out
uint8_t IF1_SW1_C     = 1;   // GPB1 Out
uint8_t IF1_SW1_C_INV = 0;   // GPB2 Out
uint8_t LO1_SW1_A     = 0;   // GPB3 Out
uint8_t LO1_SW1_B     = 0;   // GPB4 Out
uint8_t LO1_SW1_C     = 0;   // GPB5 Out
uint8_t SPMS_EN       = 0;   // GPB6 Out
uint8_t NAN_1         = 0;   // GPB7 


uint8_t portA_byte = 0;
uint8_t portB_byte = 0;

void SetPLL1OnOff(bool enable)
{
    if (enable)
        portA_byte |=  (1 << 2);  // GPA2 = 1 → PLL ON
    else
        portA_byte &= ~(1 << 2);  // GPA2 = 0 → PLL OFF

    IO_EXP1_write(GPIOA_, portA_byte);
}

void SetIOExpander()
{
  	 uint16_t IoExpData = 0;

  	 IoExpData =
							(DT_RFIN <<0) +
							(PLL_PSYNC <<1) +
							(PLL1_CE <<2) +
							(LO1_LD 	<<3) +
							(RF_DSA1_P5 	<<4) +
							(RF_DSA1_P4 	<<5) +
							(RF_DSA1_P3 <<6) +
							(RF_DSA1_P2 <<7) +
							(RF_DSA1_P1 <<8) +
							(IF1_SW1_C 	<<9) +
							(IF1_SW1_C_INV 	<<10) +
							(LO1_SW1_A 	<<11) +
							(LO1_SW1_B 	<<12) +
							(LO1_SW1_C 	<<13) +
							(SPMS_EN 	<<14) +
              (NAN_1 	<<15) ;

 	//
  portA_byte = (IoExpData & 0xFF);
  portB_byte = (IoExpData >> 8) & 0xFF;

  IO_EXP1_write(GPIOA_, portA_byte);
  IO_EXP1_write(GPIOB_, portB_byte);

}
void IO_EXP1_write (uint8_t address, uint8_t Data)
{
  uint8_t spi_buf[3];
  spi_buf[0] =  0x40;  // Write operation  (0 1 0 0 A2 A1 A0 R/W)
  spi_buf[1] =  address;
  spi_buf[2] =  Data;

  digitalWrite(IO1_CS, LOW);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(spi_buf, 3);  // 3 byte veri gönder
  SPI.endTransaction();
  digitalWrite(IO1_CS, HIGH);

 // Debug print
  if (address == GPIOA_) {
    Serial.printf("Written 0x%02X to GPA\n", Data);
  } 
  else if (address == GPIOB_) {
    Serial.printf("Written 0x%02X to GPB\n", Data);
  }

}

// OnOff == true  → Amplifier + Filter (IF1_SW1_C = 0, IF1_SW1_C_INV = 1)
// OnOff == false → Bypass             (IF1_SW1_C = 1, IF1_SW1_C_INV = 0)
void SetFilterStat(bool OnOff)
{
    if (OnOff)
    {
        // Amplifier + Filter
        portB_byte &= ~(1 << 1);   // GPB1 = 0 (IF1_SW1_C)
        portB_byte |=  (1 << 2);   // GPB2 = 1 (IF1_SW1_C_INV)
    }
    else
    {
        // Bypass
        portB_byte |=  (1 << 1);   // GPB1 = 1 (IF1_SW1_C)
        portB_byte &= ~(1 << 2);   // GPB2 = 0 (IF1_SW1_C_INV)
    }

    IO_EXP1_write(GPIOB_, portB_byte);
}


void SetAttenuator(uint8_t value)
{
    if (value > 31)
        value = 31;  // Maksimum sınır

    SetRF_DSA1(value);
}

// Frekans MHz cinsinden girilir. Doğru band seçilip LO1_SW1 A,B,C pinleri ayarlanır.
void SetFilterBand(double Freq)
{
    uint8_t A = 0, B = 0, C = 0;

    if (Freq >= 2000 && Freq <= 3000)       { A = 0; B = 0; C = 0; } // Band 1
    else if (Freq > 3000 && Freq <= 5000)   { A = 1; B = 0; C = 1; } // Band 2
    else if (Freq > 5000 && Freq <= 8300)   { A = 0; B = 1; C = 0; } // Band 3
    else if (Freq > 8300 && Freq <= 11300)  { A = 1; B = 1; C = 0; } // Band 4
    else if (Freq > 11300 && Freq <= 13300) { A = 1; B = 0; C = 0; } // Band 5
    else if (Freq > 13300 && Freq <= 18000) { A = 0; B = 0; C = 1; } // Band 6
    else
    {
        Serial.printf("Invalid frequency %.2f MHz. Must be between 2000 and 18000.\r\n", Freq);
        return;
    }

    uint8_t value = (A << 0) | (B << 1) | (C << 2);  // A,B,C → bit 0,1,2
    SetLO1_SW1(value);
}

uint8_t IO_EXP1_read (uint8_t address)
{
  uint8_t spi_buf[2];
  spi_buf[0] =  0x41;  // Read operation  (0 1 0 0 A2 A1 A0 R/W)
  spi_buf[1] =  address;
  uint8_t read_buf[1];

	digitalWrite(IO1_CS, LOW);      // pull the pin low
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(spi_buf, 2);  // Adres verisini gönder

  read_buf[0] = SPI.transfer(0x00);  // Boş bir byte göndererek okuma yap
  SPI.endTransaction();
	digitalWrite(IO1_CS, HIGH);  // pull the pin high

	return read_buf[0];
}
 
bool ReadLO1_LD()
{
    uint8_t gpioa = IO_EXP1_read(GPIOA_);
    return (gpioa >> 3) & 0x01;  // GPA3 = LO1_LD
}

void SetLO1_SW1(uint8_t value)
{
    portB_byte &= ~(0b00111000);            // GPB3, 4, 5 temizle
    portB_byte |=  (value & 0b00000111) << 3; // a,b,c → GPB3,4,5
    IO_EXP1_write(GPIOB_, portB_byte);
}


void SetRF_DSA1(uint8_t value)
{
    // Önce eski değerleri temizle
    portA_byte &= ~( (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) ); // GPA4-7 temizle
    portB_byte &= ~(1 << 0); // GPB0 temizle

    // value bit sırasına göre: [P5 P4 P3 P2 P1] → [bit4 bit3 bit2 bit1 bit0]
    if (value & (1 << 4)) portA_byte |= (1 << 4); // P5 → GPA4
    if (value & (1 << 3)) portA_byte |= (1 << 5); // P4 → GPA5
    if (value & (1 << 2)) portA_byte |= (1 << 6); // P3 → GPA6
    if (value & (1 << 1)) portA_byte |= (1 << 7); // P2 → GPA7
    if (value & (1 << 0)) portB_byte |= (1 << 0); // P1 → GPB0

    // Güncellenmiş değerleri yaz
    IO_EXP1_write(GPIOA_, portA_byte);
    IO_EXP1_write(GPIOB_, portB_byte);
}


void SetPLL1_CE(bool state)
{
    portA_byte &= ~(1 << 2);      // GPA2 = 0
    if (state)
        portA_byte |= (1 << 2);   // GPA2 = 1

    IO_EXP1_write(GPIOA_, portA_byte);
}

void SetIF1_SW1_C(bool state)
{
    if (state)
    {
        portB_byte |=  (1 << 1);   // GPB1 = 1
        portB_byte &= ~(1 << 2);   // GPB2 = 0
    }
    else
    {
        portB_byte &= ~(1 << 1);   // GPB1 = 0
        portB_byte |=  (1 << 2);   // GPB2 = 1
    }

    IO_EXP1_write(GPIOB_, portB_byte);
}



