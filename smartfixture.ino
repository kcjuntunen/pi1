// -*- tags-table-list: '("/home/pi/sketchbook/libraries/Adafruit_PN532") -*-
#include <SPI.h>

//#define DEBUG
#include <Wire.h>
#include <SparkFunBME280.h>
// #include <Adafruit_PN532.h>
#define ALTITUDE   195
// Temp-humidity
// #define dht_apin A0
// photo resistor
#define phr_apin        A1
// PIR motion
#define PIR             A0
#ifdef ADAFRUIT_PN532_H
// PN532
#define PN532_SCK       2
#define PN532_MOSI      3
#define PN532_SS        4
#define PN532_MISO      5
#endif
// small reed switch
#define small_reed_apin 6
// large reed switch
//#define large_reed_apin 6
// IR/avoidance distance sensor
#define avoidance_pin   A2
#define pir_led         10
/* Actually, 13 is the onboard led. But it
 * Turns out the device will be "busy" if something is connected
 * to pin 13. */
#define onboard_led     9

/* This was origally here so the Arduberry could decide whether the
 * light is on. */
int light_threshold = 400;

unsigned long int count = 0;
unsigned int people_count = 0;
unsigned int blink_count = 0;
bool last_blink = false;
bool has_nfc = false;
unsigned int seconds = 0;
int temp_read = 30;
unsigned int send_seconds = 5;
unsigned int reset = 30;
int pirState = LOW;

bool takeLowTime = false;
long unsigned int lowIn;
long unsigned int pause = 5000;
long unsigned motion_start = 0;
long unsigned motion_time = 0;
BME280 BME;
#ifdef ADAFRUIT_PN532_H
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
#endif
struct weather_t {
  float temperature;
  float humidity;
  float pressure;
};

void printdata();
void print_hexchar(const byte * data, const uint32_t numBytes);

bool read_switch() {
#ifdef DEBUG
  Serial.println("Gonna read switch");
#endif
  return !digitalRead(small_reed_apin);
}

float read_light() {
#ifdef DEBUG
  Serial.println("Gonna read light level");
#endif
  return analogRead(phr_apin);
}

float read_presence() {
#ifdef DEBUG
  Serial.println("Gonna check for presence");
#endif
  return (analogRead(avoidance_pin) * 5.0) / 1024.0;
}

void read_weather() {
#ifdef DEBUG
  Serial.println("Gonna get weather");
#endif
  //  DHT.read11(dht_apin);
}

bool read_pir() {
  if (digitalRead(PIR)) {
    if (pirState == LOW) {
      pirState = HIGH;
      motion_start = millis();
      //motion_time = 0;
      digitalWrite(pir_led, HIGH);
    }
    return true;
  } else {
    if (pirState == HIGH) {
      pirState = LOW;
      motion_time = (millis() - motion_start);
      people_count++;
      digitalWrite(pir_led, LOW);
    }
    return false;
  }
}

void read_pir2() {
  if (digitalRead(PIR) == HIGH) {
    digitalWrite(pir_led, LOW);
    if (pirState == LOW) {
      pirState = LOW;
      people_count++;
#ifdef DEBUG
      Serial.print("Peopel: "); Serial.println(people_count);
#endif
      //delay(50);
    }
  }

  if (digitalRead(PIR) == LOW) {
    digitalWrite(pir_led, HIGH);
  }

  if (takeLowTime) {
    lowIn = millis();
    takeLowTime = false;
  }

  if (pirState = HIGH && (millis() - lowIn > pause)) {
    pirState = HIGH;
    //delay(50);
  }
}

void light_on() {
  for (int i = 0; i < 255; i += 10) {
    analogWrite(onboard_led, i);
    read_pir();
    delay(20);
  }
}

void light_off() {
  for (int i = 255; i > 0; i -= 10) {
    analogWrite(onboard_led, i);
    read_pir();
    delay(20);
  }
}

void setup(){
  while (!Serial);
  Serial.println("Setting up.");
  BME.settings.commInterface = I2C_MODE;
  BME.settings.I2CAddress = 0x77;
  BME.settings.runMode = 3;
  BME.settings.filter = 0;
  BME.settings.tempOverSample = 1;
  BME.settings.pressOverSample = 1;
  BME.settings.humidOverSample = 1;

  pinMode(small_reed_apin, INPUT);
  // pinMode(large_reed_apin, INPUT);
  pinMode(avoidance_pin, INPUT);
  pinMode(PIR, INPUT);
  pinMode(pir_led, OUTPUT);
  pinMode(onboard_led, OUTPUT);
  Serial.begin(115200);
  BME.begin();
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if(!versiondata) {
    while(1);
  } else {
    has_nfc = true;
  }
  nfc.SAMConfig();
  delay(5000);
  light_off();
  light_on();
}

void loop(){
  light_off();
  
  if (has_nfc) {
#ifdef DEBUG
    Serial.println("I can has_nfc");
#endif
   
#ifdef ADAFRUIT_PN532_H
    read_rfid();
#endif
  } else {
#ifdef DEBUG
    Serial.println("I can't has_nfc");
#endif
  }
  light_on();
}

void wait() {
  for (int i = 0; i < 15; i++) {
    light_on();
    light_off();
  }
  light_on();
  delay(1000);
}

#ifdef ADAFRUIT_PN532_H
void print_hexchar(const byte * data, const uint32_t numBytes) {
  uint32_t szPos;
  for (szPos=0; szPos < numBytes; szPos++)
    {
      // Append leading 0 for small values
      if (data[szPos] <= 0xF)
        Serial.print(F("0"));
      Serial.print(data[szPos], HEX);
      /* if ((numBytes > 1) && (szPos != numBytes - 1)) */
      /*   { */
      /*     Serial.print(F(" ")); */
      /*   } */
    }
  /* Serial.print(F("  ")); */
  /* for (szPos = 0; szPos < numBytes; szPos++) */
  /*   { */
  /*     if (data[szPos] <= 0x1F) { */
  /*       /\* Serial.print(F(".")); *\/ */
  /*     } */
  /*     else { */
  /*       Serial.print(data[szPos], HEX); */
  /*     } */
  /*   } */
  /* Serial.println(); */
}

// Pretty much copied wholesale from:
// <https://github.com/adafruit/Adafruit-PN532/blob/master/examples/readMifare/readMifare.pde>
void read_rfid() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  /* if (millis() - count > 5000) { */
  /*   printdata(); */
  /*   count = millis(); */
  /* } */
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                    uid, &uidLength, 5000);

  if (success) {
    // Display some basic information about the card
#ifdef DEBUG
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC); Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
#endif
    Serial.print(F("{ \"RFID\": { \"UID Length\": "));
    Serial.print(uidLength, DEC); Serial.print(F(", "));
    Serial.print(F("\"UID\": \"")); print_hexchar(uid, uidLength);

    if (uidLength == 4) {
#ifdef DEBUG
      // We probably have a Mifare Classic card ...
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");

      // Now we need to try to authenticate it for read/write access
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      Serial.println("Trying to authenticate block 4 with default KEYA value");
#endif
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      // Start with block 4 (the first block of sector 1) since sector 0
      // contains the manufacturer data and it's probably better just
      // to leave it alone unless you know what you're doing
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

      if (success) {
#ifdef DEBUG
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
#endif
        uint8_t data[16];

        // If you want to write something to block 4 to test with, uncomment
        // the following line and this text should be read back in a minute
        //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
        // success = nfc.mifareclassic_WriteDataBlock (4, data);

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);

        if (success) {
#ifdef DEBUG
          // Data seems to have been read ... spit it out
          Serial.println("Reading Block 4:");
#endif
          Serial.print(F("\", "));
          Serial.print(F("\"data_blk4\": \""));
	  print_hexchar(data, 16);
	  Serial.print(F("\""));
          /* nfc.PrintHexChar(data, 16); */

          /* Serial.println(""); */

          // Wait a bit before reading the card again
          delay(1000); // We're already waiting.
        } else {
#ifdef DEBUG
          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
#endif
        }
      } else {
#ifdef DEBUG
        Serial.println("Ooops ... authentication failed: Try another key?");
#endif
      }
    }

    if (uidLength == 7) {
#ifdef DEBUG
      // We probably have a Mifare Ultralight card ...
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");

      // Try to read the first general-purpose user page (#4)
      Serial.println("Reading page 4");
#endif
      uint8_t data[32];
      success = nfc.mifareultralight_ReadPage (4, data);
      if (success) {
        // Data seems to have been read ... spit it out
        Serial.print(F(", \"data_pg4\": \"")); print_hexchar(data, 4); Serial.print(F("\""));
        nfc.PrintHexChar(data, 4);
#ifdef DEBUG
        Serial.println("");
#endif
        // Wait a bit before reading the card again
        delay(1000); // We're already waiting.
      } else {
#ifdef DEBUG
        Serial.println("Ooops ... unable to read the requested page!?");
#endif
      }
    }
    Serial.println(F(" } }"));
  }
}
#endif

void serialEvent() {
  while (Serial.available()) {
    byte inByte = Serial.read();
    switch (inByte) {
    case 0x12: //DC2
#ifndef ADAFRUIT_PN532_H
      printdata();
#endif
      break;
    default:
      break;
    }
    Serial.flush();
  }
}
#ifndef ADAFRUIT_PN532_H
static void printdata() {
  // millis:presence:lrs:srs:ll:hum:temp:lt:pir
  //DHT.read11(dht_apin);
  //Serial.print(0); Serial.print(":");
  Serial.print(F("{ \"Poll\": { "));
  Serial.print(F("\"obstruction\": ")); Serial.print(read_presence(), DEC);
  Serial.print(F(", "));
  /* Serial.print("\"reed_switch1\": "); Serial.print(0, DEC); */
  /* Serial.print(", "); */
  Serial.print(F("\"reed_switch2\": ")); Serial.print(read_switch(), DEC);
  Serial.print(F(", "));
  Serial.print(F("\"light_level\": ")); Serial.print(read_light(), DEC);
  Serial.print(F(", "));
  Serial.print(F("\"humidity\": ")); Serial.print(BME.readFloatHumidity(), DEC);
  Serial.print(F(", "));
  Serial.print(F("\"temperature\": ")); Serial.print(BME.readTempF() - 4, DEC);
  Serial.print(F(", "));
  double pressure = (BME.readFloatPressure() / 100) + 23;
  Serial.print(F("\"pressure\": ")); Serial.print(pressure, DEC);
  Serial.print(F(", "));
  Serial.print(F("\"people_count\": ")); Serial.print(people_count, DEC);
  Serial.print(F(", "));
  Serial.print(F("\"motion_time\": ")); Serial.print(motion_time, DEC);
  Serial.println(F(" } }"));
  Serial.flush();
}
#endif