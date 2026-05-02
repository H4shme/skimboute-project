#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>

#define DEBUG_TX   true
#define DEBUG_RF   false
#define DEBUG_NONE false

// ─── WiFi AP ───────────────────────────────────────────
const char* AP_SSID = "Skimboute";
const char* AP_PASS = "skimboute1423"; // min 8 chars
WebServer server(80);

// ─── RF24 ──────────────────────────────────────────────
RF24 radio(17, 5);
const byte address[6] = "00001";

const int xPin     = 34;
const int yPin     = 35;
const int DEADZONE = 400;
const int ADC_CENTER = 2047;

struct Data {
  int16_t x;
  int16_t y;
} __attribute__((packed));
Data data;

// ─── State partagé ─────────────────────────────────────
volatile int g_rawX = 0, g_rawY = 0;
volatile int g_speedX = 0, g_speedY = 0;
volatile int g_microsL = 1500, g_microsR = 1500;
volatile bool g_rfOk = false;
String g_dir = "IMMOBILE";

// ─── Web handlers ──────────────────────────────────────
void handleRoot() {
  String html = R"(
<!DOCTYPE html><html><head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Skimboute</title>
<style>
  body{font-family:monospace;background:#111;color:#0f0;padding:20px;}
  h1{color:#0ff;}
  .val{font-size:1.4em;margin:8px 0;}
  .dir{font-size:2em;color:#ff0;margin:16px 0;}
  .stop{color:#f00;}
  .ok{color:#0f0;}
  .fail{color:#f00;}
</style>
<script>
  setInterval(()=>{
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('rawX').innerText=d.rawX;
      document.getElementById('rawY').innerText=d.rawY;
      document.getElementById('sX').innerText=d.speedX;
      document.getElementById('sY').innerText=d.speedY;
      document.getElementById('mL').innerText=d.microsL;
      document.getElementById('mR').innerText=d.microsR;
      document.getElementById('dir').innerText=d.dir;
      document.getElementById('rf').innerText=d.rfOk?Connecté':'Signal perdu';
      document.getElementById('rf').className=d.rfOk?'ok':'fail';
    });
  }, 100);
</script>
</head><body>a
<h1>Skimboute</h1>
<div class='val'>rawX: <b id='rawX'>-</b></div>
<div class='val'>rawY: <b id='rawY'>-</b></div>
<div class='val'>speedX: <b id='sX'>-</b></div>
<div class='val'>speedY: <b id='sY'>-</b></div>
<div class='val'>Motor L: <b id='mL'>-</b> µs</div>
<div class='val'>Motor R: <b id='mR'>-</b> µs</div>
<div class='dir' id='dir'>-</div>
<div class='val'>Connection radio : <b id='rf'>-</b></div>
</body></html>
)";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"rawX\":" + String(g_rawX) + ",";
  json += "\"rawY\":" + String(g_rawY) + ",";
  json += "\"speedX\":" + String(g_speedX) + ",";
  json += "\"speedY\":" + String(g_speedY) + ",";
  json += "\"microsL\":" + String(g_microsL) + ",";
  json += "\"microsR\":" + String(g_microsR) + ",";
  json += "\"dir\":\"" + g_dir + "\",";
  json += "\"rfOk\":" + String(g_rfOk ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// ─── RF24 task (core 1) ────────────────────────────────
void rfTask(void* pvParameters) {
  while(1) {
    int rawX = analogRead(xPin);
    int rawY = analogRead(yPin);

    int cx = rawX - ADC_CENTER;
    int cy = rawY - ADC_CENTER;
    if (abs(cx) < DEADZONE) cx = 0;
    if (abs(cy) < DEADZONE) cy = 0;

    data.x = (int16_t)rawX;
    data.y = (int16_t)rawY;

    bool ok = radio.write(&data, sizeof(data));

    // Calcul direction (même logique que RX pour affichage)
    int sY = 0, sX = 0;
    if (abs(cx) >= DEADZONE) sY = map(cx, -ADC_CENTER, ADC_CENTER, -100, 100);
    if (abs(cy) >= DEADZONE) sX = map(cy, -ADC_CENTER, ADC_CENTER, -100, 100);

    int mL = constrain(map(sY + sX, -100, 100, 1000, 2000), 1000, 2000);
    int mR = constrain(map(sY - sX, -100, 100, 1000, 2000), 1000, 2000);

    // Update state partagé
    g_rawX = rawX; g_rawY = rawY;
    g_speedX = sX; g_speedY = sY;
    g_microsL = mL; g_microsR = mR;
    g_rfOk = ok;

    if      (sY == 0 && sX == 0)      g_dir = "IMMOBILE";
    else if (sY > 0  && abs(sX) < 20) g_dir = "AVANCE";
    else if (sY < 0  && abs(sX) < 20) g_dir = "RECULE";
    else if (sX > 0  && abs(sY) < 20) g_dir = "DROITE";
    else if (sX < 0  && abs(sY) < 20) g_dir = "GAUCHE";
    else                               g_dir = "MIXTE";

    if (!DEBUG_NONE && DEBUG_TX) {
      Serial.print("[TX] rawX="); Serial.print(rawX);
      Serial.print(" rawY="); Serial.print(rawY);
      Serial.print(" RF="); Serial.println(ok ? "OK" : "FAIL");
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ─── WiFi task (core 0) ────────────────────────────────
void wifiTask(void* pvParameters) {
  while(1) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  if (!DEBUG_NONE) Serial.println("[BOOT] ESP32 start");

  // RF24
  if (!radio.begin()) {
    Serial.println("[ERR] radio FAIL");
    while(1) { delay(100); }
  }
  radio.setPALevel(RF24_PA_MIN);
  radio.openWritingPipe(address);
  radio.stopListening();
  if (!DEBUG_NONE) Serial.println("[OK] RF24 ready");

  // WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS);
  if (!DEBUG_NONE) {
    Serial.print("[WiFi] AP: "); Serial.println(AP_SSID);
    Serial.print("[WiFi] IP: "); Serial.println(WiFi.softAPIP());
  }

  // Web routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  if (!DEBUG_NONE) Serial.println("[OK] Web server ready");

  // Tasks sur cores séparés
  xTaskCreatePinnedToCore(rfTask,   "RF",   4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wifiTask, "WiFi", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS); // loop vide, tout dans tasks
}
