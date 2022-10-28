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
const char template_mode_ap[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible" content="IE=edge"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP8266 - Mode AP</title><link href="./index.css" rel="stylesheet"><style>html{color:rgb(51 65 85);font-family:sans-serif,Arial,Helvetica;user-select:none}.title{text-align:center;font-size:20px;margin-bottom:25px}.viewer{position:absolute;top:0;left:0;bottom:0;right:0;background-color:#f1f5f9;display:flex;justify-content:center;align-items:center;padding:0 2rem;flex-direction:column}.container{padding:2rem;background-color:#fff;border-radius:1rem;box-shadow:rgba(149,157,165,.2) 0 8px 24px;max-width:380px;width:100%;box-sizing:border-box}.form{display:grid;grid-template-rows:repeat(2,minmax(0,1fr));gap:1rem}.form-fill{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));align-items:center}.form-fill-label{font-weight:700;font-size:14px}.form-fill-input{grid-column:span 2/span 2;padding:12px 16px;outline:0;border-radius:1rem;border:none;background-color:#f1f5f9}.form-fill-input:focus{box-shadow:rgba(0,0,0,.02) 0 1px 3px 0,rgba(27,31,35,.15) 0 0 0 1px}.wrap-btn{display:flex;justify-content:flex-end;margin-top:1rem}.btn-config{padding:10px 14px;border-radius:14px;outline:0;border:none;font-weight:700;background-color:#f1f5f9;cursor:pointer;transition:box-shadow .1s ease}.btn-config:active{box-shadow:rgba(0,0,0,.02) 0 1px 3px 0,rgba(27,31,35,.15) 0 0 0 1px}.container-space{margin-top:1rem}.copyright{text-align:center;font-weight:700;font-size:14px}</style></head><body><div class="viewer"><div class="container"><h2 class="title">Cấu hình kết nối WIFI</h2><div class="form"><div class="form-fill"><label class="form-fill-label" for="ssid">Tên</label> <input class="form-fill-input" placeholder="ví dụ: miru" type="text" name="ssid" id="ssid"></div><div class="form-fill"><label class="form-fill-label" for="password">Mật khẩu</label> <input class="form-fill-input" placeholder="-- mật khẩu sẽ bị ẩn --" type="password" name="password" id="password"></div></div><div class="wrap-btn"><button class="btn-config">Cấu hình</button></div></div><div class="container container-space"><p class="copyright">Bản quyền của @miru ngày 29/10/2022</p></div></div></body></html>
)rawliteral";

void setup()
{
  Serial.begin(115200);
  runner.startNow();
}

void loop()
{
  // put your main code here, to run repeatedly:
  runner.execute();
}

void setupWifiModeAP()
{
  // ACTIVE MODE AP
  while (!WiFi.softAP(mode_ap_ssid, mode_ap_pass))
    ;
  IPAddress ip_mode_ap = WiFi.softAPIP();
  Serial.printf("[WIFI-AP]( IS SETUP WITH IP: ");
  Serial.print(ip_mode_ap);
  Serial.printf(" )\n");
  while (!miruSetupWebServerAPMode.enable())
    ;
}

void setupWebserverModeAP()
{
  if (miruSetupWebServerAPMode.isFirstIteration())
  {

    server_mode_ap.on("/", []()
                      { server_mode_ap.send(200, "text/html", template_mode_ap); });

    // START WEBSERVER
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}
