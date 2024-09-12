// 5P49V59xx and CH552 based MiniOCXO
// Copyright 2023- PCS Electronics

// Arduino environment
// Install the board as described here: https://github.com/DeqingSun/ch55xduino

// https://sourceforge.net/p/sdcc/bugs/3569/

// Tools - Board - CH5xx boards - CH552
// Tools - USB settings - Default CDC
// Tools - Upload method - USB
// Tools - Clock source - 24 MHz internal

// Updating the firmware:
// Install and run the WCHISPTool from http://www.wch-ic.com/downloads/WCHISPTool_Setup_exe.html
// Tab: CH55x Series
// Chip model: CH552, download type: USB
// User file: select the HEX file with a firmware.
// Connect the PROG jumper, cycle the power (not the USB port!), remove the PROG jumper
// Check the Device Manager: under "Interface" branch, there should be a "USB module" device.
// If there is an "Unknown device", go to the WCHISPTool folder and install the driver.
// In the WCHISPTool, click Search and make sure the "MCU model: CH552" appeared in the list.
// Click Download
// Cycle the power
// Now the Device manager will list a new "USB Serial Port" under "Ports (COM & LPT".

// Alternatively, instead of WCHISPTool, use vnproch55x :
// vnproch55x firmware.hex -r2

//#define HW_V10 // Hardware version 1.0
//#define HW_V11 // Hardware version 1.1
#define HW_V13 // Hardware version 1.3

#ifdef HW_V10
#define HW_VER "1.0"
#endif

#ifdef HW_V11
#define HW_VER "1.1"
#endif

#ifdef HW_V13
#define HW_VER "1.3"
#endif

#include <string.h>

#include <SoftI2C.h>

#define I2C_ADDR 0x6A


// Globals

#define F_OCXO_HZ 10000000
#define F_OCXO_HZ_MIN 1000000
#define F_OCXO_HZ_MAX 200000000

#define F_VCO_HZ_MIN 2600000000
#define F_VCO_HZ_MAX 2900000000

#define NPROFILES 2
uint8_t current_profile;
__xdata uint32_t f_ocxo_hz;

__xdata uint32_t tmp2_uint32;
__xdata uint32_t tmp1_uint32;


// 5P49V5901, exported from IDT Timing commander

const uint8_t prog_array[] = { 
// 0x00
0x61, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x00, 0xB6, 0xB4, 0x92, 0x40, 0x2D, 0x81, 0x82, 0x00, 0x03, 0x84, 
0x10, // 0x17 Feedback divider integer
0xE0, // 0x18 Feedback divider integer
0x00, // 0x19 Feedback divider fraction
0x00, // 0x1A Feedback divider fraction
0x00, // 0x1B Feedback divider fraction
0x9F, 0xFD, 0xC8, 0x80,
// 0x20
0x00, 0x81, 
0x03, // 0x22 OD1 fraction 
0x00, // 0x23 OD1 fraction
0x00, // 0x24 OD1 fraction 
0x00, // 0x25 OD1 fraction 
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 
0x02, // 0x2D OD1 integer 
0x10, // 0x2E OD1 integer 
0x00, 0x00, 0x81, 
0x00, // 0x32 OD2 fraction 
0x00, // 0x33 OD2 fraction 
0x00, // 0x34 OD2 fraction 
0x00, // 0x35 OD2 fraction 
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 
0x03, // 0x3D OD2 integer 
0x60, // 0x3E OD2 integer 
0x00,
// 0x40
0x00, 0x81, 
0x01, // 0x42 OD3 fraction 
0x00, // 0x43 OD3 fraction 
0x00, // 0x44 OD3 fraction 
0x00, // 0x45 OD3 fraction 
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 
0x03, // 0x4D OD3 integer 
0x80, // 0x4E OD3 integer
0x00, 0x00, 0x81, 
0x03, // 0x52 OD4 fraction  
0x80, // 0x53 OD4 fraction  
0x00, // 0x54 OD4 fraction  
0x00, // 0x55 OD4 fraction  
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 
0x02, // 0x5D OD4 integer 
0xE0, // 0x5E OD4 integer 
0x00,
// 0x60
0x3B, 0x01, // 0x60, 0x61 - Clock1 output configuration
0x3B, 0x01, // 0x62, 0x63 - Clock2 output configuration 
0x3B, 0x01, // 0x64, 0x65 - Clock3 output configuration
0x3B, 0x01, // 0x66, 0x67 - Clock4 output configuration
0xFF, 0xFC
  
};

#define ARR_SIZE sizeof(prog_array)/sizeof(uint8_t)

#ifdef HW_V10 
#define LED_OUT 34           // PWM LED
#define BUTTON_IN !!!        // Button input
#else
#define LED_OUT 15           // PWM LED
#define BUTTON_IN 14        // Button input
#endif  


// PLL

void init_synth()
{
#ifdef HW_V10 
  scl_pin = 11; //extern variable in SoftI2C.h
  sda_pin = 31;
#else
  scl_pin = 17; //extern variable in SoftI2C.h
  sda_pin = 16;
#endif  
 
  I2CInit();

}

void scan_i2c()
{
  uint8_t ack_bit;

  USBSerial_println("Scanning I2C...");

  for (uint8_t i = 0; i < 128; i++) {
    I2CStart();
    ack_bit = I2CSend(i << 1 | 1); //last bit is r(1)/w(0).
    I2CStop();
    delay(1);
    if (ack_bit == 0) {
      USBSerial_print("Received ACK from 0x");
      USBSerial_print(i, HEX);
      if (i==0x6A)
        USBSerial_print(" (5P49V59xx)");      
      USBSerial_println();
    }
  }
  
  USBSerial_println("Done.");
  USBSerial_flush();
}

//#define LOAD_DEBUG

#define EEPROM_BLOCK_START 7 // Start address of first block
#define EEPROM_BLOCK_SIZE 47 // Bytes per block for each profile

uint8_t get_data_from_array(uint8_t addr)
{

  uint8_t p = EEPROM_BLOCK_START + (current_profile-1)*EEPROM_BLOCK_SIZE;
 
  switch(addr)
  {
    case 0x17: // Feedback divider integer  
      return eeprom_read_byte(p+0);
    case 0x18: // Feedback divider integer  
      return eeprom_read_byte(p+1);
    case 0x19: // Feedback divider fraction
      return eeprom_read_byte(p+2);
    case 0x1A: // Feedback divider fraction
      return eeprom_read_byte(p+3);
    case 0x1B: // Feedback divider fraction
      return eeprom_read_byte(p+4);

    case 0x22: //  OD1 fraction 
      return eeprom_read_byte(p+5);
    case 0x23: //  OD1 fraction 
      return eeprom_read_byte(p+6);
    case 0x24: //  OD1 fraction 
      return eeprom_read_byte(p+7);
    case 0x25: //  OD1 fraction 
      return eeprom_read_byte(p+8);
    case 0x2D: // OD1 integer 
      return eeprom_read_byte(p+9);
    case 0x2E: // OD1 integer 
      return eeprom_read_byte(p+10);
    
    case 0x32: //  OD2 fraction 
      return eeprom_read_byte(p+11);
    case 0x33: //  OD2 fraction 
      return eeprom_read_byte(p+12);
    case 0x34: //  OD2 fraction 
      return eeprom_read_byte(p+13);
    case 0x35: //  OD2 fraction 
      return eeprom_read_byte(p+14);
    case 0x3D: // OD2 integer 
      return eeprom_read_byte(p+15);
    case 0x3E: // OD2 integer 
      return eeprom_read_byte(p+16);
    
    case 0x42: //  OD3 fraction 
      return eeprom_read_byte(p+17);
    case 0x43: //  OD3 fraction 
      return eeprom_read_byte(p+18);
    case 0x44: //  OD3 fraction 
      return eeprom_read_byte(p+19);
    case 0x45: //  OD3 fraction 
      return eeprom_read_byte(p+20);
    case 0x4D: // OD3 integer 
      return eeprom_read_byte(p+21);
    case 0x4E: // OD3 integer 
      return eeprom_read_byte(p+22);
    
    case 0x52: //  OD4 fraction 
      return eeprom_read_byte(p+23);
    case 0x53: //  OD4 fraction 
      return eeprom_read_byte(p+24);
    case 0x54: //  OD4 fraction 
      return eeprom_read_byte(p+25);
    case 0x55: //  OD4 fraction 
      return eeprom_read_byte(p+26);
    case 0x5D: // OD4 integer 
      return eeprom_read_byte(p+27);
    case 0x5E: // OD4 integer 
      return eeprom_read_byte(p+28);

    case 0x60: // Clock output configuration 
      return eeprom_read_byte(p+29);
    case 0x61: // Clock output configuration 
      return eeprom_read_byte(p+30);
   
  };

  return prog_array[addr];
}

void load()
{
  USBSerial_print("Loading registers to the PLL chip...");
  USBSerial_flush();    

  uint8_t addr;
  uint8_t data;

  uint8_t ack_bit;

  // Program registers

  I2CStart();
  
  // Send address
  ack_bit = I2CSend(I2C_ADDR << 1); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E1");  
#endif      
  ack_bit = I2CSend(0); // Start register address
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E2");  
#endif      

  // Send data
  for (addr=0; addr<ARR_SIZE; addr++)
  {
    ack_bit = I2CSend(get_data_from_array(addr)); 
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E3");  
#endif      
    delay(1);
  }
  I2CStop();
  delay(1);

  // Verify registers


  I2CStart();
  
  // Send address
  ack_bit = I2CSend( (I2C_ADDR << 1)); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E4");  
#endif      
  ack_bit = I2CSend(0); // Start register address
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E5");  
#endif      

//  delay(1);

  I2CRestart();

  ack_bit = I2CSend( I2C_ADDR << 1 | 1); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E6");  
#endif      

  // Read data
  for (addr=0; addr<ARR_SIZE; addr++)
  {
    data  = I2CRead(); 
    I2CAck(); //  Send NAK for the last byte???
    
    if (data != get_data_from_array(addr))
    {
#ifdef LOAD_DEBUG
      USBSerial_print("Err: ");
      USBSerial_println(addr, HEX);
#endif      
    }
    delay(1);
  }
  I2CStop();
  
  delay(1000);


  // Read register 0x76

  I2CStart();
  
  // Send address
  ack_bit = I2CSend( (I2C_ADDR << 1)); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E4-1");  
#endif      
  ack_bit = I2CSend(0x76); // Start register address
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E5-1");  
#endif      

  delay(1);

  I2CRestart();

  delay(1);

  ack_bit = I2CSend( I2C_ADDR << 1 | 1); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E6-1");  
#endif      

  data  = I2CRead(); 
  I2CNak(); // 

#ifdef LOAD_DEBUG
  USBSerial_print("Reg 0x76 = ");  
  USBSerial_println(data, HEX);  
#endif      

  I2CStop();
  
  delay(1000);

  // Perform I2C reset via register 0x76

  I2CStart();
  
  // Send address
  ack_bit = I2CSend(I2C_ADDR << 1); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E1-3");  
#endif      
  ack_bit = I2CSend(0x76); // Start register address
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E2-3");  
#endif      

  ack_bit = I2CSend(data & ~0x20); 
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E3-3");  
#endif      
    delay(1);

  I2CStop();
  delay(100);
  
  
  I2CStart();
  
  // Send address
  ack_bit = I2CSend(I2C_ADDR << 1); //last bit is r(1)/w(0).
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E1-4");  
#endif      
  ack_bit = I2CSend(0x76); // Start register address
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E2-4");  
#endif      

  ack_bit = I2CSend(data | 0x20); 
#ifdef LOAD_DEBUG
  if (ack_bit != 0)
      USBSerial_println("E3-4");  
#endif      
    delay(1);

  I2CStop();
  delay(1);

  USBSerial_println("Done.");
  USBSerial_flush();    
}

void print_data()
{
    USBSerial_print(
        "-----------------------------\n"
        "OCXO HW: " HW_VER " SW: " __DATE__ "\n"
        "? for help\n"
        "fq <output> <Hz> set frequency for output 1,2 or 3\n"
        "pro <n> - select current profile, 1 or 2\n"
        "ocxo <Hz> - select OCXO frequency (1 to 200 MHz)\n"
        "mult <n> - set VCO multiplier (VCO is 2.6 to 2.9 GHz)\n"
        "load - load registers into the PLL chip\n"
        "dump - dump DataFlash\n"
        "erase - erase DataFlash\n"
        "poke <addr_hex> <data_hex> - poke DataFlash\n"
        "scan - scan I2C bus\n"
        "fact - reset to factory defaults\n"    
    );
}

void print_saved_vars()
{

  USBSerial_print("-----------------------------\n");
  
  USBSerial_print("Current profile: ");
  USBSerial_println(current_profile);

  USBSerial_print("OCXO frequency, Hz: ");
  USBSerial_println(f_ocxo_hz);

  USBSerial_print("VCO multiplier: ");

  uint8_t p = EEPROM_BLOCK_START + (current_profile-1)*EEPROM_BLOCK_SIZE;
  uint16_t vco_mult_int = (((uint16_t)eeprom_read_byte(p+0))<<4) | (((uint16_t)eeprom_read_byte(p+1))>>4);       

  USBSerial_println(vco_mult_int);
/*
  
  USBSerial_print("; ");


  uint32_t vco_fq_hz = vco_mult_int * f_ocxo_hz;
  
  USBSerial_print("VCO frequency, Hz: ");
  USBSerial_print(vco_fq_hz);
//  if (eeprom_read_byte(9) || eeprom_read_byte(10) || eeprom_read_byte(11))
//    USBSerial_print(" (fractional part not shown" );
  if ( (vco_fq_hz<F_VCO_HZ_MIN) || (vco_fq_hz>F_VCO_HZ_MAX) )
    USBSerial_print(" !!! Out of range !!!");
  USBSerial_println_only();
*/

  USBSerial_println("Output frequencies, Hz: ");
  for (int n=1; n<=3; n++)
  {
    USBSerial_print(n);
    USBSerial_print(": ");
    uint32_t fq = (((uint32_t)eeprom_read_byte(p+31))<<24) | (((uint32_t)eeprom_read_byte(p+32))<<16) |  (((uint32_t)eeprom_read_byte(p+33))<<8) | ((uint32_t)eeprom_read_byte(p+34));
    USBSerial_print(fq);
    USBSerial_print(" ");
    p+= 4;
  }
  USBSerial_println_only();
}


// Dataflash

#define SIG_0 0x23
#define SIG_1 0x9B

void init_dataflash(bool need_reset)
{
  if ( (eeprom_read_byte(0)!=SIG_0) || 
       (eeprom_read_byte(1)!=SIG_1) ||
       need_reset )
  { // Reset and write all
    eeprom_write_byte(0, SIG_0);
    eeprom_write_byte(1, SIG_1);
    
    eeprom_write_byte(2, 1); // Initially, current profile is 1

    eeprom_write_byte(3, F_OCXO_HZ>>24); eeprom_write_byte(4, F_OCXO_HZ>>16); eeprom_write_byte(5, F_OCXO_HZ>>8); eeprom_write_byte(6, F_OCXO_HZ);

    uint8_t e = EEPROM_BLOCK_START;
    
    for (uint8_t p=1; p<=NPROFILES; p++) // All profiles
    {

      eeprom_write_byte(e++, prog_array[0x17]); // Feedback divider integer
      eeprom_write_byte(e++, prog_array[0x18]); // Feedback divider integer
      eeprom_write_byte(e++, prog_array[0x19]); // Feedback divider fraction
      eeprom_write_byte(e++, prog_array[0x1A]); // Feedback divider fraction
      eeprom_write_byte(e++, prog_array[0x1B]); // Feedback divider fraction

      eeprom_write_byte(e++, prog_array[0x22]); // OD1 fraction
      eeprom_write_byte(e++, prog_array[0x23]); // OD1 fraction
      eeprom_write_byte(e++, prog_array[0x24]); // OD1 fraction
      eeprom_write_byte(e++, prog_array[0x25]); // OD1 fraction
      eeprom_write_byte(e++, prog_array[0x2D]); //  OD1 integer
      eeprom_write_byte(e++, prog_array[0x2E]); //  OD1 integer

      eeprom_write_byte(e++, prog_array[0x32]); // OD2 fraction
      eeprom_write_byte(e++, prog_array[0x33]); // OD2 fraction
      eeprom_write_byte(e++, prog_array[0x34]); // OD2 fraction
      eeprom_write_byte(e++, prog_array[0x35]); // OD2 fraction
      eeprom_write_byte(e++, prog_array[0x3D]); //  OD2 integer
      eeprom_write_byte(e++, prog_array[0x3E]); //  OD2 integer

      eeprom_write_byte(e++, prog_array[0x42]); // OD3 fraction
      eeprom_write_byte(e++, prog_array[0x43]); // OD3 fraction
      eeprom_write_byte(e++, prog_array[0x44]); // OD3 fraction
      eeprom_write_byte(e++, prog_array[0x45]); // OD3 fraction
      eeprom_write_byte(e++, prog_array[0x4D]); //  OD3 integer
      eeprom_write_byte(e++, prog_array[0x4E]); //  OD3 integer

      eeprom_write_byte(e++, prog_array[0x52]); // OD4 fraction
      eeprom_write_byte(e++, prog_array[0x53]); // OD4 fraction
      eeprom_write_byte(e++, prog_array[0x54]); // OD4 fraction
      eeprom_write_byte(e++, prog_array[0x55]); // OD4 fraction
      eeprom_write_byte(e++, prog_array[0x5D]); //  OD4 integer
      eeprom_write_byte(e++, prog_array[0x5E]); //  OD4 integer

      eeprom_write_byte(e++, prog_array[0x60]); //  Clock output configuration, 1st byte: same for all outputs
      eeprom_write_byte(e++, prog_array[0x61]); //  Clock output configuration, 2nd byte: same for all outputs

#define DEFAILT_F1  40000000
#define DEFAILT_F2  25000000
#define DEFAILT_F3  24000000
#define DEFAILT_F4  28800000

    eeprom_write_byte(e++, DEFAILT_F1>>24); 
    eeprom_write_byte(e++, DEFAILT_F1>>16); 
    eeprom_write_byte(e++, DEFAILT_F1>>8); 
    eeprom_write_byte(e++, DEFAILT_F1);

    eeprom_write_byte(e++, DEFAILT_F2>>24); 
    eeprom_write_byte(e++, DEFAILT_F2>>16); 
    eeprom_write_byte(e++, DEFAILT_F2>>8); 
    eeprom_write_byte(e++, DEFAILT_F2);

    eeprom_write_byte(e++, DEFAILT_F3>>24); 
    eeprom_write_byte(e++, DEFAILT_F3>>16); 
    eeprom_write_byte(e++, DEFAILT_F3>>8); 
    eeprom_write_byte(e++, DEFAILT_F3);

    eeprom_write_byte(e++, DEFAILT_F4>>24); 
    eeprom_write_byte(e++, DEFAILT_F4>>16); 
    eeprom_write_byte(e++, DEFAILT_F4>>8); 
    eeprom_write_byte(e++, DEFAILT_F4);
    }
  }
  // Read
  
  current_profile = eeprom_read_byte(2);
  f_ocxo_hz = (((uint32_t)eeprom_read_byte(3))<<24) | (((uint32_t)eeprom_read_byte(4))<<16) |  (((uint32_t)eeprom_read_byte(5))<<8) | ((uint32_t)eeprom_read_byte(6));
}

void dumpEEPROM() 
{
  USBSerial_println("DataFlash dump:");
  for (uint8_t i = 0; i < 128; i++) {
    uint8_t eepromData = eeprom_read_byte(i);
    if (eepromData < 0x10) USBSerial_print((char)'0');
    USBSerial_print(eepromData, HEX);
    if ((i & 15) == 15) 
      USBSerial_println_only();
    else
      USBSerial_print((char)',');
  }
  USBSerial_println("Done.");
  USBSerial_flush();
}

// Main setup routine

void setup() 
{
  init_synth();
  pinMode(LED_OUT, OUTPUT);
  digitalWrite(LED_OUT, LOW);

  pinMode(BUTTON_IN, INPUT_PULLUP);


  init_dataflash(false);

//  load();

  delay(2000);
  print_data();
  print_saved_vars();

}

// Command interpreter

#define MAX_COMMAND_LEN 20
__xdata char char_cmd[MAX_COMMAND_LEN];
uint8_t char_cmd_len = 0;

void ans_ok()
{
  USBSerial_println("OK");
}

void ans_err()
{
  USBSerial_println("ERR");
}

void cmd_interpret()
{
  if ( ( char_cmd_len == 0 ) ||
       ((char_cmd_len == 1) && (!strncmp(char_cmd, "?",1))) ||
       ((char_cmd_len == 4) && (!strncmp(char_cmd, "help",4))) )
  {
    print_data();      
    print_saved_vars();    
  }
  else 
  if ( (char_cmd_len >= 4) && (!strncmp(char_cmd, "scan", 4)))
  {
    scan_i2c();
  }
  else
  if ( (char_cmd_len >= 4) && (!strncmp(char_cmd, "dump", 4)))
  {
    dumpEEPROM();
  }
  else
  if ( (char_cmd_len >= 4) && (!strncmp(char_cmd, "fact", 4)))
  {
    init_dataflash(true);
    ans_ok();
  }
  else
  if ( (char_cmd_len >= 5) && (!strncmp(char_cmd, "erase", 5)))
  {
    for (uint8_t p=0; p<128; p++) // Erase all dataflash
      eeprom_write_byte(p, 0xFF); 

    ans_ok();
  }
  else
  if ( (char_cmd_len >= 4) && (!strncmp(char_cmd, "load", 4)))
  {
    load();
  }
  else
  if ( (char_cmd_len > 8) && (!strncmp(char_cmd, "poke", 4)))
  {
    char *endptr;
    uint8_t addr = strtoul(char_cmd+4, &endptr, 16);
    if (endptr)
    {
      uint8_t data = strtoul(endptr, endptr, 16);
      if (endptr)
      {
        addr &= 0b01111111;
        eeprom_write_byte(addr, data);
        ans_ok();
      }
      else
        ans_err();
    }
    else
      ans_err();
  }
  else
  if ( (char_cmd_len > 3) && (!strncmp(char_cmd, "pro", 3)))
  {
    uint8_t new_profile = atoi(char_cmd+3);
    
    if ((new_profile>=1) && (new_profile<=NPROFILES))
    {
      current_profile = new_profile;
      eeprom_write_byte(2, current_profile);
      ans_ok();
    }
    else
      ans_err();
  }
  else
  if ( (char_cmd_len > 4) && (!strncmp(char_cmd, "ocxo", 4)))
  {
    uint32_t new_ocxo_hz = atol(char_cmd+4);
    
    if ((new_ocxo_hz>=F_OCXO_HZ_MIN) && (new_ocxo_hz<=F_OCXO_HZ_MAX))
    {
      f_ocxo_hz = new_ocxo_hz;
      eeprom_write_byte(3, f_ocxo_hz>>24); eeprom_write_byte(4, f_ocxo_hz>>16); eeprom_write_byte(5, f_ocxo_hz>>8); eeprom_write_byte(6, f_ocxo_hz);

      uint8_t p = EEPROM_BLOCK_START + (current_profile-1)*EEPROM_BLOCK_SIZE;
      uint16_t vco_mult_int = (((uint16_t)eeprom_read_byte(p+0))<<4) | (((uint16_t)eeprom_read_byte(p+1))>>4);       

      uint32_t vco_fq_hz = new_ocxo_hz * vco_mult_int;

      if ( (vco_fq_hz<F_VCO_HZ_MIN) || (vco_fq_hz>F_VCO_HZ_MAX) )
      {
        ans_err();
        USBSerial_println("!!! VCO frequency out of range !!!");      
      }
      else
        ans_ok();
      
    }
    else
      ans_err();
    
  }
  else
  if ( (char_cmd_len > 4) && (!strncmp(char_cmd, "mult", 4)))
  {
    uint16_t new_vco_mult_int = atol(char_cmd+4);
    
    uint32_t vco_fq_hz = f_ocxo_hz * new_vco_mult_int;

    if ( (vco_fq_hz<F_VCO_HZ_MIN) || (vco_fq_hz>F_VCO_HZ_MAX) )
    {
      ans_err();
      USBSerial_println(" !!! VCO frequency out of range !!!");
    }
    else
    {

      uint8_t p = EEPROM_BLOCK_START + (current_profile-1)*EEPROM_BLOCK_SIZE;
      
      eeprom_write_byte(p+0, new_vco_mult_int >> 4); eeprom_write_byte(p+1, new_vco_mult_int<<4); eeprom_write_byte(p+2, 0);  eeprom_write_byte(p+3, 0);  eeprom_write_byte(p+4, 0); 

      ans_ok();
    }
  }
  else
  if ( (char_cmd_len > 4) && (!strncmp(char_cmd, "fq", 2)))
  {
    char *endptr;
    uint8_t chn = strtoul(char_cmd+2, &endptr, 10);
    if ( endptr && (chn>0) && (chn<5) )
    {
      uint32_t output_fq = strtoul(endptr, endptr, 10);
      if (endptr)
      {
        
        // Calculate the integer part of the divider

        uint8_t p = EEPROM_BLOCK_START + (current_profile-1)*EEPROM_BLOCK_SIZE;

        eeprom_write_byte(p+31+(chn-1)*4, output_fq>>24); 
        eeprom_write_byte(p+32+(chn-1)*4, output_fq>>16); 
        eeprom_write_byte(p+33+(chn-1)*4, output_fq>>8); 
        eeprom_write_byte(p+34+(chn-1)*4, output_fq); 
        
        uint16_t vco_mult_int = (((uint16_t)eeprom_read_byte(p+0))<<4) | (((uint16_t)eeprom_read_byte(p+1))>>4);       
        
        tmp1_uint32 = vco_mult_int * f_ocxo_hz; 

        tmp1_uint32 /= 2; // Dividend
  
        unsigned char count = 32;
        uint8_t c;

        tmp2_uint32 = 0; // Remainder

        do
        {
//      #define MSB_SET(x) ((x >> (8*sizeof(x)-1)) & 1)
  
        #define MSB_SET(x) ((x >> 24) & 0x80)
      
          // tmp2_uint32: x <- 0;
          c = MSB_SET(tmp1_uint32);
          tmp1_uint32 <<= 1; // Dividend
          tmp2_uint32 <<= 1; // Remainder
          if (c)
            tmp2_uint32 |= 1L; // Remainder
      
          if (tmp2_uint32 >= output_fq) // If the remainder is more then divisor
          {
            tmp2_uint32 -= output_fq;
            // x <- (result = 1)
            tmp1_uint32 |= 1; // Now it is quotient
          }
      
        }
        while (--count);

        // Now:
        // tmp1_uint32 - quotient
        // tmp2_uint32 - remainder

        chn = p + (chn-1)*6;

        eeprom_write_byte(9+chn, tmp1_uint32>>4); //  ODn integer
        eeprom_write_byte(10+chn, tmp1_uint32<<4); //  ODn integer
        
        // Calculate the fractional part of the divider
        
        tmp1_uint32 = 0; // Now quotient
        // tmp2_uint32 is dividend

        count = 32;

        do
        {
          tmp2_uint32 <<= 1;
          tmp1_uint32 <<= 1;

          if (tmp2_uint32 >= output_fq) 
          {
            tmp2_uint32 -= output_fq;
            tmp1_uint32 |= 1;
          }
        
      
        }
        while (--count);

        eeprom_write_byte(5+chn, tmp1_uint32>>30); // ODn fraction
        eeprom_write_byte(6+chn, tmp1_uint32>>22); // ODn fraction
        eeprom_write_byte(7+chn, tmp1_uint32>>14); // ODn fraction
        eeprom_write_byte(8+chn, (tmp1_uint32>>6) & 0b11111100); // ODn fraction
              
        ans_ok();
      }
      else
        ans_err();
    }
    else
      ans_err();

  }
  else
    ans_err();
}

static void char_interpret()
{
  while (USBSerial_available())
  {
    char c = USBSerial_read();

    if ( (c == '\n') || (c == '\r') )
    {
      char_cmd[char_cmd_len] = 0;

      // Interpret

      cmd_interpret();

      char_cmd_len = 0;
    }
    else
    {
      if (char_cmd_len < MAX_COMMAND_LEN)
      {
        char_cmd[char_cmd_len] = c;
        char_cmd_len++;
      }
    }
    
  }  
}

bool indicate_and_load = true;

void loop() 
{

  if (!digitalRead(BUTTON_IN)) // Read the button
  {
    digitalWrite(LED_OUT, LOW);

    uint8_t i;
    for (i=0; i<50; i++)
    {
      if (digitalRead(BUTTON_IN))
        break;
      delay(10);
    }

    if (i>49)
    {

//      while (!digitalRead(BUTTON_IN)) // Wait for key release
//        ;   
      
//      for (i=0; i<100; i++) // Pause
//      {
//        delay(10);
//      }

      current_profile++;
      if (current_profile>NPROFILES)
        current_profile = 1;

      indicate_and_load = true;  
    }
    else    
      digitalWrite(LED_OUT, HIGH);

  }

  if (indicate_and_load)
  {
    indicate_and_load = false;

    for (uint32_t i=0; i<current_profile; i++)
    {
        digitalWrite(LED_OUT, HIGH);
        delay(500);
        digitalWrite(LED_OUT, LOW);
        delay(500);
    }
  
    load();

    while (!digitalRead(BUTTON_IN)) // Wait for key release
      ;   
    
    digitalWrite(LED_OUT, HIGH);
  }

  char_interpret();

}
