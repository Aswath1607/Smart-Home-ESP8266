#include <ESP8266WiFi.h>
#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h>
#include <time.h>

RTC_DS3231 rtc;
const char* ap_ssid = "SmartHome";
const char* ap_password = "12345678";

const char* ssid = "aabb";
const char* password = "1122334455";

WiFiServer server(80);

int lastMinute = -1;

#define BUZZER D3
unsigned long buzzerStart = 0;
bool buzzerOn = false;

#define R1 D5
#define R2 D6
#define R3 D7
#define R4 D8

bool relayState[5] = {0,0,0,0,0};
bool autoMode[5] = {0,0,0,0,0};
int onHour[5] = {0,0,0,0,0};
int offHour[5] = {0,0,0,0,0};
int onMin[5] = {0,0,0,0,0};
int offMin[5] = {0,0,0,0,0};


void startBeep(){
  digitalWrite(BUZZER, HIGH);
  buzzerStart = millis();
  buzzerOn = true;
}

void handleBuzzer(){
  if(buzzerOn && millis() - buzzerStart >= 2000){
    digitalWrite(BUZZER, LOW);
    buzzerOn = false;
  }
}

void setRelay(int r, bool state) {

  // 🔥 ONLY BEEP IF STATE CHANGES
  if(relayState[r] != state){
    startBeep();
  }

  relayState[r] = state;

  if(r==1) digitalWrite(R1, state ? LOW : HIGH);
  if(r==2) digitalWrite(R2, state ? HIGH : LOW);
  if(r==3) digitalWrite(R3, state ? HIGH : LOW);
  if(r==4) digitalWrite(R4, state ? HIGH : LOW);
}

void applyAutoLogicNow() {

  DateTime now = rtc.now();

  int h = now.hour();
  int m = now.minute();

  for(int i=1;i<=4;i++){
    if(autoMode[i]){

      // 🔥 ON condition
      if(
        (h > onHour[i] || (h == onHour[i] && m >= onMin[i])) &&
        (h < offHour[i] || (h == offHour[i] && m < offMin[i]))
      ){
        setRelay(i, true);
      }
      else{
        setRelay(i, false);
      }
    }
  }
}

void syncTimeFromNTP() {

  configTime(19800, 0, "asia.pool.ntp.org", "time.google.com");

  Serial.println("Syncing time from NTP...");

  struct tm timeinfo;

  int retry = 0;

  // 🔥 WAIT UNTIL TIME IS RECEIVED
  while (!getLocalTime(&timeinfo) && retry < 20) {
    Serial.print(".");
    delay(500);
    retry++;
  }

  if (retry < 20) {
    Serial.println("\nNTP Sync Success!");

    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));

    Serial.println("RTC UPDATED FROM INTERNET ✅");

  } else {
    Serial.println("\nNTP FAILED ❌");
  }
}

void setup() {
  Serial.begin(115200);
rtc.begin();
if (!rtc.begin()) {
  Serial.println("RTC NOT FOUND ❌");
  while (1);
}
// ONLY RUN ONCE (then comment it)
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

Serial.println("Starting AP...");
WiFi.softAP(ap_ssid, ap_password);
Serial.println("AP started");

Serial.println("Connecting to WiFi...");


EEPROM.begin(512);
// 🔥 LOAD SAVED DATA HERE
for(int i=1;i<=4;i++){
  autoMode[i] = EEPROM.read(i);
  onHour[i]   = EEPROM.read(i+10);
  onMin[i]    = EEPROM.read(i+20);
  offHour[i]  = EEPROM.read(i+30);
  offMin[i]   = EEPROM.read(i+40);
}
  pinMode(BUZZER, OUTPUT);
digitalWrite(BUZZER, LOW);

  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);
  pinMode(R4, OUTPUT);

  setRelay(1,false);
  setRelay(2,false);
  setRelay(3,false);
  setRelay(4,false);

Serial.println("Starting AP...");
WiFi.softAP(ap_ssid, ap_password);
Serial.print("AP IP: ");
Serial.println(WiFi.softAPIP());

Serial.println("Connecting to WiFi...");
WiFi.begin(ssid, password);

unsigned long startTime = millis();

while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
  delay(500);
  Serial.print(".");
}

if (WiFi.status() == WL_CONNECTED) {
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // 🔥 Internet test
  Serial.println("Testing internet...");
  WiFiClient test;
  if(test.connect("google.com", 80)){
    Serial.println("Internet OK");
    test.stop();
  } else {
    Serial.println("No Internet ❌");
  }

  delay(2000);
  syncTimeFromNTP();

} else {
  Serial.println("\nWiFi Failed → Using AP only");
}

  // 🔥 ADD SMALL DELAY (VERY IMPORTANT)
 

WiFi.setSleepMode(WIFI_NONE_SLEEP);

// 🔥 ADD THIS LINE
server.begin();

Serial.println("Server started");
applyAutoLogicNow();
}

void loop() {
handleBuzzer();
  // 🔥 AUTO LOGIC (ALWAYS RUNS)
  DateTime now = rtc.now();
//Serial.print(now.hour());
//Serial.print(":");
//Serial.println(now.minute());

  int h = now.hour();
  int m = now.minute();

  if(m != lastMinute){
    lastMinute = m;

    for(int i=1;i<=4;i++){
      if(autoMode[i]){
        if(h == onHour[i] && m == onMin[i]) setRelay(i,true);
        if(h == offHour[i] && m == offMin[i]) setRelay(i,false);
      }
    }
  }

  // 🔻 WEB SERVER
  WiFiClient client = server.available();
if (!client) {
  delay(1);
  return;
}

  while (!client.available()) delay(1);

  String req = client.readStringUntil('\n');
  Serial.println("REQ: " + req);
  Serial.println(req);

  while(client.available()) client.read();

  // 🔹 STATUS
  if (req.indexOf("GET /status") != -1) {

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();

    client.print("{");

client.print("\"r1\":"); client.print(relayState[1]); client.print(",");
client.print("\"r2\":"); client.print(relayState[2]); client.print(",");
client.print("\"r3\":"); client.print(relayState[3]); client.print(",");
client.print("\"r4\":"); client.print(relayState[4]); client.print(",");

client.print("\"a1\":"); client.print(autoMode[1]); client.print(",");
client.print("\"a2\":"); client.print(autoMode[2]); client.print(",");
client.print("\"a3\":"); client.print(autoMode[3]); client.print(",");
client.print("\"a4\":"); client.print(autoMode[4]);

client.print("}");

    client.stop();
    return;
  }

  // 🔹 SET TIMER
  if (req.indexOf("GET /set") != -1) {

    int r = req.substring(req.indexOf("r=")+2).toInt();

    int onH = req.substring(req.indexOf("onH=")+4).toInt();
    int onM = req.substring(req.indexOf("onM=")+4).toInt();

    int offH = req.substring(req.indexOf("offH=")+5).toInt();
    int offM = req.substring(req.indexOf("offM=")+5).toInt();

    onHour[r] = onH;
    onMin[r]  = onM;

    offHour[r] = offH;
    offMin[r]  = offM;

    EEPROM.write(r+10, onHour[r]);
    EEPROM.write(r+20, onMin[r]);
    EEPROM.write(r+30, offHour[r]);
    EEPROM.write(r+40, offMin[r]);
    EEPROM.commit();

    client.println("HTTP/1.1 200 OK");
    client.println();
    client.println("SET");

    client.stop();
    return;
  }


// 🔹 TIME API
if (req.indexOf("GET /time") != -1) {

  DateTime now = rtc.now();

  int h = now.hour();
  int m = now.minute();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println();

  if(h < 10) client.print("0");
  client.print(h);
  client.print(":");
  if(m < 10) client.print("0");
  client.print(m);

  client.stop();
  return;
}

// 🔹 TOGGLE (VERY IMPORTANT)
if (req.indexOf("GET /toggle?r=") != -1) {
  int r = req.substring(req.indexOf("r=")+2).toInt();

  setRelay(r, !relayState[r]);

  client.println("HTTP/1.1 200 OK");
  client.println();
  client.println("OK");

  client.stop();
  return;
}

if (req.indexOf("GET /auto?r=") != -1) {
    int r = req.substring(req.indexOf("r=")+2).toInt();

    autoMode[r] = !autoMode[r];

    EEPROM.write(r, autoMode[r]);
    EEPROM.commit();

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("OK");

    client.stop();
    return;
  }

  // 🔹 UI PAGE
  if (req.indexOf("GET / ") != -1) {

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
client.println(R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { background:#121212; color:white; text-align:center; font-family:Arial; }

.card {
  background:#1e1e1e;
  margin:15px;
  padding:20px;
  border-radius:15px;
}

button {
  padding:12px 20px;
  margin:8px;
  border:none;
  border-radius:10px;
  font-size:16px;
  color:white;
  cursor:pointer;
}

button:active {
  transform: scale(0.95);
}

.on { background:#00c853; }
.off { background:#d50000; }
.auto { background:orange; color:black; }

.input-row {
  margin-top:10px;
}

input {
  width:60px;
  padding:8px;
  margin:5px;
  text-align:center;
  border-radius:8px;
  border:none;
  font-size:16px;
}
.timerBtn {
  background:#2962ff;   /* nice blue */
  color:white;
  padding:12px 20px;
  border:none;
  border-radius:10px;
  font-size:16px;
  margin-top:10px;
  cursor:pointer;
}

.timerBtn:active {
  transform: scale(0.95);
}
</style>

<script>

function showToast(msg){
  let t = document.createElement("div");
  t.innerText = msg;

  t.style.position = "fixed";
  t.style.top = "20px";
  t.style.right = "20px";
  t.style.background = "#222";
  t.style.color = "white";
  t.style.padding = "12px 18px";
  t.style.borderRadius = "10px";
  t.style.fontSize = "14px";
  t.style.boxShadow = "0 4px 10px rgba(0,0,0,0.5)";
  t.style.zIndex = "999";

  document.body.appendChild(t);

  setTimeout(() => {
    t.style.opacity = "0";
    setTimeout(() => t.remove(), 300);
  }, 2000);
}

let state = [0,0,0,0,0];
let autoState = [0,0,0,0,0];

function toggle(r){
  fetch(`/toggle?r=${r}`)
  .then(() => {
    showToast("Relay " + r + " toggled");
    setTimeout(fetchStatus, 200);
  });
}

function updateUI(r){

  let btn = document.getElementById("btn"+r);
  let label = document.getElementById("txt"+r);
  let autoBtn = document.getElementById("auto"+r);

  // 🔹 RELAY STATE
  if(state[r] == 1){
    btn.className = "on";
    btn.innerText = "ON";
    label.innerText = "Status: ON";
  } else {
    btn.className = "off";
    btn.innerText = "OFF";
    label.innerText = "Status: OFF";
  }

  // 🔹 AUTO STATE
  if(autoState[r] == 1){
    autoBtn.style.background = "#00c853";
    autoBtn.innerText = "AUTO ON";
  } else {
    autoBtn.style.background = "orange";
    autoBtn.innerText = "AUTO OFF";
  }

  // 🔥 ADD THIS EXACTLY HERE
  let setBtn = document.querySelector(`#btn${r}`).parentElement.querySelector('.timerBtn');

  if(autoState[r] == 1){
    setBtn.style.background = "#2962ff";   // active blue
  } else {
    setBtn.style.background = "#555";      // disabled grey
  }
}

function fetchStatus(){
  fetch("/status")
  .then(res => res.json())
  .then(data => {
    state[1]=data.r1;
state[2]=data.r2;
state[3]=data.r3;
state[4]=data.r4;

autoState[1]=data.a1;
autoState[2]=data.a2;
autoState[3]=data.a3;
autoState[4]=data.a4;

    for(let i=1;i<=4;i++) updateUI(i);
  });
}

setInterval(fetchStatus, 500);
window.onload = fetchStatus;

function autoToggle(r){
  fetch(`/auto?r=${r}`)
  .then(() => showToast("Auto toggled for Relay " + r));
}

function setTime(r){
  let onH = document.getElementById("onH"+r).value;
  let onM = document.getElementById("onM"+r).value;

  let offH = document.getElementById("offH"+r).value;
  let offM = document.getElementById("offM"+r).value;

  fetch(`/set?r=${r}&onH=${onH}&onM=${onM}&offH=${offH}&offM=${offM}`)
  .then(() => showToast("Timer Set for Relay " + r));
}

function updateTime(){
  fetch("/time")
  .then(res => res.text())
  .then(t => {
    document.getElementById("clock").innerText = "Time: " + t;
  });
}

setInterval(updateTime, 1000);

</script>

</head>
<body>

<h1>Smart Home</h1>
<h2 id="clock">Time: --:--</h2>

<!-- RELAY 1 -->
<div class="card">
<h2>Relay 1</h2>
<p id="txt1">Status: OFF</p>
<button id="btn1" class="off" onclick="toggle(1)">OFF</button><br>

<button id="auto1" class="auto" onclick="autoToggle(1)">AUTO OFF</button>

<div class="input-row">
  ON <input id="onH1" type="number" min="0" max="23"> :
     <input id="onM1" type="number" min="0" max="59">
</div>

<div class="input-row">
  OFF <input id="offH1" type="number" min="0" max="23"> :
      <input id="offM1" type="number" min="0" max="59">
</div>

<button class="timerBtn" onclick="setTime(1)">Set Timer</button>
</div>

<!-- RELAY 2 -->
<div class="card">
<h2>Relay 2</h2>
<p id="txt2">Status: OFF</p>
<button id="btn2" class="off" onclick="toggle(2)">OFF</button><br>

<button id="auto2" class="auto" onclick="autoToggle(2)">AUTO OFF</button>

<div class="input-row">
  ON <input id="onH2" type="number" min="0" max="23"> :
     <input id="onM2" type="number" min="0" max="59">
</div>

<div class="input-row">
  OFF <input id="offH2" type="number" min="0" max="23"> :
      <input id="offM2" type="number" min="0" max="59">
</div>

<button class="timerBtn" onclick="setTime(2)">Set Timer</button>
</div>

<!-- RELAY 3 -->
<div class="card">
<h2>Relay 3</h2>
<p id="txt3">Status: OFF</p>
<button id="btn3" class="off" onclick="toggle(3)">OFF</button><br>

<button id="auto3" class="auto" onclick="autoToggle(3)">AUTO OFF</button>

<div class="input-row">
  ON <input id="onH3" type="number" min="0" max="23"> :
     <input id="onM3" type="number" min="0" max="59">
</div>

<div class="input-row">
  OFF <input id="offH3" type="number" min="0" max="23"> :
      <input id="offM3" type="number" min="0" max="59">
</div>

<button class="timerBtn" onclick="setTime(3)">Set Timer</button>
</div>

<!-- RELAY 4 -->
<div class="card">
<h2>Relay 4</h2>
<p id="txt4">Status: OFF</p>
<button id="btn4" class="off" onclick="toggle(4)">OFF</button><br>

<button id="auto4" class="auto" onclick="autoToggle(4)">AUTO OFF</button>

<div class="input-row">
  ON <input id="onH4" type="number" min="0" max="23"> :
     <input id="onM4" type="number" min="0" max="59">
</div>

<div class="input-row">
  OFF <input id="offH4" type="number" min="0" max="23"> :
      <input id="offM4" type="number" min="0" max="59">
</div>

<button class="timerBtn" onclick="setTime(4)">Set Timer</button>
</div>

</body>
</html>
)====");
  }

  client.stop();
}