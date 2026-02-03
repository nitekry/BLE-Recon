/*
 * BLE Filter Config with Built-in Apple/Google Blacklist
 * No filesystem needed - filters compiled into code
 */

#ifndef BLE_FILTER_CONFIG_BUILTIN_H
#define BLE_FILTER_CONFIG_BUILTIN_H

#include <Arduino.h>
#include <vector>

// Filter types
enum FilterMode {
  FILTER_OFF,       // Show all devices
  FILTER_WHITELIST, // Show only devices in target list
  FILTER_BLACKLIST  // Hide devices in ignore list
};

struct FilterConfig {
  FilterMode mode = FILTER_OFF;
  std::vector<String> ouiList;
  std::vector<String> nameList;
  std::vector<String> uuidList;
  std::vector<String> payloadList;
};

class BLEFilter {
private:
  FilterConfig whitelist;
  FilterConfig blacklist;
  bool initialized = false;

  bool matchesOUI(const String& mac, const std::vector<String>& ouiList) {
    if (ouiList.empty()) return false;
    String macUpper = mac;
    macUpper.toUpperCase();
    
    for (const auto& pattern : ouiList) {
      String patternUpper = pattern;
      patternUpper.toUpperCase();
      
      // Check if pattern is a full MAC (17 chars with colons) or OUI (8 chars)
      if (patternUpper.length() >= 17) {
        // Full MAC - exact match
        if (macUpper == patternUpper) return true;
      } else {
        // OUI or partial MAC - prefix match
        // Extract just the prefix part we want to compare
        int compareLen = min(patternUpper.length(), macUpper.length());
        if (macUpper.substring(0, compareLen) == patternUpper.substring(0, compareLen)) {
          return true;
        }
      }
    }
    return false;
  }

  bool matchesName(const String& name, const std::vector<String>& nameList) {
    if (nameList.empty() || name.length() == 0) return false;
    String nameUpper = name;
    nameUpper.toUpperCase();
    
    for (const auto& pattern : nameList) {
      if (nameUpper.indexOf(pattern) >= 0) return true;
    }
    return false;
  }

  bool matchesUUID(const String& uuid, const std::vector<String>& uuidList) {
    if (uuidList.empty()) return false;
    String uuidUpper = uuid;
    uuidUpper.toUpperCase();
    
    for (const auto& pattern : uuidList) {
      if (uuidUpper.indexOf(pattern) >= 0) return true;
    }
    return false;
  }

  bool matchesPayload(const String& payload, const std::vector<String>& payloadList) {
    if (payloadList.empty() || payload.length() == 0) return false;
    String payloadUpper = payload;
    payloadUpper.toUpperCase();
    
    for (const auto& pattern : payloadList) {
      String patternUpper = pattern;
      patternUpper.toUpperCase();
      
      // Check if pattern appears anywhere in payload (substring match)
      if (payloadUpper.indexOf(patternUpper) >= 0) {
        return true;
      }
    }
    return false;
  }

  void loadBuiltinFilters() {
    Serial.println("[FILTER] Loading built-in Apple/Google blacklist...");
    
    // Apple OUIs (most common ones to save memory)
    const char* appleOUIs[] = {
      "A4:CF:12", "4C:57:CA", "00:00:00", "A8:88:08", "04:0C:CE",
      "98:01:A7", "3C:E0:72", "00:CD:FE", "A4:D1:8C", "78:A3:E4",
      "DC:2B:2A", "00:26:BB", "F0:DB:E2", "68:96:7B", "8C:85:90",
      "80:E6:50", "00:1F:F3", "00:23:12", "00:25:00", "00:25:BC",
      "34:15:9E", "00:88:65", "00:F4:B9", "84:38:35", "C8:2A:14",
      "F0:D1:A9", "70:73:CB", "F4:F1:5A", "D4:90:9C", "98:B8:E3",
      "AC:3C:0B", "00:3E:E1", "DC:86:D8", "3C:07:54", "60:03:08",
      "B0:65:BD", "F0:DC:E2", "94:F6:A3", "98:FE:94", "E0:C7:67",
      "70:CD:60", "BC:4C:C4", "48:43:7C", "34:C0:59", "E8:80:2E",
      "90:84:0D", "D8:30:62", "18:E7:F4", "18:20:32", "00:F7:6F"
    };
    
    // Google/Nest OUIs
    const char* googleOUIs[] = {
      "F4:F5:E8", "D0:E7:82", "2C:F0:A2", "5C:F8:A1", "7C:2F:80",
      "1C:F2:9A", "00:1A:11", "00:26:B7", "00:17:C9", "00:19:07",
      "00:21:6A", "00:21:91", "00:23:4D", "00:25:9C", "34:FC:EF",
      "3C:5A:B4", "40:B4:CD", "54:60:09", "58:CB:52", "6C:AD:F8",
      "74:E5:43", "78:D6:F0", "7C:BB:8A", "88:75:56", "90:E7:C4"
    };
    
    // Device names to block
    const char* blockedNames[] = {
      "IPHONE", "IPAD", "MACBOOK", "AIRPODS", "APPLE", "WATCH",
      "PIXEL", "GOOGLE", "NEST", "CHROMECAST", "ANDROID"
    };
    
    // Payload signatures
    const char* blockedPayloads[] = {
      "4C00",  // Apple manufacturer data
      "E000"   // Google manufacturer data
    };
    
    // Load into blacklist
    for (const auto& oui : appleOUIs) {
      blacklist.ouiList.push_back(String(oui));
    }
    
    for (const auto& oui : googleOUIs) {
      blacklist.ouiList.push_back(String(oui));
    }
    
    for (const auto& name : blockedNames) {
      blacklist.nameList.push_back(String(name));
    }
    
    for (const auto& payload : blockedPayloads) {
      blacklist.payloadList.push_back(String(payload));
    }
    
    Serial.printf("[FILTER] Loaded %d OUIs, %d names, %d payloads\n",
                  blacklist.ouiList.size(),
                  blacklist.nameList.size(),
                  blacklist.payloadList.size());
  }

public:
  bool begin() {
    initialized = true;
    
    // Load built-in filters
    loadBuiltinFilters();
    
    // Enable blacklist if we have filters
    if (!blacklist.ouiList.empty() || !blacklist.nameList.empty() || 
        !blacklist.payloadList.empty()) {
      blacklist.mode = FILTER_BLACKLIST;
      Serial.println("[FILTER] Blacklist mode ENABLED (built-in filters)");
    }
    
    return true;
  }

  bool shouldShow(const String& mac, const String& name, 
                  const String& uuid, const String& payload) {
    if (!initialized) return true;
    
    // Whitelist takes priority
    if (whitelist.mode == FILTER_WHITELIST) {
      bool matches = matchesOUI(mac, whitelist.ouiList) ||
                     matchesName(name, whitelist.nameList) ||
                     matchesUUID(uuid, whitelist.uuidList) ||
                     matchesPayload(payload, whitelist.payloadList);
      return matches;
    }
    
    // Blacklist - hide matching devices
    if (blacklist.mode == FILTER_BLACKLIST) {
      bool matches = matchesOUI(mac, blacklist.ouiList) ||
                     matchesName(name, blacklist.nameList) ||
                     matchesUUID(uuid, blacklist.uuidList) ||
                     matchesPayload(payload, blacklist.payloadList);
      return !matches;
    }
    
    return true;
  }

  void printStatus() {
    Serial.println("\n[FILTER-STATUS]");
    Serial.printf("  Whitelist: %s (%d OUI, %d names, %d UUIDs, %d payloads)\n",
                  whitelist.mode == FILTER_WHITELIST ? "ACTIVE" : "OFF",
                  whitelist.ouiList.size(), whitelist.nameList.size(),
                  whitelist.uuidList.size(), whitelist.payloadList.size());
    Serial.printf("  Blacklist: %s (%d OUI, %d names, %d UUIDs, %d payloads)\n",
                  blacklist.mode == FILTER_BLACKLIST ? "ACTIVE" : "OFF",
                  blacklist.ouiList.size(), blacklist.nameList.size(),
                  blacklist.uuidList.size(), blacklist.payloadList.size());
    
    // Show whitelist entries if any
    if (whitelist.mode == FILTER_WHITELIST) {
      if (!whitelist.ouiList.empty()) {
        Serial.println("\n  Whitelist OUI/MAC entries:");
        for (size_t i = 0; i < whitelist.ouiList.size() && i < 10; i++) {
          Serial.printf("    - %s\n", whitelist.ouiList[i].c_str());
        }
        if (whitelist.ouiList.size() > 10) {
          Serial.printf("    ... and %d more\n", whitelist.ouiList.size() - 10);
        }
      }
      
      if (!whitelist.nameList.empty()) {
        Serial.println("\n  Whitelist name entries:");
        for (size_t i = 0; i < whitelist.nameList.size() && i < 5; i++) {
          Serial.printf("    - %s\n", whitelist.nameList[i].c_str());
        }
      }
      
      if (!whitelist.uuidList.empty()) {
        Serial.println("\n  Whitelist UUID entries:");
        for (size_t i = 0; i < whitelist.uuidList.size() && i < 5; i++) {
          Serial.printf("    - %s\n", whitelist.uuidList[i].c_str());
        }
      }
      
      if (!whitelist.payloadList.empty()) {
        Serial.println("\n  Whitelist payload patterns:");
        for (size_t i = 0; i < whitelist.payloadList.size() && i < 5; i++) {
          Serial.printf("    - %s\n", whitelist.payloadList[i].c_str());
        }
      }
    }
    
    // Show blacklist OUI entries if any
    if (!blacklist.ouiList.empty()) {
      Serial.println("\n  Blacklist OUI/MAC entries:");
      for (size_t i = 0; i < blacklist.ouiList.size() && i < 10; i++) {
        Serial.printf("    - %s\n", blacklist.ouiList[i].c_str());
      }
      if (blacklist.ouiList.size() > 10) {
        Serial.printf("    ... and %d more\n", blacklist.ouiList.size() - 10);
      }
    }
    
    // Show blacklist names if any
    if (!blacklist.nameList.empty()) {
      Serial.println("\n  Blacklist name entries:");
      for (size_t i = 0; i < blacklist.nameList.size() && i < 5; i++) {
        Serial.printf("    - %s\n", blacklist.nameList[i].c_str());
      }
      if (blacklist.nameList.size() > 5) {
        Serial.printf("    ... and %d more\n", blacklist.nameList.size() - 5);
      }
    }
    
    if (!blacklist.payloadList.empty()) {
      Serial.println("\n  Blacklist payload patterns:");
      for (size_t i = 0; i < blacklist.payloadList.size() && i < 5; i++) {
        Serial.printf("    - %s\n", blacklist.payloadList[i].c_str());
      }
    }
    
    Serial.println();
  }

  // Allow runtime modification
  void addBlacklistOUI(const String& oui) {
    blacklist.ouiList.push_back(oui);
    blacklist.mode = FILTER_BLACKLIST;
  }
  
  void addBlacklistName(const String& name) {
    blacklist.nameList.push_back(name);
    blacklist.mode = FILTER_BLACKLIST;
  }
  
  void addBlacklistUUID(const String& uuid) {
    blacklist.uuidList.push_back(uuid);
    blacklist.mode = FILTER_BLACKLIST;
  }
  
  void addBlacklistPayload(const String& payload) {
    blacklist.payloadList.push_back(payload);
    blacklist.mode = FILTER_BLACKLIST;
  }
  
  void addWhitelistOUI(const String& oui) {
    whitelist.ouiList.push_back(oui);
    whitelist.mode = FILTER_WHITELIST;
  }
  
  void addWhitelistName(const String& name) {
    whitelist.nameList.push_back(name);
    whitelist.mode = FILTER_WHITELIST;
  }
  
  void addWhitelistUUID(const String& uuid) {
    whitelist.uuidList.push_back(uuid);
    whitelist.mode = FILTER_WHITELIST;
  }
  
  void addWhitelistPayload(const String& payload) {
    whitelist.payloadList.push_back(payload);
    whitelist.mode = FILTER_WHITELIST;
  }
  
  void clearBlacklist() {
    blacklist.ouiList.clear();
    blacklist.nameList.clear();
    blacklist.uuidList.clear();
    blacklist.payloadList.clear();
    blacklist.mode = FILTER_OFF;
    Serial.println("[FILTER] Blacklist cleared");
  }
  
  void clearWhitelist() {
    whitelist.ouiList.clear();
    whitelist.nameList.clear();
    whitelist.uuidList.clear();
    whitelist.payloadList.clear();
    whitelist.mode = FILTER_OFF;
    Serial.println("[FILTER] Whitelist cleared");
  }
  
  void clearAllFilters() {
    clearBlacklist();
    clearWhitelist();
    Serial.println("[FILTER] All filters cleared");
  }
  
  void disableFilters() {
    blacklist.mode = FILTER_OFF;
    whitelist.mode = FILTER_OFF;
    Serial.println("[FILTER] All filters disabled");
  }
  
  void enableFilters() {
    if (!blacklist.ouiList.empty()) {
      blacklist.mode = FILTER_BLACKLIST;
      Serial.println("[FILTER] Blacklist re-enabled");
    }
    if (!whitelist.ouiList.empty()) {
      whitelist.mode = FILTER_WHITELIST;
      Serial.println("[FILTER] Whitelist re-enabled");
    }
  }
};

#endif // BLE_FILTER_CONFIG_BUILTIN_H
