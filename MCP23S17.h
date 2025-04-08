
#ifndef _MCP23S17_H
#define _MCP23S17_H

#include <stdint.h>

// SPI Configuration
#define IO1_CS          10   // Chip Select Pin for IO Expander
#define SCLK_           12   // SPI Clock Pin
#define MOSI_           11   // SPI MOSI Pin
#define MISO_           13   // SPI MISO Pin


 
#define WRITE_CMD 0
#define READ_CMD 1

// Register addresses
#define IODIRA 0x00  // I/O direction A
#define IODIRB 0x01  // I/O direction B
#define IPOLA 0x02  // I/O polarity A
#define IPOLB 0x03  // I/O polarity B
#define GPINTENA 0x04  // interupt enable A
#define GPINTENB 0x05  // interupt enable B
#define DEFVALA 0x06  // register default value A (interupts)
#define DEFVALB 0x07  // register default value B (interupts)
#define INTCONA 0x08  // interupt control A
#define INTCONB 0x09  // interupt control B
#define IOCONA 0x0A  // I/O config
#define IOCONB 0x0B  // I/O config
#define GPPUA 0x0C  // port A pullups
#define GPPUB 0x0D  // port B pullups
#define INTFA 0x0E  // interupt flag A (where the interupt came from)
#define INTFB 0x0F  // interupt flag B
#define INTCAPA 0x10  // interupt capture A (value at interupt is saved here)
#define INTCAPB 0x11  // interupt capture B
#define GPIOA_ 0x12  // port A
#define GPIOB_ 0x13  // port B
#define OLATA 0x14  // output latch A
#define OLATB 0x15  // output latch B

// I/O config
#define BANK_OFF 0x00  // addressing mode
#define BANK_ON 0x80
#define INT_MIRROR_ON 0x40  // interupt mirror (INTa|INTb)
#define INT_MIRROR_OFF 0x00
#define SEQOP_OFF 0x20  // incrementing address pointer
#define SEQOP_ON 0x00
#define DISSLW_ON 0x10  // slew rate
#define DISSLW_OFF 0x00
#define HAEN_ON 0x08  // hardware addressing
#define HAEN_OFF 0x00
#define ODR_ON 0x04  // open drain for interupts
#define ODR_OFF 0x00
#define INTPOL_HIGH 0x02  // interupt polarity
#define INTPOL_LOW 0x00


uint8_t mcp23s17_read_reg(uint8_t reg, uint8_t hw_addr, int fd);
void mcp23s17_write_reg(uint8_t data, uint8_t reg, uint8_t hw_addr, int fd);
uint8_t IO_EXP1_read (uint8_t address);
void IO_EXP1_Init ();
void IO_EXP1_DumpAllRegisters();
void IO_EXP1_write (uint8_t address, uint8_t Data);
void SetIOExpander();
#endif
