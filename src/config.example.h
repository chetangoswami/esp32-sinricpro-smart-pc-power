#pragma once

// ==========================================
// ⚙️  USER CONFIGURATION — config.h
// ==========================================
// Steps:
//  1. Copy this file and rename it to: config.h
//  2. Fill in YOUR values below
//  3. config.h is gitignored — your secrets will never be pushed
// ==========================================

// ---------------------
// Wi-Fi Credentials
// ---------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"         // e.g. "HomeNetwork"
#define WIFI_PASS     "YOUR_WIFI_PASSWORD"      // e.g. "password123"

// ---------------------
// Sinric Pro Credentials
// Get these from https://portal.sinric.pro -> Credentials
// ---------------------
#define APP_KEY       "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define APP_SECRET    "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

// ---------------------
// Sinric Pro Device IDs
// Get these from https://portal.sinric.pro -> Devices
// ---------------------
#define SWITCH_ID               "xxxxxxxxxxxxxxxxxxxxxxxx"   // Your "PC Power" switch Device ID
#define SWITCH_ID_FORCE_RESTART "xxxxxxxxxxxxxxxxxxxxxxxx"   // Your "Force Restart" switch Device ID

// ---------------------
// PC Local IP Address
// Find yours by running 'ipconfig' in Windows and looking for IPv4 Address
// ---------------------
#define PC_IP_ADDRESS "192.168.1.XXX"

// ---------------------
// Hardware Pin
// Relay IN wire connects to this ESP32 GPIO pin
// AVOID: D5 (GPIO 5) — it is a strapping pin and causes boot issues
// SAFE pins: D21, D22, D13, D27
// ---------------------
#define RELAY_PIN 21
