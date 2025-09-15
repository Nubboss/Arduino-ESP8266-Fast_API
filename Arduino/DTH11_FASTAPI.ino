#include <DHT.h>
#include <SoftwareSerial.h>

#define DHTPIN 6
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// SoftwareSerial: Arduino RX<-ESP TX on pin2, TX->ESP RX on pin3
SoftwareSerial espSerial(2, 3);

const char* ssid = "wifi";
const char* pass = "pass";

const char* HOST = "ip";
const int   PORT = 8000;
const char* PATH = "/send";

unsigned long wifiTimeout = 30000;
unsigned long httpTimeout = 10000;

void flushSerials() {
  while (Serial.available()) Serial.read();
  while (espSerial.available()) espSerial.read();
}

String readUntil(unsigned long timeout = 5000) {
  unsigned long start = millis();
  String s = "";
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      s += (char)espSerial.read();
    }
  }
  return s;
}

bool sendCmdRaw(const String &cmd, unsigned long timeout = 5000) {
  espSerial.print(cmd);
  espSerial.print("\r\n");
  String resp = readUntil(timeout);
  Serial.print(resp);
  return resp.length() > 0;
}

bool sendCmd(const String &cmd, const String &expect, unsigned long timeout = 5000) {
  espSerial.print(cmd);
  espSerial.print("\r\n");
  String resp = readUntil(timeout);
  Serial.print(resp);
  return resp.indexOf(expect) >= 0;
}

bool connectWiFi() {
  if (!sendCmd("AT", "OK", 2000)) return false;
  sendCmd("ATE0", "OK", 2000);            // отключаем эхо
  sendCmd("AT+CWMODE=1", "OK", 2000);     // station mode
  sendCmd("AT+CWQAP", "OK", 2000);        // disconnect previous
  String cmd = String("AT+CWJAP=\"") + ssid + "\",\"" + pass + "\"";
  return sendCmd(cmd, "WIFI CONNECTED", wifiTimeout) || sendCmd(cmd, "OK", wifiTimeout);
}

bool openTcp(const char* host, int port) {
  sendCmd("AT+CIPCLOSE", "OK", 2000);
  delay(200);
  String cmd = String("AT+CIPSTART=\"TCP\",\"") + host + "\"," + port;
  return sendCmd(cmd, "CONNECT", 10000) || sendCmd(cmd, "OK", 10000);
}

String makeJsonPayload(const char* device, float temperature, float humidity) {
  String s = "{";
  s += "\"device\":\"";
  s += device;
  s += "\",";
  if (isnan(temperature)) s += "\"temperature\":null,";
  else s += "\"temperature\":" + String(temperature, 2) + ",";
  if (isnan(humidity)) s += "\"humidity\":null";
  else s += "\"humidity\":" + String(humidity, 2);
  s += "}";
  return s;
}

String httpRequest(const char* host, int port, const char* path, const String& body) {
  int bodyLen = body.length();
  String req = "";
  req += "POST ";
  req += path;
  req += " HTTP/1.1\r\n";
  req += "Host: ";
  req += host;
  req += ":";
  req += String(port);
  req += "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: ";
  req += String(bodyLen);
  req += "\r\n";
  req += "Connection: close\r\n";
  req += "\r\n"; // blank line before body
  req += body;
  return req;
}

bool waitForChar(char ch, unsigned long timeout) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      char c = espSerial.read();
      Serial.write(c);
      if (c == ch) return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  // SoftwareSerial works reliably at 9600
  espSerial.begin(9600);
  dht.begin();
  delay(1000);
  flushSerials();

  Serial.println("Connecting WiFi...");
  if (!connectWiFi()) {
    Serial.println("WiFi connect failed");
    while (1) { delay(1000); }
  }
  Serial.println("WiFi connected");
}

void sendPayload(const String &json) {
  String req = httpRequest(HOST, PORT, PATH, json);
  int totalLen = req.length();

  if (!openTcp(HOST, PORT)) {
    Serial.println("TCP open failed");
    return;
  }

  // Request CIPSEND with exact byte count
  espSerial.print("AT+CIPSEND=");
  espSerial.println(totalLen);

  // wait for '>' prompt
  if (!waitForChar('>', httpTimeout)) {
    Serial.println("No prompt for CIPSEND");
    sendCmd("AT+CIPCLOSE", "OK", 2000);
    return;
  }

  // send raw request bytes (real CRLF included)
  // use write() to avoid any extra characters
  for (size_t i = 0; i < (size_t)req.length(); ++i) {
    espSerial.write(req[i]);
  }

  // let ESP send
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (espSerial.available()) {
      char c = espSerial.read();
      Serial.write(c);
    }
  }

  // read possible +IPD or HTTP response
  String resp = readUntil(5000);
  Serial.println();
  Serial.println("HTTP resp (collected):");
  Serial.println(resp);

  sendCmd("AT+CIPCLOSE", "OK", 2000);
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) && isnan(t)) {
    Serial.println("DHT read failed");
    delay(5000);
    return;
  }

  String json = makeJsonPayload("arduino_dht11", t, h);
  Serial.print("Payload: ");
  Serial.println(json);

  sendPayload(json);

  delay(5000);
}
