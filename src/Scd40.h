#pragma once

#include <Arduino.h>
#include <SensirionI2cScd4x.h>

// Thin wrapper around the SCD40 that caches the latest measurement.
// The sensor produces a new reading every 5 s in periodic mode.
class Scd40 {
public:
    bool begin(int sdaPin, int sclPin);
    void update();  // call from loop(); reads a new sample when one is ready

    bool valid() const { return valid_; }
    uint16_t co2() const { return co2_; }            // ppm
    float temperature() const { return temperature_; } // °C
    float humidity() const { return humidity_; }       // %RH

private:
    void checkSettings();
    void logError(const char *what, int16_t error);

    SensirionI2cScd4x sensor_;
    bool valid_ = false;
    uint16_t co2_ = 0;
    float temperature_ = 0;
    float humidity_ = 0;
    uint32_t lastPollMs_ = 0;
};

extern Scd40 scd40;
