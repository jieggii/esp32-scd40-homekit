#pragma once

#include <HomeSpan.h>

#include "Scd40.h"

// CO2 Detected turns "abnormal" above the high threshold and back to
// "normal" below the low one, so it doesn't flap around a single value.
static constexpr uint16_t CO2_ALERT_HIGH_PPM = 1200;
static constexpr uint16_t CO2_ALERT_LOW_PPM = 1000;

// Minimum time between HomeKit updates of a single characteristic.
static constexpr uint32_t UPDATE_INTERVAL_MS = 10000;

// Pushes a new value to HomeKit only if it changed and the rate limit allows.
template <typename T>
static void pushIfChanged(SpanCharacteristic *c, T value, uint32_t &lastMs) {
    if (c->getVal<T>() == value || millis() - lastMs < UPDATE_INTERVAL_MS)
        return;
    c->setVal(value);
    lastMs = millis();
}

struct Co2Sensor : Service::CarbonDioxideSensor {
    SpanCharacteristic *detected;
    SpanCharacteristic *level;
    SpanCharacteristic *peak;
    uint16_t peakPpm = 0;
    uint32_t lastLevelMs = 0;
    uint32_t lastPeakMs = 0;

    Co2Sensor() {
        detected = new Characteristic::CarbonDioxideDetected(0);
        level = new Characteristic::CarbonDioxideLevel(0);
        peak = new Characteristic::CarbonDioxidePeakLevel(0);
    }

    void loop() override {
        if (!scd40.valid())
            return;
        uint16_t ppm = scd40.co2();

        pushIfChanged<float>(level, ppm, lastLevelMs);

        peakPpm = max(peakPpm, ppm);
        pushIfChanged<float>(peak, peakPpm, lastPeakMs);

        uint8_t abnormal = detected->getVal();
        if (ppm > CO2_ALERT_HIGH_PPM)
            abnormal = 1;
        else if (ppm < CO2_ALERT_LOW_PPM)
            abnormal = 0;
        // State changes are pushed immediately so alerts aren't delayed.
        if (abnormal != detected->getVal()) {
            detected->setVal(abnormal);
        }
    }
};

struct TemperatureSensor : Service::TemperatureSensor {
    SpanCharacteristic *temperature;
    uint32_t lastMs = 0;

    TemperatureSensor() {
        temperature = new Characteristic::CurrentTemperature(0);
        temperature->setRange(-40, 100);
    }

    void loop() override {
        if (!scd40.valid())
            return;
        // Round to 0.1 °C so sensor noise doesn't count as a change.
        float value = roundf(scd40.temperature() * 10) / 10;
        pushIfChanged<float>(temperature, value, lastMs);
    }
};

struct HumiditySensor : Service::HumiditySensor {
    SpanCharacteristic *humidity;
    uint32_t lastMs = 0;

    HumiditySensor() { humidity = new Characteristic::CurrentRelativeHumidity(0); }

    void loop() override {
        if (!scd40.valid())
            return;
        float value = roundf(scd40.humidity());
        pushIfChanged<float>(humidity, value, lastMs);
    }
};

struct AirQualitySensor : Service::AirQualitySensor {
    SpanCharacteristic *quality;
    uint32_t lastMs = 0;

    AirQualitySensor() { quality = new Characteristic::AirQuality(0); }  // 0 = unknown

    static uint8_t fromCo2(uint16_t ppm) {
        if (ppm < 600) return 1;   // excellent
        if (ppm < 800) return 2;   // good
        if (ppm < 1000) return 3;  // fair
        if (ppm < 1400) return 4;  // inferior
        return 5;                  // poor
    }

    void loop() override {
        if (!scd40.valid())
            return;
        pushIfChanged<int>(quality, fromCo2(scd40.co2()), lastMs);
    }
};
