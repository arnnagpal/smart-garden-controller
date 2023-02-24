#include <DHT.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HardwareSerial.h>
#include <Arduino.h>

#define ONE_WIRE_PIN 7

#define DHT_PIN 2
#define RELAY_PIN 4
#define BUTTON1_PIN 9
#define BUTTON2_PIN 8


#define DHTTYPE DHT11  // DHT 11
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

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
    delay(2000);

    pinMode(BUTTON1_PIN, INPUT);
    pinMode(BUTTON2_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
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

        Serial.print("DHT11 - H: ");
        Serial.print(h);
        Serial.print("% ");
        Serial.print(" T: ");
        Serial.print(t);
        Serial.print("C ");
        Serial.print(" HI: ");
        Serial.print(hic);
        Serial.println("C");

        Serial.print("CMSCR - V: ");
        Serial.println(moistureVal);

        if (tempC == DEVICE_DISCONNECTED_C) {
            Serial.println("Could not read soil temp data.");
            prev = now;
            return;
        }

        Serial.print("ST - V: ");
        Serial.print(tempC);
        Serial.println("C");


        str = String("EH=") + String(h) + String(";ET=") + String(t) + String(";EHI=") + String(hic) + String(";SM=") + String(moistureVal) + String(";ST=") + tempC;
        espSerial.println(str + "@");

        prev = now;
    }
}