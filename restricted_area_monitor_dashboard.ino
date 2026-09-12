#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------------- WiFi Credentials ---------------- // Enter Credentials
const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";

WebServer server(80);

// ---------------- Pin Definitions ----------------
#define SS_PIN      5     // RFID SDA/SS
#define RST_PIN     27    // RFID RST
#define PIR_PIN     4     // PIR sensor OUT
#define BUZZER_PIN  2     // Buzzer +

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------------- Authorized Cards ----------------
String authorizedUIDs[] = {
  
  "46103207"
};
const int numAuthorized = sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);

// ---------------- State ----------------
bool waitingForCard = false;
unsigned long motionTime = 0;
const unsigned long SCAN_WINDOW_MS = 10000;
const unsigned long RESULT_DISPLAY_MS = 3000;
const unsigned long CARD_COOLDOWN_MS = 10000; // Time system rests after ANY scan event or alert
unsigned long detectionPausedUntil = 0;

// ---------------- Dashboard State ----------------
String systemStatus = "Monitoring for motion...";
unsigned long totalGranted = 0;
unsigned long totalDenied = 0;

struct LogEntry {
  String time;
  String type;    // GRANTED, DENIED, INFO
  String message;
};

const int LOG_SIZE = 12;
LogEntry eventLog[LOG_SIZE];
int logIndex = 0;
int logCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init failed - check wiring/address");
    while (true) delay(10);
  }

  showMessage("Connecting", "to WiFi...", "");
  connectWiFi();

  setupWebServer();

  showIdleScreen();
  addLogEntry("INFO", "System started");
  Serial.println("System ready. Monitoring for motion...");
}

void loop() {
  server.handleClient();

  bool motionDetected = digitalRead(PIR_PIN) == HIGH;

  // NEW: Check if the cooldown has just expired while idling, and revert status back to monitoring
  if (!waitingForCard && systemStatus == "System Cooldown..." && millis() >= detectionPausedUntil) {
    systemStatus = "Monitoring for motion...";
    showIdleScreen();
    Serial.println(systemStatus);
  }

  // 1. Trigger motion ONLY if we aren't currently waiting for a card AND the cooldown period has expired
  if (motionDetected && !waitingForCard && millis() > detectionPausedUntil) {
    waitingForCard = true;
    motionTime = millis();
    rfid.PCD_Init(); // Re-init reader so it reliably detects a fresh card scan
    systemStatus = "Motion detected - waiting for card scan...";
    Serial.println(systemStatus);
    showMessage("Motion Detected", "Please scan", "your card");
  }

  // 2. Process card checking and timeout logic while actively waiting for a scan
  if (waitingForCard) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = getUID();
      Serial.println("Card scanned: " + uid);

      if (isAuthorized(uid)) {
        grantAccess(uid);
      } else {
        denyAccess("Unrecognized card (UID " + uid + ")");
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      
      // Arm cooldown immediately after a card has been completely evaluated
      detectionPausedUntil = millis() + CARD_COOLDOWN_MS;
      waitingForCard = false;
      handleDelay(RESULT_DISPLAY_MS); // Shows result screen for 3 seconds
      
      // UPDATED: Set status to Cooldown for the remaining cooldown duration
      systemStatus = "System Cooldown...";
      showIdleScreen();
    }
    else if (millis() - motionTime > SCAN_WINDOW_MS) {
      Serial.println("No card scanned within window - security alert.");
      denyAccess("No card presented");
      
      // Arm cooldown immediately after a timeout security alert executes
      detectionPausedUntil = millis() + CARD_COOLDOWN_MS;
      waitingForCard = false;
      handleDelay(RESULT_DISPLAY_MS); // Shows alert screen for 3 seconds
      
      // UPDATED: Set status to Cooldown for the remaining cooldown duration
      systemStatus = "System Cooldown...";
      showIdleScreen();
    }
  }
}

// ---------------- WiFi + Web Server ----------------

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! Dashboard IP: ");
    Serial.println(WiFi.localIP());
    showMessage("WiFi Connected", WiFi.localIP().toString(), "");
    delay(2000);
  } else {
    Serial.println();
    Serial.println("WiFi connection failed - continuing without dashboard.");
    showMessage("WiFi Failed", "Running offline", "");
    delay(2000);
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Web server started.");
}

void handleRoot() {
  server.send(200, "text/html", buildDashboardHTML());
}

void handleStatus() {
  server.send(200, "application/json", buildStatusJSON());
}

String buildDashboardHTML() {
  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Restricted Area Monitor</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: 'Segoe UI', Arial, sans-serif;
    background: #0f1420;
    color: #e6e9f0;
    padding: 20px;
  }
  .container { max-width: 640px; margin: 0 auto; }
  h1 {
    font-size: 20px;
    text-align: center;
    letter-spacing: 1px;
    color: #8fa3ff;
    margin-bottom: 20px;
  }
  .status-card {
    background: #171d2e;
    border-radius: 12px;
    padding: 24px;
    text-align: center;
    margin-bottom: 16px;
    border: 1px solid #262f45;
    transition: border-color 0.3s;
  }
  .status-card.granted { border-color: #33cc77; }
  .status-card.denied {
    border-color: #ff4d4d;
    animation: pulseBorder 1s infinite;
  }
  .status-label { font-size: 13px; color: #8892a6; text-transform: uppercase; letter-spacing: 1px; }
  .status-value { font-size: 22px; margin-top: 8px; font-weight: 600; }
  .warning-icon {
    font-size: 42px;
    display: none;
    margin-bottom: 6px;
    animation: pulseWarning 1s infinite;
  }
  .status-card.denied .warning-icon { display: block; }
  @keyframes pulseWarning {
    0%, 100% { opacity: 1; transform: scale(1); }
    50% { opacity: 0.4; transform: scale(1.15); }
  }
  @keyframes pulseBorder {
    0%, 100% { box-shadow: 0 0 0px rgba(255,77,77,0.5); }
    50% { box-shadow: 0 0 22px rgba(255,77,77,0.85); }
  }
  .stats { display: flex; gap: 16px; margin-bottom: 16px; }
  .stat-box {
    flex: 1;
    background: #171d2e;
    border-radius: 12px;
    padding: 16px;
    text-align: center;
    border: 1px solid #262f45;
  }
  .stat-num { font-size: 28px; font-weight: 700; }
  .stat-box.granted .stat-num { color: #33cc77; }
  .stat-box.denied .stat-num { color: #ff4d4d; }
  .stat-label { font-size: 12px; color: #8892a6; margin-top: 4px; }
  .log-card {
    background: #171d2e;
    border-radius: 12px;
    padding: 16px;
    border: 1px solid #262f45;
  }
  .log-title { font-size: 13px; color: #8892a6; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; }
  .log-entry {
    display: flex;
    justify-content: space-between;
    padding: 8px 4px;
    border-bottom: 1px solid #232b40;
    font-size: 13px;
  }
  .log-entry:last-child { border-bottom: none; }
  .log-time { color: #6b7590; font-family: monospace; }
  .log-msg { flex: 1; margin: 0 10px; }
  .tag { padding: 2px 8px; border-radius: 6px; font-size: 11px; font-weight: 600; }
  .tag.GRANTED { background: rgba(51,204,119,0.15); color: #33cc77; }
  .tag.DENIED { background: rgba(255,77,77,0.15); color: #ff4d4d; }
  .tag.INFO { background: rgba(143,163,255,0.15); color: #8fa3ff; }
  .footer { text-align: center; font-size: 11px; color: #4a5370; margin-top: 16px; }
</style>
</head>
<body>
<div class="container">
  <h1>RESTRICTED AREA MONITORING SYSTEM</h1>

  <div class="status-card" id="statusCard">
    <div class="warning-icon" id="warningIcon">&#9888;&#65039;</div>
    <div class="status-label">Current Status</div>
    <div class="status-value" id="statusValue">Loading...</div>
  </div>

  <div class="stats">
    <div class="stat-box granted">
      <div class="stat-num" id="grantedCount">0</div>
      <div class="stat-label">Access Granted</div>
    </div>
    <div class="stat-box denied">
      <div class="stat-num" id="deniedCount">0</div>
      <div class="stat-label">Access Denied</div>
    </div>
  </div>

  <div class="log-card">
    <div class="log-title">Recent Activity</div>
    <div id="logList"></div>
  </div>

  <div class="footer">Auto-refreshing every 1.5s</div>
</div>

<script>
async function refresh() {
  try {
    const res = await fetch('/status');
    const data = await res.json();

    const statusCard = document.getElementById('statusCard');
    const statusValue = document.getElementById('statusValue');
    statusValue.textContent = data.status;

    statusCard.className = 'status-card';
    if (data.status.includes('GRANTED')) statusCard.classList.add('granted');
    if (data.status.includes('DENIED')) statusCard.classList.add('denied');

    document.getElementById('grantedCount').textContent = data.grantedCount;
    document.getElementById('deniedCount').textContent = data.deniedCount;

    const logList = document.getElementById('logList');
    logList.innerHTML = '';
    data.log.slice().reverse().forEach(entry => {
      const row = document.createElement('div');
      row.className = 'log-entry';
      row.innerHTML =
        '<span class="log-time">' + entry.time + '</span>' +
        '<span class="log-msg">' + entry.message + '</span>' +
        '<span class="tag ' + entry.type + '">' + entry.type + '</span>';
      logList.appendChild(row);
    });
  } catch (e) {
    document.getElementById('statusValue').textContent = 'Connection lost...';
  }
}
refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)HTML";
  return html;
}

String buildStatusJSON() {
  String json = "{";
  json += "\"status\":\"" + systemStatus + "\",";
  json += "\"grantedCount\":" + String(totalGranted) + ",";
  json += "\"deniedCount\":" + String(totalDenied) + ",";
  json += "\"log\":[";

  for (int i = 0; i < logCount; i++) {
    int idx = (logIndex - logCount + i + LOG_SIZE) % LOG_SIZE;
    json += "{\"time\":\"" + eventLog[idx].time + "\",";
    json += "\"type\":\"" + eventLog[idx].type + "\",";
    json += "\"message\":\"" + eventLog[idx].message + "\"}";
    if (i < logCount - 1) json += ",";
  }

  json += "]}";
  return json;
}

// ---------------- Logging ----------------

void addLogEntry(String type, String message) {
  eventLog[logIndex].time = getUptimeString();
  eventLog[logIndex].type = type;
  eventLog[logIndex].message = message;
  logIndex = (logIndex + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
}

String getUptimeString() {
  unsigned long s = millis() / 1000;
  unsigned long h = (s / 3600) % 24;
  unsigned long m = (s / 60) % 60;
  unsigned long sec = s % 60;
  char buf[9];
  sprintf(buf, "%02lu:%02lu:%02lu", h, m, sec);
  return String(buf);
}

void handleDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient();
    delay(10);
  }
}

// ---------------- RFID Helpers ----------------

String getUID() {
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();
  return uidStr;
}

bool isAuthorized(String uid) {
  for (int i = 0; i < numAuthorized; i++) {
    if (uid == authorizedUIDs[i]) return true;
  }
  return false;
}

// ---------------- Access Actions ----------------

void grantAccess(String uid) {
  systemStatus = "ACCESS GRANTED - WELCOME";
  Serial.println(systemStatus);
  showMessage("ACCESS GRANTED", "WELCOME", "");
  totalGranted++;
  addLogEntry("GRANTED", "UID " + uid + " - Welcome");

  digitalWrite(BUZZER_PIN, HIGH);
  handleDelay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

void denyAccess(String reason) {
  systemStatus = "ACCESS DENIED - SECURITY ALERT";
  Serial.println(systemStatus + " (" + reason + ")");
  showMessage("ACCESS DENIED", "SECURITY ALERT", reason);
  totalDenied++;
  addLogEntry("DENIED", reason);

  for (int i = 0; i < 6; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    handleDelay(200);
    digitalWrite(BUZZER_PIN, LOW);
    handleDelay(150);
  }
}

// ---------------- OLED Helpers ----------------

void showMessage(String line1, String line2, String line3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 26);
  display.println(line2);

  if (line3.length() > 0) {
    display.setTextSize(1);
    display.setCursor(0, 52);
    display.println(line3);
  }
  display.display();
}

void showIdleScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RESTRICTED AREA");
  display.println("MONITORING SYSTEM");
  display.setCursor(0, 30);
  
  // UPDATED: Dynamically checks if the hardware is strictly in its cooldown phase
  if (millis() < detectionPausedUntil) {
    display.println("Status: COOLDOWN");
  } else {
    display.println("Status: Monitoring...");
  }
  
  display.setCursor(0, 45);
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP().toString());
  } else {
    display.println("Waiting for motion");
  }
  display.display();
}