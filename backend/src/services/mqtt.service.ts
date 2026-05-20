import mqtt, { MqttClient } from 'mqtt';
import { logger } from '../utils/logger';
import { Device } from '../models/schemas';
import { getSocketServer } from './socket.service';
import { TelemetryPayload } from '../models/types';

let client: MqttClient | null = null;

// Firmware topics: device/{PROJECT_ID}/{MAC_ADDRESS}/telemetry|logs
const PROJECT_ID = process.env.MQTT_PROJECT_ID || '699700a5d383e0a593047e03';

const SUBSCRIPTIONS = [
  `device/${PROJECT_ID}/+/telemetry`,
  `device/${PROJECT_ID}/+/logs`,
];

// Extract MAC address (device identifier) from topic position [2]
function extractDeviceId(topic: string): string {
  return topic.split('/')[2];
}

async function handleTelemetry(deviceStrId: string, payload: TelemetryPayload): Promise<void> {
  try {
    const versionFields: Record<string, string> = {};
    if (payload.firmware_version) versionFields.sw_version = payload.firmware_version;
    if (payload.hardware_version) versionFields.hw_version = payload.hardware_version;

    // Return old doc (new: false is Mongoose default) to detect status transitions
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

      // Notify clients if device transitioned from non-online to online
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
    logger.error('handleTelemetry error', { device: deviceStrId, error: (err as Error).message });
  }
}

async function handleDeviceLog(deviceStrId: string, payload: unknown): Promise<void> {
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

function onMessage(topic: string, message: Buffer): void {
  let parsed: unknown;
  try {
    parsed = JSON.parse(message.toString());
  } catch {
    logger.debug('Non-JSON MQTT message', { topic });
    return;
  }

  const deviceStrId = extractDeviceId(topic);

  if (topic.endsWith('/telemetry')) {
    void handleTelemetry(deviceStrId, parsed as TelemetryPayload);
  } else if (topic.endsWith('/logs')) {
    void handleDeviceLog(deviceStrId, parsed);
  }
}

export function initMqttClient(): MqttClient {
  const brokerUrl = process.env.MQTT_BROKER || 'mqtt://localhost:1883';

  if (!/^mqtts?:\/\//i.test(brokerUrl)) {
    throw new Error(`MQTT_BROKER URL is missing a protocol (got: "${brokerUrl}"). Use mqtt:// or mqtts://`);
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
    logger.info('MQTT connected', { broker: brokerUrl, project: PROJECT_ID });
    SUBSCRIPTIONS.forEach((topic) => {
      client!.subscribe(topic, { qos: 1 }, (err) => {
        if (err) logger.error('MQTT subscribe error', { topic, error: err.message });
        else logger.debug('Subscribed', { topic });
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
