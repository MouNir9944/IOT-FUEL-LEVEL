import { Request, Response } from 'express';
import { z } from 'zod';
import { Device, CommandLog } from '../models/schemas';
import { getMqttClient } from '../services/mqtt.service';
import { getSocketServer } from '../services/socket.service';
import { logger } from '../utils/logger';

const tankShapeParamsSchema = z.object({
  length_m:   z.number().optional(),
  width_m:    z.number().optional(),
  height_m:   z.number().optional(),
  radius_m:   z.number().optional(),
  radius_b_m: z.number().optional(),
}).optional();

const commandSchema = z.discriminatedUnion('cmd', [
  z.object({
    cmd: z.literal('update_config'),
    config: z.object({
      reporting_interval_s: z.number().int().min(5).max(3600).optional(),
      timezone_offset_min:  z.number().int().optional(),
      gps_enabled:          z.boolean().optional(),
      debug_mode:           z.boolean().optional(),
      tank_shape:           z.string().optional(),
      tank_shape_params:    tankShapeParamsSchema,
      tank_sections:        z.array(z.record(z.unknown())).optional(),
    }),
  }),
  z.object({
    cmd: z.literal('ota_update'),
    url: z.string().url(),
  }),
]);

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

    if (command.cmd === 'update_config') {
      // Publish config JSON to device/{MAC}/config
      mqttClient.publish(
        `device/${mac}/config`,
        JSON.stringify(command.config),
        { qos: 1, retain: false },
      );
      logger.info('Config update sent to device', { device: mac, config: command.config });
    } else if (command.cmd === 'ota_update') {
      // Publish firmware URL as plain string to device/{MAC}/ota
      mqttClient.publish(`device/${mac}/ota`, command.url, { qos: 1, retain: false });
      logger.info('OTA update triggered', { device: mac, url: command.url });
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
