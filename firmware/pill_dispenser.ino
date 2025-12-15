/**************************************************************************

 * Pill Dispenser

 /*
 Smart Pill Dispenser
 Author: Mohd Yasir
 License: MIT
 GitHub: https://github.com/yasir22a/pill_dispenser
*/

 **************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "LittleFS.h"
#include <WebServer.h>
#include <Wire.h>
#include <uRTCLib.h>
#include <ArduinoJson.h>
#include <time.h>

// ====== AUDIO & GSM LIBRARIES ======
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ====== User WiFi ======
const char* ssid     = "Add your network";   //// network name ////
const char* password = "network password";    //// network password ////

// ====== Hardware Config ======
const int MOTOR_PINS[] = {18, 19}; 
const int NUM_MOTORS   = 2;
const int ANALOG_PIN   = 34;       

// PWM Settings
const int PWM_FREQ = 5000;
const int PWM_RES  = 8; 

// ====== NEW HARDWARE PINS ======
// SIM800L
#define SIM800_TX 17 // Connect to SIM800L RX
#define SIM800_RX 16 // Connect to SIM800L TX

// MAX98357A I2S
#define I2S_LRC   25
#define I2S_BCLK  26
#define I2S_DIN   27

// Globals for Hardware
int motorPwmValues[NUM_MOTORS]  = {120, 120}; 
int thresholdValues[NUM_MOTORS] = {50, 50}; 
String globalPhoneNumber = ""; // Stored in settings

// Audio Objects
AudioGeneratorWAV *wav;
AudioFileSourceLittleFS *file;
AudioOutputI2S *out;
bool isPlayingAudio = false;

// Global File Object for Uploads
File uploadFile; 

// Timing Constants
const unsigned long motorInterval = 1;         
const int interCycleDelay = 500;                
const unsigned long scheduleCheckInterval = 2000; 

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

WebServer server(80);
uRTCLib rtc;

// Global variables
bool motorRunning = false;
int currentMotor = -1;
bool stopControl = false;
unsigned long previousMillis = 0;
int scheduledContainer = 0;
int scheduledMotorSpeed = 0;
int scheduledTriggerThreshold = 0;
int scheduledPillCountRemaining = 0;
bool scheduleActive = false;
unsigned long lastScheduleCheck = 0;
int lastDispensedMinute = -1; 

struct DispenseTask { int container; int motorSpeed; int triggerThreshold; int pillCount; };
std::vector<DispenseTask> dispenseQueue;

// ==========================================
// SMS FUNCTION (FIXED)
// ==========================================
void sendSMS(String message) {
  if (globalPhoneNumber == "" || globalPhoneNumber.length() < 10) {
    Serial.println("SMS Failed: Phone number empty or too short"); 
    return;
  }
  
  Serial.println("--- SENDING SMS to " + globalPhoneNumber + " ---");
  
  Serial2.println("AT"); 
  delay(200);
  Serial2.println("AT+CMGF=1"); // Text Mode
  delay(200);
  Serial2.println("AT+CSMP=17,167,0,0"); // Force Compatibility
  delay(200);

  Serial2.print("AT+CMGS=\"");
  Serial2.print(globalPhoneNumber);
  Serial2.println("\"");
  
  delay(1000); // Wait for '>'

  Serial2.print(message);
  delay(200);
  Serial2.write(26); // Ctrl+Z
  
  delay(5000); // Wait for network
  Serial.println("--- SMS COMMAND SENT ---");
}

void playVoiceFile(int container) {
  String filename = "/voice" + String(container) + ".wav";
  if (!LittleFS.exists(filename)) {
    Serial.println("No audio file for container " + String(container));
    return;
  }

  for(int i=0; i<3; i++) {
    file = new AudioFileSourceLittleFS(filename.c_str());
    out = new AudioOutputI2S();
    out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    wav = new AudioGeneratorWAV();
    wav->begin(file, out);
    while(wav->isRunning()) {
      if (!wav->loop()) wav->stop();
    }
    delete wav; delete out; delete file;
    delay(500); 
  }
}

// ==========================================
// WEBSITE PARTS
// ==========================================
const char UI_PART_1[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Pill Dispenser</title><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
<style>*{box-sizing:border-box;margin:0;padding:0}body{font-family:'Roboto',sans-serif;background:#f6f7fb;color:#222;padding:18px}
header{max-width:1000px;margin:0 auto 18px auto;text-align:center}h1{font-size:28px;display:inline-block;margin-right:8px}
.byline{font-size:13px;color:#666;margin-top:6px}.container-grid{max-width:1000px;margin:16px auto;display:flex;flex-direction:column;gap:18px}
.container-block{background:#fff;border-radius:10px;padding:18px;box-shadow:0 6px 18px rgba(0,0,0,0.06);border-top:6px solid transparent}
.container-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}.container-header h2{margin:0;font-size:20px;color:#1f3b4d}
.actions button,.upload-btn-wrapper button{background:#6c5ce7;color:#fff;border:none;padding:8px 12px;border-radius:8px;cursor:pointer;margin-left:8px}
table{width:100%;border-collapse:collapse;margin-top:6px}th,td{padding:10px;text-align:left;font-size:14px;border-bottom:1px solid rgba(0,0,0,0.06)}
th{color:#444}.disabled{opacity:0.6}#container1-block{background:#dfeeff;border-top-color:#6ea8ff}#container2-block{background:#e8ffea;border-top-color:#7ed96a}
#container3-block{background:#fff8d9;border-top-color:#f2d94b}#container4-block{background:#ffeef2;border-top-color:#ff9cc1}
#overlay{position:fixed;left:0;top:0;width:100%;height:100%;background:rgba(0,0,0,0.4);display:none;z-index:999}
.dialog{display:none;position:fixed;left:50%;top:50%;transform:translate(-50%,-50%);z-index:1000;background:#fff;padding:14px;border-radius:8px;box-shadow:0 8px 30px rgba(0,0,0,0.2);width:320px}
.dialog .header{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}.dialog .footer{display:flex;justify-content:center;gap:8px;margin-top:12px}
.pill-counter{display:inline-flex;align-items:center;gap:8px}input[type="time"]{padding:6px}label{font-size:14px}.small-muted{color:#666;font-size:13px}
.top-actions{max-width:1000px;margin:12px auto;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px}button.active{background:#444;color:#fff}
.phone-setup{display:flex;gap:8px;align-items:center} .phone-setup input{padding:8px;border:1px solid #ccc;border-radius:4px}
.upload-area{margin-top:10px;border-top:1px dashed #ccc;padding-top:10px;display:flex;align-items:center;gap:10px}
</style></head><body><header><div style="font-size:28px;display:flex;align-items:center;justify-content:center;gap:8px"><span>💊</span><h1>Pill Dispenser</h1></div>
<div class="byline">Yasir electro</div></header>
<div class="top-actions">
  <div class="phone-setup">
    <input type="text" id="globalPhone" placeholder="+919876543210">
    <button onclick="savePhone()">Save Number</button>
  </div>
  <div>
    <button onclick="testSchedule()">Run Test</button><button onclick="setRTC()">Sync RTC</button>
  </div>
</div>
<div class="container-grid">
<div class="container-block" id="container1-block">
  <div class="container-header"><h2>Container 1</h2><div class="actions"><button onclick="openScheduleDialog(1,null)">+ Schedule</button><button onclick="openSettingsDialog(1)">Settings</button></div></div>
  <table id="container1-table"><thead><tr><th>Days</th><th>Pills</th><th>Times</th><th></th></tr></thead><tbody></tbody></table>
  <div class="upload-area"><label>Voice Alert:</label><input type="file" id="file1" accept=".wav"><button onclick="uploadVoice(1)">Upload</button></div>
</div>
<div class="container-block" id="container2-block">
  <div class="container-header"><h2>Container 2</h2><div class="actions"><button onclick="openScheduleDialog(2,null)">+ Schedule</button><button onclick="openSettingsDialog(2)">Settings</button></div></div>
  <table id="container2-table"><thead><tr><th>Days</th><th>Pills</th><th>Times</th><th></th></tr></thead><tbody></tbody></table>
   <div class="upload-area"><label>Voice Alert:</label><input type="file" id="file2" accept=".wav"><button onclick="uploadVoice(2)">Upload</button></div>
</div>
<div class="container-block disabled" id="container3-block"><div class="container-header"><h2>Container 3 (Disabled)</h2><div class="actions"><button disabled>+ Add</button></div></div></div></div>
<div id="overlay"></div>
<div id="scheduleDialog" class="dialog">
<div class="header"><strong id="dialogTitle">Add Schedule</strong><span style="cursor:pointer" onclick="closeScheduleDialog()">✕</span></div><div>
<div style="margin-bottom:8px"><button id="everydayBtn" onclick="toggleEveryday()">Everyday</button></div>
<div class="days-grid" style="display:flex;gap:6px;flex-wrap:wrap;justify-content:center;margin-bottom:8px">
<button data-day="Monday" onclick="toggleDay(this)">Mon</button><button data-day="Tuesday" onclick="toggleDay(this)">Tue</button>
<button data-day="Wednesday" onclick="toggleDay(this)">Wed</button><button data-day="Thursday" onclick="toggleDay(this)">Thu</button>
<button data-day="Friday" onclick="toggleDay(this)">Fri</button><button data-day="Saturday" onclick="toggleDay(this)">Sat</button>
<button data-day="Sunday" onclick="toggleDay(this)">Sun</button></div><div style="margin-bottom:8px"><label>Pill Count:</label><div class="pill-counter">
<button onclick="decrementPillCount()">−</button><input id="pillCount" value="1" readonly style="width:48px;text-align:center"/><button onclick="incrementPillCount()">+</button></div></div>
<div id="timeInputs"></div></div><div class="footer"><button onclick="saveSchedule()">Save</button><button onclick="closeScheduleDialog()">Cancel</button></div></div>
<div id="settingsDialog" class="dialog"><div class="header"><strong id="settingsDialogTitle">Container Settings</strong><span style="cursor:pointer" onclick="closeSettingsDialog()">✕</span></div>
<div><div style="margin-bottom:8px"><label>Motor speed PWM: <span id="motorSpeedValue">128</span></label><br>
<input id="motorSpeed" type="range" min="0" max="255" value="128" oninput="document.getElementById('motorSpeedValue').textContent=this.value"></div>
<div style="margin-bottom:8px"><label>Trigger threshold: <span id="triggerThresholdValue">50</span></label><br>
<input id="triggerThreshold" type="range" min="0" max="3000" value="50" oninput="document.getElementById('triggerThresholdValue').textContent=this.value"></div>
<div style="display:flex;gap:8px;justify-content:center"><button onclick="saveSettings()">Save</button><button onclick="closeSettingsDialog()">Cancel</button></div></div></div>
)rawliteral";

const char UI_PART_2[] PROGMEM = R"rawliteral(
<script>
var schedulesData=[];var settingsData={};var currentContainer=null;var editingScheduleId=null;var currentSettingsContainer=null;
function loadSchedules(){fetch('/getSchedules').then(r=>r.json()).then(d=>{schedulesData=d||[];renderAllSchedules()}).catch(()=>{schedulesData=[];renderAllSchedules()})}
function loadSettings(){fetch('/getSettings').then(r=>r.json()).then(d=>{settingsData=d||{};
if(settingsData.phone) document.getElementById('globalPhone').value = settingsData.phone;
}).catch(()=>{settingsData={}})}
function saveSchedulesToServer(){fetch('/saveSchedules',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(schedulesData)})}
function saveSettingsToServer(){fetch('/saveSettings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(settingsData)})}
function savePhone(){var p=document.getElementById('globalPhone').value; settingsData.phone=p; saveSettingsToServer(); alert("Phone number saved!");}

function uploadVoice(c){
  var fi=document.getElementById('file'+c);
  if(!fi.files.length){alert("Select .wav file");return;}
  var fd=new FormData();
  fd.append("file",fi.files[0]);
  fetch('/uploadVoice?container='+c, {method:'POST', body:fd}).then(r=>r.text()).then(t=>alert(t)).catch(e=>alert("Upload Failed"));
}

function openScheduleDialog(c,s){currentContainer=c;editingScheduleId=s;resetDialog();if(s!==null){var x=schedulesData.find(i=>i.id===s);if(x){document.getElementById('pillCount').value=x.pillCount||1;updateTimeFields();var ti=document.querySelectorAll('#timeInputs input[type="time"]');(x.times||[]).forEach((tv,i)=>{if(ti[i])ti[i].value=tv});var all=["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"];if(all.every(d=>x.days&&x.days.indexOf(d)!==-1))document.getElementById('everydayBtn').classList.add('active');document.querySelectorAll('.days-grid button').forEach(b=>{if(x.days&&x.days.indexOf(b.getAttribute('data-day'))!==-1)b.classList.add('active')})}}document.getElementById('overlay').style.display='block';document.getElementById('scheduleDialog').style.display='block'}
function closeScheduleDialog(){document.getElementById('overlay').style.display='none';document.getElementById('scheduleDialog').style.display='none'}
function resetDialog(){document.getElementById('everydayBtn').classList.remove('active');document.querySelectorAll('.days-grid button').forEach(b=>b.classList.remove('active'));document.getElementById('pillCount').value=1;updateTimeFields()}
function toggleDay(b){b.classList.toggle('active')}
function toggleEveryday(){var b=document.getElementById('everydayBtn');b.classList.toggle('active');if(b.classList.contains('active'))document.querySelectorAll('.days-grid button').forEach(x=>x.classList.remove('active'))}
function updateTimeFields(){var p=parseInt(document.getElementById('pillCount').value)||1;var d=document.getElementById('timeInputs');d.innerHTML='';for(var i=1;i<=p;i++){var x=document.createElement('div');x.innerHTML='Time #'+i+': <input type="time" value="08:00">';d.appendChild(x)}}
function incrementPillCount(){var e=document.getElementById('pillCount');e.value=(parseInt(e.value)||1)+1;updateTimeFields()}
function decrementPillCount(){var e=document.getElementById('pillCount');var v=parseInt(e.value)||1;if(v>1)e.value=v-1;updateTimeFields()}
function saveSchedule(){var days=[];if(document.getElementById('everydayBtn').classList.contains('active')){days=["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"]}else{document.querySelectorAll('.days-grid button.active').forEach(b=>days.push(b.getAttribute('data-day')))}var p=parseInt(document.getElementById('pillCount').value)||1;var times=[];document.querySelectorAll('#timeInputs input[type="time"]').forEach(i=>{if(i.value)times.push(i.value)});if(editingScheduleId===null){schedulesData.push({id:Date.now(),container:currentContainer,days:days,pillCount:p,times:times})}else{schedulesData=schedulesData.map(s=>{if(s.id===editingScheduleId){s.container=currentContainer;s.days=days;s.pillCount=p;s.times=times}return s})}saveSchedulesToServer();closeScheduleDialog();renderAllSchedules()}
function deleteSchedule(id){schedulesData=schedulesData.filter(s=>s.id!==id);saveSchedulesToServer();renderAllSchedules()}
function renderAllSchedules(){for(var c=1;c<=4;c++){var tb=document.querySelector('#container'+c+'-table tbody');if(tb)tb.innerHTML=''}schedulesData.forEach(s=>{var tb=document.querySelector('#container'+s.container+'-table tbody');if(!tb)return;var tr=document.createElement('tr');tr.innerHTML='<td>'+(s.days||[]).join(', ')+'</td><td>'+(s.pillCount||0)+'</td><td>'+(s.times||[]).join(', ')+'</td>';var a=document.createElement('td');if(s.container<=2){a.innerHTML='<button onclick="openScheduleDialog('+s.container+','+s.id+')">Edit</button> <button onclick="deleteSchedule('+s.id+')">Delete</button>'}else{a.innerHTML='<span style="color:#999">Disabled</span>'}tr.appendChild(a);tb.appendChild(tr)})}
function openSettingsDialog(c){if(c>2)return;currentSettingsContainer=c;var s=settingsData[c]||{};document.getElementById('motorSpeed').value=(s.motorSpeed!==undefined)?s.motorSpeed:128;document.getElementById('triggerThreshold').value=(s.triggerThreshold!==undefined)?s.triggerThreshold:50;document.getElementById('motorSpeedValue').textContent=document.getElementById('motorSpeed').value;document.getElementById('triggerThresholdValue').textContent=document.getElementById('triggerThreshold').value;document.getElementById('overlay').style.display='block';document.getElementById('settingsDialog').style.display='block'}
function closeSettingsDialog(){document.getElementById('overlay').style.display='none';document.getElementById('settingsDialog').style.display='none'}
function saveSettings(){var m=parseInt(document.getElementById('motorSpeed').value);var t=parseInt(document.getElementById('triggerThreshold').value);settingsData[currentSettingsContainer]={motorSpeed:m,triggerThreshold:t};saveSettingsToServer();closeSettingsDialog()}
function testSchedule(){fetch('/testSchedule').then(r=>r.text()).then(t=>alert(t)).catch(()=>alert('Error'))}
function setRTC(){fetch('/setRTC').then(r=>r.text()).then(t=>alert(t)).catch(()=>alert('Failed'))}
window.addEventListener('load',function(){loadSchedules();loadSettings()});
</script></body></html>
)rawliteral";

// ==========================================
// C++ LOGIC
// ==========================================

void setMotorActive(int motorIndex) {
  for (int i = 0; i < NUM_MOTORS; ++i) {
    if (i == motorIndex) {
      ledcWrite(MOTOR_PINS[i], constrain(motorPwmValues[i], 0, 255));
    } else {
      ledcWrite(MOTOR_PINS[i], 0);
    }
  }
}

void addDispenseTask(int container, int motorSpeed, int triggerThreshold, int pillCount) {
  DispenseTask t; t.container = container; t.motorSpeed = motorSpeed; t.triggerThreshold = triggerThreshold; t.pillCount = pillCount;
  dispenseQueue.push_back(t);
}

String getCurrentDayName() {
  time_t now; time(&now);
  struct tm *ti = localtime(&now);
  switch (ti->tm_wday) {
    case 0: return "Sunday"; case 1: return "Monday"; case 2: return "Tuesday";
    case 3: return "Wednesday"; case 4: return "Thursday"; case 5: return "Friday";
    case 6: return "Saturday"; default: return "";
  }
}

// ====== UPDATED: Anti-Duplicate Logic ======
void checkSchedules() {
  if (!LittleFS.exists("/schedules.json")) return;
  
  time_t now = time(NULL);
  struct tm tmnow; 
  localtime_r(&now, &tmnow);
  
  // 1. Minute Lock: If we already ran for this minute, do NOTHING.
  if (tmnow.tm_min == lastDispensedMinute) return; 

  File f = LittleFS.open("/schedules.json","r");
  if (!f) return;
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, f);
  f.close();

  JsonArray arr = doc.as<JsonArray>();
  char curtime[6]; 
  snprintf(curtime, sizeof(curtime), "%02d:%02d", tmnow.tm_hour, tmnow.tm_min);
  String day = getCurrentDayName();
  
  bool taskAdded = false;
  
  // 2. Container Checklist: Ensures one task per container per minute
  bool containerTriggered[NUM_MOTORS] = {false, false}; 

  for (JsonObject s : arr) {
    bool dayMatch = false;
    if (s.containsKey("days")) {
      for (JsonVariant dv : s["days"].as<JsonArray>()) {
        if (dv.as<String>() == day) { dayMatch = true; break; }
      }
    }
    if (!dayMatch) continue;
    
    if (s.containsKey("times")) {
      for (JsonVariant tv : s["times"].as<JsonArray>()) {
        if (tv.as<String>() == String(curtime)) {
          int container = s["container"].as<int>();
          int idx = container - 1;

          // 3. The Gatekeeper: Only add if NOT already added this minute
          if (idx >= 0 && idx < NUM_MOTORS && !containerTriggered[idx]) {
             addDispenseTask(container, motorPwmValues[idx], thresholdValues[idx], s["pillCount"]);
             taskAdded = true;
             containerTriggered[idx] = true; // Mark as done for this container
          }
        }
      }
    }
  }
  
  if (taskAdded) lastDispensedMinute = tmnow.tm_min;
}

// ====== HTTP HANDLERS ======
void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); 
  server.sendContent_P(UI_PART_1);
  server.sendContent_P(UI_PART_2);
}

void handleGetSchedules() {
  if (LittleFS.exists("/schedules.json")) {
    File f = LittleFS.open("/schedules.json","r");
    if (f) { server.streamFile(f,"application/json"); f.close(); return; }
  }
  server.send(200,"application/json","[]");
}

void handleSaveSchedules() {
  if (!server.hasArg("plain")) return server.send(400,"text/plain","No data");
  File f = LittleFS.open("/schedules.json","w");
  if (f) { f.print(server.arg("plain")); f.close(); server.send(200,"text/plain","OK"); }
  else server.send(500,"text/plain","Fail");
}

void handleGetSettings() {
  if (LittleFS.exists("/settings.json")) {
    File f = LittleFS.open("/settings.json","r");
    if (f) { 
      // Load into global var
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, f);
      if(doc.containsKey("phone")) globalPhoneNumber = doc["phone"].as<String>();
      f.seek(0);
      server.streamFile(f,"application/json"); 
      f.close(); 
      return; 
    }
  }
  server.send(200,"application/json","{}");
}

void handleSaveSettings() {
  if (!server.hasArg("plain")) return server.send(400,"text/plain","No data");
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server.arg("plain"));
  if(doc.containsKey("phone")) globalPhoneNumber = doc["phone"].as<String>();

  File f = LittleFS.open("/settings.json","w");
  if (f) { serializeJson(doc, f); f.close(); server.send(200,"text/plain","OK"); }
  else server.send(500,"text/plain","Fail");
}

void handleSetRTC() {
  configTime(19800, 0, ntpServer1, ntpServer2);
  struct tm t;
  if (getLocalTime(&t, 5000)) {
    rtc.set(t.tm_sec, t.tm_min, t.tm_hour, 0, t.tm_mday, t.tm_mon + 1, t.tm_year - 100);
    rtc.refresh();
    server.send(200,"text/plain","RTC Synced (India Time)");
  } else {
    server.send(500,"text/plain","NTP Fail");
  }
}

void handleGetRTCTime() {
  rtc.refresh();
  char buf[32];
  snprintf(buf,sizeof(buf),"%04d-%02d-%02d %02d:%02d:%02d", rtc.year()+2000, rtc.month(), rtc.day(), rtc.hour(), rtc.minute(), rtc.second());
  server.send(200,"text/plain",buf);
}

// === NEW FIXED UPLOAD HANDLER ===
void handleUploadVoice() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String c = server.arg("container");
    if (c == "") c = "1"; 
    
    String filename = "/voice" + c + ".wav";
    Serial.print("UPLOAD START: "); Serial.println(filename);

    if (LittleFS.exists(filename)) LittleFS.remove(filename);
    uploadFile = LittleFS.open(filename, "w");
    
    if (!uploadFile) {
      Serial.println("CRITICAL ERROR: Could not create file. Check Partition Scheme!");
    }
  } 
  
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } 
  
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.print("UPLOAD END. Size: "); Serial.println(upload.totalSize);
    }
  }
}

void handleTestMotor() {
  if (!server.hasArg("container")) return server.send(400,"text/plain","Missing");
  int container = server.arg("container").toInt();
  int idx = container - 1;
  if (idx < 0 || idx >= NUM_MOTORS) return;
  motorPwmValues[idx] = constrain(server.arg("motorSpeed").toInt(), 0, 255);
  thresholdValues[idx] = server.arg("triggerThreshold").toInt();
  if (!motorRunning) {
    currentMotor = idx;
    motorRunning = true;
    stopControl = false;
    previousMillis = millis();
    setMotorActive(currentMotor);
  }
  server.send(200,"text/plain","Started");
}

void handleTestSchedule() {
  if (LittleFS.exists("/schedules.json")) {
     File f = LittleFS.open("/schedules.json","r");
     DynamicJsonDocument doc(4096);
     deserializeJson(doc, f);
     f.close();
     for (JsonObject sch : doc.as<JsonArray>()) {
        int idx = sch["container"].as<int>() - 1;
        if (idx >= 0 && idx < NUM_MOTORS) 
           addDispenseTask(sch["container"], motorPwmValues[idx], thresholdValues[idx], sch["pillCount"]);
     }
  }
  server.send(200,"text/plain","Test Added");
}

void setup() {
  Serial.begin(115200);
  
  // === CRITICAL: START GSM SERIAL ===
  Serial2.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX); 
  delay(1000); 
  Serial2.println("AT"); 
  delay(500);
  Serial2.println("AT+CMGF=1"); // Set Text Mode
  delay(500);
  
  Wire.begin(); 
  rtc.set_rtc_address(0x68);
  rtc.set_model(URTCLIB_MODEL_DS1307);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(WiFi.localIP());

  if (!LittleFS.begin()) { LittleFS.format(); LittleFS.begin(); }

  // Load Settings for Phone
  if (LittleFS.exists("/settings.json")) {
    File f = LittleFS.open("/settings.json","r");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, f);
    if(doc.containsKey("phone")) globalPhoneNumber = doc["phone"].as<String>();
    f.close();
  }

  for (int i = 0; i < NUM_MOTORS; ++i) {
    ledcAttach(MOTOR_PINS[i], PWM_FREQ, PWM_RES);
    ledcWrite(MOTOR_PINS[i], 0);
  }

  configTime(19800, 0, ntpServer1, ntpServer2);

  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/getSchedules", HTTP_GET, handleGetSchedules);
  server.on("/saveSchedules", HTTP_POST, handleSaveSchedules);
  server.on("/getSettings", HTTP_GET, handleGetSettings);
  server.on("/saveSettings", HTTP_POST, handleSaveSettings);
  server.on("/setRTC", HTTP_GET, handleSetRTC);
  server.on("/getRTCTime", HTTP_GET, handleGetRTCTime);
  server.on("/testMotor", HTTP_GET, handleTestMotor);
  server.on("/testSchedule", HTTP_GET, handleTestSchedule);
  
  server.on("/uploadVoice", HTTP_POST, [](){ server.send(200, "text/plain", "Upload Success"); }, handleUploadVoice);

  server.begin();
}

void startMotorForSchedule() {
  int idx = scheduledContainer - 1;
  if (idx < 0 || idx >= NUM_MOTORS) { scheduleActive = false; return; }
  
  if (scheduleActive && (scheduledPillCountRemaining == -1 || scheduledPillCountRemaining > 0)) {
     // === PRIORITY CHANGE: SEND SMS FIRST ===
     String msg = "Alert: Time to take medicine from Container " + String(scheduledContainer);
     sendSMS(msg);

     // === THEN PLAY AUDIO ===
     playVoiceFile(scheduledContainer);
  }

  motorPwmValues[idx] = scheduledMotorSpeed;
  thresholdValues[idx] = scheduledTriggerThreshold;
  currentMotor = idx;
  motorRunning = true;
  stopControl = false;
  previousMillis = millis();
  setMotorActive(currentMotor);
}

void loop() {
  server.handleClient();

  // === MOTOR LOGIC ===
  if (motorRunning && !stopControl && currentMotor >= 0) {
    if (millis() - previousMillis >= motorInterval) {
      previousMillis = millis();
      int val = analogRead(ANALOG_PIN);
      
      if (val > thresholdValues[currentMotor]) {
        ledcWrite(MOTOR_PINS[currentMotor], 0); 
        motorRunning = false;
        stopControl = true;
        currentMotor = -1;
        delay(interCycleDelay);
        if (scheduleActive && scheduledPillCountRemaining > 1) {
          scheduledPillCountRemaining--;
          delay(1000);
           int idx = scheduledContainer - 1;
           motorPwmValues[idx] = scheduledMotorSpeed;
           thresholdValues[idx] = scheduledTriggerThreshold;
           currentMotor = idx;
           motorRunning = true;
           stopControl = false;
           previousMillis = millis();
           setMotorActive(currentMotor);
        } else {
          scheduleActive = false;
        }
      }
    }
  }

  if (!motorRunning && !dispenseQueue.empty() && !scheduleActive) {
    DispenseTask t = dispenseQueue.front(); dispenseQueue.erase(dispenseQueue.begin());
    scheduledContainer = t.container;
    scheduledMotorSpeed = t.motorSpeed;
    scheduledTriggerThreshold = t.triggerThreshold;
    scheduledPillCountRemaining = t.pillCount;
    scheduleActive = true;
    startMotorForSchedule();
  }

  if (millis() - lastScheduleCheck >= scheduleCheckInterval) {
    lastScheduleCheck = millis();
    checkSchedules();
  }
}
