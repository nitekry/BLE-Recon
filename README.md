# BLE Scanner for nRF52840

Interactive Bluetooth Low Energy (BLE) scanner with advanced filtering capabilities, designed for security research and recon.

![Platform](https://img.shields.io/badge/platform-nRF52840-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-green)
![License](https://img.shields.io/badge/license-MIT-orange)

## Features

### Core Capabilities
- Interactive Command Interface** - Full menu-driven control
- Advanced Filtering - Runtime blacklist/whitelist by MAC, OUI, name, UUID, or payload pattern
- Deduplication - Only show new or changed devices
- Color-Coded Output - ANSI colors for easy AD structure identification
- Detailed Payload Analysis - Complete hex dumps with ASCII view
- GAEN Detection - Automatic COVID-19 exposure notification beacon identification
- Built-in Filters - /data text files
- Auto-Scan Mode - Continuous monitoring
- Zero Filesystem Dependencies - Filters compiled into code
- Complete advertisement data parsing
- AD structure identification with color coding
- Manufacturer data decoding (Apple, Google, Samsung, Microsoft, etc.)
- Service UUID recognition
- RSSI tracking with significant change detection
- Rolling Proximity Identifier (RPI) change detection for GAEN beacons

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Installation](#setup)
- [Usage Guide](#usage-guide)
- [Command Reference](#command-reference)
- [Filtering System](#filtering-system)
- [Advanced Features](#advanced-features)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)

## Hardware Requirements

### Supported Boards

| Board | Status | Notes |
|-------|--------|-------|
| **Seeed XIAO nRF52840** | Tested | Primary target, USB-C, tiny form factor |
| Seeed XIAO nRF52840 Sense | Tested | With IMU and microphone |
| Adafruit Feather nRF52840 | Compatible | Larger, more pins |
| Adafruit ItsyBitsy nRF52840 | Compatible | Compact alternative |
| Nordic nRF52840 DK | Compatible | Development kit |



### Setup

**Seeed XIAO nRF52840:**
1. Connect via USB-C
2. No external antenna needed (built-in)
3. Optional: Connect 3.7V LiPo battery for portable operation

### 2. Software Installation
I always suggest using venv.

```bash
gitclone https://github.com/XXXX
cd xxxx
python3 -m venv venv
source venv/bin/activate
pip install platformio
python3 -m pip install --pre -U git+https://github.com/makerdiary/uf2utils.git@main
pio run 

##Find your build <project>
ls -lah .pio/build/* | head
uf2conv -c -f 0xADA52840 -o firmware.uf2 .pio/build/nrf5<project>/firmware.hex

##double tap reset and copy the uf2 firmware over

pyserial-miniterm /dev/tty.usbmodem1114401 115200 --raw -f direct 
```

## Usage Guide

### Basic Commands

#### Scanning

```bash
s           # Scan for 10 seconds (default)
a           # Auto-scan mode (continuous)
a 15        # Auto-scan with 15 second intervals
m           # Stop auto-scan, return to manual mode
```

#### Filters

```bash
f           # Show current filter status
b           # Add to blacklist (hide devices)
w           # Add to whitelist (only show matching)
x           # Clear all filters
i           # Interactive filter from last scan
```

#### Settings

```bash
d           # Toggle deduplication on/off
c           # Toggle colors on/off
h           # Show help menu
```

### Interactive Command Interface

```
[COMMAND] Enter command:
  Scanning:
    s [seconds]  - Scan for N seconds
    a [seconds]  - Auto-scan mode
    m            - Manual mode
  Filters:
    f            - Show filter status
    b            - Add to blacklist
    w            - Add to whitelist
    x            - Clear all filters
    i            - Interactive filter
  Settings:
    c            - Toggle colors
    d            - Toggle deduplication
    h            - Show help
> _
```

## Command Reference

### Scan Commands

| Command | Description | Example |
|---------|-------------|---------|
| `s` | Scan with default time (10s) | `> s` |
| `s N` | Scan for N seconds (1-300) | `> s 30` |
| `a` | Auto-scan continuous | `> a` |
| `a N` | Auto-scan with N second scans | `> a 15` |
| `m` | Manual mode (stop auto-scan) | `> m` |

### Filter Commands

| Command | Description | Use Case |
|---------|-------------|----------|
| `b` | Add blacklist entry | Hide specific devices |
| `w` | Add whitelist entry | Only show specific devices |
| `i` | Interactive filter | Quick filter from last scan |
| `x` | Clear all filters | Reset to default |
| `f` | Show filter status | Check active filters |

### Setting Commands

| Command | Description | Default |
|---------|-------------|---------|
| `d` | Toggle deduplication | ON |
| `c` | Toggle colors | ON |
| `h` | Show help | - |

## Filtering System

### Filter Types

The scanner supports 5 filter types:

#### 1. MAC Address (Exact Match)
```
> b
> 1
Enter value: A4:CF:12:34:56:78
```
Matches only this exact device.

#### 2. OUI (MAC Prefix)
```
> b
> 2
Enter value: A4:CF:12
```
Matches all devices with MAC starting `A4:CF:12:*:*:*`

#### 3. Device Name (Substring)
```
> b
> 3
Enter value: IPHONE
```
Matches any device with "iPhone" in name (case-insensitive).

#### 4. UUID (Substring)
```
> w
> 4
Enter value: FD6F
```
Matches devices advertising this service UUID.

#### 5. Payload Pattern (Hex Substring)
```
> w
> 5
Enter value: 4C00
```
Matches devices with `4C00` anywhere in raw advertisement data.

### Blacklist vs Whitelist

**Blacklist** - Hides matching devices:
```
> b
> 2
Enter value: A4:CF:12
[BLACKLIST] Added OUI: A4:CF:12
```

**Whitelist** - Shows ONLY matching devices:
```
> w
> 3
Enter value: SENSOR
[WHITELIST] Added name: SENSOR
[INFO] ONLY matching devices will be shown
```

⚠️ **Note:** Whitelist takes priority over blacklist!

### Built-in Filters

Pre-loaded on startup: based on /data/blacklist.txt
- **75+ Apple OUI prefixes** (iPhone, iPad, MacBook, AirPods, etc.)
- **25+ Google/Nest OUI prefixes**
- **11 common device names** (iPhone, iPad, Google, Pixel, etc.)
- **2 manufacturer signatures** (Apple 0x004C, Google 0x00E0)

Clear built-in filters:
```
> x
[FILTER] All filters cleared
```

### Interactive Filtering

Quick filter from scan results:

```
> s 10
[BLE-DEVICE] #1: A4:CF:12:34:56:78 (iPhone)
[BLE-DEVICE] #2: F4:F5:E8:11:22:33 (Google Home)

> i
[INTERACTIVE] Select device to filter:
   1 - A4:CF:12:34:56:78 (iPhone)
   2 - F4:F5:E8:11:22:33 (Google Home)
Select device number: 1

[FILTER] What to filter?
  1 - Hide this exact MAC
  2 - Hide this OUI
  3 - Hide all devices named 'iPhone'
> 2

[BLACKLIST] Hiding OUI: A4:CF:12
```

## Advanced Features

### Deduplication

Shows only new devices or devices with changed data:

```
> d
[CMD] Deduplication ENABLED

> s 30
[BLE-DEVICE] NEW Device Detected
MAC: 12:34:56:78:90:AB

> s 30
[BLE-DEVICE] CHANGED Device Detected
MAC: 12:34:56:78:90:AB
(Payload changed - RPI rotation!)

[SUMMARY]
  Total callbacks:  150
  Duplicates:       140 ← Skipped
  Displayed:        10  ← Only new/changed
```

### Color-Coded Output

ANSI color coding for AD structures:

| Color | AD Type | Code |
|-------|---------|------|
| **Cyan** | Flags | `0x01` |
| **Bright Green** | Complete Name | `0x09` |
| **Bright Blue** | 16-bit UUIDs | `0x03` |
| **Bright Magenta** | Service Data | `0x16` |
| **Bright Yellow** | Manufacturer Data | `0xFF` |

Toggle: `> c`

### Auto-Scan Mode

Continuous monitoring:

```
> a 20
[CMD] Auto-scan enabled (20 seconds per scan)

[SCAN] Starting scan #1...
[SCAN] Starting scan #2...

> m
[CMD] Auto-scan stopped
```

## Examples

### Example 1: Find All GAEN Beacons

```bash
> x              # Clear filters
> w              # Whitelist
> 5              # Payload pattern
Enter value: FD6F
> s 60           # Scan
```

### Example 2: Hide Consumer Devices

```bash
> f              # Check built-in filters
[FILTER-STATUS]
  Blacklist: ACTIVE (75 OUI, 11 names)
  
> s 30           # Scan without Apple/Google
```

### Example 3: Monitor Your Sensors

```bash
> x              # Clear
> w              # Whitelist
> 2              # OUI
Enter value: 30:AE:A4  # ESP32
> a 15           # Auto-scan
```

### Example 4: Track Specific Device

```bash
> s 10           # Scan first
> i              # Interactive
> 1              # Select device
> 4              # Whitelist MAC
> a 30           # Monitor
```

## Troubleshooting

### Upload Issues

**Can't upload to XIAO:**
1. Double-click reset button
2. Green LED breathes
3. Drag `firmware.uf2` to XIAO drive

### No Devices Found

**Solutions:**
1. Disable filters: `> x`
2. Increase scan time: `> s 60`
3. Check antenna connection

### Filters Not Working

**Debug:**
```bash
> f              # Check status
> x              # Clear all
> b              # Add new filter
> 2
Enter value: A4:CF:12
> f              # Verify added
> s 10           # Test
```

### Serial Monitor Issues

**Solutions:**
1. Check baud rate: `115200`
2. Try: `pio device monitor`
3. Check USB cable (data-capable)

## Output Example

```
================================================================================
[BLE-DEVICE] NEW Device Detected
================================================================================

[BASIC-INFO]
  MAC Address:  4D:1D:BB:E8:AB:74
  RSSI:         -61 dBm
  Address Type: Random Private Resolvable

[RAW-PAYLOAD]
  Total Length: 31 bytes
  Complete Advertisement:
  Offset  Hex                                              ASCII
  ------  -----------------------------------------------  ----------------
  0x0000  02 01 02 1B FF 75 00 02  18 61 A1 F7 95 E3 69 B1  .....u...a....i.
  0x0010  69 6F B4 06 03 C6 7D 87  F7 2F 73 19 68 A5 D0     io....}../s.h..


[AD-STRUCTURES] Advertisement Data Structures:
  Legend:
    Flags | Name | UUIDs | Service Data | Mfg Data | Other
  ----------------
  [1] Type 0x01: Flags (Length: 1 bytes)
      Data: 0x02 (LE General)
  [2] Type 0xFF: Manufacturer Data (Length: 26 bytes)
      Data: Company: 0x0075 (Samsung), Data: 021861A1F795E369B1696FB40603C67D87F72F731968A5D0

================================================================================

[SUMMARY] Scan complete
  Total callbacks:  45
  Displayed:        12 (new/changed)
  Unique devices:   35
```

## Security & Privacy

- **No data logging** - RAM only
- **No network** - Offline operation
- **Local processing** - All on-device
- **Research use** - Designed for legitimate monitoring and security research

## Performance

| Mode | Devices/sec | Battery (500mAh) |
|------|-------------|------------------|
| Aggressive | ~15 | ~33 hours |
| Balanced | ~12 | ~62 hours |
| Economy | ~8 | ~100 hours |
