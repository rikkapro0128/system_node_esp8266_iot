#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>

// GLOBAL VARIABLE
size_t eepromSize = 255;

// JSON DOCUMENT
DynamicJsonDocument bufferBodyPaser(8192);

// FUNCTION PROTOTYPE - TASK
void eepromReset();
void setupWifiModeAP();
void setupWebserverModeAP();

// FUNCTION PROTOTYPE - COMPONENT
void eepromWriteConfigWifi(String ssid, String password);
String eepromReadWifi(String key);

// INSTANCE
ESP8266WebServer server_mode_ap(80);
Scheduler runner;

// TASK
Task miruSetupWifiAPMode(TASK_IMMEDIATE, TASK_ONCE, &setupWifiModeAP, &runner);
Task miruSetupWebServerAPMode(TASK_IMMEDIATE, TASK_FOREVER, &setupWebserverModeAP, &runner);

// WIFI MODE - AP
String mode_ap_ssid = "esp8266-miru";
String mode_ap_pass = "44448888";
const char template_mode_ap[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible" content="IE=edge"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP8266 - Mode AP</title><link rel="shortcut icon" href="data:image/svg+xml;base64,PHN2ZyBoZWlnaHQ9IjQ4MHB0IiB2aWV3Qm94PSIwIDAgNDgwIDQ4MCIgd2lkdGg9IjQ4MHB0IiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjxwYXRoIGQ9Im00MzIgMjQwYzAgMTA2LjAzOTA2Mi04NS45NjA5MzggMTkyLTE5MiAxOTJzLTE5Mi04NS45NjA5MzgtMTkyLTE5MiA4NS45NjA5MzgtMTkyIDE5Mi0xOTIgMTkyIDg1Ljk2MDkzOCAxOTIgMTkyem0wIDAiIGZpbGw9IiNjZmQyZmMiLz48cGF0aCBkPSJtMjQwIDQ4MGMtMTMyLjU0Njg3NSAwLTI0MC0xMDcuNDUzMTI1LTI0MC0yNDBzMTA3LjQ1MzEyNS0yNDAgMjQwLTI0MCAyNDAgMTA3LjQ1MzEyNSAyNDAgMjQwYy0uMTQ4NDM4IDEzMi40ODQzNzUtMTA3LjUxNTYyNSAyMzkuODUxNTYyLTI0MCAyNDB6bTAtNDY0Yy0xMjMuNzEwOTM4IDAtMjI0IDEwMC4yODkwNjItMjI0IDIyNHMxMDAuMjg5MDYyIDIyNCAyMjQgMjI0IDIyNC0xMDAuMjg5MDYyIDIyNC0yMjRjLS4xNDA2MjUtMTIzLjY1MjM0NC0xMDAuMzQ3NjU2LTIyMy44NTkzNzUtMjI0LTIyNHptMCAwIiBmaWxsPSIjODY5MGZhIi8+PHBhdGggZD0ibTM1MiAyNDBjMCA2MS44NTU0NjktNTAuMTQ0NTMxIDExMi0xMTIgMTEycy0xMTItNTAuMTQ0NTMxLTExMi0xMTIgNTAuMTQ0NTMxLTExMiAxMTItMTEyIDExMiA1MC4xNDQ1MzEgMTEyIDExMnptMCAwIiBmaWxsPSIjNTE1M2ZmIi8+PC9zdmc+"><style>html{color:rgb(51 65 85);font-family:sans-serif,Arial,Helvetica;user-select:none}.title{text-align:center;font-size:20px;margin-bottom:25px}.viewer{position:absolute;top:0;left:0;bottom:0;right:0;background-color:#f1f5f9;display:flex;justify-content:center;align-items:center;padding:0 2rem;flex-direction:column}.container{padding:2rem;background-color:#fff;border-radius:1rem;box-shadow:rgba(149,157,165,.2) 0 8px 24px;max-width:380px;width:100%;box-sizing:border-box;position:relative;transition:filter .2s ease-in-out}.form{display:grid;grid-template-rows:repeat(2,minmax(0,1fr));gap:1rem}.form-fill{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));align-items:center}.form-fill-label{font-weight:700;font-size:14px}.form-fill-input{grid-column:span 2/span 2;padding:12px 16px;outline:0;border-radius:1rem;border:none;background-color:#f1f5f9}.form-fill-input:focus{box-shadow:rgba(0,0,0,.02) 0 1px 3px 0,rgba(27,31,35,.15) 0 0 0 1px}.wrap-btn{display:flex;justify-content:flex-end;margin-top:1rem}.btn{padding:10px 14px;border-radius:14px;outline:0;border:none;font-weight:700;background-color:#f1f5f9;cursor:pointer;transition:box-shadow .1s ease}.btn:active{box-shadow:rgba(0,0,0,.02) 0 1px 3px 0,rgba(27,31,35,.15) 0 0 0 1px}.container-space{margin-top:1rem}.copyright{text-align:center;font-weight:700;font-size:14px}.state-config.active{z-index:999;opacity:1}.state-config{position:absolute;top:0;left:0;right:0;bottom:0;background-color:#fff;border-radius:1rem;display:flex;justify-content:center;align-items:center;z-index:-1;opacity:0;transition:opacity .2s ease-in}.state-config .state{padding:16px;width:fit-content;border-radius:50%;display:flex;justify-content:center;align-items:center;fill:#fff}.state-config>div{display:flex;flex-direction:column;align-items:center}.state-config-sucess .wrap .state{background-color:#4ade80}.state-config-error .wrap .state{background-color:#de4a4a}.state-config-info .wrap .state{background-color:#b9de4a}.state-config .message{margin-top:2rem;text-align:center;font-size:1rem;padding:0 2rem}.disable{filter:blur(3px);pointer-events:none;overflow:hidden}</style></head><body><div class="viewer"><div class="container"><h2 class="title">Cấu hình kết nối WIFI</h2><div class="form"><div class="form-fill"><label class="form-fill-label" for="ssid">Tên</label> <input class="form-fill-input" placeholder="ví dụ: miru" type="text" name="ssid" id="ssid"></div><div class="form-fill"><label class="form-fill-label" for="password">Mật khẩu</label> <input class="form-fill-input" placeholder="-- mật khẩu sẽ bị ẩn --" type="password" name="password" id="password"></div></div><div class="wrap-btn"><button class="btn btn-config">Cấu hình</button></div><div class="state-config state-config-sucess"><div class="wrap"><div class="state"><svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" id="Capa_1" x="0px" y="0px" viewBox="0 0 507.506 507.506" style="enable-background:new 0 0 507.506 507.506" xml:space="preserve" width="40" height="40"><g><path d="M163.865,436.934c-14.406,0.006-28.222-5.72-38.4-15.915L9.369,304.966c-12.492-12.496-12.492-32.752,0-45.248l0,0   c12.496-12.492,32.752-12.492,45.248,0l109.248,109.248L452.889,79.942c12.496-12.492,32.752-12.492,45.248,0l0,0   c12.492,12.496,12.492,32.752,0,45.248L202.265,421.019C192.087,431.214,178.271,436.94,163.865,436.934z"/></g></svg></div><h3 class="message">Bạn đã cấu hình kết nối tới wifi<br><span>[ miru ]</span><br>thành công</h3></div></div><div class="state-config state-config-error"><div class="wrap"><div class="state"><svg xmlns="http://www.w3.org/2000/svg" id="Outline" viewBox="0 0 24 24" width="40" height="40"><path d="M12,0A12,12,0,1,0,24,12,12.013,12.013,0,0,0,12,0Zm0,22A10,10,0,1,1,22,12,10.011,10.011,0,0,1,12,22Z"/><path d="M12,10H11a1,1,0,0,0,0,2h1v6a1,1,0,0,0,2,0V12A2,2,0,0,0,12,10Z"/><circle cx="12" cy="6.5" r="1.5"/></svg></div><h3 class="message">Có lỗi xảy ra khi cấu hình kết nối tới wifi<br><span>[ miru ]</span></h3></div></div><div class="state-config state-config-info"><div class="wrap"><div class="state"><svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" id="Capa_1" x="0px" y="0px" viewBox="0 0 512 512" style="enable-background:new 0 0 512 512" xml:space="preserve" width="40" height="40"><g><path d="M256,0C114.615,0,0,114.615,0,256s114.615,256,256,256s256-114.615,256-256C511.847,114.678,397.322,0.153,256,0z M256,448   c-106.039,0-192-85.961-192-192S149.961,64,256,64s192,85.961,192,192C447.882,361.99,361.99,447.882,256,448z"/><path d="M256,321.941c17.673,0,32-14.327,32-32V140.608c0-17.673-14.327-32-32-32s-32,14.327-32,32v149.333   C224,307.614,238.327,321.941,256,321.941z"/><circle cx="256.107" cy="373.333" r="32"/></g></svg></div><h3 class="message">Bạn không được bỏ trống dữ liệu</h3><div class="wrap-btn"><button class="btn btn-accept-error">Xác nhận</button></div></div></div></div><div class="container container-space"><p class="copyright">Bản quyền của @miru ngày 29/10/2022</p></div></div></body><script>const eleSSID=document.querySelector(".form-fill-input[name=ssid]"),elePASSWORD=document.querySelector(".form-fill-input[name=password]"),eleBtnConfig=document.querySelector(".btn-config"),eleBtnAcceptInfo=document.querySelector(".btn-accept-error"),eleConfigInfo=document.querySelector(".state-config-info"),eleConfigError=document.querySelector(".state-config-error"),eleConfigSuccess=document.querySelector(".state-config-sucess");async function handleSendConfig(e){try{eleSSID.value.length&&elePASSWORD.value.length?"CONFIGURATION WIFI SUCCESSFULLY"===(await(await fetch("/config",{method:"POST",body:JSON.stringify({ssid:eleSSID.value,password:elePASSWORD.value}),headers:{"Content-Type":"application/json"}})).json()).message&&eleConfigSuccess.classList.add("active"):eleConfigInfo.classList.add("active")}catch(e){console.log(e)}}function handleAcceptInfo(){eleConfigInfo.classList.remove("active")}eleBtnConfig.addEventListener("click",handleSendConfig),eleBtnAcceptInfo.addEventListener("click",handleAcceptInfo),(async()=>{var e=await(await fetch("/isconfig")).json();"WIFI HAS BEEN CONFIG"===e.message?eleConfigSuccess.classList.add("active"):"WIFI NOT YET CONFIG"===e.message&&eleConfigError.classList.add("active")})()</script></html>
)rawliteral";

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  // Serial.println();
  // Serial.print("ssid: ");
  // Serial.println(eepromReadWifi("ssid"));
  // Serial.print("password: ");
  // Serial.println(eepromReadWifi("password"));
  runner.startNow();
  miruSetupWifiAPMode.enable();
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
  // Serial.printf("[WIFI-AP]( IS SETUP WITH IP: ");
  // Serial.print(ip_mode_ap);
  // Serial.printf(" )\n");
  while (!miruSetupWebServerAPMode.enable())
    ;
}

void setupWebserverModeAP()
{
  if (miruSetupWebServerAPMode.isFirstIteration())
  {

    server_mode_ap.on("/", [](){ server_mode_ap.send(200, "text/html", template_mode_ap); });
    server_mode_ap.on("/isconfig", [](){ 
      if(eepromReadWifi("ssid") != "" && eepromReadWifi("password") != "") {
        server_mode_ap.send(200, "application/json", "{\"message\":\"WIFI HAS BEEN CONFIG\"}");
      }else {
        server_mode_ap.send(200, "application/json", "{\"message\":\"WIFI NOT YET CONFIG\"}");
      }
    });
    server_mode_ap.on("/config", []() {
      if(server_mode_ap.method() == HTTP_POST && server_mode_ap.hasArg("plain")) {
        deserializeJson(bufferBodyPaser, server_mode_ap.arg("plain"));
        bufferBodyPaser.shrinkToFit();
        JsonObject body = bufferBodyPaser.as<JsonObject>();
        // Serial.print("ssid: ");
        // Serial.println(String(body["ssid"]));
        // Serial.print("password: ");
        // Serial.println(String(body["password"]));
        eepromWriteConfigWifi(String(body["ssid"]), String(body["password"]));
        // Serial.println("[Save config EEPROM]");
        server_mode_ap.send(200, "application/json", "{\"message\":\"CONFIGURATION WIFI SUCCESSFULLY\"}");
      }
    });

    // START WEBSERVER
    server_mode_ap.begin();
  }
  server_mode_ap.handleClient();
}

void eepromReset() {
  for(size_t i = 0; i < eepromSize; i++) {
    EEPROM.write(0x0F + i, '*');
  }
  EEPROM.commit();
}

void eepromWriteConfigWifi(String ssid, String password) {
  String temp_wifi = "ssid:" + ssid + "*" + "password:" + password;
  eepromReset();
  for(size_t i = 0; i <= temp_wifi.length(); i++) {
    EEPROM.write(0x0F + i, temp_wifi[i]);
  }
  EEPROM.commit();
}

String eepromReadWifi(String key) {
  String val;
  // EXTRACT CONFIGURATION FROM EEPROM
  for(size_t i = 0; i < eepromSize; i++)
  {
    uint8_t num = EEPROM.read(0x0F + i);
    if(num >= 32 && num <= 126) {
      val += char(num);
    }
  }
  // START SEARCH
  String result = "";
  int position = val.indexOf(key);
  if(position == -1) return result;
  position = (position + key.length() + 1);
  for(size_t i = position; i < val.length(); i++)
  {
    if(val[i] != '*') {
      result += val[i];
    }else {
      break;
    }
  }
  return result;
}
