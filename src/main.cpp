#include <DHT.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>
#include <Arduino.h>
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

bool prevState1 = 1;  //set button1 state
bool prevState2 = 1;  //set button2 state

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
    // temperature in the range of 10-18.3 Celsius
    // humidity in the range of 50-60%
    // wavelength of 400-700nm
    // intensity > 0
    if (sMP >= 20 && sMP <= 30) {
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

    sensors.requestTemperatures();

    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    float hic = dht.computeHeatIndex(t, h, false);
    int moistureVal = analogRead(A2);
    float tempC = sensors.getTempCByIndex(0);

    uint16_t visibleLight = SI1151.ReadHalfWord_VISIBLE();
    uint16_t infraredLight = SI1151.ReadHalfWord();
    auto uvLight = (float) SI1151.ReadHalfWord_UV();
    int pumpState = digitalRead(RELAY_PIN);
    float smP = map(moistureVal * 1.0f, 1023.0f, 292.0f, 0.0f, 100.0f);

    bool pump = shouldPump(smP, t, h, uvLight, infraredLight);

    if (Serial.available() > 0) {
        Serial.println(Serial.readStringUntil('\n'));
    }

    // toggle relay on button press
    // (green button)
    if (digitalRead(BUTTON1_PIN) == 0 && prevState1 == 1) {
        digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
        prevState1 = false;
    } else if (digitalRead(BUTTON1_PIN) == 1 && prevState1 == 0) {
        prevState1 = true;
    }

    // turn on relay if button2 is held down
    // (white button)
    if (digitalRead(BUTTON2_PIN) == 0 && prevState2 == 1) {
        digitalWrite(RELAY_PIN, HIGH);
        prevState2 = false;
    } else if (digitalRead(BUTTON2_PIN) == 1 && prevState2 == 0) {
        digitalWrite(RELAY_PIN, LOW);
        prevState2 = true;
    }

    // stop watering if should not pump or if the burst of 1 second is over
    if (!pump || now - pumpPrev >= 1000L) {
        digitalWrite(RELAY_PIN, LOW);
        pumpPrev = now;
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