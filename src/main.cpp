#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TaskScheduler.h>

// FUNCTION PROTOTYPE
void setupWifiModeAP();
void setupWebserverModeAP();

// INSTANCE
ESP8266WebServer server_mode_ap(80);
Scheduler runner;

// TASK
Task miruSetupWifiAPMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeAP, &runner, true);
Task miruSetupWebServerAPMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeAP, &runner);

// WIFI MODE - AP
String mode_ap_ssid = "esp8266-miru";
String mode_ap_pass = "44448888";
const char mode_ap_template[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP8266 - Mode AP</title>
</head>
<body>
  <div>
    <h2>Cấu hình kết nối WIFI</h2>
    <div>
      <label for="ssid">Tên</label>
      <input type="text" name="ssid" id="ssid">
    </div>
    <div>
      <label for="password">Mật khẩu</label>
      <input type="text" name="password" id="password">
    </div>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  runner.startNow();
}

void loop() {
  // put your main code here, to run repeatedly:
  runner.execute();
}

void setupWifiModeAP() {
  // ACTIVE MODE AP
  while(!WiFi.softAP(mode_ap_ssid, mode_ap_pass));
  IPAddress ip_mode_ap = WiFi.softAPIP();
  Serial.printf("[WIFI-AP]( IS SETUP WITH IP: ");
  Serial.print(ip_mode_ap);
  Serial.printf(" )\n");
  while(!miruSetupWebServerAPMode.enable());
}

void setupWebserverModeAP() {
  if(miruSetupWebServerAPMode.isFirstIteration()) {
    server_mode_ap.on("/", []() {
      server_mode_ap.send(200, "text/html", mode_ap_template);
    });
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}
