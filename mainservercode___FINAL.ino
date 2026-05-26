#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define IR_PIN 13  // IR sensor (LOW = vehicle present)

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* mdnsName = "evcharger";
LiquidCrystal_PCF8574 lcd(0x27);

WiFiServer server(80);
String scrollText = "";
int scrollPos = 0;
unsigned long lastScroll = 0;
bool chargingActive = false;
bool wifiConnected = true;
String currentVehicle = "None";

struct Vehicle {
  String name;
  String voltage;
  String current;
  String chargingTime;
};

Vehicle vehicles[] = {
  {"Tata MANZA EV", "400V AC", "50A", "56min 0-80%"},
  {"Mahindra BE 6E ", "350V AC", "45A", "50min 0-80%"},
  {"Lamborghini EV", "800V AC", "300A", "12min 0-80%"},
  {"Ferrari SF90 EV", "900V AC", "500A", "8min 0-80%"},
  {"G WAGON EV", "800V AC", "270A", "22.5min 0-80%"}
};

void printStatus(String message) {
  Serial.println("[STATUS] " + message);
}

void updateDisplay(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0,16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0,16));
  scrollText = line2;
  scrollPos = 0;
}

int getVehicleIndex() {
  for(int i=0; i<sizeof(vehicles)/sizeof(vehicles[0]); i++) {
    if(vehicles[i].name == currentVehicle) return i;
  }
  return -1;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nEV CHARGING STATION BOOTING...");
  
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.print("Booting System...");
  delay(1000);

  pinMode(IR_PIN, INPUT_PULLUP);

  lcd.clear();
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi ");
  WiFi.begin(ssid, password);
  
  unsigned long start = millis();
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    if(millis() - start > dotCount * 500) {
      lcd.print(".");
      if(++dotCount > 3) {
        lcd.setCursor(5, 1);
        lcd.print("    ");
        lcd.setCursor(5, 1);
        dotCount = 0;
      }
    }
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    String ipAddress = WiFi.localIP().toString();
    lcd.clear();
    lcd.print("Connected!");
    lcd.setCursor(0, 1);
    lcd.print("IP: " + ipAddress);
    delay(2000);

    MDNS.begin(mdnsName);
    server.begin();
    updateDisplay("Idle Mode", "IP:" + ipAddress);
  } else {
    wifiConnected = false;
    updateDisplay("Connection Failed", "Check WiFi Config");
  }
}

String generateHTML() {
  bool vehiclePresent = (digitalRead(IR_PIN) == LOW);

  String html = R"rawliteral(
    <!DOCTYPE html><html>
    <head>
      <title>EV Charging Station</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        :root {
          --primary: #1e3a8a;
          --success: #15803d;
          --danger: #b91c1c;
          --background: #f8fafc;
        }

        body {
          font-family: 'Segoe UI', system-ui;
          margin: 0;
          padding: 20px;
          background: #f1f5f9;
          min-height: 100vh;
          display: flex;
          flex-direction: column;
        }

        .container {
          max-width: 1000px;
          margin: 0 auto;
          flex: 1;
        }

        .header {
          background: linear-gradient(135deg, #1e3a8a, #2563eb);
          color: white;
          padding: 2rem;
          border-radius: 1rem;
          margin-bottom: 2rem;
          text-align: center;
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .status-card {
          background: white;
          padding: 1.5rem;
          border-radius: 1rem;
          margin-bottom: 1.5rem;
          box-shadow: 0 2px 4px rgba(0,0,0,0.05);
        }

        .vehicle-grid {
          display: grid;
          gap: 1.5rem;
          grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
        }

        .vehicle-card {
          background: white;
          padding: 1.5rem;
          border-radius: 1rem;
          transition: transform 0.2s;
          box-shadow: 0 2px 4px rgba(0,0,0,0.05);
        }

        .vehicle-card:hover {
          transform: translateY(-3px);
        }

        .badge {
          display: inline-block;
          padding: 0.25rem 0.75rem;
          border-radius: 1rem;
          font-size: 0.875rem;
          font-weight: 500;
        }

        .voltage { background: #e0f2fe; color: #075985; }
        .current { background: #fef3c7; color: #854d0e; }
        .time { background: #dcfce7; color: #166534; }

        button {
          background: var(--primary);
          color: white;
          border: none;
          padding: 0.75rem;
          border-radius: 0.75rem;
          font-weight: 600;
          cursor: pointer;
          transition: opacity 0.2s;
          width: 100%;
        }

        button:hover {
          opacity: 0.9;
        }

        .emergency {
          background: var(--danger);
          margin-top: 1.5rem;
        }

        .warning-banner {
          background: #fffbeb;
          color: #854d0e;
          padding: 1rem;
          border-radius: 0.75rem;
          margin: 1.5rem 0;
          display: flex;
          align-items: center;
          gap: 1rem;
          border: 2px solid #f59e0b;
        }

        .status-indicator {
          display: flex;
          gap: 1rem;
          justify-content: center;
          margin-top: 1rem;
        }

        .status-item {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          background: rgba(255,255,255,0.1);
          padding: 0.5rem 1rem;
          border-radius: 0.5rem;
        }

        footer {
          margin-top: auto;
          padding: 2rem 1rem;
          text-align: center;
          color: #64748b;
          border-top: 1px solid #e2e8f0;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <div class="header">
          <h1 style="margin:0;font-weight:600;">SMART EV CHARGER</h1>
          <div class="status-indicator">
  )rawliteral";

  // WiFi Status
  html += "<div class=\"status-item\">";
  html += "<div style=\"width:12px;height:12px;border-radius:50%;background:";
  html += wifiConnected ? "#4ade80" : "#ef4444";
  html += ";\"></div>";
  html += "<span>WiFi: ";
  html += wifiConnected ? "Connected" : "Disconnected";
  html += "</span></div>";

  // Charging Status
  html += "<div class=\"status-item\">";
  html += "<div style=\"width:12px;height:12px;border-radius:50%;background:";
  html += chargingActive ? "#4ade80" : "#94a3b8";
  html += ";\"></div>";
  html += "<span>Status: ";
  html += chargingActive ? "CHARGING" : "IDLE";
  html += "</span></div></div></div>";

  // Warning Banner
  if (!vehiclePresent) {
    html += R"rawliteral(
        <div class="warning-banner">
          <svg style="width:24px;height:24px" viewBox="0 0 24 24">
            <path fill="#d97706" d="M13 14h-2v-5h2m0 10h-2v-2h2M1 21h22L12 2 1 21z"/>
          </svg>
          <span>No vehicle detected in charging bay</span>
        </div>
    )rawliteral";
  }

  // Charging Status Card
  html += R"rawliteral(
        <div class="status-card">
          <h2 style="margin-top:0;">Charging Status</h2>
  )rawliteral";

  if (chargingActive) {
    html += "<div style=\"display:grid;gap:1rem;\">";
    html += "<div><strong>Vehicle:</strong> " + currentVehicle + "</div>";
    html += "<div style=\"display:flex;gap:2rem;flex-wrap:wrap;\">";
    html += "<div><strong>Voltage:</strong><br>" + vehicles[getVehicleIndex()].voltage + "</div>";
    html += "<div><strong>Current:</strong><br>" + vehicles[getVehicleIndex()].current + "</div>";
    html += "<div><strong>Est. Time:</strong><br>" + vehicles[getVehicleIndex()].chargingTime + "</div>";
    html += "</div></div>";
  } else {
    html += "<div style=\"color:#64748b;text-align:center;padding:1rem;\">";
    html += vehiclePresent ? "Select vehicle to begin charging" : "Please position vehicle in charging bay";
    html += "</div>";
  }

  // Emergency Button
  html += R"rawliteral(
          <button class="emergency" onclick="window.location.href='/emergency'">
            EMERGENCY STOP
          </button>
        </div>

        <h2 style="margin-bottom:1rem;">Available Vehicles</h2>
        <div class="vehicle-grid">
  )rawliteral";

  // Vehicle Cards
  for (int i = 0; i < sizeof(vehicles)/sizeof(vehicles[0]); i++) {
    html += R"rawliteral(
          <div class="vehicle-card">
            <h3 style="margin:0 0 0.5rem;">)rawliteral";
    html += vehicles[i].name;
    html += R"rawliteral(</h3>
            <div style="display:flex;gap:0.5rem;margin-bottom:1rem;flex-wrap:wrap;">
              <div class="badge voltage">)rawliteral";
    html += vehicles[i].voltage;
    html += R"rawliteral(</div>
              <div class="badge current">)rawliteral";
    html += vehicles[i].current;
    html += R"rawliteral(</div>
              <div class="badge time">)rawliteral";
    html += vehicles[i].chargingTime;
    html += R"rawliteral(</div>
            </div>
            <button onclick="window.location.href='/start?vehicle=)rawliteral";
    html += String(i);
    html += R"rawliteral('">
              START CHARGING
            </button>
          </div>
    )rawliteral";
  }

  // Footer and closing tags
  html += R"rawliteral(
        </div>
      </div>
      
      <footer>
        <p>ASE ELC23005 ELC23017 ELC23055</p>
        <p>@2025 AMRITA VISHWA VIDYAPEETHAM AMRITAPURI</p>
      </footer>
    </body>
    </html>
  )rawliteral";

  return html;
}

void handleClient(WiFiClient &client) {
  String request = client.readStringUntil('\r');
  client.clear();

  if (request.indexOf("GET /start?vehicle=") != -1) {
    if(digitalRead(IR_PIN) == LOW) {
      int index = request.substring(request.indexOf('=')+1).toInt();
      if(index >=0 && index < sizeof(vehicles)/sizeof(vehicles[0])){
        chargingActive = true;
        currentVehicle = vehicles[index].name;
        updateDisplay(currentVehicle, vehicles[index].voltage + " | " + vehicles[index].current);
      }
    } else {
      printStatus("Charging denied: No vehicle detected");
    }
  }
  else if (request.indexOf("GET /emergency") != -1) {
    chargingActive = false;
    currentVehicle = "None";
    updateDisplay("Emergency Stop", "System Reset");
    delay(2000);
    updateDisplay("Idle Mode", "IP: " + WiFi.localIP().toString());
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.println(generateHTML());
}

void handleScroll() {
  if(millis() - lastScroll > 300 && chargingActive && !scrollText.isEmpty()) {
    String displayText = scrollText + "    ";
    lcd.setCursor(0, 1);
    lcd.print(displayText.substring(scrollPos, scrollPos + 16));
    scrollPos = (scrollPos + 1) % (scrollText.length() + 4);
    lastScroll = millis();
  }
}

void loop() {
  static bool lastVehicleState = false;
  bool vehiclePresent = (digitalRead(IR_PIN) == LOW);

  if(vehiclePresent != lastVehicleState) {
    if(!vehiclePresent && chargingActive) {
      chargingActive = false;
      currentVehicle = "None";
      updateDisplay("Vehicle Removed", "Charging Stopped");
      delay(2000);
    }
    updateDisplay(vehiclePresent ? "Idle Mode":"No Vehicle", 
                 vehiclePresent ? "IP:" + WiFi.localIP().toString() : "Please Park");
    lastVehicleState = vehiclePresent;
  }

  if(wifiConnected) {
    WiFiClient client = server.accept();
    if (client) handleClient(client);
  }
  
  handleScroll();
  delay(10);
}
