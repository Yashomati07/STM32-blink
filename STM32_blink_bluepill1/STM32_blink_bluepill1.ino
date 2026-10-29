
#include <Wire.h>

// ==================================================
// STM32 BLUE PILL PIN CONFIGURATION
// ==================================================
#define GREEN_LED   PA0
#define YELLOW_LED  PA1
#define RED_LED     PA2
#define BUZZER      PB0

// SGP30 I2C address
#define SGP30_ADDRESS 0x58

// SGP30 commands
#define SGP30_INIT_AIR_QUALITY    0x2003
#define SGP30_MEASURE_AIR_QUALITY 0x2008

// ==================================================
// TVOC THRESHOLDS (ppb)
// ==================================================
#define TVOC_GOOD_MAX       220
#define TVOC_MODERATE_MAX   660
#define TVOC_EXTREME_MIN   1000

// ==================================================
// AIR QUALITY STATES
// ==================================================
enum AirQuality {
  AIR_GOOD,
  AIR_MODERATE,
  AIR_POOR,
  AIR_EXTREME
};

AirQuality airQuality = AIR_GOOD;

uint16_t eco2 = 400;
uint16_t tvoc = 0;

// ==================================================
// SGP30 CRC-8 CALCULATION
// ==================================================
uint8_t calculateCRC(uint8_t data[], uint8_t length) {
  uint8_t crc = 0xFF;

  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

// ==================================================
// SEND COMMAND TO SGP30
// ==================================================
bool sendSGP30Command(uint16_t command) {
  Wire.beginTransmission(SGP30_ADDRESS);

  Wire.write((uint8_t)(command >> 8));
  Wire.write((uint8_t)(command & 0xFF));

  return Wire.endTransmission() == 0;
}

// ==================================================
// READ eCO2 AND TVOC
// ==================================================
bool readSGP30(uint16_t &co2, uint16_t &voc) {
  if (!sendSGP30Command(SGP30_MEASURE_AIR_QUALITY)) {
    return false;
  }

  delay(15);

  uint8_t received = Wire.requestFrom(
    (uint8_t)SGP30_ADDRESS,
    (uint8_t)6
  );

  if (received != 6) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  uint8_t data[6];

  for (uint8_t i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  uint8_t eco2Data[2] = {data[0], data[1]};
  uint8_t tvocData[2] = {data[3], data[4]};

  if (calculateCRC(eco2Data, 2) != data[2]) {
    return false;
  }

  if (calculateCRC(tvocData, 2) != data[5]) {
    return false;
  }

  co2 = ((uint16_t)data[0] << 8) | data[1];
  voc = ((uint16_t)data[3] << 8) | data[4];

  return true;
}

// ==================================================
// LED AND BUZZER CONTROL
// Assumes active-HIGH LEDs and buzzer
// ==================================================
void setLEDs(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED,  green  ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(RED_LED,    red    ? HIGH : LOW);
}

void setBuzzer(bool enabled) {
  digitalWrite(BUZZER, enabled ? HIGH : LOW);
}

// ==================================================
// AIR QUALITY DECISION
// ==================================================
void updateAirQuality() {

  if (tvoc > TVOC_EXTREME_MIN) {

    // EXTREME: Red LED + buzzer ON
    airQuality = AIR_EXTREME;

    setLEDs(false, false, true);
    setBuzzer(true);

  } else if (tvoc > TVOC_MODERATE_MAX) {

    // POOR: Red LED, buzzer OFF
    airQuality = AIR_POOR;

    setLEDs(false, false, true);
    setBuzzer(false);

  } else if (tvoc > TVOC_GOOD_MAX) {

    // MODERATE: Yellow LED, buzzer OFF
    airQuality = AIR_MODERATE;

    setLEDs(false, true, false);
    setBuzzer(false);

  } else {

    // GOOD: Green LED, buzzer OFF
    airQuality = AIR_GOOD;

    setLEDs(true, false, false);
    setBuzzer(false);
  }
}

// ==================================================
// PRINT SENSOR VALUES AND STATUS
// ==================================================
void printResults() {
  Serial.print("eCO2: ");
  Serial.print(eco2);
  Serial.print(" ppm | TVOC: ");
  Serial.print(tvoc);
  Serial.print(" ppb | Status: ");

  switch (airQuality) {
    case AIR_GOOD:
      Serial.println("GOOD - Green LED");
      break;

    case AIR_MODERATE:
      Serial.println("MODERATE - Yellow LED");
      break;

    case AIR_POOR:
      Serial.println("POOR - Red LED, Buzzer OFF");
      break;

    case AIR_EXTREME:
      Serial.println("EXTREME - Red LED, BUZZER ON");
      break;
  }
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  setLEDs(false, false, false);
  setBuzzer(false);

  // STM32 Blue Pill I2C1: PB7 SDA, PB6 SCL
  Wire.setSDA(PB7);
  Wire.setSCL(PB6);
  Wire.begin();
  Wire.setClock(100000);

  Serial.println();
  Serial.println("Wearable Air Quality Monitoring Pendant");
  Serial.println("STM32 Blue Pill + SGP30");
  Serial.println("----------------------------------------");

  if (sendSGP30Command(SGP30_INIT_AIR_QUALITY)) {
    Serial.println("SGP30 detected at 0x58.");
    Serial.println("Initialization successful.");
  } else {
    Serial.println("ERROR: SGP30 initialization failed.");
    Serial.println("Check sensor power, SDA and SCL.");

    while (true) {
      setLEDs(false, false, true);
      setBuzzer(false);
      delay(300);

      setLEDs(false, false, false);
      delay(300);
    }
  }

  // Sensor warm-up
  Serial.println("Warming up sensor for 15 seconds...");

  setLEDs(false, true, false);
  setBuzzer(false);

  for (int seconds = 15; seconds > 0; seconds--) {
    Serial.print("Starting in ");
    Serial.print(seconds);
    Serial.println(" seconds");
    delay(1000);
  }

  setLEDs(false, false, false);

  Serial.println("Monitoring started.");
  Serial.println("----------------------------------------");
}

// ==================================================
// MAIN LOOP
// ==================================================
void loop() {
  if (readSGP30(eco2, tvoc)) {
    updateAirQuality();
    printResults();
  } else {
    Serial.println("SGP30 read/CRC error.");

    // Never leave the buzzer active after a read error
    setBuzzer(false);
    setLEDs(false, false, false);
  }

  delay(1000);
}
