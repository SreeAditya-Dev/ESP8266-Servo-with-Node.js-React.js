#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Servo.h>

#define EEPROM_SIZE 64
#define SERVO_PIN D4
#define SERVER_URL "http://192.168.29.156:5000/get-servo" // Replace with Render backend URL

ESP8266WebServer server(80);
WiFiClient client;
Servo myServo;

// WiFi Credentials
String ssid = "";
String password = "";

// AP Mode Credentials
const char* AP_ssid = "NodeMCU_AP";
const char* AP_password = "12345678"; 

int lastAngle = -1;

// 📌 Serve HTML page to enter WiFi credentials
void handleRoot() {
  String html = "<html><body><h1>Enter WiFi Credentials</h1>";
  html += "<form action='/connect' method='POST'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<input type='submit' value='Connect'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

// 📌 Handle WiFi credential submission
void handleConnect() {
  ssid = server.arg("ssid");
  password = server.arg("password");

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < ssid.length(); i++) EEPROM.write(i, ssid[i]);
  EEPROM.write(ssid.length(), '\0');
  for (int i = 0; i < password.length(); i++) EEPROM.write(32 + i, password[i]);
  EEPROM.write(32 + password.length(), '\0');
  EEPROM.commit();
  EEPROM.end();

  Serial.println("✅ WiFi credentials saved. Restarting...");
  server.send(200, "text/html", "<h1>WiFi Credentials Saved! Restarting...</h1>");
  delay(3000);
  ESP.restart();
}

// 📌 Handle webpage to show connection details
void handleWebPage() {
  String html = "<html><body><h1>ESP8266 Connected</h1>";
  html += "<p>Connected to: " + ssid + "</p>";
  html += "<p>Device IP: " + WiFi.localIP().toString() + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  
  char stored_ssid[32], stored_password[32];
  EEPROM.get(0, stored_ssid);
  EEPROM.get(32, stored_password);
  EEPROM.end();
  
  ssid = String(stored_ssid);
  password = String(stored_password);

  myServo.attach(SERVO_PIN);

  if (ssid.length() == 0) {
    Serial.println("⚠️ No WiFi credentials. Starting AP mode...");
    WiFi.softAP(AP_ssid, AP_password);
    Serial.println("AP started. Connect to SSID: " + String(AP_ssid) + " and visit http://192.168.4.1");
  } else {
    Serial.println("📡 Connecting to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
      delay(1000);  
      Serial.print(".");
      attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Connected to WiFi! IP: " + WiFi.localIP().toString());
      server.on("/", handleWebPage);
    } else {
      Serial.println("\n❌ Failed to connect! Starting AP mode...");
      WiFi.softAP(AP_ssid, AP_password);
      Serial.println("AP started. Connect to SSID: " + String(AP_ssid));
    }
  }

  server.on("/", handleRoot);
  server.on("/connect", handleConnect);
  server.begin();
  Serial.println("🌐 Web server started.");
}

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, SERVER_URL);

    int httpCode = http.GET();
    if (httpCode > 0) { // ✅ Ensure request was successful
      Serial.print("HTTP Response Code: ");
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.print("Received angle: ");
        Serial.println(response);

        // Remove double quotes from the response
        response.replace("\"", ""); // Remove all double quotes

        int angle = response.toInt(); // Convert to integer
        Serial.print("Parsed angle: ");
        Serial.println(angle);

        if (angle >= 0 && angle <= 180) { // ✅ Validate angle range
          if (angle != lastAngle) {
            Serial.print("Moving servo to: ");
            Serial.println(angle);
            myServo.write(angle);  // ✅ Move servo
            lastAngle = angle;
          }
        } else {
          Serial.println("⚠️ Invalid angle received");
        }
      }
    } else {
      Serial.print("❌ HTTP Request Failed: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  delay(2000);
}