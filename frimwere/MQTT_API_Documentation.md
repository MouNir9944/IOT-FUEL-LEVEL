# MQTT API Documentation — Fuel Level Monitor (SOJI Sensor)

This document describes the MQTT communication protocol between the ESP32 fuel monitoring device and the web application. It is intended for web/backend developers integrating with the device.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Connection Details](#2-connection-details)
3. [Topic Structure](#3-topic-structure)
4. [Telemetry Payload (Device → Server)](#4-telemetry-payload-device--server)
5. [Configuration Update (Server → Device)](#5-configuration-update-server--device)
6. [OTA Firmware Update (Server → Device)](#6-ota-firmware-update-server--device)
7. [Log Messages (Device → Server)](#7-log-messages-device--server)
8. [Tank Shape Configuration Reference](#8-tank-shape-configuration-reference)
9. [Message Flow Diagrams](#9-message-flow-diagrams)
10. [Offline Behavior & Retry](#10-offline-behavior--retry)
11. [Quick-Start Example (JavaScript)](#11-quick-start-example-javascript)

---

## 1. Overview

The device is an **ESP32** board paired with a **SOJI ultrasonic sensor** that measures fuel level inside a tank. It communicates over a cellular network (SIM7600 modem) to an MQTT broker.

**Responsibilities of each side:**

| Side | Publishes to | Subscribes to |
|------|-------------|---------------|
| Device (ESP32) | `telemetry`, `logs` | `config`, `ota` |
| Web App / Backend | `config`, `ota` | `telemetry`, `logs` |

---

## 2. Connection Details

| Parameter | Value |
|-----------|-------|
| Broker | `broker.hivemq.com` |
| Port | `1883` (plain TCP) |
| Protocol | MQTT v3.1.1 |
| QoS (device publishes) | **1** (at-least-once) |
| Keep-alive | 60 seconds |
| Clean session | `true` |
| Authentication | None (public broker) |
| Will topic | `esp32/sim7000/status` |
| Will message | `"offline"` |

> **Production note:** The broker and credentials are configurable in firmware. Replace `broker.hivemq.com` with your private broker in production.

---

## 3. Topic Structure

All topics follow this pattern:

```
device/{PROJECT_ID}/{MAC_ADDRESS}/{channel}
```

| Placeholder | Description | Example |
|-------------|-------------|---------|
| `PROJECT_ID` | Fixed project identifier | `699700a5d383e0a593047e03` |
| `MAC_ADDRESS` | Device Wi-Fi/BT MAC (unique per unit) | `A4:CF:12:7E:3B:01` |
| `channel` | Message type | `telemetry`, `config`, `ota`, `logs` |

### Full topic list

| Channel | Direction | Full Topic Example |
|---------|-----------|-------------------|
| `telemetry` | Device → Server | `device/699700a5d383e0a593047e03/A4:CF:12:7E:3B:01/telemetry` |
| `config` | Server → Device | `device/A4:CF:12:7E:3B:01/config` |
| `ota` | Server → Device | `device/A4:CF:12:7E:3B:01/ota` |
| `logs` | Device → Server | `device/699700a5d383e0a593047e03/A4:CF:12:7E:3B:01/logs` |

> **Note:** The `telemetry` and `logs` topics include the `PROJECT_ID` segment. The `config` and `ota` subscribe topics use a shorter path without it.

To subscribe to all devices in the project at once use the wildcard:
```
device/699700a5d383e0a593047e03/+/telemetry
```

---

## 4. Telemetry Payload (Device → Server)

**Publish interval:** every **30 seconds** by default (configurable via `config`).
The device only publishes if at least one of `level_cm`, `temp_c`, or `rssi` has changed since the last send.

### Payload — JSON

```json
{
  "level_pct": 45.2,
  "level_cm": 90.4,
  "volume_l": 180.8,
  "capacity_l": 400.0,
  "temp_c": 25,
  "battery_mv": 3850,
  "rssi": -85,
  "firmware_version": "v1.0.0",
  "hardware_version": "v1.0",
  "ts": 1702500123,
  "gps": {
    "lat": 40.7128,
    "lng": -74.0060,
    "alt": 10.5,
    "accuracy": 0.0
  }
}
```

### Field Reference

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `level_pct` | `float` | % | Fuel level as percentage of tank capacity (0–100) |
| `level_cm` | `float` | cm | Absolute fuel height in the tank |
| `volume_l` | `float` | liters | Calculated fuel volume based on tank geometry |
| `capacity_l` | `float` | liters | Total tank capacity |
| `temp_c` | `int` | °C | Fuel temperature measured by the sensor |
| `battery_mv` | `int` | mV | Device battery voltage |
| `rssi` | `int` | dBm | Cellular signal strength (negative; closer to 0 = better) |
| `firmware_version` | `string` | — | Firmware version string |
| `hardware_version` | `string` | — | Hardware version string |
| `ts` | `int` | Unix epoch (s) | Timestamp of the measurement (UTC) |
| `gps.lat` | `float` | degrees | Latitude (WGS84) |
| `gps.lng` | `float` | degrees | Longitude (WGS84) |
| `gps.alt` | `float` | meters | Altitude above sea level |
| `gps.accuracy` | `float` | meters | GPS accuracy estimate (`0.0` if unavailable) |

### Signal Strength Guide

| `rssi` range | Quality |
|--------------|---------|
| > −70 dBm | Excellent |
| −70 to −85 dBm | Good |
| −85 to −100 dBm | Fair |
| < −100 dBm | Poor |

### Level Calculation Notes

The sensor returns a raw 16-bit value (0–65535). The device converts it:

```
level_pct = (raw / 65535) × 100
level_cm  = (raw / 65535) × max_tank_height_cm
volume_l  = calculated using configured tank geometry
```

---

## 5. Configuration Update (Server → Device)

Send a JSON message to the device's `config` topic to update its settings. The device applies the configuration immediately and persists it to flash (survives reboot).

**Topic:** `device/{MAC_ADDRESS}/config`

### Payload — JSON

```json
{
  "reporting_interval_s": 30,
  "timezone_offset_min": -300,
  "gps_enabled": true,
  "debug_mode": false,
  "tank_shape": "rectangular",
  "tank_shape_params": {
    "length_m": 5.0,
    "width_m": 3.0,
    "height_m": 2.0,
    "radius_m": 0.0,
    "radius_b_m": 0.0
  },
  "tank_sections": []
}
```

### Field Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `reporting_interval_s` | `int` | `30` | How often the device publishes telemetry (seconds) |
| `timezone_offset_min` | `int` | `0` | UTC offset in minutes (e.g., UTC−5 = `−300`) |
| `gps_enabled` | `bool` | `true` | Enable/disable GPS readings |
| `debug_mode` | `bool` | `false` | When `true`, device also publishes verbose logs to `logs` topic |
| `tank_shape` | `string` | `"rectangular"` | Tank geometry type (see [Section 8](#8-tank-shape-configuration-reference)) |
| `tank_shape_params` | `object` | — | Dimensions for the selected shape |
| `tank_sections` | `array` | `[]` | Used for multi-section tanks only (see below) |

### `tank_shape_params` Fields

| Field | Unit | Used by shapes |
|-------|------|---------------|
| `length_m` | meters | rectangular, cylinder_horizontal, capsule |
| `width_m` | meters | rectangular |
| `height_m` | meters | rectangular, cylinder_vertical, cone_vertical, ellipse_vertical, capsule |
| `radius_m` | meters | cylinder_*, cone_vertical, ellipse_vertical, sphere, capsule |
| `radius_b_m` | meters | ellipse_vertical (second semi-axis) |

### Partial Updates

You can send only the fields you want to change. Omitted fields retain their current values.

```json
{ "reporting_interval_s": 60 }
```

---

## 6. OTA Firmware Update (Server → Device)

Send an HTTP(S) URL pointing to a `.bin` firmware file. The device downloads and flashes it automatically, then reboots.

**Topic:** `device/{MAC_ADDRESS}/ota`

### Payload — Plain string (URL)

```
https://your-server.com/firmware/v1.2.0.bin
```

> The device will verify the download before applying. If the download fails, the existing firmware remains active.

---

## 7. Log Messages (Device → Server)

When an error occurs or `debug_mode` is `true`, the device publishes log entries.

**Topic:** `device/699700a5d383e0a593047e03/{MAC_ADDRESS}/logs`

### Payload — JSON

```json
{
  "level": "Failed",
  "code": "SENSOR_CRC_ERROR",
  "message": "Checksum mismatch: received 0x05, calculated 0x03",
  "ts": 1702500123
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `level` | `string` | Severity — `"Info"`, `"Warning"`, `"Failed"` |
| `code` | `string` | Machine-readable error code |
| `message` | `string` | Human-readable description |
| `ts` | `int` | Unix timestamp (UTC) |

### Common Error Codes

| Code | Meaning |
|------|---------|
| `SENSOR_CRC_ERROR` | Sensor response failed CRC8 validation |
| `SENSOR_TIMEOUT` | No response from SOJI sensor |
| `MQTT_PUBLISH_FAIL` | Failed to publish; payload saved to SD card |
| `MQTT_CONNECT_FAIL` | Could not connect to MQTT broker |
| `INVALID_JSON` | Received malformed JSON on config/ota topic |
| `OTA_FAIL` | Firmware download or flash failed |
| `SD_SAVE_FAIL` | Could not write failed payload to SD card |

---

## 8. Tank Shape Configuration Reference

The `tank_shape` string in the config payload selects the volume calculation algorithm.

| Value | Shape | Required params |
|-------|-------|----------------|
| `"rectangular"` | Box | `length_m`, `width_m`, `height_m` |
| `"cylinder_vertical"` | Upright cylinder | `radius_m`, `height_m` |
| `"cylinder_horizontal"` | Lying cylinder | `radius_m`, `length_m` |
| `"cone_vertical"` | Vertical cone | `radius_m`, `height_m` |
| `"ellipse_vertical"` | Vertical ellipsoid | `radius_m`, `radius_b_m`, `height_m` |
| `"sphere"` | Sphere | `radius_m` |
| `"capsule"` | Cylinder + hemispherical caps | `radius_m`, `height_m` |
| `"multi_section"` | Compound shape | `tank_sections` array |

### Multi-section tank example

```json
{
  "tank_shape": "multi_section",
  "tank_sections": [
    { "shape": "cylinder_vertical", "height_m": 1.0, "radius_m": 0.5 },
    { "shape": "rectangular",       "height_m": 0.5, "length_m": 1.0, "width_m": 1.0 }
  ]
}
```

Up to **8 sections** are supported. Sections are stacked from bottom to top.

---

## 9. Message Flow Diagrams

### Normal telemetry flow

```
Device                        MQTT Broker                  Web App
  │                               │                            │
  │── publish telemetry ─────────>│                            │
  │   (every 30s, QoS 1)          │── deliver to subscriber ──>│
  │                               │                            │
  │<─────────────── PUBACK ───────│                            │
```

### Remote configuration flow

```
Web App                       MQTT Broker                   Device
  │                               │                            │
  │── publish config ────────────>│                            │
  │   topic: device/{MAC}/config  │── deliver ────────────────>│
  │                               │                            │── apply + save to flash
  │                               │                            │── publish ack on logs (optional)
```

### Offline fallback and retry

```
Device           SD Card               MQTT Broker
  │                 │                       │
  │  [broker down]  │                       │
  │── save JSON ───>│                       │
  │                 │                       │
  │  [broker up]    │                       │
  │<── read file ───│                       │
  │── publish ──────────────────────────────>
  │── delete file ──>│                       │
```

---

## 10. Offline Behavior & Retry

When the cellular connection is unavailable:

1. Failed telemetry payloads are saved to the device's **SD card** as individual JSON files.
2. The payload format on disk wraps the original telemetry:
   ```json
   {
     "timestamp": 1702500123,
     "error_code": -1,
     "payload": "{ ...original telemetry JSON... }"
   }
   ```
3. When connectivity is restored, the device replays all saved payloads to the broker **in order**, using the same `telemetry` topic.
4. Successfully retried files are deleted from the SD card.

**Implication for the web app:** Retried messages may arrive **out of chronological order** relative to their `ts` field. Always use the `ts` field (not arrival time) for ordering.

---

## 11. Quick-Start Example (JavaScript)

Using the [MQTT.js](https://github.com/mqttjs/MQTT.js) library:

```javascript
import mqtt from 'mqtt';

const BROKER      = 'mqtt://broker.hivemq.com:1883';
const PROJECT_ID  = '699700a5d383e0a593047e03';
const DEVICE_MAC  = 'A4:CF:12:7E:3B:01'; // replace with your device MAC

const client = mqtt.connect(BROKER, {
  clientId: 'webapp_' + Math.random().toString(16).slice(2),
  clean: true,
  keepalive: 60,
});

client.on('connect', () => {
  console.log('Connected to broker');

  // Subscribe to telemetry from a specific device
  client.subscribe(`device/${PROJECT_ID}/${DEVICE_MAC}/telemetry`, { qos: 1 });

  // Or subscribe to all devices in the project
  client.subscribe(`device/${PROJECT_ID}/+/telemetry`, { qos: 1 });

  // Subscribe to logs
  client.subscribe(`device/${PROJECT_ID}/${DEVICE_MAC}/logs`, { qos: 1 });
});

client.on('message', (topic, message) => {
  const payload = JSON.parse(message.toString());

  if (topic.endsWith('/telemetry')) {
    console.log(`[${new Date(payload.ts * 1000).toISOString()}]`);
    console.log(`  Level   : ${payload.level_pct.toFixed(1)} %`);
    console.log(`  Volume  : ${payload.volume_l.toFixed(1)} L / ${payload.capacity_l} L`);
    console.log(`  Temp    : ${payload.temp_c} °C`);
    console.log(`  Battery : ${payload.battery_mv} mV`);
    console.log(`  Signal  : ${payload.rssi} dBm`);
  }

  if (topic.endsWith('/logs')) {
    console.warn(`[${payload.level}] ${payload.code}: ${payload.message}`);
  }
});

// --- Send a configuration update ---
function updateConfig(mac, config) {
  const topic = `device/${mac}/config`;
  client.publish(topic, JSON.stringify(config), { qos: 1 });
}

// Example: change reporting interval to 60 seconds
updateConfig(DEVICE_MAC, { reporting_interval_s: 60 });

// Example: set a cylindrical tank (radius 0.5 m, height 2 m)
updateConfig(DEVICE_MAC, {
  tank_shape: 'cylinder_vertical',
  tank_shape_params: { radius_m: 0.5, height_m: 2.0 },
});

// --- Trigger OTA update ---
function triggerOTA(mac, firmwareUrl) {
  client.publish(`device/${mac}/ota`, firmwareUrl, { qos: 1 });
}
```

---

*Document generated from firmware source — `Fuel level with SOJI SENSOR2`*
