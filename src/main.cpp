#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <ESP32Ping.h>
#include "config.h"  // << Fill in your credentials here (never committed)

// PC Monitoring Configuration
IPAddress pc_ip;                // Resolved at runtime from PC_HOSTNAME
bool pcIpResolved = false;      // True once hostname has been resolved successfully
bool lastKnownPCState = false;  // false = OFF, true = ON
bool wasWifiConnected = false;  // Track WiFi state for reconnect detection
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 5000; // Ping every 5 seconds

// PC State & Networking Tuning
int pingFailureCount = 0;
const int MAX_PING_FAILURES = 3;
unsigned long lastMdnsAttempt = 0;
unsigned long mdnsBackoffInterval = 5000;
const unsigned long MAX_MDNS_BACKOFF = 60000; // 1 minute maximum backoff

// Non-blocking Relay State Machine
unsigned long relayTriggerStartTime = 0;
unsigned long relayTriggerDuration = 0;
bool isRelayTriggerActive = false;

// ==========================================
// INCHING ROUTINE (Non-Blocking)
// ==========================================
void triggerRelay() {
  Serial.println("[Relay] Triggering PC Power Button (700ms pulse)...");
  // Hardware Workaround: Utilizing high-impedance INPUT state to simulate
  // an Open-Drain output, safely interfacing 3.3V logic with a 5V relay.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   // Ground the pin → relay ON (0V)
  
  // Start non-blocking timer
  relayTriggerStartTime = millis();
  relayTriggerDuration = 700;
  isRelayTriggerActive = true;
}

void triggerRelayForce() {
  Serial.println("\r\n======================================");
  Serial.println("[Relay] EMERGENCY: Triggering Hard Reset (8-second hold)...");
  Serial.println("======================================\r\n");
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   // Ground the pin → relay ON (0V)
  
  // Start non-blocking timer
  relayTriggerStartTime = millis();
  relayTriggerDuration = 8000;
  isRelayTriggerActive = true;
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
  
  Serial.printf("\r\n[WiFi] Reconnected!\r\n[WiFi] IP Address: %s\r\n", WiFi.localIP().toString().c_str());

  // Start mDNS client so we can resolve hostnames like DESKTOP-XXXX.local
  MDNS.begin("esp32");

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
  wasWifiConnected = true; // Mark as connected so loop() doesn't fire a false reconnect event

  // Resolve PC hostname to IP once at boot using mDNS
  Serial.printf("[mDNS] Resolving PC hostname: %s.local ...\r\n", PC_HOSTNAME);
  IPAddress resolved = MDNS.queryHost(PC_HOSTNAME, 2000);
  if (resolved != INADDR_NONE) {
    pc_ip = resolved;
    pcIpResolved = true;
    Serial.printf("[mDNS] Resolved to: %s\r\n", pc_ip.toString().c_str());
  } else {
    Serial.println("[mDNS] Resolution failed at boot. Will retry on reconnect.");
  }

  // Setup Sinric Pro Cloud connection
  setupSinricPro();
}

void loop() {
  // --- Non-blocking Relay State Machine ---
  if (isRelayTriggerActive) {
    if (millis() - relayTriggerStartTime >= relayTriggerDuration) {
      pinMode(RELAY_PIN, INPUT);      // Become invisible → relay OFF (floats to 5V)
      isRelayTriggerActive = false;
      Serial.println("[Relay] Trigger sequence complete. Pin returned to INPUT (Safe / Floating).");
    }
  }

  // Handle Sinric Pro tasks
  SinricPro.handle();

  // Non-blocking Ping Loop
  unsigned long currentMillis = millis();
  if (currentMillis - lastPingTime >= PING_INTERVAL) {
    lastPingTime = currentMillis;
    
    // Check if WiFi is actually connected
    if (WiFi.status() == WL_CONNECTED) {
      // If we don't have an IP yet, try to resolve it now with exponential backoff
      if (!pcIpResolved) {
        if (currentMillis - lastMdnsAttempt >= mdnsBackoffInterval) {
          lastMdnsAttempt = currentMillis;
          Serial.printf("[mDNS] Attempting to resolve PC hostname: %s.local ...\r\n", PC_HOSTNAME);
          IPAddress resolved = MDNS.queryHost(PC_HOSTNAME, 2000);
          
          if (resolved != INADDR_NONE) {
            pc_ip = resolved;
            pcIpResolved = true;
            mdnsBackoffInterval = 5000; // Reset backoff on success
            Serial.printf("[mDNS] Resolved to: %s\r\n", pc_ip.toString().c_str());
          } else {
            Serial.printf("[mDNS] Hostname resolution failed. Backing off for %lu ms...\r\n", mdnsBackoffInterval);
            mdnsBackoffInterval = min(mdnsBackoffInterval * 2, MAX_MDNS_BACKOFF); // Exponential backoff maxing at 60s
          }
        }
      }

      // If we have an IP, ping it
      if (pcIpResolved) {
        bool pingSuccess = Ping.ping(pc_ip, 1); // Send 1 ping attempt

        if (pingSuccess) {
          pingFailureCount = 0; // Reset failure counter
          if (!lastKnownPCState) {
            // PC State change from OFF to ON
            lastKnownPCState = true;
            SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
            mySwitch.sendPowerStateEvent(true); // Push the live state to your phone
            
            Serial.println("[Telemetry] Ping Success! PC is now running.");
            mySwitch.sendPushNotification("Your PC has successfully booted and is now online!");
          }
        } else {
          // Ping failed
          pingFailureCount++;
          if (pingFailureCount >= MAX_PING_FAILURES) {
            if (lastKnownPCState) {
              // PC State change from ON to OFF
              lastKnownPCState = false;
              SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
              mySwitch.sendPowerStateEvent(false); // Push the live state to your phone
              Serial.println("[Telemetry] Ping failed 3 consecutive times. PC is now officially OFF.");
            }
            pingFailureCount = MAX_PING_FAILURES; // Cap counter
          } else {
            Serial.printf("[Telemetry] Ping dropped (Attempt %d/%d). Filtering jitter...\r\n", pingFailureCount, MAX_PING_FAILURES);
          }
        }
      }
    }
  }

  // Non-blocking WiFi resilience logic.
  // We track the previous WiFi state so we can detect the exact moment it
  // reconnects. On reconnect, we force an immediate ping so the safety guards 
  // are accurate before any cloud commands arrive.
  bool isNowConnected = (WiFi.status() == WL_CONNECTED);

  if (!isNowConnected) {
    // Not connected. If this is a *new* disconnect, log it.
    if (wasWifiConnected) {
      Serial.println("[WiFi] Connection lost! Auto-reconnect is active, waiting...");
      wasWifiConnected = false;
    }
    // Do NOT use delay() here — that would block SinricPro.handle()
  } else if (isNowConnected && !wasWifiConnected) {
    // --- RECONNECT EVENT DETECTED ---
    Serial.println("[WiFi] Reconnected! Restoring connections...");
    wasWifiConnected = true;
    MDNS.begin("esp32"); // Restart mDNS client after reconnect

    Serial.printf("[mDNS] Re-resolving PC hostname: %s.local ...\r\n", PC_HOSTNAME);
    IPAddress resolved = MDNS.queryHost(PC_HOSTNAME, 2000);
    if (resolved != INADDR_NONE) {
      pc_ip = resolved;
      pcIpResolved = true;
      Serial.printf("[mDNS] PC resolved to: %s\r\n", pc_ip.toString().c_str());
    } else {
      Serial.println("[mDNS] Hostname resolution failed after reconnect.");
      if (pcIpResolved) {
        Serial.printf("[mDNS] Falling back to last known IP: %s\r\n", pc_ip.toString().c_str());
      }
    }

    if (pcIpResolved) {
      bool pingSuccess = Ping.ping(pc_ip, 1);
      if (pingSuccess != lastKnownPCState) {
        lastKnownPCState = pingSuccess;
        SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
        mySwitch.sendPowerStateEvent(lastKnownPCState);
      }
      Serial.printf("[WiFi] Post-reconnect PC state: %s\r\n", lastKnownPCState ? "ON" : "OFF");
    }
    lastPingTime = millis();
  }
}
