import { Request, Response } from 'express';
import { z } from 'zod';
import { Device, CommandLog } from '../models/schemas';
import { getMqttClient } from '../services/mqtt.service';
import { getSocketServer } from '../services/socket.service';
import { logger } from '../utils/logger';

// ── Shape name map: mobile app → firmware ─────────────────────────────────────
const SHAPE_TO_FIRMWARE: Record<string, string> = {
  cylindrical_vertical:   'cylinder_vertical',
  cylindrical_horizontal: 'cylinder_horizontal',
  rectangular:            'rectangular',
  cone_vertical:          'cone_vertical',
  ellipse_vertical:       'ellipse_vertical',
  sphere:                 'sphere',
  capsule:                'capsule',
};

// ── Validation schemas ────────────────────────────────────────────────────────

const tankShapeParamsSchema = z.object({
  length_m:   z.number().optional(),
  width_m:    z.number().optional(),
  height_m:   z.number().optional(),
  radius_m:   z.number().optional(),
  radius_b_m: z.number().optional(),
}).optional();

const commandSchema = z.discriminatedUnion('cmd', [
  // ── update_config: native device format (metres, firmware shape names) ──────
  z.object({
    cmd: z.literal('update_config'),
    config: z.object({
      reporting_interval_s: z.number().int().min(5).max(86400).optional(),
      timezone_offset_min:  z.number().int().optional(),
      gps_enabled:          z.boolean().optional(),
      debug_mode:           z.boolean().optional(),
      tank_shape:           z.string().optional(),
      tank_shape_params:    tankShapeParamsSchema,
      tank_sections:        z.array(z.record(z.unknown())).optional(),
    }),
  }),

  // ── set_config: mobile-app format (cm, display shape names) ─────────────────
  // The backend translates this before forwarding to the device.
  z.object({
    cmd: z.literal('set_config'),
    value: z.object({
      shape:                 z.string().optional(),
      height_cm:             z.number().positive().optional(),
      diameter_cm:           z.number().positive().optional(),
      length_cm:             z.number().positive().optional(),
      width_cm:              z.number().positive().optional(),
      sensor_offset_cm:      z.number().min(0).optional(),
      alert_threshold_pct:   z.number().min(0).max(100).optional(),
      temp_alert_c:          z.number().min(0).optional(),
      reporting_interval_s:  z.number().int().min(5).max(86400).optional(),
      timezone_offset_min:   z.number().int().optional(),
      gps_enabled:           z.boolean().optional(),
      debug_mode:            z.boolean().optional(),
    }),
  }),

  // ── ota_update ───────────────────────────────────────────────────────────────
  z.object({
    cmd: z.literal('ota_update'),
    url: z.string().url(),
  }),

  // ── report_config: ask device to publish its stored config ───────────────────
  z.object({
    cmd: z.literal('report_config'),
  }),
]);

// ── Controller ────────────────────────────────────────────────────────────────

export async function sendCommand(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const parsed = commandSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  try {
    const device = await Device.findById(id).select('device_id last_status').lean();
    if (!device) {
      res.status(404).json({ error: 'Device not found' });
      return;
    }

    const mqttClient = getMqttClient();
    if (!mqttClient) {
      res.status(503).json({ error: 'MQTT broker not connected' });
      return;
    }

    const command = parsed.data;
    const mac = device.device_id;

    // ── Route each command type ───────────────────────────────────────────────

    if (command.cmd === 'update_config') {
      // Already in device format — publish directly to device/{MAC}/config
      mqttClient.publish(
        `device/${mac}/config`,
        JSON.stringify(command.config),
        { qos: 1, retain: false },
      );
      logger.info('Config update sent to device', { device: mac, config: command.config });

    } else if (command.cmd === 'set_config') {
      // Translate mobile-app format → firmware format
      const v = command.value;
      const deviceCfg: Record<string, unknown> = {};

      // ── Shape ────────────────────────────────────────────────────────────────
      if (v.shape !== undefined) {
        deviceCfg.tank_shape = SHAPE_TO_FIRMWARE[v.shape] ?? v.shape;
      }

      // ── Dimensions: cm → m, diameter → radius ───────────────────────────────
      const params: Record<string, number> = {};
      if (v.height_cm   !== undefined) params.height_m  = v.height_cm  / 100;
      if (v.diameter_cm !== undefined) params.radius_m  = v.diameter_cm / 200; // ÷2 for radius, ÷100 for m
      if (v.length_cm   !== undefined) params.length_m  = v.length_cm  / 100;
      if (v.width_cm    !== undefined) params.width_m   = v.width_cm   / 100;
      if (Object.keys(params).length > 0) deviceCfg.tank_shape_params = params;

      // ── Other settable fields ────────────────────────────────────────────────
      if (v.reporting_interval_s !== undefined)
        deviceCfg.reporting_interval_s = v.reporting_interval_s;
      if (v.timezone_offset_min !== undefined)
        deviceCfg.timezone_offset_min = v.timezone_offset_min;
      if (v.gps_enabled  !== undefined) deviceCfg.gps_enabled  = v.gps_enabled;
      if (v.debug_mode   !== undefined) deviceCfg.debug_mode   = v.debug_mode;

      // ── App-side-only thresholds: stored on the device doc, not sent to firmware ──
      // (firmware has no alert logic — thresholds are evaluated in the mobile app)
      const dbUpdate: Record<string, unknown> = {};
      if (v.alert_threshold_pct !== undefined)
        dbUpdate['last_telemetry.alert_threshold_pct'] = v.alert_threshold_pct;
      if (v.temp_alert_c !== undefined)
        dbUpdate['last_telemetry.temp_alert_c'] = v.temp_alert_c;
      if (Object.keys(dbUpdate).length > 0) {
        await Device.findByIdAndUpdate(id, dbUpdate);
      }

      mqttClient.publish(
        `device/${mac}/config`,
        JSON.stringify(deviceCfg),
        { qos: 1, retain: false },
      );
      logger.info('set_config translated and sent to device', {
        device: mac,
        mobile: v,
        firmware: deviceCfg,
      });

    } else if (command.cmd === 'ota_update') {
      // Firmware expects JSON {"cmd":"ota_update","url":"..."} on the /ota topic
      mqttClient.publish(
        `device/${mac}/ota`,
        JSON.stringify({ cmd: 'ota_update', url: command.url }),
        { qos: 1, retain: false },
      );
      logger.info('OTA update triggered', { device: mac, url: command.url });

    } else if (command.cmd === 'report_config') {
      // Ask the device to publish its current NVS config to device/{mac}/config_report.
      // Publish to device/{mac}/config — the same channel the firmware already subscribes
      // to for all config updates — because it doesn't subscribe to device/{mac}/cmd.
      // The firmware identifies this as a command (not a config write) by checking for
      // the "cmd" field in the payload.
      mqttClient.publish(
        `device/${mac}/config`,
        JSON.stringify({ cmd: 'report_config' }),
        { qos: 1, retain: false },
      );
      logger.info('report_config request forwarded to device', { device: mac });
    }

    await CommandLog.create({
      device_id: id,
      command,
      source_user_id: req.user!.userId,
      source_type: 'manual',
    });

    const io = getSocketServer();
    if (io) {
      io.to(`device:${mac}`).emit('command_ack', {
        device_id: mac,
        command,
        sent_at: new Date().toISOString(),
      });
    }

    res.json({ message: 'Command sent', command, device_id: mac });
  } catch (err) {
    logger.error('sendCommand error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}
