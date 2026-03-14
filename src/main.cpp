#include <Arduino.h>
#include <WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <ESP32Ping.h>
#include "config.h"  // << Fill in your credentials here (never committed)

// PC Monitoring Configuration
IPAddress pc_ip;
bool lastKnownPCState = false; // false = OFF, true = ON
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 5000; // Ping every 5 seconds

// ==========================================
// INCHING ROUTINE 
// ==========================================
void triggerRelay() {
  Serial.println("[Relay] Triggering PC Power Button (Inching)...");
  // Ultimate Jugaad: Input-Output Toggle (True Open-Drain for 5V power)
  // Instead of driving HIGH (3.3V), we become an INPUT (Invisible/disconnected)
  // to let the 5V relay turn itself OFF.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   // Ground the pin → relay ON loud (0V)
  delay(700);                     // Hold for 700ms 
  pinMode(RELAY_PIN, INPUT);      // Become invisible → relay OFF (floats to 5V)
  Serial.println("[Relay] Trigger complete.");
}

void triggerRelayForce() {
  Serial.println("\r\n======================================");
  Serial.println("[Relay] EMERGENCY: Triggering FORCE RESTART (5-second hold)...");
  Serial.println("======================================\r\n");
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   // Ground the pin → relay ON loud (0V)
  delay(8000);                    // Hold for 8000ms (Hardware Kill)
  pinMode(RELAY_PIN, INPUT);      // Become invisible → relay OFF 
  
  Serial.println("[Relay] Force Restart trigger complete. PC should be dead.");
}

// ==========================================
// SINRIC PRO CALLBACKS
// ==========================================

// This function is invoked when a power state change is requested via Sinric Pro
bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("[Sinric] Device %s requested state: %s\r\n", deviceId.c_str(), state ? "on" : "off");

  if (state == true) {
    // User requested "Turn ON"
    if (lastKnownPCState == true) {
      Serial.println("[Safety] PC is already ON (Ping succeeded). Ignoring 'Turn ON' command.");
      // Force Sinric back to ON without physical trigger
      state = true;
      return true;
    } else {
      Serial.println("[Safety] PC is currently OFF. Executing power trigger...");
      triggerRelay();
      // Because we triggered it, assume it will be ON soon. 
      // The ping loop will confirm it later.
      state = true;
      return true;
    }
  } else {
    // User requested "Turn OFF"
    if (lastKnownPCState == false) {
      Serial.println("[Safety] PC is already OFF (Ping failed). Ignoring 'Turn OFF' command.");
      // Force Sinric back to OFF
      state = false;
      return true;
    } else {
      Serial.println("[Safety] PC is currently ON. Executing power trigger (Graceful Shutdown)...");
      triggerRelay();
      // The ping loop will automatically switch it to OFF when the PC actually dies in ~10 seconds.
      // For now, keep it ON in the UI until the ping confirms it.
      state = true; 
      return true;
    }
  }
}

// This function is invoked when the Force Restart switch is triggered
bool onForceRestartState(const String &deviceId, bool &state) {
  Serial.printf("[Sinric] Force Restart Switch triggered!\r\n");

  if (state == true) {
    if (lastKnownPCState == false) {
      Serial.println("[Safety] PC is already OFF. Ignoring Force Restart command.");
      state = false;
      return true;
    } else {
      Serial.println("[Safety] PC is ON. Executing 5-Second Kill Switch...");
      triggerRelayForce();
      
      // We know for a fact the PC is dead now. Update the state visually.
      state = false; 
      return true;
    }
  } else {
    // If the user somehow sends an "OFF" command to the Force Restart button, ignore it.
    state = false;
    return true;
  }
}

// ==========================================
// SYSTEM SETUP
// ==========================================

void setupWiFi() {
  Serial.printf("\r\n[WiFi] Connecting to %s", WIFI_SSID);
  
  // Set WiFi to automatically reconnect if connection is lost
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  
  Serial.printf("\r\n[WiFi] Connected!\r\n[WiFi] IP Address: %s\r\n", WiFi.localIP().toString().c_str());

  // Sync internal clock via NTP to ensure SSL connections succeed
  Serial.print("[WiFi] Syncing time with NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\r\n[WiFi] Time synced successfully.");
}

void setupSinricPro() {
  // Add the normal switch device to Sinric Pro
  SinricProSwitch &mySwitch = SinricPro[SWITCH_ID];
  mySwitch.onPowerState(onPowerState);

  // Add the Force Restart switch device
  SinricProSwitch &forceSwitch = SinricPro[SWITCH_ID_FORCE_RESTART];
  forceSwitch.onPowerState(onForceRestartState);

  // Setup Sinric Pro callbacks
  SinricPro.onConnected([](){ 
    Serial.println("[SinricPro] Connected to cloud"); 
  });
  
  SinricPro.onDisconnected([](){ 
    Serial.println("[SinricPro] Disconnected from cloud"); 
  });

  // Disable restoring last state on reconnect - prevents unwanted relay
  // triggers every time the ESP32 reconnects to Sinric Pro cloud.
  SinricPro.restoreDeviceStates(false);
  
  // Start Sinric Pro with credentials
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  // Initialize Serial debugging
  Serial.begin(115200);
  delay(100);
  Serial.println("\r\n\r\n[System] Starting DIY Smart PC Power Switch...");

  // Boot Safety Jugaad: Start as INPUT immediately so we are 'invisible'
  // to the 5V relay, letting it stay OFF by default.
  pinMode(RELAY_PIN, INPUT);
  Serial.println("[Hardware] Relay pin set to INPUT (Invisible / Safe OFF).");

  // Setup Wi-Fi
  setupWiFi();

  // Setup Sinric Pro Cloud connection
  setupSinricPro();
}

void loop() {
  // Handle Sinric Pro tasks
  SinricPro.handle();

  // Non-blocking Ping Loop
  unsigned long currentMillis = millis();
  if (currentMillis - lastPingTime >= PING_INTERVAL) {
    lastPingTime = currentMillis;
    
    // Check if WiFi is actually connected before pinging
    if (WiFi.status() == WL_CONNECTED) {
      bool pingSuccess = Ping.ping(pc_ip, 1); // Send 1 ping attempt

      if (pingSuccess != lastKnownPCState) {
        // The PC state has changed! Let's update Sinric and the local variable
        lastKnownPCState = pingSuccess;
        
        SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
        mySwitch.sendPowerStateEvent(lastKnownPCState); // Push the live state to your phone
        
        if (lastKnownPCState) {
          Serial.println("[Telemetry] Ping Success! PC is now running.");
          // Send Push Notification to phone
          mySwitch.sendPushNotification("Your PC has successfully booted and is now online!");
        } else {
          Serial.println("[Telemetry] Ping Failed! PC is off or unreachable.");
        }
      }
    }
  }

  // Additional Wi-Fi Resilience Logic
  // Check if Wi-Fi is still connected, and manually attempt to reconnect if not.
  // WiFi.setAutoReconnect(true) handles connection drops gracefully on the ESP32,
  // but this block ensures we aggressively try to recover if the router drops the connection.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost! Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
    
    unsigned long startReconnect = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startReconnect < 5000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\r\n[WiFi] Reconnected successfully.\r\n");
    } else {
      Serial.println("\r\n[WiFi] Reconnect failed. Will try again on next loop.\r\n");
    }
  }
}
