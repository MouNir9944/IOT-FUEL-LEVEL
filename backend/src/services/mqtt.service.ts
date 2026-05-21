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
];

// MAC address is always slot [1]:  device / {MAC} / {suffix}
function extractDeviceId(topic: string): string {
  return topic.split('/')[1];
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
