#include <DHT.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>
#include <Arduino.h>
#include <ezButton.h>
#include "Si115X.h"

#define ONE_WIRE_PIN 7

#define DHT_PIN 2
#define RELAY_PIN 4
#define BUTTON1_PIN 9
#define BUTTON2_PIN 8


#define DHTTYPE DHT11  // DHT 11
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
Si115X SI1151 = Si115X();

SoftwareSerial espSerial(5, 6);
DHT dht(DHT_PIN, DHTTYPE);
String str;

ezButton toggle_button(BUTTON1_PIN);
ezButton hold_button(BUTTON2_PIN);

bool pumpOverride = false;

void setup() {
    Serial.begin(9600);
    espSerial.begin(9600);
    dht.begin();
    sensors.begin();
    while (!SI1151.Begin()) {
        Serial.println("SI1151 not found");
        delay(1000);
    }
    delay(2000);

    pinMode(RELAY_PIN, OUTPUT);
}

String intToString(int b) {
    if (b == 1) {
        return "true";
    } else {
        return "false";
    }
}

bool shouldPump(float sMP, float eT, float ehP, float uV, float iR) {
    // soil moisture in the range of 20-30
    // temperature in the range of 10-18.3 Celsius
    // humidity in the range of 50-60%
    // wavelength of 400-700nm
    // intensity > 0
    if (sMP <= 50) {
        return true;
    } else {
        return false;
    }
}

template<typename T, typename T2>
inline T map(T2 val, T2 in_min, T2 in_max, T out_min, T out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void loop() {
    static unsigned long prev = 0;
    static unsigned long pumpPrev = 0;
    unsigned long now = millis();
    int pumpState = digitalRead(RELAY_PIN);
    int moistureVal = analogRead(A2);

    toggle_button.loop();
    hold_button.loop();

    if (toggle_button.isPressed()) {
        pumpOverride = !pumpOverride;
        Serial.println("Toggled pump override: " + intToString(pumpOverride));
    }

    if (!pumpOverride && hold_button.isPressed()) {
        pumpOverride = true;
        Serial.println("Toggled pump override: " + intToString(pumpOverride));
    }

    if (pumpOverride) {
        digitalWrite(RELAY_PIN, HIGH);
        pumpState = 1;

        if (hold_button.isReleased()) {
            pumpOverride = false;
            Serial.println("Toggled pump override: " + intToString(pumpOverride));
        }
    }

    sensors.requestTemperatures();

    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    float hic = dht.computeHeatIndex(t, h, false);
    float tempC = sensors.getTempCByIndex(0);

    uint16_t visibleLight = SI1151.ReadHalfWord_VISIBLE();
    uint16_t infraredLight = SI1151.ReadHalfWord();
    auto uvLight = (float) SI1151.ReadHalfWord_UV();
    float smP = map(moistureVal * 1.0f, 1023.0f, 292.0f, 0.0f, 100.0f);

    bool pump = shouldPump(smP, t, h, uvLight, infraredLight);

    if (Serial.available() > 0) {
        Serial.println(Serial.readStringUntil('\n'));
    }

    // stop watering if should not pump or if the burst of 1 second is over
    if (!pump || now - pumpPrev >= 1000L) {
        if (!pumpOverride) {
            digitalWrite(RELAY_PIN, LOW);
            pumpPrev = now;
        }
    }

    if (now - prev >= 10000L) {
        Serial.print("DHT11 - H: ");
        Serial.print(h);
        Serial.print("% ");
        Serial.print(" T: ");
        Serial.print(t);
        Serial.print("C ");
        Serial.print(" HI: ");
        Serial.print(hic);
        Serial.println("C");

        Serial.print("SU - VIS: ");
        Serial.print(visibleLight);

        Serial.print("lm UV: ");
        Serial.print(uvLight);

        Serial.print("UVI IR: ");
        Serial.print(infraredLight);
        Serial.println("lm");

        Serial.print("CMSCR - V: ");
        Serial.print(moistureVal);
        Serial.print(" P: ");
        Serial.print(smP);
        Serial.println("%");

        if (tempC == DEVICE_DISCONNECTED_C) {
            Serial.println("Could not read soil temp data.");
            prev = now;
            return;
        }

        Serial.print("ST - V: ");
        Serial.print(tempC);
        Serial.println("C");


        // start a burst of 1 second of watering
        if (pump && pumpState == LOW) {
            digitalWrite(RELAY_PIN, HIGH);
            pumpPrev = now;
        }

        Serial.println("Pumping: " + intToString(pumpState));
        Serial.println("Should pump: " + intToString(pump));

        str = String("EH=") + String(h) +
              String(";ET=") + String(t) +
              String(";EHI=") + String(hic) +
              String(";SM=") + String(smP) + "%" +
              String(";ST=") + tempC +
              String(";UV=") + uvLight +
              String(";VIS=") + visibleLight +
              String(";IR=") + infraredLight +
              String(";P=") + intToString(pump);
        espSerial.println(str + "@");

        prev = now;
    }
}