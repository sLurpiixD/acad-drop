/**
 * TEC Test Bench — Data Acquisition Firmware
 * Target  : ESP32-S3-N16R8 (DevKitC-1)
 * Toolchain: arduino-esp32 v3.x  (espressif32 platform, PlatformIO)
 *
 * Serial protocol (115200 baud, CSV):
 *   RX  ← LabVIEW sends integer 0–255 followed by '\n' to set fan PWM duty
 *   TX  → LabVIEW receives: Current_A,Fan_RPM,Tamb_C,Thot_C,Tcold_C
 *         Sensor fault value = -999.00
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "Adafruit_MAX31855.h"
#include "max6675.h"
#include "INA226.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define MAX31855_CLK    12
#define MAX31855_CS     11
#define MAX31855_MISO   14

#define MAX6675_CLK     12   // Shared CLK is safe — software SPI, CS-gated
#define MAX6675_CS      10
#define MAX6675_MISO    13

#define I2C_SDA          8
#define I2C_SCL          9

#define ONE_WIRE_BUS     4

#define FAN_PWM_PIN      5
#define FAN_TACH_PIN     6

// ============================================================
// CONSTANTS
// ============================================================

// --- Current sensing ---
// Shunt resistor physical label "100" = 100 mΩ = 0.1 Ω
static constexpr float SHUNT_OHMS = 0.1f;

// --- Tachometer ---
// Standard 4-pin PC fan (Intel spec) outputs 2 pulses per revolution
static constexpr uint8_t TACH_PPR = 2;

// --- Timing ---
static constexpr uint32_t TELEMETRY_MS      = 1000;
static constexpr uint16_t SERIAL_TIMEOUT_MS = 50;   // parseInt / readString timeout
                                                     // FIX: was 1000ms (Arduino default),
                                                     // which blocked the telemetry loop

// --- Fault sentinel transmitted to LabVIEW on sensor error ---
static constexpr float FAULT_SENTINEL = -999.0f;

// --- Fan PWM (Intel 4-pin spec: 25 kHz, 8-bit resolution → 0–255) ---
static constexpr uint32_t PWM_FREQ       = 25000;
static constexpr uint8_t  PWM_RESOLUTION = 8;
static constexpr uint8_t  PWM_CHANNEL    = 0;

// ============================================================
// PERIPHERAL OBJECTS
// ============================================================
Adafruit_MAX31855 max31855(MAX31855_CLK, MAX31855_CS,  MAX31855_MISO);
MAX6675           max6675 (MAX6675_CLK,  MAX6675_CS,   MAX6675_MISO);
INA226            ina(0x40);
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature ambientSensor(&oneWire);

// ============================================================
// TACHOMETER STATE
// ============================================================
// FIX: explicit uint32_t (vs. unsigned int) — makes 32-bit width unambiguous
// across platforms; Xtensa 32-bit read/write is atomic, but read+clear below
// is NOT, hence the portDISABLE_INTERRUPTS guard in the telemetry block.
volatile uint32_t tachPulses = 0;
unsigned long     lastMillis  = 0;
float             currentRPM  = 0.0f;

void IRAM_ATTR tachISR() {
  tachPulses++;
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(SERIAL_TIMEOUT_MS);  

  Wire.begin(I2C_SDA, I2C_SCL);

  // --- INA226 ---
  if (!ina.begin()) {
    Serial.println("# ERR: INA226 not found — check I2C address/wiring");
    // Non-fatal: continue so other sensors still report
  }

  // --- DS18B20 ---
  ambientSensor.begin();

  // --- Fan PWM (arduino-esp32 v2.x API) ---
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(FAN_PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);   // Fan off at boot

  // --- Tachometer interrupt ---
  pinMode(FAN_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), tachISR, FALLING);

  delay(500);   // Let sensors stabilise before first sample
  Serial.println("Current_A,Fan_RPM,Tamb_C,Thot_C,Tcold_C");
}

// ============================================================
// LOOP
// ============================================================
void loop() {

  // ----------------------------------------------------------
  // 1.  RECEIVE FAN SPEED COMMAND FROM LABVIEW
  //     Expected format: ASCII integer 0–255 followed by '\n'
  //     Example: "128\n"
  // ----------------------------------------------------------
  if (Serial.available() > 0) {

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() > 0 && isDigit(cmd.charAt(0))) {
      int targetPWM = cmd.toInt();
      if (targetPWM >= 0 && targetPWM <= 255) {
        ledcWrite(PWM_CHANNEL, (uint32_t)targetPWM);
      }
    }
  }

  // ----------------------------------------------------------
  // 2.  TELEMETRY TRANSMIT (every 1000 ms)
  // ----------------------------------------------------------
  unsigned long now = millis();
  if (now - lastMillis >= TELEMETRY_MS) {
    lastMillis = now;

    portDISABLE_INTERRUPTS();
    uint32_t pulses = tachPulses;
    tachPulses = 0;
    portENABLE_INTERRUPTS();

    // pulses counted over exactly 1 second → convert to RPM
    currentRPM = (pulses / (float)TACH_PPR) * 60.0f;

    // --- DS18B20 ambient temperature ---
    ambientSensor.requestTemperatures();
    float t_amb = ambientSensor.getTempCByIndex(0);
    if (t_amb == DEVICE_DISCONNECTED_C) t_amb = FAULT_SENTINEL;

    // --- MAX6675 hot-side thermocouple ---
    float t_hot = max6675.readCelsius();
    // MAX6675 has no fault register; returns 0 on open circuit — no NaN risk

    // --- MAX31855 cold-side thermocouple ---
    float t_cold = max31855.readCelsius();
    // FIX: MAX31855 returns NaN on open thermocouple / short fault.
    //      Sending NaN over serial breaks LabVIEW numeric parsing.
    if (isnan(t_cold)) t_cold = FAULT_SENTINEL;

    // --- INA226 current (bypass calibration register) ---
    // Reading raw shunt voltage avoids the 0.00A lockout that occurs when
    // the INA226 calibration register (CAL) is zero.
    // I = V_shunt / R_shunt   →   R = 100 mΩ (shunt label "100")
    float current_A = ina.getShuntVoltage() / SHUNT_OHMS;

    // --- Transmit CSV line to LabVIEW ---
    Serial.print(current_A, 3); Serial.print(',');
    Serial.print(currentRPM, 0); Serial.print(',');
    Serial.print(t_amb,  2);    Serial.print(',');
    Serial.print(t_hot,  2);    Serial.print(',');
    Serial.println(t_cold, 2);
  }
}