#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>

#define DEBUG_TX   true
#define DEBUG_RF   false
#define DEBUG_HUM  true
#define DEBUG_NONE false

const char* AP_SSID = "Skimboute";
const char* AP_PASS = "skimboute1423";
WebServer server(80);

RF24 radio(17, 5);
const byte address[6] = "00001";

const int xPin       = 34;
const int yPin       = 35;
const int SW_PIN     = 32;
const int DEADZONE   = 400;
const int ADC_CENTER = 2047;

struct DataOut {
  int16_t x;
  int16_t y;
  bool    emergency;
} __attribute__((packed));

struct DataAck {
  int16_t humidity;
} __attribute__((packed));

DataOut dataOut;
DataAck dataAck;

volatile int  g_rawX      = 0, g_rawY = 0;
volatile int  g_speedX    = 0, g_speedY = 0;
volatile int  g_microsL   = 1500, g_microsR = 1500;
volatile bool g_rfOk      = false;
volatile int  g_humidity  = 0;
volatile bool g_emergency = false;
bool g_emergencyLatch     = false; // toggle
bool g_lastSW             = false;
String g_dir = "IMMOBILE";

unsigned long g_lastPing  = 0;

// ─── Logs ──────────────────────────────────────────────
#define MAX_LOGS 50
String g_logs[MAX_LOGS];
int    g_logIndex = 0;
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

void addLog(String msg) {
  portENTER_CRITICAL(&logMux);
  g_logs[g_logIndex % MAX_LOGS] = msg;
  g_logIndex++;
  portEXIT_CRITICAL(&logMux);
}

// ─── Web ───────────────────────────────────────────────
void handleRoot() {
  String html = R"(
<!DOCTYPE html><html><head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Skimboute</title>
<style>
  body{font-family:monospace;background:#111;color:#0f0;padding:16px;margin:0;}
  h1{color:#0ff;margin:0 0 12px;}
  .val{font-size:1.2em;margin:6px 0;}
  .dir{font-size:2em;color:#ff0;margin:12px 0;font-weight:bold;}
  .ok{color:#0f0;} .fail{color:#f00;}
  .hum{font-size:1.4em;color:#0ff;margin:8px 0;}
  .emergency{font-size:1.6em;color:#f00;font-weight:bold;
             padding:8px;border:2px solid #f00;display:none;margin:8px 0;}
  .emergency.active{display:block;}
  #logs{background:#000;border:1px solid #333;padding:10px;height:200px;
        overflow-y:auto;font-size:0.85em;color:#aaa;margin-top:12px;}
  .log-entry{border-bottom:1px solid #1a1a1a;padding:3px 0;}
</style>
<script>
  let lastLog = 0;

  setInterval(()=>{
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('rawX').innerText  = d.rawX;
      document.getElementById('rawY').innerText  = d.rawY;
      document.getElementById('sX').innerText    = d.speedX;
      document.getElementById('sY').innerText    = d.speedY;
      document.getElementById('mL').innerText    = d.microsL;
      document.getElementById('mR').innerText    = d.microsR;
      document.getElementById('dir').innerText   = d.dir;
      document.getElementById('hum').innerText   = d.humidity + '%';
      document.getElementById('rf').innerText    = d.rfOk ? 'Connecté' : 'Signal perdu';
      document.getElementById('rf').className    = d.rfOk ? 'ok' : 'fail';
      let em = document.getElementById('emerg');
      em.className = 'emergency' + (d.emergency ? ' active' : '');
    });
  }, 100);

  setInterval(()=>{
    fetch('/logs?from='+lastLog).then(r=>r.json()).then(d=>{
      if(d.entries.length === 0) return;
      lastLog = d.next;
      let box = document.getElementById('logs');
      d.entries.forEach(e=>{
        let div = document.createElement('div');
        div.className = 'log-entry';
        div.innerText = e;
        box.appendChild(div);
      });
      box.scrollTop = box.scrollHeight;
    });
  }, 500);
</script>
</head><body>
<h1>🏄 Skimboute</h1>
<div class='emergency' id='emerg'>🚨 ARRÊT D'URGENCE ACTIVÉ</div>
<div class='hum'>💧 Humidité: <b id='hum'>-</b></div>
<div class='dir' id='dir'>-</div>
<div class='val'>Motor L: <b id='mL'>-</b> µs | Motor R: <b id='mR'>-</b> µs</div>
<div class='val'>speedX: <b id='sX'>-</b> | speedY: <b id='sY'>-</b></div>
<div class='val'>rawX: <b id='rawX'>-</b> | rawY: <b id='rawY'>-</b></div>
<div class='val'>Radio: <b id='rf'>-</b></div>
<div id='logs'></div>
</body></html>
)";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"rawX\":"      + String(g_rawX)      + ",";
  json += "\"rawY\":"      + String(g_rawY)      + ",";
  json += "\"speedX\":"    + String(g_speedX)    + ",";
  json += "\"speedY\":"    + String(g_speedY)    + ",";
  json += "\"microsL\":"   + String(g_microsL)   + ",";
  json += "\"microsR\":"   + String(g_microsR)   + ",";
  json += "\"humidity\":"  + String(g_humidity)  + ",";
  json += "\"dir\":\""     + g_dir               + "\",";
  json += "\"emergency\":" + String(g_emergencyLatch ? "true" : "false") + ",";
  json += "\"rfOk\":"      + String(g_rfOk ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogs() {
  int from    = server.hasArg("from") ? server.arg("from").toInt() : 0;
  int current = g_logIndex;
  int start   = max(from, current - MAX_LOGS);

  String json = "{\"entries\":[";
  bool first  = true;
  for (int i = start; i < current; i++) {
    String entry = g_logs[i % MAX_LOGS];
    if (entry.length() == 0) continue;
    if (!first) json += ",";
    // Escape quotes
    entry.replace("\"", "\\\"");
    json += "\"" + entry + "\"";
    first = false;
  }
  json += "],\"next\":" + String(current) + "}";
  server.send(200, "application/json", json);
}

// ─── RF Task core 1 ────────────────────────────────────
void rfTask(void* pvParameters) {
  while(1) {
    int rawX = analogRead(xPin);
    int rawY = analogRead(yPin);
    bool sw  = (digitalRead(SW_PIN) == LOW);

    // Toggle urgence sur front montant bouton
    if (sw && !g_lastSW) {
      g_emergencyLatch = !g_emergencyLatch;
      if (g_emergencyLatch) addLog("[URGENCE] Arrêt d'urgence ACTIVÉ");
      else                  addLog("[URGENCE] Arrêt d'urgence désactivé");
    }
    g_lastSW = sw;

    int cx = rawX - ADC_CENTER;
    int cy = rawY - ADC_CENTER;
    if (abs(cx) < DEADZONE) cx = 0;
    if (abs(cy) < DEADZONE) cy = 0;

    dataOut.x         = (int16_t)rawX;
    dataOut.y         = (int16_t)rawY;
    dataOut.emergency = g_emergencyLatch;

    unsigned long t = micros();
    bool ok = radio.write(&dataOut, sizeof(dataOut));
    unsigned long latency = micros() - t;
    g_rfOk  = ok;

    if (!ok) addLog("[RF] Signal perdu");

    // ACK payload humidité
    if (ok && radio.isAckPayloadAvailable()) {
      radio.read(&dataAck, sizeof(dataAck));
      // Capteur: 0=trempé, 1023=sec -> inverser
      g_humidity = map(dataAck.humidity, 0, 1023, 0, 100);
      g_humidity = constrain(g_humidity, 0, 100);
    }

    // Direction — alignée avec RX
    int sY = 0, sX = 0;
    if (abs(cx) >= DEADZONE) sY = -map(cx, -ADC_CENTER, ADC_CENTER, -100, 100);
    if (abs(cy) >= DEADZONE) sX = -map(cy, -ADC_CENTER, ADC_CENTER, -100, 100);

    int mL = constrain(map(sY + sX, -100, 100, 1000, 2000), 1000, 2000);
    int mR = constrain(map(sY - sX, -100, 100, 1000, 2000), 1000, 2000);

    g_rawX   = rawX; g_rawY = rawY;
    g_speedX = sX;   g_speedY = sY;
    g_microsL = mL;  g_microsR = mR;

    if      (sY == 0 && sX == 0)      g_dir = "IMMOBILE";
    else if (sY > 0  && abs(sX) < 20) g_dir = "AVANCE";
    else if (sY < 0  && abs(sX) < 20) g_dir = "RECULE";
    else if (sX > 0  && abs(sY) < 20) g_dir = "DROITE";
    else if (sX < 0  && abs(sY) < 20) g_dir = "GAUCHE";
    else                               g_dir = "MIXTE";

    // Ping toutes les 10 secondes
    unsigned long now = millis();
    if (now - g_lastPing >= 10000) {
    // Dans le ping log:
      addLog("[PING] RF=" + String(ok ? "OK" : "FAIL") +
             " latence=" + String(latency/1000) + "ms" +
             " HUM=" + String(g_humidity) + "%" +
           " DIR=" + g_dir);
    }

    if (!DEBUG_NONE && DEBUG_TX) {
      Serial.print("[TX] rawX="); Serial.print(rawX);
      Serial.print(" rawY="); Serial.print(rawY);
      Serial.print(" EMG="); Serial.print(g_emergencyLatch);
      Serial.print(" RF="); Serial.println(ok ? "OK" : "FAIL");
    }
    if (!DEBUG_NONE && DEBUG_HUM) {
      Serial.print("[HUM] "); Serial.print(g_humidity); Serial.println("%");
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ─── WiFi Task core 0 ──────────────────────────────────
void wifiTask(void* pvParameters) {
  while(1) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(SW_PIN, INPUT_PULLUP);
  if (!DEBUG_NONE) Serial.println("[BOOT] ESP32 start");

  if (!radio.begin()) {
    Serial.println("[ERR] radio FAIL");
    while(1) { delay(100); }
  }
  radio.setPALevel(RF24_PA_MIN);
  radio.enableAckPayload();
  radio.openWritingPipe(address);
  radio.stopListening();
  if (!DEBUG_NONE) Serial.println("[OK] RF24 ready");

  WiFi.softAP(AP_SSID, AP_PASS);
  if (!DEBUG_NONE) {
    Serial.print("[WiFi] AP: "); Serial.println(AP_SSID);
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.softAPIP());
  }

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/logs", handleLogs);
  server.begin();
  if (!DEBUG_NONE) Serial.println("[OK] Web server ready");

  addLog("[BOOT] Skimboute démarré");

  xTaskCreatePinnedToCore(rfTask,   "RF",   4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wifiTask, "WiFi", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
