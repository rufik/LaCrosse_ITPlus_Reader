// Tested with JeeLink v3 (2012-02-11)
// polling RFM12B to decode FSK iT+ with a JeeNode/JeeLink from Jeelabs.
// device  : iT+ TX29IT 04/2010 v36 D1     LA Crosse Technology (c) Europpe
//         : iT+ TX29DTH-IT 08/2008 v12 D1 LA Crosse Technology (c) Europe
//         : iT+ TX25U-IT                  LA Crosse Technology (c) USA
// info    : http://forum.jeelabs.net/node/110
//           http://fredboboss.free.fr/tx29/tx29_sw.php
//           http://www.f6fbb.org/domo/sensors/
//           http://www.mikrocontroller.net/topic/67273 benedikt.k org
//           rinie,marf,joop 1 nov 2011
//           slightly modified by Rufik (r.markiewicz@gmail.com)
//
// Changelog:
//  2012-02-11: initial release 1.0
//

#include <JeeLib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define clrb(sfr, bit)     (_SFR_BYTE(sfr) &= ~_BV(bit)) 
#define setb(sfr, bit)     (_SFR_BYTE(sfr) |= _BV(bit)) 

#define RF_PORT	PORTB
#define RF_DDR	DDRB
#define RF_PIN	PINB

#define SDI 3              // polling  4 Pins ,3 SPI + Chipselect
#define SCK 5              // differs with Jee-node lib !
#define CS  2  
#define SDO 4 

#define LED_PIN     9     // activity LED, comment out to disable
#define CRC_POLY    0x31  // CRC-8 = 0x31 is for: x8 + x5 + x4 + 1
#define DEBUG       0     // set to 1 to see debug messages

// **********************************************************************

void loop ()
{
  unsigned int frame[5];
  //get message
  receive(frame);
  //check if valid for LaCrosse IT+ sensor
  if (is_mg_valid(frame[0])) {
    //check CRC
    if (is_crc_valid(frame[4])) {
      //calculate temp and humidity
      float temp = calculate_temp(frame[1], frame[2]);
      unsigned int hum = frame[3];
      //print into serial port
      String out = prepare_output_string(get_sensor_id(frame[0], frame[1]), temp, hum);
      Serial.println(out);
      Serial.flush();
    }
  }
  //simple delay, not really needed :)
  delay(100);
}


//receive message, data[] is length of 5
void receive(unsigned int *data)
{
  unsigned char msg[5];
  
  if (DEBUG) { Serial.println("Start receiving"); }

  rf12_rxdata(msg, 5);
  //just blink once
  activityLed(1);
  delay(70);
  activityLed(0);

  if (DEBUG) {
    Serial.print("End receiving, HEX raw data: ");
    for (int i=0; i<5; i++) {
      Serial.print(msg[i], HEX);
      Serial.print(" ");
    }
    Serial.println();  	
  }
  
  //rewrite into unsigned int table
  for (int i=0; i<5; i++) {
    data[i] = (unsigned int) msg[i];
  }
}


//prepares fixed-length output message
String prepare_output_string(unsigned int sensor_id, float temperature, unsigned int humidity) {
  char buffer[20];
  String output = "D:";
  String sid = String(sensor_id, HEX);
  if (temperature > 99.9) {
    temperature = 99.9;
  }
  sid.toUpperCase();
  dtostrf(temperature, 3, 1, buffer);
  String temp = String(buffer);
  //padding
  if (temp.length() < 5) {
    int i=temp.length();
    for (i; i<5; i++) {
      temp = " " + temp;
    }
  }
  if (humidity > 99) {
    humidity = 99;
  }
  String hum = String(humidity);
  //padding
  if (hum.length() < 2) {
    hum = " " + hum;
  }
  output += sid;
  output += ":";
  output += temp;
  output += ":";
  output += hum;
  
  return output;
}

static void activityLed (byte on) {
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !on);
#endif
}

static void ledBlink(unsigned int blinksCount) {
  if (blinksCount > 0) {
    if (blinksCount > 10) {
      blinksCount = 10; //max. 10 blinks are allowed
    }
    for (int i=0; i<blinksCount; i++) {
      activityLed(1);
      delay(50);
      activityLed(0);
      delay(50);
    }

  }
}

/*
* compute CRC for LaCrosse IT+ sensor
*/
static uint8_t compute_crc(uint8_t b) {
  uint8_t do_xor;
  uint8_t reg;

  reg = 0;
  do_xor = (reg & 0x80);

  reg <<=1;
  reg |= b;

  if (do_xor) {
    reg ^= CRC_POLY;
  }

  return reg;
}

//init
void setup () {
  Serial.begin(57600);
  if (DEBUG) {
    Serial.println("*** LaCrosse weather station wireless receiver for IT+ sensors ***");
  }
  rf12_la_init();
  if (DEBUG) {
    Serial.println("Radio setup complete. Starting to receive messages");
  }
  ledBlink(5);
}


//checks if CRC is OK
boolean is_crc_valid(unsigned int crcByte) {
  boolean result = false;
  
  uint8_t crc_computed = compute_crc((uint8_t) crcByte);
  if ((unsigned int) crc_computed == crcByte) {
    result = true;
    if (DEBUG) { Serial.print("CRC match: "); Serial.println(crc_computed, HEX); }
  } else {
    if (DEBUG) {
      Serial.print("CRC error! Computed is "); Serial.print(crc_computed, HEX);
      Serial.print(" but received is "); Serial.println(crcByte, HEX);
    }
    ledBlink(5); //blink 5 times if CRC is corrupted
  }
  return result;
}


//checks if msg starts with 9
boolean is_mg_valid(unsigned int msgFirstByte) {
  boolean result = false;

  unsigned int msgFlag = (msgFirstByte & 240) >> 4;
  if (msgFlag == 9) {
    result = true;
    if (DEBUG) { Serial.println("OK, msg starts with 9"); }
  } else {
    if (DEBUG) { Serial.print("Msg does not start with 9, it's: "); Serial.println(msgFlag, HEX);}
  } 
  return result;
}


//gets sensor ID from first two bytes of msg
unsigned int get_sensor_id(unsigned int firstByte, unsigned int secondByte) {
  unsigned int sensor_id = 0;
  
  firstByte = firstByte & 15;    //clear 4 most significant bytes
  secondByte = secondByte & 192; //clear 6 less significant bytes
  sensor_id = (firstByte << 2) | secondByte;
  
  return sensor_id;
}


//calculates temperature
float calculate_temp(unsigned int firstByte, unsigned int secondByte) {
  float temp = 0.0;
  unsigned int firstBCD;
  unsigned int secondBCD;
  unsigned int thirdBCD;

  //BCD encoding
  firstBCD = firstByte & 15; //clear 4 most significant bits
  secondBCD = (secondByte & 240) >> 4;
  thirdBCD = secondByte & 15;
  //minimal temp. is -39.9, so 40 offset is used to calculate temperature
  temp = (((firstBCD * 100.0) + (secondBCD * 10.0) + thirdBCD) / 10.0) - 40.0;

  return temp;
}


void rf12_rxdata(unsigned char *data, unsigned int number)
{	
  uint8_t  i;
  rf12_xfer(0x82C8);			// receiver on
  rf12_xfer(0xCA81);			// set FIFO mode
  rf12_xfer(0xCA83);			// enable FIFO
  for (i=0; i<number; i++)
  {	
    rf12_ready();
    *data++ = rf12_xfer(0xB000);
  }
  rf12_xfer(0x8208);			// Receiver off 
}

// ******** SPI + RFM 12B functies   ************************************************************************

unsigned short rf12_xfer(unsigned short value)
{	
  uint8_t i;

  clrb(RF_PORT, CS);
  for (i=0; i<16; i++)
  {	
    if (value&32768)
      setb(RF_PORT, SDI);
    else
      clrb(RF_PORT, SDI);
    value<<=1;
    if (RF_PIN&(1<<SDO))
      value|=1;
    setb(RF_PORT, SCK);
    asm("nop");
    asm("nop");
    clrb(RF_PORT, SCK);
  }
  setb(RF_PORT, CS);
  return value;
}


void rf12_ready(void)
{
  clrb(RF_PORT, CS);
  asm("nop");
  asm("nop");
  while (!(RF_PIN&(1<<SDO))); // wait until FIFO ready
  setb(RF_PORT, CS);
}

//radio settings for IT+ sensor (868.300MHz, FSK)
static void rf12_la_init() 
{
  RF_DDR=(1<<SDI)|(1<<SCK)|(1<<CS);
  RF_PORT=(1<<CS);
  for (uint8_t  i=0; i<10; i++) _delay_ms(10); // wait until POR done
  rf12_xfer(0x80E8); // 80e8 CONFIGURATION EL,EF,868 band,12.5pF      // iT+ 915  80f8 
  rf12_xfer(0xA67c); // a67c FREQUENCY SETTING 868.300                // a67c = 915.450 MHz
  rf12_xfer(0xC613); // c613 DATA RATE c613  17.241 kbps
  rf12_xfer(0xC26a); // c26a DATA FILTER COMMAND 
  rf12_xfer(0xCA12); // ca12 FIFO AND RESET  8,SYNC,!ff,DR 
  rf12_xfer(0xCEd4); // ced4 SYNCHRON PATTERN  0x2dd4 
  rf12_xfer(0xC49f); // c49f AFC during VDI HIGH +15 -15 AFC_control_commAND
  rf12_xfer(0x94a0); // 94a0 RECEIVER CONTROL VDI Medium 134khz LNA max DRRSI 103 dbm  
  rf12_xfer(0xCC77); // cc77 not in RFM01 
  rf12_xfer(0x9872); // 9872 transmitter not checked
  rf12_xfer(0xE000); // e000 NOT USE 
  rf12_xfer(0xC800); // c800 NOT USE 
  rf12_xfer(0xC040); // c040 1.66MHz,2.2V 
}






