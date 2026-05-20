import { Request, Response } from 'express';
import { z } from 'zod';
import { Types } from 'mongoose';
import { Device, Site, UserSite, DeviceConnectionLog } from '../models/schemas';
import { logger } from '../utils/logger';

const PROJECT_ID = process.env.MQTT_PROJECT_ID || '699700a5d383e0a593047e03';

function buildTopics(deviceId: string) {
  return {
    telemetry: `device/${PROJECT_ID}/${deviceId}/telemetry`,
    logs:      `device/${PROJECT_ID}/${deviceId}/logs`,
    config:    `device/${deviceId}/config`,
    ota:       `device/${deviceId}/ota`,
  };
}

function parseQr(qrData: string): { id?: string; broker?: string } {
  if (qrData.includes('|')) {
    const [id, broker] = qrData.split('|');
    return { id: id?.trim(), broker: broker?.trim() };
  }
  try {
    return JSON.parse(qrData);
  } catch {
    return {};
  }
}

const createDeviceSchema = z.object({
  device_id: z.string().min(1),
  name: z.string().optional(),
  site_id: z.string().min(1),
  qr_data: z.string().optional(),
  mqtt_broker: z.string().optional(),
});

async function getDevicesForUser(userId: string, role: string) {
  if (role === 'superadmin') {
    return Device.find()
      .populate('site_id', 'name')
      .sort({ created_at: -1 })
      .lean({ virtuals: true });
  }

  if (role === 'admin') {
    const sites = await Site.find({ admin_id: userId }).select('_id').lean();
    const siteIds = sites.map((s) => s._id);
    return Device.find({ site_id: { $in: siteIds } })
      .populate('site_id', 'name')
      .sort({ created_at: -1 })
      .lean({ virtuals: true });
  }

  // user / technician
  const userSites = await UserSite.find({ user_id: userId }).lean();
  if (!userSites.length) return [];

  const conditions: Record<string, unknown>[] = [];
  for (const us of userSites) {
    if (!us.device_ids || us.device_ids.length === 0) {
      conditions.push({ site_id: us.site_id });
    } else {
      conditions.push({ _id: { $in: us.device_ids }, site_id: us.site_id });
    }
  }

  return Device.find({ $or: conditions })
    .populate('site_id', 'name')
    .sort({ created_at: -1 })
    .lean({ virtuals: true });
}

function flattenDevice(d: Record<string, unknown>) {
  const siteRef = d.site_id as { _id?: unknown; id?: string; name?: string } | null;
  const rawId   = d._id as Types.ObjectId | string | undefined;

  return {
    ...d,
    id:        typeof rawId === 'string' ? rawId : (rawId?.toString() ?? ''),
    site_name: siteRef?.name ?? null,
    site_id:   siteRef?.id ?? siteRef?._id?.toString() ?? d.site_id,
  };
}

export async function listDevices(req: Request, res: Response): Promise<void> {
  const { userId, role } = req.user!;
  const { site_id } = req.query;

  try {
    let devices = (await getDevicesForUser(userId, role)).map(flattenDevice);

    if (site_id) {
      devices = devices.filter((d) => String(d.site_id) === String(site_id));
    }

    res.json({ devices });
  } catch (err) {
    logger.error('listDevices error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function createDevice(req: Request, res: Response): Promise<void> {
  const parsed = createDeviceSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  let { device_id, name, site_id, qr_data, mqtt_broker } = parsed.data;

  if (qr_data) {
    const pq = parseQr(qr_data);
    if (pq.id) device_id = pq.id;
    if (pq.broker) mqtt_broker = pq.broker;
  }

  const broker = mqtt_broker || process.env.MQTT_BROKER || 'mqtt://localhost:1883';
  const topics = buildTopics(device_id);
  const adminId = req.user!.userId;

  try {
    const siteDoc = await Site.findOne({ _id: site_id, admin_id: adminId });
    if (!siteDoc) {
      res.status(403).json({ error: 'Site not found or not owned by this admin' });
      return;
    }

    const existing = await Device.findOne({ device_id });
    if (existing) {
      res.status(409).json({ error: 'Device ID already exists' });
      return;
    }

    const device = await Device.create({
      device_id,
      name: name ?? device_id,
      site_id,
      qr_data: qr_data ?? null,
      mqtt_broker: broker,
      topics,
    });

    logger.info('Device created', { device_id, site_id });
    res.status(201).json({ device });
  } catch (err) {
    logger.error('createDevice error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getDeviceStatus(req: Request, res: Response): Promise<void> {
  const { id } = req.params;

  try {
    const device = await Device.findById(id)
      .populate('site_id', 'name')
      .lean({ virtuals: true });

    if (!device) {
      res.status(404).json({ error: 'Device not found' });
      return;
    }

    const logs = await DeviceConnectionLog.find({ device_id: id })
      .sort({ created_at: -1 })
      .limit(50)
      .lean({ virtuals: true });

    res.json({ device: flattenDevice(device as Record<string, unknown>), connection_history: logs });
  } catch (err) {
    logger.error('getDeviceStatus error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getOfflineDevices(req: Request, res: Response): Promise<void> {
  const { userId, role } = req.user!;
  try {
    const devices = (await getDevicesForUser(userId, role))
      .map(flattenDevice)
      .filter((d) => (d as Record<string, unknown>).last_status === 'offline');
    res.json({ devices });
  } catch (err) {
    logger.error('getOfflineDevices error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getUnstableDevices(req: Request, res: Response): Promise<void> {
  const { userId, role } = req.user!;
  try {
    const devices = (await getDevicesForUser(userId, role))
      .map(flattenDevice)
      .filter((d) => (d as Record<string, unknown>).last_status === 'unstable');
    res.json({ devices });
  } catch (err) {
    logger.error('getUnstableDevices error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function ackOffline(req: Request, res: Response): Promise<void> {
  logger.info('Offline acknowledged', { deviceId: req.params.id, userId: req.user!.userId });
  res.json({ message: 'Acknowledged' });
}
