#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <max6675.h>
#include <Adafruit_MAX31855.h>
#include <Adafruit_INA260.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =============================================================
//  PIN DEFINITIONS (ESP32-S3)
// =============================================================

// MAX6675 (SPI) - Hot Side Thermocouple
const int SPI_CLK       = 12;
const int SPI_MISO      = 13;
const int CS_MAX6675    = 10;   // Hot side CS

// MAX31855 (SPI) - Cold Side Thermocouple (supports sub-zero)
const int CS_MAX31855   = 9;    // Cold side CS (shares CLK + MISO)

// INA260 (I2C) - Power Monitoring
const int I2C_SDA       = 3;
const int I2C_SCL       = 2;

// DS18B20 (OneWire) - Ambient Temperature
const int ONE_WIRE_BUS  = 8;

// Fan Control
const int FAN_PWM_PIN   = 5;    // PWM output to AOD4184 gate
const int FAN_TACH_PIN  = 47;   // TACH input from fan

// TEC Control
const int TEC_PIN       = 46;   // TEC on/off (via relay or MOSFET)

// =============================================================
//  SENSOR OBJECTS
// =============================================================
MAX6675               thermocoupleHot(SPI_CLK, CS_MAX6675, SPI_MISO);
Adafruit_MAX31855     thermocoupleCold(SPI_CLK, CS_MAX31855, SPI_MISO);
Adafruit_INA260       ina260;
OneWire               oneWire(ONE_WIRE_BUS);
DallasTemperature     ambientSensor(&oneWire);

// =============================================================
//  FAN RPM VARIABLES
// =============================================================
volatile unsigned long tachPulseCount = 0;
unsigned long lastRPMCalc             = 0;
float fanRPM                          = 0.0;

void IRAM_ATTR tachISR() {
  tachPulseCount++;
}

// =============================================================
//  SERIAL COMMAND VARIABLES
// =============================================================
String inputBuffer = "";

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // --- INA260 (I2C) ---
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ina260.begin()) {
    Serial.println("ERROR: INA260 not found. Check wiring!");
    while (1);
  }
  // INA260 has internal 2mΩ shunt, no calibration needed
  ina260.setAveragingCount(INA260_COUNT_16);
  ina260.setVoltageConversionTime(INA260_TIME_1_1_ms);
  ina260.setCurrentConversionTime(INA260_TIME_1_1_ms);

  // --- DS18B20 ---
  ambientSensor.begin();

  // --- Fan TACH interrupt ---
  pinMode(FAN_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), tachISR, FALLING);

  // --- Fan PWM (default 50%) ---
  pinMode(FAN_PWM_PIN, OUTPUT);
  ledcSetup(0, 25000, 8);        // Channel 0, 25kHz, 8-bit resolution
  ledcAttachPin(FAN_PWM_PIN, 0);
  ledcWrite(0, 128);             // 50% duty cycle default

  // --- TEC pin ---
  pinMode(TEC_PIN, OUTPUT);
  digitalWrite(TEC_PIN, LOW);    // TEC off by default

  // --- CSV Header ---
  // LabVIEW Python script will parse this line first
  Serial.println("Time_ms,Voltage_V,Current_A,Power_W,Temp_Hot_C,Temp_Cold_C,Temp_Ambient_C,Fan_RPM");
}

// =============================================================
//  LOOP
// =============================================================
void loop() {

  // --- 1. Handle incoming serial commands from LabVIEW ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      inputBuffer.trim();
      handleCommand(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // --- 2. Calculate Fan RPM every 1 second ---
  unsigned long now = millis();
  if (now - lastRPMCalc >= 1000) {
    noInterrupts();
    unsigned long pulses = tachPulseCount;
    tachPulseCount = 0;
    interrupts();

    // Most fans send 2 pulses per revolution
    fanRPM = (pulses / 2.0) * 60.0;
    lastRPMCalc = now;
  }

  // --- 3. Read INA260 ---
  float voltage = ina260.readBusVoltage() / 1000.0;  // mV to V
  float current = ina260.readCurrent()    / 1000.0;  // mA to A
  float power   = ina260.readPower()      / 1000.0;  // mW to W

  // --- 4. Read MAX6675 (Hot Side) ---
  float tempHot = thermocoupleHot.readCelsius();
  delay(250); // MAX6675 requires min 250ms between reads

  // --- 5. Read MAX31855 (Cold Side — supports sub-zero) ---
  float tempCold = thermocoupleCold.readCelsius();
  if (isnan(tempCold)) tempCold = -999.0; // Flag error

  // --- 6. Read DS18B20 (Ambient) ---
  ambientSensor.requestTemperatures();
  float tempAmbient = ambientSensor.getTempCByIndex(0);

  // --- 7. Send CSV line to LabVIEW via Serial ---
  Serial.print(millis());         Serial.print(",");
  Serial.print(voltage, 3);       Serial.print(",");
  Serial.print(current, 3);       Serial.print(",");
  Serial.print(power, 3);         Serial.print(",");
  Serial.print(tempHot, 2);       Serial.print(",");
  Serial.print(tempCold, 2);      Serial.print(",");
  Serial.print(tempAmbient, 2);   Serial.print(",");
  Serial.println(fanRPM, 0);

  delay(750); // Total loop ~1 second (250ms MAX6675 delay + 750ms here)
}

// =============================================================
//  COMMAND HANDLER
//  Commands from LabVIEW via Serial:
//    FAN:<0-100>     → Set fan speed percentage
//    TEC:<0|1>       → Turn TEC off or on
// =============================================================
void handleCommand(String cmd) {
  cmd.toUpperCase();

  if (cmd.startsWith("FAN:")) {
    int pct = cmd.substring(4).toInt();
    pct = constrain(pct, 0, 100);
    int duty = map(pct, 0, 100, 0, 255);
    ledcWrite(0, duty);
    Serial.print("ACK:FAN:");
    Serial.println(pct);
  }
  else if (cmd.startsWith("TEC:")) {
    int state = cmd.substring(4).toInt();
    digitalWrite(TEC_PIN, state ? HIGH : LOW);
    Serial.print("ACK:TEC:");
    Serial.println(state);
  }
  else {
    Serial.print("ERR:Unknown command: ");
    Serial.println(cmd);
  }
}
