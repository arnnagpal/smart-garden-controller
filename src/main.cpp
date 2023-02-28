#include <DHT.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>
#include <Arduino.h>
#include "SI114X.h"

#define ONE_WIRE_PIN 7

#define DHT_PIN 2
#define RELAY_PIN 4
#define BUTTON1_PIN 9
#define BUTTON2_PIN 8


#define DHTTYPE DHT11  // DHT 11
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
SI114X SI1145 = SI114X();

SoftwareSerial espSerial(5, 6);
DHT dht(DHT_PIN, DHTTYPE);
String str;

bool state1 = 1;  //set button1 state
bool state2 = 1;  //set button2 state

void setup() {
    Serial.begin(9600);
    espSerial.begin(9600);
    dht.begin();
    sensors.begin();
    while (!SI1145.Begin()) {
        Serial.println("SI1145 not found");
        delay(1000);
    }
    delay(2000);

    pinMode(BUTTON1_PIN, INPUT);
    pinMode(BUTTON2_PIN, INPUT);
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
    // temperature in the range of 10-18.3 celsius
    // humidity in the range of 50-60%
    // wavelength of 400-700nm
    // intensity > 0
    if (sMP >= 20 && sMP <= 30
        && eT >= 10 && eT <= 18.3
        && ehP >= 50 && ehP <= 60
        && uV > 0
        && iR > 0) {
        return true;
    } else {
        return false;
    }
}

void loop() {
    static unsigned long prev = 0;
    unsigned long now = millis();
    state1 = digitalRead(BUTTON1_PIN);
    state2 = digitalRead(BUTTON2_PIN);

    if (Serial.available() > 0) {
        Serial.println(Serial.readStringUntil('\n'));
    }

    if (state1 == 0) {
        digitalWrite(RELAY_PIN, HIGH);
    }

    if (state2 == 0) {
        digitalWrite(RELAY_PIN, LOW);
    }

    if (now - prev >= 10000L) {
        sensors.requestTemperatures();

        float h = dht.readHumidity();
        // Read temperature as Celsius (the default)
        float t = dht.readTemperature();
        float hic = dht.computeHeatIndex(t, h, false);
        int moistureVal = analogRead(A2);
        float tempC = sensors.getTempCByIndex(0);

        uint16_t visibleLight = SI1145.ReadVisible();
        uint16_t infraredLight = SI1145.ReadIR();
        float uvLight = (float) SI1145.ReadUV() / 100;
        int pumpState = digitalRead(RELAY_PIN);

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

        Serial.print(" CMSCR - V: ");
        Serial.println(moistureVal);

        if (tempC == DEVICE_DISCONNECTED_C) {
            Serial.println("Could not read soil temp data.");
            prev = now;
            return;
        }

        Serial.print("ST - V: ");
        Serial.print(tempC);
        Serial.println("C");

        Serial.println("Pumping: " + intToString(pumpState));


        str = String("EH=") + String(h) +
              String(";ET=") + String(t) +
              String(";EHI=") + String(hic) +
              String(";SM=") + String(moistureVal) +
              String(";ST=") + tempC +
              String(";UV=") + uvLight +
              String(";VIS=") + visibleLight +
              String(";IR=") + infraredLight +
              String(";P=") + intToString(pumpState);
        espSerial.println(str + "@");

        prev = now;
    }
}