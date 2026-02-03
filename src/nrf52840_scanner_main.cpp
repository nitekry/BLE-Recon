/*
 * Professional BLE Scanner with Configurable Filtering
 * nRF52840 with Nordic SoftDevice
 * 
 * Features:
 * - Clean output without emojis
 * - Detailed beacon and payload information
 * - Whitelist/Blacklist filtering via text files
 * - Interactive command mode
 * - Color-coded AD structures
 */

#include <Arduino.h>
#include <bluefruit.h>
#include "ble_filter_config_builtin.h"  // Built-in filters, no filesystem needed

// Color configuration - Set to false if your terminal doesn't support ANSI colors
#define ENABLE_COLORS true

#if ENABLE_COLORS
  // ANSI Color codes
  #define COLOR_RESET   "\033[0m"
  #define COLOR_RED     "\033[31m"
  #define COLOR_GREEN   "\033[32m"
  #define COLOR_YELLOW  "\033[33m"
  #define COLOR_BLUE    "\033[34m"
  #define COLOR_MAGENTA "\033[35m"
  #define COLOR_CYAN    "\033[36m"
  #define COLOR_WHITE   "\033[37m"
  #define COLOR_BRIGHT_RED     "\033[91m"
  #define COLOR_BRIGHT_GREEN   "\033[92m"
  #define COLOR_BRIGHT_YELLOW  "\033[93m"
  #define COLOR_BRIGHT_BLUE    "\033[94m"
  #define COLOR_BRIGHT_MAGENTA "\033[95m"
  #define COLOR_BRIGHT_CYAN    "\033[96m"
#else
  // No colors - empty strings
  #define COLOR_RESET   ""
  #define COLOR_RED     ""
  #define COLOR_GREEN   ""
  #define COLOR_YELLOW  ""
  #define COLOR_BLUE    ""
  #define COLOR_MAGENTA ""
  #define COLOR_CYAN    ""
  #define COLOR_WHITE   ""
  #define COLOR_BRIGHT_RED     ""
  #define COLOR_BRIGHT_GREEN   ""
  #define COLOR_BRIGHT_YELLOW  ""
  #define COLOR_BRIGHT_BLUE    ""
  #define COLOR_BRIGHT_MAGENTA ""
  #define COLOR_BRIGHT_CYAN    ""
#endif

// BLE AD Type codes (from Bluetooth specification)
#define AD_TYPE_FLAGS                    0x01
#define AD_TYPE_16BIT_SERVICE_UUIDS      0x03
#define AD_TYPE_COMPLETE_LOCAL_NAME      0x09
#define AD_TYPE_TX_POWER                 0x0A
#define AD_TYPE_SERVICE_DATA_16BIT       0x16
#define AD_TYPE_MANUFACTURER_DATA        0xFF
#define AD_TYPE_128BIT_SERVICE_UUIDS     0x07

// Scan configuration
static uint32_t g_scanTimeSeconds = 10;  // Default 10 seconds
static bool g_autoScan = false;          // Manual mode by default
static bool g_colorsEnabled = ENABLE_COLORS;  // Runtime color toggle
static bool g_deduplication = true;      // Deduplication enabled by default

// Statistics
static uint32_t g_scanCount = 0;
static uint32_t g_deviceCount = 0;
static uint32_t g_filteredCount = 0;
static uint32_t g_duplicateCount = 0;

// Device tracking for deduplication
struct SeenDevice {
  String mac;
  String name;
  String payload;
  int rssi;
  uint32_t lastSeen;
};

static std::vector<SeenDevice> g_seenDevices;

// Filter instance
static BLEFilter g_filter;

// Forward declarations
void processCommand();
void addToBlacklist();
void addToWhitelist();
void interactiveFilter();

// Helper: Convert bytes to hex string
static String toHex(const uint8_t* data, size_t len) {
  String s;
  s.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) s += '0';
    s += String(data[i], HEX);
  }
  s.toUpperCase();
  return s;
}

// Helper: Get AD type name and color
static const char* getADTypeColor(uint8_t type) {
  switch (type) {
    case AD_TYPE_FLAGS:                return COLOR_CYAN;
    case AD_TYPE_COMPLETE_LOCAL_NAME:  return COLOR_BRIGHT_GREEN;
    case AD_TYPE_16BIT_SERVICE_UUIDS:  return COLOR_BRIGHT_BLUE;
    case AD_TYPE_128BIT_SERVICE_UUIDS: return COLOR_BLUE;
    case AD_TYPE_SERVICE_DATA_16BIT:   return COLOR_BRIGHT_MAGENTA;
    case AD_TYPE_MANUFACTURER_DATA:    return COLOR_BRIGHT_YELLOW;
    case AD_TYPE_TX_POWER:             return COLOR_YELLOW;
    default:                           return COLOR_WHITE;
  }
}

static const char* getADTypeName(uint8_t type) {
  switch (type) {
    case AD_TYPE_FLAGS:                return "Flags";
    case AD_TYPE_COMPLETE_LOCAL_NAME:  return "Complete Local Name";
    case AD_TYPE_16BIT_SERVICE_UUIDS:  return "16-bit Service UUIDs";
    case AD_TYPE_128BIT_SERVICE_UUIDS: return "128-bit Service UUIDs";
    case AD_TYPE_SERVICE_DATA_16BIT:   return "Service Data (16-bit UUID)";
    case AD_TYPE_MANUFACTURER_DATA:    return "Manufacturer Data";
    case AD_TYPE_TX_POWER:             return "TX Power Level";
    case 0x02:                         return "Incomplete 16-bit UUIDs";
    case 0x04:                         return "Incomplete 32-bit UUIDs";
    case 0x05:                         return "Complete 32-bit UUIDs";
    case 0x06:                         return "Incomplete 128-bit UUIDs";
    case 0x08:                         return "Shortened Local Name";
    case 0x0D:                         return "Class of Device";
    case 0x14:                         return "List of 16-bit Solicitation UUIDs";
    case 0x19:                         return "Appearance";
    case 0x1A:                         return "Advertising Interval";
    default:                           return "Unknown Type";
  }
}

// Helper: Print hex dump with ASCII
static void printHexDump(const uint8_t* data, size_t len, const char* label) {
  Serial.println(label);
  Serial.println("  Offset  Hex                                              ASCII");
  Serial.println("  ------  -----------------------------------------------  ----------------");
  
  for (size_t i = 0; i < len; i += 16) {
    Serial.printf("  0x%04X  ", i);
    
    // Hex values
    for (size_t j = 0; j < 16; j++) {
      if (i + j < len) {
        Serial.printf("%02X ", data[i + j]);
      } else {
        Serial.print("   ");
      }
      if (j == 7) Serial.print(" ");
    }
    
    Serial.print(" ");
    
    // ASCII
    for (size_t j = 0; j < 16 && i + j < len; j++) {
      uint8_t c = data[i + j];
      Serial.write((c >= 32 && c <= 126) ? c : '.');
    }
    
    Serial.println();
  }
  Serial.println();
}

// Helper: Parse and print AD structures with color coding
static void printADStructures(const uint8_t* payload, size_t len) {
  Serial.println("\n[AD-STRUCTURES] Advertisement Data Structures:");
  
#if ENABLE_COLORS
  Serial.println("  Legend:");
  Serial.printf("    %sFlags%s | ", COLOR_CYAN, COLOR_RESET);
  Serial.printf("%sName%s | ", COLOR_BRIGHT_GREEN, COLOR_RESET);
  Serial.printf("%sUUIDs%s | ", COLOR_BRIGHT_BLUE, COLOR_RESET);
  Serial.printf("%sService Data%s | ", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  Serial.printf("%sMfg Data%s | ", COLOR_BRIGHT_YELLOW, COLOR_RESET);
  Serial.printf("%sOther%s\n", COLOR_WHITE, COLOR_RESET);
#else
  Serial.println("  Types: Flags | Name | UUIDs | Service Data | Mfg Data | Other");
#endif
  
  Serial.println("  ----------------");
  
  size_t offset = 0;
  int structNum = 1;
  
  while (offset < len) {
    if (len - offset < 2) break;
    
    uint8_t adLen = payload[offset];
    if (adLen == 0) break;
    
    if (offset + adLen + 1 > len) break;
    
    uint8_t adType = payload[offset + 1];
    const uint8_t* adData = &payload[offset + 2];
    size_t adDataLen = adLen - 1;
    
    const char* color = getADTypeColor(adType);
    const char* typeName = getADTypeName(adType);
    
    Serial.printf("  %s[%d] Type 0x%02X: %s (Length: %d bytes)%s\n",
                  color, structNum++, adType, typeName, adDataLen, COLOR_RESET);
    
    Serial.print("      Data: ");
    
    switch (adType) {
      case AD_TYPE_FLAGS:
        if (adDataLen >= 1) {
          uint8_t flags = adData[0];
          Serial.printf("%s0x%02X%s (", color, flags, COLOR_RESET);
          bool first = true;
          if (flags & 0x01) { Serial.print("LE Limited"); first = false; }
          if (flags & 0x02) { if (!first) Serial.print(", "); Serial.print("LE General"); first = false; }
          if (flags & 0x04) { if (!first) Serial.print(", "); Serial.print("BR/EDR Not Supported"); first = false; }
          if (flags & 0x08) { if (!first) Serial.print(", "); Serial.print("LE+BR/EDR Controller"); first = false; }
          if (flags & 0x10) { if (!first) Serial.print(", "); Serial.print("LE+BR/EDR Host"); }
          Serial.println(")");
        }
        break;
        
      case AD_TYPE_COMPLETE_LOCAL_NAME:
      case 0x08:
        Serial.printf("%s\"", color);
        for (size_t i = 0; i < adDataLen; i++) {
          char c = adData[i];
          Serial.write((c >= 32 && c <= 126) ? c : '?');
        }
        Serial.printf("\"%s\n", COLOR_RESET);
        break;
        
      case AD_TYPE_16BIT_SERVICE_UUIDS:
      case 0x02:
        Serial.printf("%s", color);
        for (size_t i = 0; i < adDataLen; i += 2) {
          if (i + 1 < adDataLen) {
            uint16_t uuid = adData[i] | (adData[i+1] << 8);
            Serial.printf("0x%04X", uuid);
            if (i + 2 < adDataLen) Serial.print(", ");
          }
        }
        Serial.printf("%s\n", COLOR_RESET);
        break;
        
      case AD_TYPE_128BIT_SERVICE_UUIDS:
      case 0x06:
        Serial.printf("%s", color);
        if (adDataLen >= 16) {
          for (int i = 15; i >= 0; i--) {
            Serial.printf("%02X", adData[i]);
            if (i == 12 || i == 10 || i == 8 || i == 6) Serial.print("-");
          }
        }
        Serial.printf("%s\n", COLOR_RESET);
        break;
        
      case AD_TYPE_SERVICE_DATA_16BIT:
        if (adDataLen >= 2) {
          uint16_t uuid = adData[0] | (adData[1] << 8);
          Serial.printf("%sUUID: 0x%04X, Data: %s%s\n",
                       color, uuid, 
                       toHex(adData + 2, adDataLen - 2).c_str(),
                       COLOR_RESET);
        }
        break;
        
      case AD_TYPE_MANUFACTURER_DATA:
        if (adDataLen >= 2) {
          uint16_t companyId = adData[0] | (adData[1] << 8);
          Serial.printf("%sCompany: 0x%04X", color, companyId);
          
          switch (companyId) {
            case 0x004C: Serial.print(" (Apple)"); break;
            case 0x0075: Serial.print(" (Samsung)"); break;
            case 0x00E0: Serial.print(" (Google)"); break;
            case 0x0006: Serial.print(" (Microsoft)"); break;
            case 0x0059: Serial.print(" (Nordic Semi)"); break;
          }
          
          Serial.printf(", Data: %s%s\n",
                       toHex(adData + 2, adDataLen - 2).c_str(),
                       COLOR_RESET);
        }
        break;
        
      case AD_TYPE_TX_POWER:
        if (adDataLen >= 1) {
          int8_t power = (int8_t)adData[0];
          Serial.printf("%s%d dBm%s\n", color, power, COLOR_RESET);
        }
        break;
        
      default:
        Serial.printf("%s%s%s\n", color, toHex(adData, adDataLen).c_str(), COLOR_RESET);
        break;
    }
    
    offset += adLen + 1;
  }
  
  Serial.println();
}

// BLE scan callback
void scan_callback(ble_gap_evt_adv_report_t* report) {
  g_deviceCount++;
  
  // Gather device information
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          report->peer_addr.addr[5], report->peer_addr.addr[4],
          report->peer_addr.addr[3], report->peer_addr.addr[2],
          report->peer_addr.addr[1], report->peer_addr.addr[0]);
  String mac = macStr;
  
  int rssi = report->rssi;
  
  // Parse advertisement data
  uint8_t* payload = report->data.p_data;
  uint8_t len = report->data.len;
  
  // Extract name, UUID, and payload for filtering
  String name = "";
  String uuid = "";
  String payloadHex = toHex(payload, len);
  
  // Quick parse for name and UUID
  size_t offset = 0;
  while (offset < len) {
    if (len - offset < 2) break;
    uint8_t adLen = payload[offset];
    if (adLen == 0) break;
    if (offset + adLen + 1 > len) break;
    
    uint8_t adType = payload[offset + 1];
    const uint8_t* adData = &payload[offset + 2];
    size_t adDataLen = adLen - 1;
    
    if (adType == AD_TYPE_COMPLETE_LOCAL_NAME || adType == 0x08) {
      name = "";
      for (size_t i = 0; i < adDataLen; i++) {
        name += (char)adData[i];
      }
    } else if (adType == AD_TYPE_16BIT_SERVICE_UUIDS && adDataLen >= 2) {
      uint16_t svcUuid = adData[0] | (adData[1] << 8);
      char uuidBuf[8];
      sprintf(uuidBuf, "%04X", svcUuid);
      uuid = uuidBuf;
    }
    
    offset += adLen + 1;
  }
  
  // Apply filter
  if (!g_filter.shouldShow(mac, name, uuid, payloadHex)) {
    g_filteredCount++;
    Bluefruit.Scanner.resume();
    return;
  }
  
  // Deduplication check
  if (g_deduplication) {
    bool isNewOrChanged = true;
    
    for (auto& dev : g_seenDevices) {
      if (dev.mac == mac) {
        // Device seen before - check if anything changed
        bool nameChanged = (dev.name != name && name.length() > 0);
        bool payloadChanged = (dev.payload != payloadHex);
        bool rssiSignificantChange = abs(dev.rssi - rssi) > 10;  // >10 dBm change
        
        if (!nameChanged && !payloadChanged && !rssiSignificantChange) {
          // Nothing changed - skip display
          isNewOrChanged = false;
          dev.lastSeen = millis();
        } else {
          // Something changed - update and display
          if (nameChanged) dev.name = name;
          if (payloadChanged) dev.payload = payloadHex;
          dev.rssi = rssi;
          dev.lastSeen = millis();
        }
        break;
      }
    }
    
    // New device - add to tracking
    if (isNewOrChanged && g_seenDevices.size() < 100) {  // Limit to 100 devices
      bool found = false;
      for (const auto& dev : g_seenDevices) {
        if (dev.mac == mac) {
          found = true;
          break;
        }
      }
      
      if (!found) {
        SeenDevice newDev;
        newDev.mac = mac;
        newDev.name = name;
        newDev.payload = payloadHex;
        newDev.rssi = rssi;
        newDev.lastSeen = millis();
        g_seenDevices.push_back(newDev);
      }
    }
    
    if (!isNewOrChanged) {
      g_duplicateCount++;
      Bluefruit.Scanner.resume();
      return;
    }
  }
  
  // Print device header
  Serial.println();
  for (int i = 0; i < 80; i++) Serial.print("=");
  Serial.println();
  
  // Check if device is new or changed
  bool isNew = true;
  for (const auto& dev : g_seenDevices) {
    if (dev.mac == mac && dev.lastSeen < millis() - 1000) {  // Seen more than 1 sec ago
      isNew = false;
      break;
    }
  }
  
  if (isNew) {
    Serial.println("[BLE-DEVICE] NEW Device Detected");
  } else {
    Serial.println("[BLE-DEVICE] CHANGED Device Detected");
  }
  
  for (int i = 0; i < 80; i++) Serial.print("=");
  Serial.println();
  
  // Basic information
  Serial.println("\n[BASIC-INFO]");
  Serial.printf("  MAC Address:  %s\n", mac.c_str());
  Serial.printf("  RSSI:         %d dBm\n", rssi);
  Serial.printf("  Address Type: ");
  
  switch (report->peer_addr.addr_type) {
    case BLE_GAP_ADDR_TYPE_PUBLIC:
      Serial.println("Public");
      break;
    case BLE_GAP_ADDR_TYPE_RANDOM_STATIC:
      Serial.println("Random Static");
      break;
    case BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE:
      Serial.println("Random Private Resolvable");
      break;
    case BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE:
      Serial.println("Random Private Non-Resolvable");
      break;
    default:
      Serial.println("Unknown");
  }
  
  if (name.length() > 0) {
    Serial.printf("  Device Name:  %s\n", name.c_str());
  }
  
  // Raw Advertisement Payload
  Serial.println("\n[RAW-PAYLOAD]");
  Serial.printf("  Total Length: %d bytes\n", len);
  printHexDump(payload, len, "  Complete Advertisement:");
  
  // Parse and display AD structures with colors
  printADStructures(payload, len);
  
  // Footer
  for (int i = 0; i < 80; i++) Serial.print("=");
  Serial.println("\n");
  
  // Resume scanning
  Bluefruit.Scanner.resume();
}

// Process user commands
void processCommand() {
  Serial.println();
  Serial.println("================================================================================");
  Serial.println("[COMMAND] Enter command:");
  Serial.println("  Scanning:");
  Serial.println("    s [seconds]  - Scan for N seconds (e.g., 's 30' for 30 sec scan)");
  Serial.println("    a [seconds]  - Auto-scan mode: continuous scanning");
  Serial.println("    m            - Manual mode (wait for command between scans)");
  Serial.println("  Filters:");
  Serial.println("    f            - Show filter status");
  Serial.println("    b            - Add to blacklist (hide devices)");
  Serial.println("    w            - Add to whitelist (only show devices)");
  Serial.println("    x            - Clear all filters");
  Serial.println("    i            - Interactive filter from last scan");
  Serial.println("  Settings:");
  Serial.println("    c            - Toggle colors on/off");
  Serial.println("    d            - Toggle deduplication on/off");
  Serial.println("    h            - Show this help");
  Serial.println("================================================================================");
  Serial.print("> ");
  
  // Clear any pending input
  while (Serial.available()) {
    Serial.read();
  }
  
  // Wait for complete command line (until Enter is pressed)
  String cmd = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      // Enter key pressed - process command
      if (c == '\n' || c == '\r') {
        Serial.println();  // Echo newline
        break;
      }
      
      // Backspace handling
      if (c == '\b' || c == 127) {
        if (cmd.length() > 0) {
          cmd.remove(cmd.length() - 1);
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      
      // Add character to command
      if (c >= 32 && c <= 126) {  // Printable characters only
        cmd += c;
        Serial.write(c);  // Echo character
      }
    }
    delay(10);
  }
  
  cmd.trim();
  
  if (cmd.length() == 0) {
    Serial.println("(no command)");
    return;
  }
  
  // Parse command
  char cmdChar = cmd.charAt(0);
  String args = "";
  if (cmd.length() > 1) {
    args = cmd.substring(1);
    args.trim();
  }
  
  // Flag to indicate if we should scan after this command
  bool shouldScan = false;
  
  switch (cmdChar) {
    case 's':
    case 'S':
      // Scan command
      if (args.length() > 0) {
        int duration = args.toInt();
        if (duration > 0 && duration <= 300) {
          g_scanTimeSeconds = duration;
          Serial.printf("[CMD] Will scan for %d seconds\n", g_scanTimeSeconds);
          shouldScan = true;
        } else {
          Serial.println("[ERROR] Invalid duration (1-300 seconds)");
        }
      } else {
        Serial.printf("[CMD] Will scan for %d seconds (default)\n", g_scanTimeSeconds);
        shouldScan = true;
      }
      g_autoScan = false;
      break;
      
    case 'a':
    case 'A':
      // Auto-scan mode
      if (args.length() > 0) {
        int duration = args.toInt();
        if (duration > 0 && duration <= 300) {
          g_scanTimeSeconds = duration;
        }
      }
      g_autoScan = true;
      shouldScan = true;
      Serial.printf("[CMD] Auto-scan mode enabled (%d seconds per scan)\n", g_scanTimeSeconds);
      Serial.println("[CMD] Press 'm' to stop auto-scanning");
      break;
      
    case 'm':
    case 'M':
      // Manual mode
      g_autoScan = false;
      Serial.println("[CMD] Manual mode enabled (wait for command between scans)");
      break;
      
    case 'f':
    case 'F':
      // Show filter status
      g_filter.printStatus();
      break;
      
    case 'b':
    case 'B':
      // Add to blacklist
      addToBlacklist();
      break;
      
    case 'w':
    case 'W':
      // Add to whitelist
      addToWhitelist();
      break;
      
    case 'x':
    case 'X':
      // Clear filters
      g_filter.clearAllFilters();
      Serial.println("[CMD] All filters cleared");
      break;
      
    case 'i':
    case 'I':
      // Interactive filter from last scan
      interactiveFilter();
      break;
      
    case 'c':
    case 'C':
      // Toggle colors
#if ENABLE_COLORS
      g_colorsEnabled = !g_colorsEnabled;
      Serial.printf("[CMD] Colors %s\n", g_colorsEnabled ? "ENABLED" : "DISABLED");
      Serial.println("[INFO] Note: Color toggle only affects future output");
#else
      Serial.println("[INFO] Colors are disabled at compile-time");
      Serial.println("[INFO] Set ENABLE_COLORS to true and recompile to use colors");
#endif
      break;
      
    case 'd':
    case 'D':
      // Toggle deduplication
      g_deduplication = !g_deduplication;
      Serial.printf("[CMD] Deduplication %s\n", g_deduplication ? "ENABLED" : "DISABLED");
      if (g_deduplication) {
        Serial.println("[INFO] Only new devices or changed data will be displayed");
      } else {
        Serial.println("[INFO] All detected devices will be displayed");
      }
      break;
      
    case 'h':
    case 'H':
      // Show help - just show menu again
      break;
      
    default:
      Serial.printf("[ERROR] Unknown command: '%c'\n", cmdChar);
      Serial.println("[CMD] Type 'h' for help");
      break;
  }
  
  // Set global flag for loop to check
  if (!shouldScan) {
    g_autoScan = false;  // Ensure we stay in manual mode
    g_scanTimeSeconds = 0;  // Signal to loop not to scan
  }
  
  Serial.println();
}

// Add to blacklist interactively
void addToBlacklist() {
  Serial.println("\n[BLACKLIST] Add filter to hide devices");
  Serial.println("  1 - Add MAC address (exact match)");
  Serial.println("  2 - Add OUI (MAC prefix, first 3 bytes)");
  Serial.println("  3 - Add device name (partial match)");
  Serial.println("  4 - Add UUID (partial match)");
  Serial.println("  5 - Add payload hex pattern (partial match in raw data)");
  Serial.println("  0 - Cancel");
  Serial.print("> ");
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  // Wait for single character choice
  while (!Serial.available()) delay(10);
  char choiceChar = Serial.read();
  Serial.println(choiceChar);  // Echo
  
  String choice = String(choiceChar);
  
  if (choice == "0") {
    Serial.println("[CMD] Cancelled");
    return;
  }
  
  if (choice != "1" && choice != "2" && choice != "3" && choice != "4" && choice != "5") {
    Serial.println("[ERROR] Invalid choice");
    return;
  }
  
  // Clear any remaining input
  delay(100);
  while (Serial.available()) Serial.read();
  
  Serial.print("Enter value: ");
  
  // Read full line for value
  String value = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        Serial.println();
        break;
      }
      
      if (c == '\b' || c == 127) {
        if (value.length() > 0) {
          value.remove(value.length() - 1);
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      
      if (c >= 32 && c <= 126) {
        value += c;
        Serial.write(c);
      }
    }
    delay(10);
  }
  
  value.trim();
  value.toUpperCase();
  
  if (value.length() == 0) {
    Serial.println("[ERROR] Empty value");
    return;
  }
  
  if (choice == "1") {
    // Full MAC address
    g_filter.addBlacklistOUI(value);
    Serial.printf("[BLACKLIST] Added MAC: %s\n", value.c_str());
  } else if (choice == "2") {
    // OUI (should be format XX:XX:XX)
    if (value.length() < 8) {
      Serial.println("[ERROR] OUI must be format XX:XX:XX (e.g., A4:CF:12)");
      return;
    }
    g_filter.addBlacklistOUI(value.substring(0, 8));
    Serial.printf("[BLACKLIST] Added OUI: %s\n", value.substring(0, 8).c_str());
  } else if (choice == "3") {
    // Device name
    g_filter.addBlacklistName(value);
    Serial.printf("[BLACKLIST] Added name: %s\n", value.c_str());
  } else if (choice == "4") {
    // UUID
    g_filter.addBlacklistUUID(value);
    Serial.printf("[BLACKLIST] Added UUID: %s\n", value.c_str());
  } else if (choice == "5") {
    // Payload hex pattern
    g_filter.addBlacklistPayload(value);
    Serial.printf("[BLACKLIST] Added payload pattern: %s\n", value.c_str());
  }
  
  Serial.println("[BLACKLIST] Filter added successfully");
  Serial.println("[INFO] New filter will apply to next scan");
}

// Add to whitelist interactively
void addToWhitelist() {
  Serial.println("\n[WHITELIST] Add filter to ONLY show matching devices");
  Serial.println("  WARNING: Whitelist hides everything except matches!");
  Serial.println("  1 - Add MAC address (exact match)");
  Serial.println("  2 - Add OUI (MAC prefix, first 3 bytes)");
  Serial.println("  3 - Add device name (partial match)");
  Serial.println("  4 - Add UUID (partial match)");
  Serial.println("  5 - Add payload hex pattern (partial match in raw data)");
  Serial.println("  0 - Cancel");
  Serial.print("> ");
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  // Wait for single character choice
  while (!Serial.available()) delay(10);
  char choiceChar = Serial.read();
  Serial.println(choiceChar);  // Echo
  
  String choice = String(choiceChar);
  
  if (choice == "0") {
    Serial.println("[CMD] Cancelled");
    return;
  }
  
  if (choice != "1" && choice != "2" && choice != "3" && choice != "4" && choice != "5") {
    Serial.println("[ERROR] Invalid choice");
    return;
  }
  
  // Clear any remaining input
  delay(100);
  while (Serial.available()) Serial.read();
  
  Serial.print("Enter value: ");
  
  // Read full line for value
  String value = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        Serial.println();
        break;
      }
      
      if (c == '\b' || c == 127) {
        if (value.length() > 0) {
          value.remove(value.length() - 1);
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      
      if (c >= 32 && c <= 126) {
        value += c;
        Serial.write(c);
      }
    }
    delay(10);
  }
  
  value.trim();
  value.toUpperCase();
  
  if (value.length() == 0) {
    Serial.println("[ERROR] Empty value");
    return;
  }
  
  if (choice == "1") {
    g_filter.addWhitelistOUI(value);
    Serial.printf("[WHITELIST] Added MAC: %s\n", value.c_str());
  } else if (choice == "2") {
    if (value.length() < 8) {
      Serial.println("[ERROR] OUI must be format XX:XX:XX (e.g., A4:CF:12)");
      return;
    }
    g_filter.addWhitelistOUI(value.substring(0, 8));
    Serial.printf("[WHITELIST] Added OUI: %s\n", value.substring(0, 8).c_str());
  } else if (choice == "3") {
    g_filter.addWhitelistName(value);
    Serial.printf("[WHITELIST] Added name: %s\n", value.c_str());
  } else if (choice == "4") {
    g_filter.addWhitelistUUID(value);
    Serial.printf("[WHITELIST] Added UUID: %s\n", value.c_str());
  } else if (choice == "5") {
    // Payload hex pattern
    g_filter.addWhitelistPayload(value);
    Serial.printf("[WHITELIST] Added payload pattern: %s\n", value.c_str());
  }
  
  Serial.println("[WHITELIST] Filter added successfully");
  Serial.println("[INFO] ONLY matching devices will be shown in next scan");
}

// Interactive filter from last scan
void interactiveFilter() {
  if (g_seenDevices.empty()) {
    Serial.println("[ERROR] No devices from last scan. Run a scan first.");
    return;
  }
  
  Serial.println("\n[INTERACTIVE] Select device to filter:");
  for (size_t i = 0; i < g_seenDevices.size() && i < 20; i++) {
    Serial.printf("  %2d - %s", (int)(i + 1), g_seenDevices[i].mac.c_str());
    if (g_seenDevices[i].name.length() > 0) {
      Serial.printf(" (%s)", g_seenDevices[i].name.c_str());
    }
    Serial.println();
  }
  
  if (g_seenDevices.size() > 20) {
    Serial.printf("  ... and %d more\n", (int)(g_seenDevices.size() - 20));
  }
  
  Serial.println("  0 - Cancel");
  Serial.print("Select device number: ");
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  // Read device selection
  String choice = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        Serial.println();
        break;
      }
      
      if (c == '\b' || c == 127) {
        if (choice.length() > 0) {
          choice.remove(choice.length() - 1);
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      
      if (c >= '0' && c <= '9') {
        choice += c;
        Serial.write(c);
      }
    }
    delay(10);
  }
  
  int idx = choice.toInt();
  if (idx == 0) {
    Serial.println("[CMD] Cancelled");
    return;
  }
  
  if (idx < 1 || idx > (int)g_seenDevices.size()) {
    Serial.println("[ERROR] Invalid selection");
    return;
  }
  
  SeenDevice& dev = g_seenDevices[idx - 1];
  
  Serial.println("\n[FILTER] What to filter?");
  Serial.println("  1 - Hide this exact MAC");
  Serial.println("  2 - Hide this OUI (all devices with same prefix)");
  if (dev.name.length() > 0) {
    Serial.printf("  3 - Hide all devices named '%s'\n", dev.name.c_str());
  }
  Serial.println("  4 - ONLY show this exact MAC (whitelist)");
  Serial.println("  5 - ONLY show this OUI (whitelist)");
  Serial.println("  0 - Cancel");
  Serial.print("> ");
  
  // Clear input buffer
  while (Serial.available()) Serial.read();
  
  // Wait for single character choice
  while (!Serial.available()) delay(10);
  char filterChoiceChar = Serial.read();
  Serial.println(filterChoiceChar);  // Echo
  
  String filterChoice = String(filterChoiceChar);
  
  if (filterChoice == "0") {
    Serial.println("[CMD] Cancelled");
    return;
  }
  
  if (filterChoice == "1") {
    g_filter.addBlacklistOUI(dev.mac);
    Serial.printf("[BLACKLIST] Hiding MAC: %s\n", dev.mac.c_str());
  } else if (filterChoice == "2") {
    String oui = dev.mac.substring(0, 8);
    g_filter.addBlacklistOUI(oui);
    Serial.printf("[BLACKLIST] Hiding OUI: %s\n", oui.c_str());
  } else if (filterChoice == "3" && dev.name.length() > 0) {
    g_filter.addBlacklistName(dev.name);
    Serial.printf("[BLACKLIST] Hiding name: %s\n", dev.name.c_str());
  } else if (filterChoice == "4") {
    g_filter.addWhitelistOUI(dev.mac);
    Serial.printf("[WHITELIST] ONLY showing MAC: %s\n", dev.mac.c_str());
  } else if (filterChoice == "5") {
    String oui = dev.mac.substring(0, 8);
    g_filter.addWhitelistOUI(oui);
    Serial.printf("[WHITELIST] ONLY showing OUI: %s\n", oui.c_str());
  } else {
    Serial.println("[ERROR] Invalid choice");
    return;
  }
  
  Serial.println("[FILTER] Applied successfully");
  Serial.println("[INFO] Filter will take effect in next scan");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(1000);
  
  Serial.println();
  Serial.println("================================================================================");
  Serial.println("           Professional BLE Scanner with Configurable Filtering");
  Serial.println("                            nRF52840 + Nordic SoftDevice");
  Serial.println("================================================================================");
  Serial.println();
  
  // Initialize filter system
  Serial.println("[FILTER] Initializing filter system...");
  if (g_filter.begin()) {
    g_filter.printStatus();
  } else {
    Serial.println("[FILTER] Running without filters (showing all devices)");
  }
  
  // Initialize Bluefruit
  Serial.println("[BLE] Initializing Bluefruit...");
  Bluefruit.begin();
  Bluefruit.setName("nRF52_Scanner");
  
  // Set max power for scanning
  Bluefruit.setTxPower(8);  // 8 dBm max for nRF52840
  
  // Configure scanner
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(80, 80);     // Fast: 50ms interval, 50ms window
  Bluefruit.Scanner.filterRssi(-127);        // Show ALL devices (no RSSI filter)
  Bluefruit.Scanner.useActiveScan(true);     // Active scan for more data
  // Note: No UUID filter by default
  
  Serial.println("[BLE] Scanner initialized successfully");
  Serial.printf("[CONFIG] Default Scan Time: %d seconds\n", g_scanTimeSeconds);
  Serial.printf("[CONFIG] Scan Settings: Fast (50ms interval, 50ms window)\n");
  Serial.printf("[CONFIG] RSSI Filter: DISABLED (shows all devices)\n");
  Serial.printf("[CONFIG] Mode: %s\n", g_autoScan ? "Auto-scan" : "Manual (interactive)");
  Serial.printf("[CONFIG] Deduplication: %s\n", g_deduplication ? "ENABLED" : "DISABLED");
#if ENABLE_COLORS
  Serial.printf("[CONFIG] Colors: %s (toggle with 'c' command)\n", g_colorsEnabled ? "ENABLED" : "DISABLED");
#else
  Serial.println("[CONFIG] Colors: DISABLED (set ENABLE_COLORS=true to enable)");
#endif
  Serial.println();
  
  Serial.println("[READY] Scanner ready for commands");
  Serial.println("        Type 's' to scan, 's 30' for 30-second scan");
  Serial.println("        Type 'd' to toggle deduplication");
  Serial.println("        Type 'h' for help");
  
  for (int i = 0; i < 80; i++) Serial.print("-");
  Serial.println("\n");
}

void loop() {
  // In manual mode, wait for command
  if (!g_autoScan) {
    processCommand();
    
    // Check if we should scan (scanTimeSeconds will be 0 if command doesn't want to scan)
    if (g_scanTimeSeconds == 0) {
      g_scanTimeSeconds = 10;  // Reset to default for next time
      return;  // Don't scan, wait for next command
    }
  }
  
  g_scanCount++;
  g_deviceCount = 0;
  g_filteredCount = 0;
  g_duplicateCount = 0;
  
  // Clear seen devices at start of each scan for fresh tracking
  g_seenDevices.clear();
  
  Serial.printf("\n[SCAN] Starting scan #%lu (%lu seconds)...\n", 
                (unsigned long)g_scanCount, (unsigned long)g_scanTimeSeconds);
  if (g_deduplication) {
    Serial.println("[INFO] Deduplication enabled - only new/changed devices shown");
  }
  
  unsigned long scanStart = millis();
  
  // Start scanning (0 = continuous until stopped)
  Bluefruit.Scanner.start(0);
  
  // Let it scan for the configured duration
  // Process in small chunks to allow resume() to work properly
  unsigned long scanDuration = 0;
  while (scanDuration < g_scanTimeSeconds * 1000) {
    delay(100);  // Small delay to let callbacks process
    scanDuration = millis() - scanStart;
    
    // In auto mode, check for 'm' to stop
    if (g_autoScan && Serial.available()) {
      char c = Serial.read();
      if (c == 'm' || c == 'M') {
        Bluefruit.Scanner.stop();
        g_autoScan = false;
        Serial.println("\n[CMD] Auto-scan stopped - returning to manual mode");
        return;
      }
    }
  }
  
  // Stop scanning
  Bluefruit.Scanner.stop();
  
  scanDuration = (millis() - scanStart) / 1000;
  
  Serial.printf("\n[SUMMARY] Scan #%lu complete (took %lu seconds)\n", 
                (unsigned long)g_scanCount, (unsigned long)scanDuration);
  Serial.printf("  Total callbacks:  %lu\n", (unsigned long)g_deviceCount);
  Serial.printf("  Filtered out:     %lu\n", (unsigned long)g_filteredCount);
  
  if (g_deduplication) {
    Serial.printf("  Duplicates:       %lu\n", (unsigned long)g_duplicateCount);
    Serial.printf("  Displayed:        %lu (new or changed)\n", 
                  (unsigned long)(g_deviceCount - g_filteredCount - g_duplicateCount));
    Serial.printf("  Unique devices:   %lu\n", (unsigned long)g_seenDevices.size());
  } else {
    Serial.printf("  Displayed:        %lu\n", 
                  (unsigned long)(g_deviceCount - g_filteredCount));
  }
  Serial.println();
  
  // Print filter status every 5 scans
  if (g_scanCount % 5 == 0) {
    g_filter.printStatus();
  }
  
  // In auto mode, brief pause then continue
  if (g_autoScan) {
    Serial.println("[AUTO] Auto-scan mode: Starting next scan in 3 seconds...");
    Serial.println("       (Send 'm' to stop)");
    for (int i = 0; i < 80; i++) Serial.print("-");
    Serial.println();
    
    unsigned long pauseStart = millis();
    while (millis() - pauseStart < 3000) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 'm' || c == 'M') {
          g_autoScan = false;
          Serial.println("\n[CMD] Auto-scan stopped - returning to manual mode");
          return;
        }
      }
      delay(100);
    }
  } else {
    for (int i = 0; i < 80; i++) Serial.print("-");
    Serial.println();
  }
}
