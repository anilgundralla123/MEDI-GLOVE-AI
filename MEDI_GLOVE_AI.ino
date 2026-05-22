#include <WiFi.h>
#include <PubSubClient.h>

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "MAX30105.h"
#include <TinyGPSPlus.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "F14";
const char* wifiPassword = "SAPP1234";

// =====================================================
// THINGSPEAK MQTT
// =====================================================

const char* mqttServer = "mqtt3.thingspeak.com";

const int mqttPort = 1883;

const char* mqttUser = "IBEDHioNEyknMh41EyMiHAo";

const char* mqttClientID = "IBEDHioNEyknMh41EyMiHAo";

const char* mqttPassword = "oKWGB1DKus1hGERxUHUqDwU4";

const char* publishTopic = "channels/3376776/publish";

// =====================================================
// MQTT
// =====================================================

WiFiClient espClient;

PubSubClient client(espClient);

// =====================================================
// MPU6050
// =====================================================

const int MPU_ADDR = 0x68;

int16_t AcX, AcY, AcZ;
int16_t GyX, GyY, GyZ;

TwoWire I2CMPU = TwoWire(0);

// =====================================================
// MAX30102
// =====================================================

MAX30105 sensor;

TwoWire I2CMAX = TwoWire(1);

long irValue;

long lastBeat = 0;

float bpmAvg = 0;

bool rising = false;

long prevIR = 0;

// =====================================================
// DS18B20
// =====================================================

#define TEMP_PIN 2

OneWire oneWire(TEMP_PIN);

DallasTemperature tempSensor(&oneWire);

// =====================================================
// GSR
// =====================================================

#define GSR_PIN 3

// =====================================================
// ECG
// =====================================================

#define ECG_PIN 6

// =====================================================
// EMG
// =====================================================

#define EMG_PIN 7

// =====================================================
// GPS
// =====================================================

TinyGPSPlus gps;

HardwareSerial gpsSerial(2);

#define GPS_RX 9
#define GPS_TX 10

// =====================================================
// WIFI CONNECT
// =====================================================

void connectWiFi() {

  Serial.println("==================================");
  Serial.println("CONNECTING TO WIFI");
  Serial.println("==================================");

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("WIFI CONNECTED");

  Serial.print("IP ADDRESS: ");

  Serial.println(WiFi.localIP());

  Serial.println();
}

// =====================================================
// MQTT CONNECT
// =====================================================

void connectMQTT() {

  while (!client.connected()) {

    Serial.println("CONNECTING MQTT...");

    if (client.connect(mqttClientID, mqttUser, mqttPassword)) {

      Serial.println("MQTT CONNECTED");

    } else {

      Serial.print("MQTT FAILED: ");

      Serial.println(client.state());

      delay(2000);
    }
  }

  Serial.println();
}

// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println("FULL HEALTH MONITORING SYSTEM");
  Serial.println("==================================");

  // =====================================================
  // WIFI
  // =====================================================

  connectWiFi();

  // =====================================================
  // MQTT
  // =====================================================

  client.setServer(mqttServer, mqttPort);

  connectMQTT();

  // =====================================================
  // MPU6050 INIT
  // =====================================================

  I2CMPU.begin(18, 17);

  I2CMPU.setClock(100000);

  I2CMPU.beginTransmission(MPU_ADDR);

  I2CMPU.write(0x6B);

  I2CMPU.write(0x00);

  I2CMPU.endTransmission(true);

  Serial.println("MPU6050 READY");

  // =====================================================
  // MAX30102 INIT
  // =====================================================

  I2CMAX.begin(4, 5);

  I2CMAX.setClock(50000);

  if (!sensor.begin(I2CMAX)) {

    Serial.println("MAX30102 NOT DETECTED");

    while (1);
  }

  sensor.setup();

  Serial.println("MAX30102 READY");

  // =====================================================
  // TEMP SENSOR
  // =====================================================

  tempSensor.begin();

  Serial.println("DS18B20 READY");

  // =====================================================
  // GPS INIT
  // =====================================================

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  Serial.println("GPS STARTED");

  // =====================================================
  // ADC PINS
  // =====================================================

  pinMode(GSR_PIN, INPUT);

  pinMode(ECG_PIN, INPUT);

  pinMode(EMG_PIN, INPUT);

  // =====================================================

  Serial.println("ALL SYSTEMS READY");

  Serial.println();
}

// =====================================================

void loop() {

  // =====================================================
  // MQTT CHECK
  // =====================================================

  if (!client.connected()) {

    connectMQTT();
  }

  client.loop();

  // =====================================================
  // GPS READ
  // =====================================================

  while (gpsSerial.available()) {

    char c = gpsSerial.read();

    gps.encode(c);
  }

  // =====================================================
  // MPU6050 READ
  // =====================================================

  I2CMPU.beginTransmission(MPU_ADDR);

  I2CMPU.write(0x3B);

  I2CMPU.endTransmission(false);

  I2CMPU.requestFrom(MPU_ADDR, 14, true);

  if (I2CMPU.available() == 14) {

    AcX = I2CMPU.read() << 8 | I2CMPU.read();
    AcY = I2CMPU.read() << 8 | I2CMPU.read();
    AcZ = I2CMPU.read() << 8 | I2CMPU.read();

    I2CMPU.read();
    I2CMPU.read();

    GyX = I2CMPU.read() << 8 | I2CMPU.read();
    GyY = I2CMPU.read() << 8 | I2CMPU.read();
    GyZ = I2CMPU.read() << 8 | I2CMPU.read();
  }

  // =====================================================
  // GSR
  // =====================================================

  long gsrSum = 0;

  for (int i = 0; i < 10; i++) {

    gsrSum += analogRead(GSR_PIN);

    delay(5);
  }

  int gsr = gsrSum / 10;

  // =====================================================
  // ECG
  // =====================================================

  int ecg = analogRead(ECG_PIN);

  // =====================================================
  // EMG
  // =====================================================

  int emg = analogRead(EMG_PIN);

  // =====================================================
  // TEMPERATURE
  // =====================================================

  tempSensor.requestTemperatures();

  float tempC = tempSensor.getTempCByIndex(0);

  // =====================================================
  // MAX30102
  // =====================================================

  irValue = sensor.getIR();

  bool beat = false;

  if (irValue > prevIR && !rising) {

    rising = true;
  }

  if (irValue < prevIR && rising) {

    beat = true;

    rising = false;
  }

  prevIR = irValue;

  if (beat) {

    long delta = millis() - lastBeat;

    lastBeat = millis();

    float bpm = 60.0 / (delta / 1000.0);

    if (bpm > 40 && bpm < 180) {

      bpmAvg = (bpmAvg + bpm) / 2.0;
    }
  }

  // =====================================================
  // GPS VALUES
  // =====================================================

  float latitude = 0.0;

  float longitude = 0.0;

  if (gps.location.isValid()) {

    latitude = gps.location.lat();

    longitude = gps.location.lng();
  }

  // =====================================================
  // MOTION
  // =====================================================

  float motion =
    sqrt((AcX * AcX) +
         (AcY * AcY) +
         (AcZ * AcZ));

  // =====================================================
  // RISK SCORE
  // =====================================================

  int riskScore = 0;

  if (bpmAvg > 120)
    riskScore += 20;

  if (tempC > 38)
    riskScore += 20;

  if (gsr > 3000)
    riskScore += 20;

  // =====================================================
  // SERIAL OUTPUT
  // =====================================================

  Serial.println("==================================");
  Serial.println("FULL HEALTH DATA");
  Serial.println("==================================");

  Serial.print("Accel X: ");
  Serial.print(AcX);
  Serial.print(" Y: ");
  Serial.print(AcY);
  Serial.print(" Z: ");
  Serial.println(AcZ);

  Serial.print("Gyro X: ");
  Serial.print(GyX);
  Serial.print(" Y: ");
  Serial.print(GyY);
  Serial.print(" Z: ");
  Serial.println(GyZ);

  Serial.print("GSR: ");
  Serial.println(gsr);

  Serial.print("ECG: ");
  Serial.println(ecg);

  Serial.print("EMG: ");
  Serial.println(emg);

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" C");

  Serial.print("IR Value: ");
  Serial.println(irValue);

  Serial.print("BPM: ");
  Serial.println((int)bpmAvg);

  if (gps.location.isValid()) {

    Serial.print("Latitude: ");
    Serial.println(latitude, 6);

    Serial.print("Longitude: ");
    Serial.println(longitude, 6);

  } else {

    Serial.println("GPS FIX NOT AVAILABLE");
  }

  // =====================================================
  // MQTT PAYLOAD
  // =====================================================

  String payload =
    "field1=" + String((int)bpmAvg) +
    "&field2=" + String(irValue) +
    "&field3=" + String(tempC) +
    "&field4=" + String(gsr) +
    "&field5=" + String(motion) +
    "&field6=" + String(latitude, 6) +
    "&field7=" + String(longitude, 6) +
    "&field8=" + String(riskScore);

  Serial.println("----------------------------------");
  Serial.println("MQTT PAYLOAD");
  Serial.println("----------------------------------");

  Serial.println(payload);

  // =====================================================
  // SEND MQTT
  // =====================================================

  Serial.println("SENDING DATA TO THINGSPEAK...");

  if (client.publish(publishTopic, payload.c_str())) {

    Serial.println("DATA SENT SUCCESSFULLY");

  } else {

    Serial.println("MQTT SEND FAILED");
  }

  Serial.println("==================================");
  Serial.println();

  delay(15000);
}