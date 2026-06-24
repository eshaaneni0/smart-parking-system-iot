#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================= WIFI =================
const char* ssid = "eshaan";
const char* password = "eshaan1234";

// ================= MQTT =================
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

// ================= SERVO =================
Servo gateServo;

#define SERVO_PIN 21

// ================= ENTRY / EXIT =================
#define entryTrig 14
#define entryEcho 18

#define exitTrig 19
#define exitEcho 4

// ================= PARKING SLOTS =================
#define slot1Trig 23
#define slot1Echo 25

#define slot2Trig 26
#define slot2Echo 27

#define slot3Trig 32
#define slot3Echo 33

// ================= SETTINGS =================
const int entryExitThreshold = 4;
const int slotThreshold = 5;
const int maxSlots = 3;

// ================= STATE =================
bool gateOpen = false;
unsigned long gateTimer = 0;

int carCount = 0;

unsigned long lastEntryTime = 0;
unsigned long lastExitTime = 0;

// ================= WIFI =================
void setup_wifi() {
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// ================= MQTT =================
void reconnect() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("ESP32_Parking_System")) {
            Serial.println("Connected!");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            delay(2000);
        }
    }
}

// ================= DISTANCE =================
float readDistance(int trig, int echo) {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    digitalWrite(trig, HIGH);
    delayMicroseconds(10);

    digitalWrite(trig, LOW);

    long duration = pulseIn(echo, HIGH, 30000);
    if (duration == 0) return 999;

    return duration * 0.034 / 2;
}

// ================= SLOT DETECTION =================
// ================= SLOT DETECTION =================
bool isOccupied(int trig, int echo) {

    float distance = readDistance(trig, echo);

    Serial.print("Distance: ");
    Serial.println(distance);

    return distance < slotThreshold;
}

// ================= SETUP =================
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);

    pinMode(entryTrig, OUTPUT); pinMode(entryEcho, INPUT);
    pinMode(exitTrig, OUTPUT);  pinMode(exitEcho, INPUT);

    pinMode(slot1Trig, OUTPUT); pinMode(slot1Echo, INPUT);
    pinMode(slot2Trig, OUTPUT); pinMode(slot2Echo, INPUT);
    pinMode(slot3Trig, OUTPUT); pinMode(slot3Echo, INPUT);

    

    gateServo.setPeriodHertz(50);
    gateServo.attach(SERVO_PIN, 500, 2400);
    gateServo.write(0);

    setup_wifi();
    client.setServer(mqtt_server, 1883);
}

// ================= LOOP =================
void loop() {

    if (!client.connected()) reconnect();
    client.loop();

    // ================= SLOT STATUS =================
    bool slot1 = isOccupied(slot1Trig, slot1Echo);
    delay(40);
    bool slot2 = isOccupied(slot2Trig, slot2Echo);
    delay(40);
    bool slot3 = isOccupied(slot3Trig, slot3Echo);

    bool isFull = (slot1 && slot2 && slot3);


    // ================= ENTRY =================
   float entryDist = readDistance(entryTrig, entryEcho);

if (entryDist < entryExitThreshold && entryDist > 1 &&
    carCount < maxSlots &&
    !gateOpen &&
    millis() - lastEntryTime > 3000) {

    Serial.println("ENTRY DETECTED");

    gateServo.write(90);
    gateOpen = true;
    gateTimer = millis();

    carCount++;
    lastEntryTime = millis();
}

    // ================= EXIT =================
  float exitDist = readDistance(exitTrig, exitEcho);

if (exitDist < entryExitThreshold && exitDist > 2 &&
    carCount > 0 &&
    !gateOpen &&
    millis() - lastExitTime > 3000) {

    Serial.println("EXIT DETECTED");

    gateServo.write(90);
    gateOpen = true;
    gateTimer = millis();

    carCount--;
    lastExitTime = millis();
}


    // ================= CLOSE GATE =================
    if (gateOpen && millis() - gateTimer > 3000) {
        gateServo.write(0);
        gateOpen = false;
    }


Serial.println("------ SLOT STATUS ------");
Serial.print("Slot 1: "); Serial.println(slot1 ? "OCCUPIED" : "FREE");
Serial.print("Slot 2: "); Serial.println(slot2 ? "OCCUPIED" : "FREE");
Serial.print("Slot 3: "); Serial.println(slot3 ? "OCCUPIED" : "FREE");

Serial.print("Car Count: "); Serial.println(carCount);

Serial.print("Entry Distance: "); Serial.println(entryDist);
Serial.print("Exit Distance: "); Serial.println(exitDist);

Serial.println("--------------------------");





    // ================= MQTT =================
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate > 2000) {

        client.publish("parking/slots/1", slot1 ? "OCCUPIED" : "FREE");
        client.publish("parking/slots/2", slot2 ? "OCCUPIED" : "FREE");
        client.publish("parking/slots/3", slot3 ? "OCCUPIED" : "FREE");

        client.publish("parking/status/full", isFull ? "FULL" : "AVAILABLE");

        char countStr[5];
        sprintf(countStr, "%d", carCount);
        client.publish("parking/status/count", countStr);

        lastUpdate = millis();
    }

    delay(100);
}