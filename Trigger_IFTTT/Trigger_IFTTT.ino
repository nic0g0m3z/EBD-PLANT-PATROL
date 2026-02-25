#include <WiFi.h>
#include <WiFiClientSecure.h>

// ====== WIFI ======
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

// ====== IFTTT ======
const char* IFTTT_EVENT = "pot_empty";
const char* IFTTT_KEY   = "pKvv44enpRrdWCGAu-on4LQxT-wd70Ks79L_BquhJW9";

// ====== PIN ======
const int HALL_PIN = A1;  

// ====== TIMING ======
const unsigned long ACTIVE_CONFIRM_MS   = 50;   
const unsigned long INACTIVE_CONFIRM_MS = 200;  

// ====== STATE ======
bool latched = false;
unsigned long activeSince = 0;
unsigned long inactiveSince = 0;

// ====== WIFI ======
void connectWiFi() {
  Serial.println("[WiFi] Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection FAILED");
  }
}

// ====== IFTTT ======
bool sendIFTTT() {
  Serial.println("[IFTTT] Sending trigger...");

  WiFiClientSecure client;
  client.setInsecure();

  const char* host = "maker.ifttt.com";
  String url = String("/trigger/") + IFTTT_EVENT + "/with/key/" + IFTTT_KEY;

  if (!client.connect(host, 443)) {
    Serial.println("[IFTTT] TLS connect failed");
    return false;
  }

  client.print(String("POST ") + url + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("User-Agent: esp32\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: 0\r\n\r\n");

  // Read HTTP status line
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();

  Serial.print("[IFTTT] HTTP: ");
  Serial.println(statusLine);

  bool ok = statusLine.startsWith("HTTP/1.1 2") || statusLine.startsWith("HTTP/2 2");

  // Drain response
  unsigned long t = millis();
  while (client.connected() && millis() - t < 1500) {
    while (client.available()) client.read();
    delay(5);
  }

  client.stop();
  Serial.println(ok ? "[IFTTT] SUCCESS" : "[IFTTT] FAILED");
  return ok;
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n=== ESP32 Hall Sensor (Latched) ===");

  // External pull-up present
  pinMode(HALL_PIN, INPUT);

  connectWiFi();
  Serial.println("[System] Ready");
}

// ====== LOOP ======
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected → reconnecting");
    connectWiFi();
  }

  int state = digitalRead(HALL_PIN); 
  unsigned long now = millis();

  static int lastState = HIGH;
  if (state != lastState) {
    Serial.print("[Input] ");
    Serial.println(state == LOW ? "LOW (active)" : "HIGH (inactive)");
    lastState = state;
  }

  if (state == LOW) {
    inactiveSince = 0;

    if (activeSince == 0) {
      activeSince = now;
      Serial.println("[Logic] LOW detected → confirming...");
    }

    if (!latched && (now - activeSince) >= ACTIVE_CONFIRM_MS) {
      Serial.println("[Logic] ACTIVE confirmed");

      if (WiFi.status() == WL_CONNECTED) {
        bool ok = sendIFTTT();
        if (ok) {
          latched = true;
          Serial.println("[Logic] Latched (notification sent)");
        } else {
          Serial.println("[Logic] Not latched (send failed, will retry)");
        }
      }
    }

  } else { 
    activeSince = 0;

    if (inactiveSince == 0) {
      inactiveSince = now;
      Serial.println("[Logic] HIGH detected → re-arm timer");
    }

    if (latched && (now - inactiveSince) >= INACTIVE_CONFIRM_MS) {
      latched = false;
      Serial.println("[Logic] Re-armed");
    }
  }

  delay(10);
}
