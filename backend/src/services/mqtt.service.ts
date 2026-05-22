import mqtt, { MqttClient } from 'mqtt';
import { logger } from '../utils/logger';
import { Device } from '../models/schemas';
import { getSocketServer } from './socket.service';
import { handleOnline, handleLWT } from './lwt.service';

let client: MqttClient | null = null;

// Device firmware publishes to: device/{MAC}/telemetry|logs|status
// (no PROJECT_ID in the path)
const SUBSCRIPTIONS = [
  'device/+/telemetry',
  'device/+/logs',
  'device/+/status',
  'device/+/config_report',   // device publishes current NVS config here after every config write
];

// MAC address is always slot [1]:  device / {MAC} / {suffix}
function extractDeviceId(topic: string): string {
  return topic.split('/')[1];
}

// Returns true only for the MAC-address format our firmware uses (e.g. "1C:69:20:35:13:90").
// Filters out unrelated devices that share the same MQTT broker but publish to the
// same topic pattern (e.g. "smart_relay_773X" sending JSON on device/+/status).
function isProjectDevice(id: string): boolean {
  return /^[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}$/.test(id);
}

// ── Telemetry field normalisation ─────────────────────────────────────────────
// The firmware sends compact field names (level_pct, volume_l, temp_c, ts …).
// We store the canonical names (fuel_level_pct, fuel_volume_l, temperature_c,
// timestamp) so the mobile app and API always see a consistent shape.
function normalizeTelemetry(raw: Record<string, unknown>): Record<string, unknown> {
  // Resolve Unix epoch → ISO-8601 timestamp
  let timestamp: string;
  if (typeof raw.timestamp === 'string' && raw.timestamp.length > 0) {
    timestamp = raw.timestamp;
  } else if (typeof raw.ts === 'number') {
    timestamp = new Date(raw.ts * 1000).toISOString();
  } else {
    timestamp = new Date().toISOString();
  }

  return {
    // preserve everything the firmware sends (useful for raw storage)
    ...raw,

    // canonical names expected by the mobile Telemetry model
    fuel_level_pct:    (raw.fuel_level_pct ?? raw.level_pct  ?? 0)  as number,
    fuel_volume_l:     (raw.fuel_volume_l  ?? raw.volume_l   ?? 0)  as number,
    temperature_c:     (raw.temperature_c  ?? raw.temp_c)            as number | undefined,
    alert_threshold_pct: (raw.alert_threshold_pct ?? 20)             as number,
    timestamp,
  };
}

async function handleTelemetry(
  deviceStrId: string,
  raw: Record<string, unknown>,
): Promise<void> {
  try {
    const payload = normalizeTelemetry(raw);

    const versionFields: Record<string, string> = {};
    if (typeof payload.firmware_version === 'string')
      versionFields.sw_version = payload.firmware_version;
    if (typeof payload.hardware_version === 'string')
      versionFields.hw_version = payload.hardware_version;

    const oldDevice = await Device.findOneAndUpdate(
      { device_id: deviceStrId },
      {
        last_telemetry: payload,
        last_telemetry_at: new Date(),
        last_status: 'online',
        last_status_at: new Date(),
        ...versionFields,
      },
    );

    const io = getSocketServer();
    if (io) {
      io.to(`device:${deviceStrId}`).emit('telemetry_update', {
        ...payload,
        device_id: deviceStrId,
      });

      if (oldDevice && oldDevice.last_status !== 'online') {
        io.to(`device:${deviceStrId}`).emit('device_status_change', {
          device_id: deviceStrId,
          status: 'online',
          timestamp: new Date().toISOString(),
        });
        logger.info('Device came online (telemetry received)', { device: deviceStrId });
      }
    }
  } catch (err) {
    logger.error('handleTelemetry error', {
      device: deviceStrId,
      error: (err as Error).message,
    });
  }
}

// ── Firmware config key normaliser ───────────────────────────────────────────
// The firmware may publish compact keys (interval, tz, gps, debug, shape, h, r…)
// or the full canonical names used by the backend.  We accept both forms.
function normalizeFirmwareConfig(raw: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = { ...raw };

  // compact → canonical
  if ('interval' in raw && !('reporting_interval_s' in raw))
    out.reporting_interval_s = Number(raw.interval);
  if ('tz' in raw && !('timezone_offset_min' in raw))
    out.timezone_offset_min = Number(raw.tz);
  if ('gps' in raw && !('gps_enabled' in raw))
    out.gps_enabled = raw.gps === 1 || raw.gps === true || raw.gps === '1';
  if ('debug' in raw && !('debug_mode' in raw))
    out.debug_mode = raw.debug === 1 || raw.debug === true || raw.debug === '1';
  if ('shape' in raw && !('tank_shape' in raw))
    out.tank_shape = raw.shape;

  // flat dimension fields → tank_shape_params object
  if (!('tank_shape_params' in raw)) {
    const keyMap: Record<string, string> = {
      h: 'height_m', r: 'radius_m', rb: 'radius_b_m', l: 'length_m', w: 'width_m',
    };
    const params: Record<string, number> = {};
    for (const [k, canonical] of Object.entries(keyMap)) {
      if (typeof raw[k] === 'number') params[canonical] = raw[k] as number;
    }
    if (Object.keys(params).length > 0) out.tank_shape_params = params;
  }

  return out;
}

// Handle device/{MAC}/config_report
// Firmware publishes its current NVS config here after every config write.
// We persist it to last_config and relay it to connected app clients.
async function handleConfigReport(
  deviceStrId: string,
  rawConfig: Record<string, unknown>,
): Promise<void> {
  try {
    const normalized = normalizeFirmwareConfig(rawConfig);

    // Persist actual device NVS values; fetch thresholds (app-only, not in firmware NVS).
    const old = await Device.findOneAndUpdate(
      { device_id: deviceStrId },
      { last_config: normalized },
    ).select('last_telemetry').lean();

    const tel        = old?.last_telemetry as Record<string, unknown> | null;
    const alertPct   = (tel?.alert_threshold_pct as number | undefined) ?? 20;
    const tempAlertC = (tel?.temp_alert_c         as number | undefined) ?? 80;

    const io = getSocketServer();
    if (io) {
      io.to(`device:${deviceStrId}`).emit('config_report', {
        device_id: deviceStrId,
        config: {
          ...normalized,
          alert_threshold_pct: alertPct,
          temp_alert_c:        tempAlertC,
          receivedAt: new Date().toISOString(),
        },
      });
      logger.info('Config report received and relayed', { device: deviceStrId });
    }
  } catch (err) {
    logger.error('handleConfigReport error', {
      device: deviceStrId,
      error: (err as Error).message,
    });
  }
}

async function handleDeviceLog(
  deviceStrId: string,
  payload: unknown,
): Promise<void> {
  const io = getSocketServer();
  if (io) {
    const message =
      typeof payload === 'string'
        ? payload
        : typeof payload === 'object'
          ? JSON.stringify(payload)
          : String(payload);

    io.to(`device:${deviceStrId}`).emit('device_log', {
      device_id: deviceStrId,
      log: message,
      timestamp: new Date().toISOString(),
    });
  }
  logger.debug('Device log received', { device: deviceStrId });
}

// Handle device/{MAC}/status  — firmware sends the plain string "online" or
// "offline" (LWT is also "offline").  The broker may or may not wrap the value
// in quotes depending on the SIM7600 AT command implementation, so strip them.
async function handleStatus(deviceStrId: string, raw: string): Promise<void> {
  const status = raw.trim().replace(/^"+|"+$/g, '').toLowerCase();

  if (status === 'online') {
    await handleOnline(deviceStrId, {
      device_id: deviceStrId,
      status: 'online',
      last_seen: new Date().toISOString(),
      rssi: 0,
      battery: 0,
      uptime_seconds: 0,
    });
  } else if (status === 'offline') {
    await handleLWT(deviceStrId, {
      device_id: deviceStrId,
      status: 'offline',
      last_seen: new Date().toISOString(),
      reason: 'unexpected_disconnect',
    });
  } else {
    logger.debug('Unrecognised status payload', { device: deviceStrId, raw });
  }
}


function onMessage(topic: string, message: Buffer): void {
  const deviceStrId = extractDeviceId(topic);

  // Silently ignore messages from devices that don't belong to this project.
  // Any device ID that isn't a colon-separated MAC address is from another system
  // sharing the same broker — processing those messages produces spurious log noise.
  if (!isProjectDevice(deviceStrId)) {
    return;
  }

  if (topic.endsWith('/telemetry')) {
    let parsed: unknown;
    try {
      parsed = JSON.parse(message.toString());
    } catch {
      logger.debug('Non-JSON telemetry message', { topic });
      return;
    }
    void handleTelemetry(deviceStrId, parsed as Record<string, unknown>);
    return;
  }

  if (topic.endsWith('/status')) {
    void handleStatus(deviceStrId, message.toString());
    return;
  }

  if (topic.endsWith('/logs')) {
    let parsed: unknown;
    try {
      parsed = JSON.parse(message.toString());
    } catch {
      parsed = message.toString();
    }
    void handleDeviceLog(deviceStrId, parsed);
    return;
  }

  if (topic.endsWith('/config_report')) {
    let parsed: unknown;
    try {
      parsed = JSON.parse(message.toString());
    } catch {
      logger.debug('Non-JSON config_report message', { topic });
      return;
    }
    void handleConfigReport(deviceStrId, parsed as Record<string, unknown>);
  }
}

export function initMqttClient(): MqttClient {
  const brokerUrl = process.env.MQTT_BROKER || 'mqtt://localhost:1883';

  if (!/^mqtts?:\/\//i.test(brokerUrl)) {
    throw new Error(
      `MQTT_BROKER URL is missing a protocol (got: "${brokerUrl}"). Use mqtt:// or mqtts://`,
    );
  }

  const mqttUser = process.env.MQTT_USERNAME || undefined;
  const mqttPass = process.env.MQTT_PASSWORD || undefined;

  client = mqtt.connect(brokerUrl, {
    clientId: process.env.MQTT_CLIENT_ID || 'fuel-level-backend',
    ...(mqttUser ? { username: mqttUser, password: mqttPass } : {}),
    keepalive: 60,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
    clean: true,
  });

  client.on('connect', () => {
    logger.info('MQTT connected', { broker: brokerUrl });
    SUBSCRIPTIONS.forEach((topic) => {
      client!.subscribe(topic, { qos: 1 }, (err) => {
        if (err) logger.error('MQTT subscribe error', { topic, error: err.message });
        else logger.info('MQTT subscribed', { topic });
      });
    });
  });

  client.on('message', onMessage);
  client.on('error', (err) => logger.error('MQTT error', { error: err.message }));
  client.on('reconnect', () => logger.warn('MQTT reconnecting...'));
  client.on('offline', () => logger.warn('MQTT client offline'));

  return client;
}

export function getMqttClient(): MqttClient | null {
  return client;
}
