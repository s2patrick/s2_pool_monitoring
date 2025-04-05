#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Ezo_i2c.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <EEPROM.h>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFiClientSecure.h>

// Version und aktuelles Datum
const char* version = "24.7.28"; // Jahr.Monat.Iteration
const char* releaseDate = "24.07.2024";

WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;
const unsigned long HOTSPOT_DURATION = 15 * 60 * 1000; // 15 Minuten

// Sensoren
Ezo_board PH = Ezo_board(99, "PH");
Ezo_board ORP = Ezo_board(98, "ORP");
Ezo_board RTD = Ezo_board(102, "RTD");

unsigned long previousReconnectAttempt = 0;
const unsigned long reconnectInterval = 60000;

bool apModeActive = false;
unsigned long apModeStartTime = 0;

String lastCalibrationPHLow = "Keine Kalibrierung";
String lastCalibrationPHMid = "Keine Kalibrierung";
String lastCalibrationPHHigh = "Keine Kalibrierung";
String lastCalibrationORP = "Keine Kalibrierung";
String lastCalibrationTemp = "Keine Kalibrierung";

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600 * 2, 60000); // Zeitzone UTC+2

IPAddress requestedIP;

bool autoUpdateEnabled = true;
unsigned long lastUpdateCheck = 0;
const unsigned long updateInterval = 7 * 24 * 60 * 60 * 1000; // 1 Woche

// Root CA Zertifikat
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEozCCBEmgAwIBAgIQTij3hrZsGjuULNLEDrdCpTAKBggqhkjOPQQDAjCBjzEL\n" \
"MAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE\n" \
"BxMHU2FsZm9yZDEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5T\n" \
"ZWN0aWdvIEVDQyBEb21haW4gVmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMB4X\n" \
"DTI0MDMwNzAwMDAwMFoXDTI1MDMwNzIzNTk1OVowFTETMBEGA1UEAxMKZ2l0aHVi\n" \
"LmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABARO/Ho9XdkY1qh9mAgjOUkW\n" \
"mXTb05jgRulKciMVBuKB3ZHexvCdyoiCRHEMBfFXoZhWkQVMogNLo/lW215X3pGj\n" \
"ggL+MIIC+jAfBgNVHSMEGDAWgBT2hQo7EYbhBH0Oqgss0u7MZHt7rjAdBgNVHQ4E\n" \
"FgQUO2g/NDr1RzTK76ZOPZq9Xm56zJ8wDgYDVR0PAQH/BAQDAgeAMAwGA1UdEwEB\n" \
"/wQCMAAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMEkGA1UdIARCMEAw\n" \
"NAYLKwYBBAGyMQECAgcwJTAjBggrBgEFBQcCARYXaHR0cHM6Ly9zZWN0aWdvLmNv\n" \
"bS9DUFMwCAYGZ4EMAQIBMIGEBggrBgEFBQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0\n" \
"dHA6Ly9jcnQuc2VjdGlnby5jb20vU2VjdGlnb0VDQ0RvbWFpblZhbGlkYXRpb25T\n" \
"ZWN1cmVTZXJ2ZXJDQS5jcnQwIwYIKwYBBQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3Rp\n" \
"Z28uY29tMIIBgAYKKwYBBAHWeQIEAgSCAXAEggFsAWoAdwDPEVbu1S58r/OHW9lp\n" \
"LpvpGnFnSrAX7KwB0lt3zsw7CAAAAY4WOvAZAAAEAwBIMEYCIQD7oNz/2oO8VGaW\n" \
"WrqrsBQBzQH0hRhMLm11oeMpg1fNawIhAKWc0q7Z+mxDVYV/6ov7f/i0H/aAcHSC\n" \
"Ii/QJcECraOpAHYAouMK5EXvva2bfjjtR2d3U9eCW4SU1yteGyzEuVCkR+cAAAGO\n" \
"Fjrv+AAABAMARzBFAiEAyupEIVAMk0c8BVVpF0QbisfoEwy5xJQKQOe8EvMU4W8C\n" \
"IGAIIuzjxBFlHpkqcsa7UZy24y/B6xZnktUw/Ne5q5hCAHcATnWjJ1yaEMM4W2zU\n" \
"3z9S6x3w4I4bjWnAsfpksWKaOd8AAAGOFjrv9wAABAMASDBGAiEA+8OvQzpgRf31\n" \
"uLBsCE8ktCUfvsiRT7zWSqeXliA09TUCIQDcB7Xn97aEDMBKXIbdm5KZ9GjvRyoF\n" \
"9skD5/4GneoMWzAlBgNVHREEHjAcggpnaXRodWIuY29tgg53d3cuZ2l0aHViLmNv\n" \
"bTAKBggqhkjOPQQDAgNIADBFAiEAru2McPr0eNwcWNuDEY0a/rGzXRfRrm+6XfZe\n" \
"SzhYZewCIBq4TUEBCgapv7xvAtRKdVdi/b4m36Uyej1ggyJsiesA\n" \
"-----END CERTIFICATE-----\n";

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  EEPROM.begin(512);
  WiFi.mode(WIFI_AP_STA);

  // Einmaligen WiFi-Reset auskommentiert
  // handleWiFiReset();

  // Default IP address in case of factory reset
  if (EEPROM.read(64) == 0) {
    EEPROM.writeString(64, "192.168.0.111");
    EEPROM.commit();
  }

  // Default auto-update setting
  if (EEPROM.read(128) == 0) {
    EEPROM.write(128, 1); // Enable auto-update by default
    EEPROM.commit();
  } else {
    autoUpdateEnabled = EEPROM.read(128) == 1;
  }

  Serial.println("Versuche, eine Verbindung zum WiFi herzustellen...");

  if (!connectToWiFi()) {
    Serial.println("Konnte keine Verbindung zum WiFi herstellen.");
  }

  startAPMode();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/calibrategui", HTTP_GET, handleCalibratePage); // Neue Route fuer Kalibrierungsseite
  server.on("/calibrate", HTTP_GET, handleCalibration); // Route fuer Kalibrierung via URL
  server.on("/calibrate", HTTP_POST, handleCalibration); // Route fuer Kalibrierung via POST
  server.on("/reset", HTTP_POST, handleReset); // Route fuer Zuruecksetzen
  server.on("/help", HTTP_GET, handleHelp);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/wifireset", HTTP_GET, handleWiFiReset);
  server.on("/factoryreset", HTTP_POST, handleFactoryReset);
  server.on("/sensors", HTTP_GET, handleSensors);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP-Server gestartet.");

  timeClient.begin();
  timeClient.update();

  if (autoUpdateEnabled) {
    scheduleAutoUpdate();
  }
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
  timeClient.update();

  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousReconnectAttempt >= reconnectInterval) {
      Serial.println("Verbindung verloren. Versuche erneut zu verbinden...");
      previousReconnectAttempt = currentMillis;
      connectToWiFi();
    }
  }

  if (apModeActive && millis() - apModeStartTime >= HOTSPOT_DURATION) {
    Serial.println("Hotspot-Dauer abgelaufen, Hotspot wird deaktiviert...");
    WiFi.softAPdisconnect(true);
    apModeActive = false;
  }

  if (autoUpdateEnabled) {
    checkForAutoUpdate();
  }
}

bool connectToWiFi() {
  char ssid[32];
  char password[32];
  char ip[16];
  EEPROM.get(0, ssid);
  EEPROM.get(32, password);
  EEPROM.get(64, ip);

  if (strlen(ssid) == 0 || strlen(password) == 0) {
    Serial.println("Keine gespeicherten WiFi-Daten gefunden.");
    return false;
  }

  requestedIP.fromString(ip);
  Serial.print("Verbinde mit SSID: ");
  Serial.println(ssid);
  Serial.print("Mit der IP-Adresse: ");
  Serial.println(ip);

  WiFi.config(requestedIP);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // 20 Sekunden warten
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden, IP-Adresse: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nVerbindung zum WiFi fehlgeschlagen.");
    return false;
  }
}

void startAPMode() {
  IPAddress local_ip(192, 168, 6, 1);
  IPAddress gateway(192, 168, 6, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP("s2_Pool_AccessPoint");
  dnsServer.start(DNS_PORT, "*", local_ip);
  Serial.println("Hotspot gestartet, IP-Adresse: ");
  Serial.println(local_ip);
  apModeActive = true;
  apModeStartTime = millis();
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Willkommen zum Pool Monitoring!</h1>";
  html += "<form action='/config' method='get'><button type='submit'>WiFi konfigurieren</button></form><br><br>";
  html += "<form action='/update' method='get'><button type='submit'>Update Software</button></form><br><br>";
  html += "<form action='/help' method='get'><button type='submit'>Hilfe</button></form><br><br>";
  html += "<form action='/sensors' method='get'><button type='submit'>Messwerte</button></form><br><br>";
  html += "<form action='/calibrategui' method='get'><button type='submit'>Kalibrieren</button></form><br><br>"; // Neuer Button fuer Kalibrierung

  html += "<p>Hotspot IP: " + WiFi.softAPIP().toString() + "</p>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<p>WiFi IP: " + WiFi.localIP().toString() + "</p>";
  } else {
    html += "<p>WiFi nicht verbunden</p>";
  }
  html += "<br>";

  html += "<p>Version: " + String(version) + "</p>";
  html += "<p>Datum: " + String(releaseDate) + "</p>";
  html += "<br>";
  html += "<p>(c) s2 - seidl solutions</p>";
  html += "<p>www.seidl-solutions.at</p>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSensors() {
  timeClient.update();
  struct tm *timeinfo;
  time_t now = timeClient.getEpochTime();
  timeinfo = localtime(&now);

  char timeString[64];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeinfo);

  float phValue = getSensorReading(PH);
  float orpValue = getSensorReading(ORP);
  float tempValue = getSensorReading(RTD);

  String html = "<html><body>";
  html += "<h1>Messwerte</h1>";
  html += "<p>pH-Wert: " + String(phValue, 1) + " (pH-Wert Exakt: " + String(phValue) + ")</p>";
  html += "<p>ORP-Wert: " + String((int)orpValue) + " (ORP-Wert Exakt: " + String(orpValue) + ")</p>";
  html += "<p>Temperatur: " + String((int)tempValue) + " (Temperatur Exakt: " + String(tempValue) + ")</p>";
  html += "<br>";
  html += "<p>Letzte Aktualisierung: " + String(timeString) + " UTC+2</p>";
  html += "<p>Letzte pH Low Kalibrierung: " + lastCalibrationPHLow + "</p>";
  html += "<p>Letzte pH Mid Kalibrierung: " + lastCalibrationPHMid + "</p>";
  html += "<p>Letzte pH High Kalibrierung: " + lastCalibrationPHHigh + "</p>";
  html += "<p>Letzte ORP Kalibrierung: " + lastCalibrationORP + "</p>";
  html += "<p>Letzte Temperatur Kalibrierung: " + lastCalibrationTemp + "</p>";
  html += "<br><br><form action='/sensors' method='get'><button type='submit'>Aktualisieren</button></form><br><br>";
  html += "<form action='/' method='get'><button type='submit'>Zurueck</button></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

float getSensorReading(Ezo_board& sensor) {
  sensor.send_read_cmd();
  delay(1000);

  if (sensor.receive_read_cmd() == Ezo_board::SUCCESS) {
    return sensor.get_last_received_reading();
  } else {
    return NAN;
  }
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleCalibratePage() {
  String html = "<html><body>";
  html += "<h1>Kalibrierung der Sensoren</h1>";
  
  html += "<h2>pH-Sensor</h2>";
  html += "<form action='/calibrate' method='post'>";
  html += "Low (4.0): <input type='text' name='ph_low'><input type='submit' value='Kalibrieren'><br>";
  html += "</form>";
  html += "<form action='/reset' method='post'>";
  html += "<input type='hidden' name='sensor' value='PH_LOW'><input type='submit' value='Zuruecksetzen'><br>";
  html += "</form>";

  html += "<form action='/calibrate' method='post'>";
  html += "Mid (7.0): <input type='text' name='ph_mid'><input type='submit' value='Kalibrieren'><br>";
  html += "</form>";
  html += "<form action='/reset' method='post'>";
  html += "<input type='hidden' name='sensor' value='PH_MID'><input type='submit' value='Zuruecksetzen'><br>";
  html += "</form>";

  html += "<form action='/calibrate' method='post'>";
  html += "High (10.0): <input type='text' name='ph_high'><input type='submit' value='Kalibrieren'><br>";
  html += "</form>";
  html += "<form action='/reset' method='post'>";
  html += "<input type='hidden' name='sensor' value='PH_HIGH'><input type='submit' value='Zuruecksetzen'><br>";
  html += "</form>";

  html += "<h2>ORP-Sensor</h2>";
  html += "<form action='/calibrate' method='post'>";
  html += "Wert: <input type='text' name='orp'><input type='submit' value='Kalibrieren'><br>";
  html += "</form>";
  html += "<form action='/reset' method='post'>";
  html += "<input type='hidden' name='sensor' value='ORP'><input type='submit' value='Zuruecksetzen'><br>";
  html += "</form>";

  html += "<h2>Temperatur-Sensor</h2>";
  html += "<form action='/calibrate' method='post'>";
  html += "Wert: <input type='text' name='temp'><input type='submit' value='Kalibrieren'><br>";
  html += "</form>";
  html += "<form action='/reset' method='post'>";
  html += "<input type='hidden' name='sensor' value='TEMP'><input type='submit' value='Zuruecksetzen'><br>";
  html += "</form>";

  html += "<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCalibration() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("ph_low")) {
      String phLow = server.arg("ph_low");
      PH.send_cmd(("cal,low," + phLow).c_str());
      lastCalibrationPHLow = "Kalibrierung durchgeführt am " + getTimeString();
    }
    if (server.hasArg("ph_mid")) {
      String phMid = server.arg("ph_mid");
      PH.send_cmd(("cal,mid," + phMid).c_str());
      lastCalibrationPHMid = "Kalibrierung durchgeführt am " + getTimeString();
    }
    if (server.hasArg("ph_high")) {
      String phHigh = server.arg("ph_high");
      PH.send_cmd(("cal,high," + phHigh).c_str());
      lastCalibrationPHHigh = "Kalibrierung durchgeführt am " + getTimeString();
    }
    if (server.hasArg("orp")) {
      String orp = server.arg("orp");
      ORP.send_cmd(("cal," + orp).c_str());
      lastCalibrationORP = "Kalibrierung durchgeführt am " + getTimeString();
    }
    if (server.hasArg("temp")) {
      String temp = server.arg("temp");
      RTD.send_cmd(("cal," + temp).c_str());
      lastCalibrationTemp = "Kalibrierung durchgeführt am " + getTimeString();
    }

    server.send(200, "text/plain", "Kalibrierung durchgeführt<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>");
  } else if (server.method() == HTTP_GET && server.hasArg("sensor") && server.hasArg("command")) {
    String sensor = server.arg("sensor");
    String command = server.arg("command");

    if (sensor == "PH") {
      PH.send_cmd(command.c_str());
      lastCalibrationPHLow = "Kalibrierung durchgeführt am " + getTimeString();
    } else if (sensor == "ORP") {
      ORP.send_cmd(command.c_str());
      lastCalibrationORP = "Kalibrierung durchgeführt am " + getTimeString();
    } else if (sensor == "RTD") {
      RTD.send_cmd(command.c_str());
      lastCalibrationTemp = "Kalibrierung durchgeführt am " + getTimeString();
    }

    server.send(200, "text/plain", "Kalibrierung durchgeführt<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>");
  } else {
    handleCalibratePage();
  }
}

void handleReset() {
  if (server.method() == HTTP_POST && server.hasArg("sensor")) {
    String sensor = server.arg("sensor");

    if (sensor == "PH_LOW") {
      PH.send_cmd("cal,clear");
      lastCalibrationPHLow = "Kalibrierung zurueckgesetzt am " + getTimeString();
    }
    if (sensor == "PH_MID") {
      PH.send_cmd("cal,clear");
      lastCalibrationPHMid = "Kalibrierung zurueckgesetzt am " + getTimeString();
    }
    if (sensor == "PH_HIGH") {
      PH.send_cmd("cal,clear");
      lastCalibrationPHHigh = "Kalibrierung zurueckgesetzt am " + getTimeString();
    }
    if (sensor == "ORP") {
      ORP.send_cmd("cal,clear");
      lastCalibrationORP = "Kalibrierung zurueckgesetzt am " + getTimeString();
    }
    if (sensor == "TEMP") {
      RTD.send_cmd("cal,clear");
      lastCalibrationTemp = "Kalibrierung zurueckgesetzt am " + getTimeString();
    }

    server.send(200, "text/plain", "Kalibrierung zurueckgesetzt<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>");
  } else {
    server.send(400, "text/plain", "Fehlende Parameter<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>");
  }
}

void handleUpdate() {
  String url = "https://github.com/s2patrick/s2_pool_monitoring/raw/1f33cb79561cc0468000b97c0533e865b6f9dd2f/s2_pool_monitoring.ino.bin";
  int httpCode;
  String newUrl;

  WiFiClientSecure client; // WiFiClientSecure for HTTPS
  HTTPClient http;

  Serial.println("Verbinde mit " + url);

  client.setCACert(rootCACertificate); // Set the root CA certificate
  http.begin(client, url); // Begin with HTTPS client
  httpCode = http.GET();

  while (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_SEE_OTHER || httpCode == HTTP_CODE_TEMPORARY_REDIRECT) {
    newUrl = http.getLocation();
    Serial.println("Umgeleitet zu: " + newUrl);
    http.end();
    http.begin(client, newUrl);
    httpCode = http.GET();
  }

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    size_t totalSize = http.getSize();
    size_t writtenSize = 0;

    if (!Update.begin(totalSize)) {
      Serial.println("Update.begin fehlgeschlagen");
      server.send(500, "text/plain", "Update.begin fehlgeschlagen<br><br><form action='/' method='get'><button type='submit'>Zurück</button></form>");
      return;
    }

    uint8_t buff[128] = { 0 };
    while (http.connected() && (writtenSize < totalSize || totalSize == -1)) {
      size_t size = stream->available();
      if (size) {
        size = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        Update.write(buff, size);
        writtenSize += size;
      }
      delay(1);
    }

    if (Update.end(true)) {
      Serial.println("Update erfolgreich.");
      server.send(200, "text/plain", "Update erfolgreich<br><br><form action='/' method='get'><button type='submit'>Zurück</button></form>");
    } else {
      Serial.println("Update fehlgeschlagen: " + String(Update.getError()));
      server.send(500, "text/plain", "Update fehlgeschlagen: " + String(Update.getError()) + "<br><br><form action='/' method='get'><button type='submit'>Zurück</button></form>");
    }
  } else {
    Serial.println("Update fehlgeschlagen: HTTP error: " + String(httpCode));
    server.send(500, "text/plain", "Update fehlgeschlagen: HTTP error: " + String(httpCode) + "<br><br><form action='/' method='get'><button type='submit'>Zurück</button></form>");
  }
  http.end();
}

void handleHelp() {
  String ip = WiFi.localIP().toString();
  String response = "<html><body>";
  response += "<h1>Hilfe</h1>";
  response += "<p>Verfuegbare Befehle:</p>";
  response += "<ul>";
  response += "<li>/ - Zeigt aktuelle Sensorwerte an</li>";
  response += "<li>/calibrate - Kalibrierung der Sensoren</li>";
  response += "<li>  Parameter: sensor (PH, ORP, RTD), command (Kalibrierbefehl)</li>";
  response += "<li>  Beispiel: http://" + ip + "/calibrate?sensor=PH&command=cal,mid,7</li>";
  response += "<li>/help - Zeigt diese Hilfeseite an</li>";
  response += "<li>/update - Fuehrt ein Firmware-Update durch</li>";
  response += "<li>  Beispiel: http://" + ip + "/update</li>";
  response += "<li>/config - Oeffnet die Konfigurationsseite</li>";
  response += "<li>/wifireset - Setzt die WiFi-Einstellungen zurueck und startet den Hotspot</li>";
  response += "</ul>";
  response += "<br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form>";
  response += "</body></html>";

  server.send(200, "text/html", response);
}

void handleConfig() {
  char ssid[32];
  char ip[16];
  EEPROM.get(0, ssid);
  EEPROM.get(64, ip);

  String html = "<html><body><form action='/config' method='post'>";
  html += "SSID: <input type='text' name='ssid' value='" + String(ssid) + "'><br>";
  html += "Passwort: <input type='password' name='password'><br>";
  html += "<br>";
  html += "Bevorzugte IP-Adresse: <input type='text' name='ip' value='" + String(ip) + "'><br>";
  html += "<br>";
  html += "<label for='autoupdate'>Automatische Updates:</label>";
  html += "<input type='checkbox' id='autoupdate' name='autoupdate' " + String(autoUpdateEnabled ? "checked" : "") + "><br>";
  html += "<br>";
  html += "<input type='submit' value='Speichern'>";
  html += "</form>";
  html += "<form action='/factoryreset' method='post'>";
  html += "<input type='submit' value='Auf Werkseinstellungen zuruecksetzen'>";
  html += "</form><br><br><form action='/' method='get'><button type='submit'>Zurueck</button></form></body></html>";

  server.send(200, "text/html", html);
}

void handleConfigPost() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("ip")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String ip = server.arg("ip");
    bool autoupdate = server.hasArg("autoupdate");

    Serial.print("Speichere SSID: ");
    Serial.println(ssid);
    Serial.print("Speichere Passwort: ");
    Serial.println(password);
    Serial.print("Speichere IP-Adresse: ");
    Serial.println(ip);
    Serial.print("Automatische Updates: ");
    Serial.println(autoupdate ? "Aktiviert" : "Deaktiviert");

    EEPROM.writeString(0, ssid);
    EEPROM.writeString(32, password);
    EEPROM.writeString(64, ip);
    EEPROM.write(128, autoupdate ? 1 : 0);
    EEPROM.commit();

    autoUpdateEnabled = autoupdate;

    server.send(200, "text/plain", "Einstellungen gespeichert. Neustarten...");
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Fehlende Parameter<br><br><form action='/' method='get'><button type='submit'>Zurueck</button>");
  }
}

void handleWiFiReset() {
  for (int i = 0; i < 128; i++) {
    EEPROM.write(i, 0); // WiFi-Einstellungen loeschen
  }
  EEPROM.commit();
  Serial.println("WiFi-Einstellungen zurueckgesetzt. Neustarten...");
}

void handleFactoryReset() {
  handleWiFiReset();
}

String getTimeString() {
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

String getCurrentDateString() {
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
  return String(buffer);
}

void scheduleAutoUpdate() {
  time_t now = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&now);

  if (timeinfo->tm_wday == 3 && timeinfo->tm_hour == 3) {
    handleUpdate();
    lastUpdateCheck = millis();
  }
}

void checkForAutoUpdate() {
  if (millis() - lastUpdateCheck >= updateInterval) {
    scheduleAutoUpdate();
  }
}
