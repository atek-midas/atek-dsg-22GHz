
#include "main.h"
#include "Lmx2820.h"

#define PLL_DEN 1000 
#define OSC_2X 2  
#define MULT 1
#define PLL_R_PRE 1
#include <SPI.h>

// R79: OUTB_PD=1 (bit8), OUTB_MUX=01 (bits5:4), OUTA_PWR=val (bits3:1), diğerleri 0
void Lmx2820SetOUTA_PWR(uint8_t val)
{
  if (val > 7) val = 7;                 // 3-bit sınır
  uint32_t r79 = (1u << 8) | (1u << 4) | ((val & 0x7u) << 1);
  PLL_write(0x4F, r79);                  // OUTB_MUX=01’e zorlar (default)
}


bool Lmx2820SetFreqinMHz(double target_freq , double ref_clock)  
{
	target_freq = target_freq*1000000;
  char buf[64];
  snprintf(buf, sizeof(buf), "Set Freq(Hz): %.0f", target_freq);
  Serial.println(buf);

	//char buf[32];
  snprintf(buf, sizeof(buf), "Freq: %.0f", target_freq);
  //Serial.println(buf);
	//Serial.println("\r\n");

	   uint8_t cikis_bolucu = 1;
	    double vco_frekansi = target_freq * cikis_bolucu;
	    uint8_t DBLR_CAL_EN = 0;

	    // Eğer hedef frekans VCO max'tan büyükse doubler kullan
	    if (target_freq > VCO_MAX) {
	        vco_frekansi = target_freq / 2;
	        DBLR_CAL_EN = 1;
	    }

	    // Eğer hedef frekans VCO min'in altındaysa uygun çıkış bölücüyü bul
	    if (target_freq < VCO_MIN) {
	        for (int i = 7; i >= 1; i--) {
	            if ((target_freq * pow(2, i)) < VCO_MAX) {
	                cikis_bolucu = (1 << i);
	                break;
	            }
	        }
	        vco_frekansi = target_freq * cikis_bolucu;
	    }

	    // PFD Frekansı Hesaplaması
	    double PFD_FREKANSI = (ref_clock * OSC_2X * MULT) / (PLL_R_PRE);

	    // N Divider Integer ve Fractional Bileşenlerini Doğru Hesapla
	    double N_divider_exact = vco_frekansi / PFD_FREKANSI;
	    uint32_t PLL_N = (uint32_t)N_divider_exact;  // Tam sayı bileşeni

	    // Hassas Fractional hesaplama
	    double N_fractional = round((N_divider_exact - PLL_N) * PLL_DEN) / PLL_DEN;
	    uint32_t PLL_NUM = (uint32_t)(N_fractional * PLL_DEN);
	    double N_fractional_final = (double)PLL_NUM / PLL_DEN;

 
    uint32_t CHDIVA;

    switch (cikis_bolucu) {
        case 2:   CHDIVA = 0; break;
        case 4:   CHDIVA = 1; break;
        case 8:   CHDIVA = 2; break;
        case 16:  CHDIVA = 3; break;
        case 32:  CHDIVA = 4; break;
        case 64:  CHDIVA = 5; break;
        case 128: CHDIVA = 6; break;
        default:  CHDIVA = 7;  
                  break;
    }

  uint32_t OUTA_MUX;

    if (target_freq < VCO_MIN) 
    {
        OUTA_MUX = 0x0; //Channel divider
      
    }
    else if (target_freq >= VCO_MIN && target_freq <= VCO_MAX)
    {
        OUTA_MUX = 0x1;// VCO
    }
    else
    {
        OUTA_MUX = 0x2;// Doubler
    }


	    // Register değerleri
	    uint32_t reg_N = PLL_N;
	    uint32_t reg_NUM1 = (PLL_NUM >> 16) & 0xFFFF;
	    uint32_t reg_NUM2 = PLL_NUM & 0xFFFF;
	    uint32_t reg_DEN1 = (PLL_DEN >> 16) & 0xFFFF;
	    uint32_t reg_DEN2 = PLL_DEN & 0xFFFF;

	    uint32_t reg_R_DIV = ((CHDIVA) << 6) | 0x1001;
      uint16_t read_val;

      PLL_write(0x2C, 0x8000);
      //read_val = PLL_read(0x2C);
      //printf("Reg 0x2C: Wrote 0x%04X, Read back 0x%04X\r\n", 0x8000, read_val);

      PLL_write(0x24, reg_N);
      //read_val = PLL_read(0x24);
      //printf("Reg 0x24: Wrote 0x%04X, Read back 0x%04X\r\n", reg_N, read_val);

      PLL_write(0x2B, reg_NUM2);
      //read_val = PLL_read(0x2B);
      //printf("Reg 0x2B: Wrote 0x%04X, Read back 0x%04X\r\n", reg_NUM2, read_val);

      PLL_write(0x4E, OUTA_MUX);
      //read_val = PLL_read(0x4E);
      //printf("Reg 0x4E: Wrote 0x%04X, Read back 0x%04X\r\n", OUTA_MUX, read_val);

      PLL_write(0x2A, reg_NUM1);
      //read_val = PLL_read(0x2A);
      //printf("Reg 0x2A: Wrote 0x%04X, Read back 0x%04X\r\n", reg_NUM1, read_val);

      PLL_write(0x27, reg_DEN2);
      //read_val = PLL_read(0x27);
      //printf("Reg 0x27: Wrote 0x%04X, Read back 0x%04X\r\n", reg_DEN2, read_val);

      PLL_write(0x26, reg_DEN1);
      //read_val = PLL_read(0x26);
      //printf("Reg 0x26: Wrote 0x%04X, Read back 0x%04X\r\n", reg_DEN1, read_val);

      PLL_write(0x20, reg_R_DIV);
      //read_val = PLL_read(0x20);
      //printf("Reg 0x20: Wrote 0x%04X, Read back 0x%04X\r\n", reg_R_DIV, read_val);

      //delay(50);
      PLL_write(0x00, 0x6070);
      //read_val = PLL_read(0x00);
      //printf("Reg 0x00: Wrote 0x%04X, Read back 0x%04X\r\n", 0x6070, read_val);


  /*
		char buffer[100];
		sprintf(buffer, "VCO Freq: %.2f MHz\r\n", vco_frekansi / 1000000.0);
		Serial.print(buffer);
		sprintf(buffer, "PFD Freq: %.2f MHz\r\n", PFD_FREKANSI  / 1000000.0);
		Serial.print(buffer);
		sprintf(buffer, "N Divider Exact: %.5f\r\n", N_divider_exact);
		Serial.print(buffer);
		sprintf(buffer, "PLL_N (Integer): %d\r\n", PLL_N);
		Serial.print(buffer);
		sprintf(buffer, "N Fractional: %.5f\r\n", N_fractional);
		Serial.print(buffer);
		sprintf(buffer, "PLL_NUM: %d (0x%04X)\r\n", PLL_NUM, PLL_NUM);
		Serial.print(buffer);
		sprintf(buffer, "PLL_DEN: %d (0x%04X)\r\n", PLL_DEN, PLL_DEN);
		Serial.print(buffer);
		sprintf(buffer, "Cikis Bolucu: %d\r\n", cikis_bolucu);
		Serial.print(buffer);
    Serial.print("\r\n");
  */
    delay(50);
    // Check Lock Detect
     return isPLL_Locked();
  
}

bool isPLL_Locked() {
    delay(1);
    uint16_t read_val = PLL_read(0x4A);
    //printf("Reg 0x4A: Read back 0x%04X\r\n", read_val);
    uint8_t rb_LD = (read_val >> 14) & 0x03;
    
    if (rb_LD == 0b10) {
        //printf("PLL LOCKED\n");
        return true;
    } else {
        //printf("PLL NOT LOCKED (rb_LD = %u)\n", rb_LD);
        return false;
    }
    
}



uint16_t PLL_read(uint8_t address) {
    address |= 0x80;  // read operation
    uint8_t spi_buf[2];

    digitalWrite(PLL_CS, LOW);  // pull the pin low
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(&address, 1);       // send address
    SPI.transfer(spi_buf, 2);        // receive 2 bytes data
    SPI.endTransaction();
    digitalWrite(PLL_CS, HIGH);  // pull the pin high

    return (uint16_t)((spi_buf[0] << 8) | spi_buf[1]);
}

// PLL_write fonksiyonu
void PLL_write(uint8_t address, uint16_t Data) {
    address &= 0x7F;  // write operation
    uint8_t spi_buf[2];
    spi_buf[1] = lowByte(Data);
    spi_buf[0] = highByte(Data);

    digitalWrite(PLL_CS, LOW);  // pull the pin low
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(&address, 1);       // send address
    SPI.transfer(spi_buf, 2);        // send data
    SPI.endTransaction();
    digitalWrite(PLL_CS, HIGH);  // pull the pin high
}



void InitPLL()
{

  SPI.begin(SCLK_, MISO_, MOSI_, PLL_CS);
  pinMode(PLL_CS, OUTPUT);
  digitalWrite(PLL_CS, HIGH); 

	// Test for SPI accessibility Test (address 63 used as a test address)
	PLL_write(63,0x0000); 				// Test Write PLL
	uint16_t dumy1_0x0000 = PLL_read(63); 	// Test Read PLL
	SetPLLToDefault(); // Sets all regs to default value.

	uint16_t dumy1_0xC350 = PLL_read(63); 	// Test Read PLL
	//
	if (dumy1_0xC350==0xC350 && dumy1_0x0000 ==0x0000)
	{
		Serial.println("PLL Initialization Successful.\r\n");
	}
	else
	{
		Serial.println("PLL Initialization Failed!!!\r\n");
	}

}

void SetPLLToDefault()
{
	  uint16_t Regs1[123];

    Regs1[112]  = 0xFFFF;
    Regs1[110]  = 0x001F;
    Regs1[105]  = 0x000A;
    Regs1[104]  = 0x0014;
    Regs1[103]  = 0x0014;
    Regs1[102]  = 0x0028;
    Regs1[101]  = 0x03E8;
    Regs1[100]  = 0x0533;
    Regs1[99]  = 0x1989;
    Regs1[98]  = 0x1C80;
    Regs1[96]  = 0x17F8;
    Regs1[93]  = 0x1000;
    Regs1[88]  = 0x03FF;
    Regs1[87]  = 0xFF00;
    Regs1[86]  = 0x0040;
    Regs1[84]  = 0x0040;
    Regs1[83]  = 0x0F00;


	  Regs1[80]  = 0x01C0;
	  Regs1[79]  = 0x0110;
	  Regs1[78]  = 0x0000;
	  Regs1[77]  = 0x0608;
	  Regs1[76]  = 0x0000;
	  Regs1[75]  = 0x0000; // 
	  Regs1[74]  = 0x0000; // 
	  Regs1[73]  = 0x0000; //
	  Regs1[72]  = 0x0000; //
	  Regs1[71]  = 0x0000; //
	  Regs1[70]  = 0x000E;
	  Regs1[69]  = 0x0011;
	  Regs1[68]  = 0x0020;
	  Regs1[67]  = 0x1000;
	  Regs1[66]  = 0x003F;
	  Regs1[65]  = 0x0000;
	  Regs1[64]  = 0x0080;
	  Regs1[63]  = 0xC350;
	  Regs1[62]  = 0x0000;
	  Regs1[61]  = 0x03E8;
	  Regs1[60]  = 0x01F4;
	  Regs1[59]  = 0x1388;
	  Regs1[58]  = 0x0000;
	  Regs1[57]  = 0x0001;
	  Regs1[56]  = 0x0001;
	  Regs1[55]  = 0x0002;
	  Regs1[54]  = 0x0000;
	  Regs1[53]  = 0x0000;
	  Regs1[52]  = 0x0000;
	  Regs1[51]  = 0x203F;
	  Regs1[50]  = 0x0080;
	  Regs1[49]  = 0x0000;
	  Regs1[48]  = 0x4180;
	  Regs1[47]  = 0x0300;
	  Regs1[46]  = 0x0300;
	  Regs1[45]  = 0x0000;
	  Regs1[44]  = 0x0000;  // 
	  Regs1[43]  = 0x01F4;
	  Regs1[42]  = 0x0000;
	  Regs1[41]  = 0x0000;
	  Regs1[40]  = 0x0000;
	  Regs1[39]  = 0x03E8;
	  Regs1[38]  = 0x0000;
	  Regs1[37]  = 0x0500;
	  Regs1[36]  = 0x0034;
	  Regs1[35]  = 0x3100;
	  Regs1[34]  = 0x0010;
	  Regs1[33]  = 0x0000;
	  Regs1[32]  = 0x1001;
	  Regs1[31]  = 0x0401;
	  Regs1[30]  = 0xB18C;
	  Regs1[29]  = 0x318C;
	  Regs1[28]  = 0x0639;
	  Regs1[27]  = 0x8001;
	  Regs1[26]  = 0x0DB0;
	  Regs1[25]  = 0x0624;
	  Regs1[24]  = 0x0E34;
	  Regs1[23]  = 0x1102;
	  Regs1[22]  = 0xE2BF;
	  Regs1[21]  = 0x1C64;
	  Regs1[20]  = 0x272C;
	  Regs1[19]  = 0x2120;
	  Regs1[18]  = 0x0000;
	  Regs1[17]  = 0x15C0;
	  Regs1[16]  = 0x171C;
	  Regs1[15]  = 0x2001;
	  Regs1[14]  = 0x3001;
	  Regs1[13]  = 0x0038;
	  Regs1[12]  = 0x0408;
	  Regs1[11]  = 0x0612;
	  Regs1[10]  = 0x0800;
	  Regs1[9]   = 0x0005;
	  Regs1[8]   = 0xC802;
	  Regs1[7]   = 0x0000;
	  Regs1[6]   = 0x0A43;
	  Regs1[5]   = 0x0032;
	  Regs1[4]   = 0x4204;
	  Regs1[3]   = 0x0041;
	  Regs1[2]   = 0x81F4;
	  Regs1[1]   = 0x57A0;
	  Regs1[0]   = 0x6470;


	  for (int i = 122; i >= 0; --i) {

			PLL_write(i,Regs1[i]); //  descending order.
	    delay(1);
	  }

}

 

void DumpPLLRegisters() {
  Serial.println("------ PLL Register Dump ------");
  for (int i = 122; i >= 0; --i) {
    uint16_t value = PLL_read(i);
    Serial.print("Reg[");
    if (i < 10) Serial.print("0");  // Tek haneli adresler için ön sıfır
    Serial.print(i);
    Serial.print("] = 0x");
    if (value < 0x1000) Serial.print("0"); // Görsel hizalama için
    if (value < 0x100) Serial.print("0");
    if (value < 0x10) Serial.print("0");
    Serial.println(value, HEX);
  }
  Serial.println("-------------------------------");
}

