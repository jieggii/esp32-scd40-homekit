#include "Scd40.h"

#include <Wire.h>

Scd40 scd40;

static constexpr uint32_t POLL_INTERVAL_MS = 1000;

// Compensates for the sensor's self-heating. Sensirion's default is 4 °C;
// this value was tuned for this board against a reference thermometer.
static constexpr float TEMPERATURE_OFFSET_C = 6.4f;

bool Scd40::begin(int sdaPin, int sclPin) {
    Wire.begin(sdaPin, sclPin);
    sensor_.begin(Wire, SCD40_I2C_ADDR_62);

    // The sensor keeps measuring across an ESP32 soft reset; it must be
    // stopped before it accepts other commands.
    sensor_.wakeUp();
    sensor_.stopPeriodicMeasurement();
    sensor_.reinit();

    checkSettings();

    int16_t error = sensor_.startPeriodicMeasurement();
    if (error) {
        logError("startPeriodicMeasurement", error);
        return false;
    }
    Serial.println("SCD40: periodic measurement started");
    return true;
}

void Scd40::update() {
    uint32_t now = millis();
    if (now - lastPollMs_ < POLL_INTERVAL_MS)
        return;
    lastPollMs_ = now;

    bool ready = false;
    int16_t error = sensor_.getDataReadyStatus(ready);
    if (error) {
        logError("getDataReadyStatus", error);
        return;
    }
    if (!ready)
        return;

    uint16_t co2;
    float temperature, humidity;
    error = sensor_.readMeasurement(co2, temperature, humidity);
    if (error) {
        logError("readMeasurement", error);
        return;
    }
    if (co2 == 0)  // sensor reports 0 ppm for an invalid sample
        return;

    co2_ = co2;
    temperature_ = temperature;
    humidity_ = humidity;
    valid_ = true;
    Serial.printf("SCD40: CO2=%u ppm, T=%.1f C, RH=%.1f %%\n", co2, temperature, humidity);
}

// Prints the settings stored in the sensor's EEPROM and restores the
// temperature offset if a previous owner left a different one persisted.
// Must run while periodic measurement is stopped.
void Scd40::checkSettings() {
    float offset;
    uint16_t altitude = 0, asc = 0;
    int16_t error = sensor_.getTemperatureOffset(offset);
    if (error) {
        logError("getTemperatureOffset", error);
        return;
    }
    if ((error = sensor_.getSensorAltitude(altitude)))
        logError("getSensorAltitude", error);
    if ((error = sensor_.getAutomaticSelfCalibrationEnabled(asc)))
        logError("getAutomaticSelfCalibrationEnabled", error);
    Serial.printf("SCD40: temp offset=%.1f C, altitude=%u m, ASC=%s\n",
                  offset, altitude, asc ? "on" : "off");

    // Only write when needed: the EEPROM has a limited number of write cycles.
    if (fabsf(offset - TEMPERATURE_OFFSET_C) <= 0.5f)
        return;
    if ((error = sensor_.setTemperatureOffset(TEMPERATURE_OFFSET_C))) {
        logError("setTemperatureOffset", error);
        return;
    }
    if ((error = sensor_.persistSettings())) {
        logError("persistSettings", error);
        return;
    }
    Serial.printf("SCD40: temperature offset corrected %.1f -> %.1f C (saved)\n",
                  offset, TEMPERATURE_OFFSET_C);
}

void Scd40::logError(const char *what, int16_t error) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("SCD40: %s failed: %s\n", what, message);
}
