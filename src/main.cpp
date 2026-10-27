#include <Arduino.h>
#include <HomeSpan.h>

#include "Scd40.h"
#include "Services.h"

static constexpr int SDA_PIN = 21;
static constexpr int SCL_PIN = 22;
static constexpr int STATUS_LED_PIN = 2;  // on-board LED
static constexpr int CONTROL_PIN = 0;     // BOOT button

// 8-digit code entered in the Home app when pairing (shown as 314-15-926).
static const char *PAIRING_CODE = "31415926";

void setup() {
    Serial.begin(115200);

    scd40.begin(SDA_PIN, SCL_PIN);

    homeSpan.setStatusPin(STATUS_LED_PIN);
    homeSpan.setControlPin(CONTROL_PIN);
    homeSpan.setPairingCode(PAIRING_CODE);
    homeSpan.begin(Category::Sensors, "CO2 Monitor");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("CO2 Monitor");
    new Characteristic::Manufacturer("DIY");
    new Characteristic::Model("ESP32 + SCD40");

    new Co2Sensor();
    new TemperatureSensor();
    new HumiditySensor();
    new AirQualitySensor();
}

void loop() {
    scd40.update();
    homeSpan.poll();
}
