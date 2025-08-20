#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <time.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ================== Hardware & App Settings ==================
#define RELAY_PIN 4
#define BELL_MS   20000               // Bell ON time in milliseconds (20 seconds)
#define SCHEDULE_FILE "/schedule.json"
#define MAX_SLOTS 16

// Rwanda is UTC+02:00
const long TZ_OFFSET_SEC = 2L * 3600L;  // change if needed
const int  DST_OFFSET_SEC = 0;

// ================== Wi-Fi ==================
const char* ssid     = "Chadai Developer";
const char* password = "Umutana123";

// ================== Globals ==================
RTC_DS3231 rtc;
WebServer server(80);

// 7 days x 16 slots each, stored as "HH:MM" or empty ""
String schedules[7][MAX_SLOTS];

// Debounce so we don't ring multiple times within the same minute
int lastTriggeredDay = -1;       // 0=Sunday ... 6=Saturday
int lastTriggeredMin = -1;       // minutes since midnight

// ================== HTML/CSS/JS UI ==================
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Weekly Bell Scheduler</title>
<style>
  :root {
    --bg:#0b1220; --card:#121a2b; --border:#223152; --text:#e9eefc; --muted:#9aa9c2;
    --accent:#5a86ff; --ok:#22c55e; --warn:#f59e0b; --bad:#ef4444;
  }
  * { box-sizing:border-box; color:var(--text); font-family:system-ui, -apple-system, Segoe UI, Roboto, Ubuntu, Cantarell, "Helvetica Neue", Arial; }
  body { margin:0; background:linear-gradient(180deg,#0b1220,#0a1020); }
  .wrap { max-width:1100px; margin:24px auto; padding:12px; }
  .card { background:var(--card); border:1px solid var(--border); border-radius:16px; padding:16px; box-shadow:0 10px 30px rgba(0,0,0,.35); }
  h1 { margin:0 0 6px; font-size:22px; }
  .muted { color:var(--muted); font-size:14px; }
  .row { display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
  button { background:var(--accent); border:none; padding:10px 14px; border-radius:10px; font-weight:600; cursor:pointer; }
  button:hover { filter:brightness(1.05); }
  .btn-ghost { background:transparent; border:1px solid var(--border); }
  .btn-ok { background:var(--ok); }
  .btn-warn { background:var(--warn); }
  .btn-bad { background:var(--bad); }
  .pill { background:#0b1328; border:1px solid var(--border); padding:6px 10px; border-radius:999px; font-size:12px; }
  .mono { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace; }

  table { width:100%; border-collapse:collapse; margin-top:14px; }
  thead th { text-align:left; padding:8px; background:#0e172f; border-bottom:1px solid var(--border); }
  tbody td, tbody th { border-bottom:1px solid var(--border); padding:8px; vertical-align:top; }
  tbody th { width:150px; font-weight:600; }
  .slot { display:inline-flex; align-items:center; gap:6px; margin:4px 6px; }
  .slot label { font-size:12px; color:var(--muted); padding:2px 6px; border:1px solid var(--border); border-radius:6px; }
  input[type="time"] { background:#0b1328; border:1px solid var(--border); padding:8px; border-radius:8px; color:var(--text); }
  .status { margin-top:10px; }
  .big { font-size:26px; }
  .grid { display:grid; grid-template-columns: 1fr 1fr; gap:16px; }
  @media (max-width: 900px){ .grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
  <div class="wrap">
    <div class="grid">
      <div class="card">
        <h1>ACEJ/KARAMA TSS AUTO-BELL</h1>
        <div class="muted">Set times for each day. Bell rings for <span id="bellMs">--</span> seconds.</div>
        <div class="row" style="margin-top:8px;">
          <span class="pill">IP: <span id="ip" class="mono">--</span></span>
          <span class="pill">RTC: <span id="rtc" class="mono">--</span></span>
          <span class="pill">Status: <span id="status" class="mono">Idle</span></span>
        </div>
        <div class="row" style="margin-top:10px;">
          <button id="sync">Sync RTC from NTP</button>
          <button id="test" class="btn-ghost">Test Bell (20s)</button>
        </div>
        <div class="status muted" id="log"></div>
      </div>

      <div class="card">
        <h1>Real Time</h1>
        <div class="muted">From DS3231</div>
        <div class="mono big" id="bigTime">--:--:--</div>
        <div class="mono" id="bigDate">----/--/-- (---)</div>
      </div>
    </div>

    <div class="card" style="margin-top:16px;">
      <table>
        <thead>
          <tr>
            <th>Day</th>
            <th>Time Slots (HH:MM)</th>
          </tr>
        </thead>
        <tbody id="tbody"></tbody>
      </table>
    </div>
  </div>

<script>
const days = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
const tbody = document.getElementById('tbody');
const statusEl = document.getElementById('status');
const logEl = document.getElementById('log');
const ipEl = document.getElementById('ip');
const rtcEl = document.getElementById('rtc');
const bigTime = document.getElementById('bigTime');
const bigDate = document.getElementById('bigDate');
const bellMsEl = document.getElementById('bellMs');

function setStatus(t){ statusEl.textContent = t; }
function log(t){ logEl.textContent = t; }

function rowHTML(dayIndex, arr){
  let cells = '';
  for(let i=0;i<16;i++){
    const val = (arr && arr[i]) ? arr[i] : "";
    cells += `
      <span class="slot">
        <label>#${i+1}</label>
        <input type="time" value="${val}" onchange="updateSchedule(${dayIndex},${i},this.value)">
      </span>`;
  }
  return `<tr>
    <th>${days[dayIndex]}</th>
    <td>${cells}</td>
  </tr>`;
}

async function loadSchedule(){
  const r = await fetch('/schedule');
  const data = await r.json(); // {"0":[...], "1":[...], ...}
  tbody.innerHTML = '';
  for(let d=0; d<7; d++){
    tbody.insertAdjacentHTML('beforeend', rowHTML(d, data[String(d)]));
  }
}

async function updateSchedule(day,slot,value){
  setStatus('Saving…');
  try{
    await fetch('/schedule',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:`day=${day}&slot=${slot}&value=${encodeURIComponent(value)}`
    });
    setStatus('Saved ✓');
    log(`Saved slot #${slot+1} for ${days[day]}: ${value || '(empty)'}`);
    setTimeout(()=>setStatus('Idle'),1200);
  }catch(e){
    setStatus('Save failed');
  }
}

async function loadTime(){
  try{
    const r = await fetch('/time');
    const t = await r.json();
    ipEl.textContent = t.ip || '--';
    rtcEl.textContent = t.rtc || 'OK';
    bellMsEl.textContent = Math.round((t.bell_ms||20000)/1000);
    bigTime.textContent = `${String(t.hh).padStart(2,'0')}:${String(t.mm).padStart(2,'0')}:${String(t.ss).padStart(2,'0')}`;
    const daynames = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
    bigDate.textContent = `${t.y}/${String(t.m).padStart(2,'0')}/${String(t.d).padStart(2,'0')} (${daynames[t.dow]})`;
  }catch(e){}
}

document.getElementById('sync').onclick = async ()=>{
  setStatus('Syncing RTC…');
  try{
    await fetch('/sync_ntp',{method:'POST'});
    setStatus('RTC synced ✓');
    setTimeout(()=>setStatus('Idle'),1200);
  }catch(e){
    setStatus('Sync failed');
  }
};
document.getElementById('test').onclick = async ()=>{
  setStatus('Bell testing…');
  try{
    await fetch('/trigger',{method:'POST'});
    setStatus('Bell triggered ✓');
    setTimeout(()=>setStatus('Idle'),1200);
  }catch(e){
    setStatus('Test failed');
  }
};

async function init(){
  await loadSchedule();
  loadTime();
  setInterval(loadTime, 1000);
}
init();
</script>
</body>
</html>
)HTML";

// ================== Utils ==================
static inline String two(uint8_t v){ return (v<10) ? "0"+String(v) : String(v); }
static inline String ipToString(IPAddress ip){
  return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]);
}

// ================== SPIFFS Save/Load using ArduinoJson ==================
bool saveScheduleToFile(){
  // Create JSON: { "0":[...16...], "1":[...], ... "6":[...] }
  DynamicJsonDocument doc(4096);
  for(int d=0; d<7; d++){
    JsonArray arr = doc.createNestedArray(String(d));
    for(int s=0; s<MAX_SLOTS; s++){
      arr.add(schedules[d][s]); // may be "" if empty
    }
  }

  File f = SPIFFS.open(SCHEDULE_FILE, FILE_WRITE);
  if(!f){
    Serial.println("[SPIFFS] Failed to open schedule for writing");
    return false;
  }
  size_t n = serializeJson(doc, f);
  f.close();

  if(n == 0){
    Serial.println("[SPIFFS] Failed to serialize JSON");
    return false;
  }
  Serial.println("[SPIFFS] Schedule saved to SPIFFS");
  return true;
}

bool loadScheduleFromFile(){
  if(!SPIFFS.exists(SCHEDULE_FILE)){
    Serial.println("[SPIFFS] No schedule file found. Starting with empty schedule.");
    // Initialize empty
    for(int d=0; d<7; d++) for(int s=0; s<MAX_SLOTS; s++) schedules[d][s] = "";
    // Create an empty file so GET /schedule returns a valid structure
    return saveScheduleToFile();
  }

  File f = SPIFFS.open(SCHEDULE_FILE, FILE_READ);
  if(!f){
    Serial.println("[SPIFFS] Failed to open schedule for read");
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if(err){
    Serial.print("[SPIFFS] JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  for(int d=0; d<7; d++){
    JsonArray arr = doc[String(d)];
    for(int s=0; s<MAX_SLOTS; s++){
      if(arr.isNull() || s >= arr.size()) { schedules[d][s] = ""; }
      else {
        const char* v = arr[s] | "";
        schedules[d][s] = String(v);
      }
    }
  }

  Serial.println("[SPIFFS] Schedule loaded from SPIFFS");
  return true;
}

// ================== HTTP Handlers ==================
void handleRoot(){
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleTime(){
  DateTime now = rtc.now();

  DynamicJsonDocument doc(256);
  doc["y"] = now.year();
  doc["m"] = now.month();
  doc["d"] = now.day();
  doc["hh"] = now.hour();
  doc["mm"] = now.minute();
  doc["ss"] = now.second();
  doc["dow"] = now.dayOfTheWeek(); // 0=Sunday
  doc["ip"] = ipToString(WiFi.localIP());
  doc["rtc"] = "OK";
  doc["bell_ms"] = BELL_MS;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleScheduleGet(){
  DynamicJsonDocument doc(4096);
  for(int d=0; d<7; d++){
    JsonArray arr = doc.createNestedArray(String(d));
    for(int s=0; s<MAX_SLOTS; s++){
      arr.add(schedules[d][s]);
    }
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleSchedulePost(){
  if(!(server.hasArg("day") && server.hasArg("slot") && server.hasArg("value"))){
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  int day  = server.arg("day").toInt();
  int slot = server.arg("slot").toInt();
  String value = server.arg("value");

  if(day < 0 || day > 6 || slot < 0 || slot >= MAX_SLOTS){
    server.send(400, "text/plain", "Out of range");
    return;
  }

  // Accept either "" (clear) or HH:MM
  if(value.length() == 0){
    schedules[day][slot] = "";
  }else{
    // Simple validation HH:MM
    if(value.length() == 5 && value.charAt(2) == ':'){
      schedules[day][slot] = value;
    }else{
      server.send(400, "text/plain", "Invalid time format");
      return;
    }
  }

  Serial.printf("[SCHEDULE] Updated: Day %d, Slot #%d -> '%s'\n", day, slot+1, schedules[day][slot].c_str());
  saveScheduleToFile();
  server.send(200, "text/plain", "OK");
}

void handleSyncNTP(){
  configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  struct tm tinfo;
  if(getLocalTime(&tinfo, 8000)){
    DateTime dt(
      tinfo.tm_year + 1900, tinfo.tm_mon + 1, tinfo.tm_mday,
      tinfo.tm_hour, tinfo.tm_min, tinfo.tm_sec
    );
    rtc.adjust(dt);
    Serial.printf("[RTC] Synced from NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
      dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
    server.send(200, "text/plain", "OK");
  }else{
    Serial.println("[RTC] NTP sync failed.");
    server.send(500, "text/plain", "NTP failed");
  }
}

void handleTrigger(){
  Serial.println("[BELL] Manual trigger: ON");
  digitalWrite(RELAY_PIN, HIGH);
  delay(BELL_MS);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("[BELL] Manual trigger: OFF");
  server.send(200, "text/plain", "OK");
}

// ================== Bell Logic ==================
static inline uint16_t minutesSinceMidnight(const DateTime& t){
  return (uint16_t)(t.hour() * 60 + t.minute());
}

void ringBell(const DateTime& now, const char* reason){
  Serial.printf("[BELL] ON at %04d-%02d-%02d %02d:%02d:%02d (%s)\n",
    now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), reason);
  digitalWrite(RELAY_PIN, HIGH);
  delay(BELL_MS);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("[BELL] OFF");
}

void checkBell(){
  DateTime now = rtc.now();
  int dow = now.dayOfTheWeek(); // 0=Sunday
  uint16_t minuteOfDay = minutesSinceMidnight(now);

  // Only evaluate once per (day, minute) to prevent re-triggering
  if(lastTriggeredDay == dow && lastTriggeredMin == (int)minuteOfDay){
    return;
  }

  // Match only at the start of a minute (second == 0) so we don't miss/duplicate
  if(now.second() != 0) return;

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.hour(), now.minute());

  for(int s=0; s<MAX_SLOTS; s++){
    if(schedules[dow][s].length() == 5 && schedules[dow][s] == String(hhmm)){
      ringBell(now, "scheduled");
      lastTriggeredDay = dow;
      lastTriggeredMin = minuteOfDay;
      break; // if multiple entries same minute, ring once
    }
  }
}

// ================== Setup / Loop ==================
void setup(){
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  if(!SPIFFS.begin(true)){
    Serial.println("[SPIFFS] Mount failed!");
  }else{
    loadScheduleFromFile();
  }

  // Wi-Fi
  Serial.printf("[WiFi] Connecting to '%s' ...\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while(WiFi.status() != WL_CONNECTED && millis()-t0 < 20000){
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if(WiFi.status() == WL_CONNECTED){
    Serial.print("[WiFi] Connected. IP: "); Serial.println(WiFi.localIP());
  }else{
    Serial.println("[WiFi] Not connected (dashboard will be unavailable).");
  }

  // RTC
  Wire.begin(); // SDA=21, SCL=22
  if(!rtc.begin()){
    Serial.println("[RTC] DS3231 not found. Check wiring.");
  }else{
    if(rtc.lostPower()){
      Serial.println("[RTC] Lost power; syncing from NTP...");
      handleSyncNTP();
    }
  }

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/time", HTTP_GET, handleTime);
  server.on("/schedule", HTTP_GET, handleScheduleGet);
  server.on("/schedule", HTTP_POST, handleSchedulePost);
  server.on("/sync_ntp", HTTP_POST, handleSyncNTP);
  server.on("/trigger", HTTP_POST, handleTrigger);
  server.begin();
  Serial.println("[WEB] HTTP server started.");
}

void loop(){
  server.handleClient();

  // Check schedule ~4 times per second to catch second==0 reliably
  static unsigned long lastTick = 0;
  if(millis() - lastTick >= 250){
    lastTick = millis();
    checkBell();
  }
}
