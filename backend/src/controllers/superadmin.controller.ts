import { Request, Response } from 'express';
import mongoose from 'mongoose';
import path from 'path';
import fs from 'fs';
import multer from 'multer';
import { User, Site, Device, DeviceConnectionLog } from '../models/schemas';
import { logger } from '../utils/logger';
import { revokeAllUserRefreshTokens } from '../utils/jwt.utils';
import { getMqttClient } from '../services/mqtt.service';

/* ── Multer: store firmware .bin files ── */
const firmwareDir = process.env.NODE_ENV === 'production'
  ? '/tmp/firmware'
  : path.join(__dirname, '..', '..', 'firmware');
if (!fs.existsSync(firmwareDir)) fs.mkdirSync(firmwareDir, { recursive: true });

const storage = multer.diskStorage({
  destination: (_req, _file, cb) => cb(null, firmwareDir),
  filename: (_req, file, cb) => {
    const safe = file.originalname.replace(/[^a-zA-Z0-9._-]/g, '_');
    cb(null, `${Date.now()}_${safe}`);
  },
});

export const firmwareUpload = multer({
  storage,
  limits: { fileSize: 4 * 1024 * 1024 }, // 4 MB max
  fileFilter: (_req, file, cb) => {
    if (file.originalname.endsWith('.bin')) cb(null, true);
    else cb(new Error('Only .bin firmware files are accepted'));
  },
});

export async function listAdmins(req: Request, res: Response): Promise<void> {
  try {
    const admins = await User.aggregate([
      { $match: { role: 'admin' } },
      {
        $lookup: {
          from: 'sites',
          localField: '_id',
          foreignField: 'admin_id',
          as: 'sites',
        },
      },
      {
        $addFields: { site_ids: '$sites._id' },
      },
      {
        $lookup: {
          from: 'devices',
          localField: 'site_ids',
          foreignField: 'site_id',
          as: 'devices',
        },
      },
      {
        $project: {
          id: { $toString: '$_id' },
          email: 1,
          full_name: 1,
          active: 1,
          created_at: 1,
          site_count: { $size: '$sites' },
          device_count: { $size: '$devices' },
        },
      },
      { $sort: { created_at: -1 } },
    ]);

    res.json({ admins });
  } catch (err) {
    logger.error('listAdmins error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function suspendAdmin(req: Request, res: Response): Promise<void> {
  const { id } = req.params;

  try {
    const admin = await User.findOne({ _id: id, role: 'admin' });

    if (!admin) {
      res.status(404).json({ error: 'Admin not found' });
      return;
    }

    admin.active = !admin.active;
    await admin.save();

    if (!admin.active) {
      await revokeAllUserRefreshTokens(id);
    }

    logger.info('Admin suspended/unsuspended', { adminId: id, active: admin.active });
    res.json({ admin });
  } catch (err) {
    logger.error('suspendAdmin error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getAdminSites(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  try {
    const sites = await Site.aggregate([
      { $match: { admin_id: new mongoose.Types.ObjectId(id) } },
      {
        $lookup: {
          from: 'devices',
          localField: '_id',
          foreignField: 'site_id',
          as: 'devices',
        },
      },
      {
        $project: {
          id: { $toString: '$_id' },
          name: 1,
          address: 1,
          device_count: { $size: '$devices' },
          created_at: 1,
        },
      },
      { $sort: { created_at: -1 } },
    ]);
    res.json({ sites });
  } catch (err) {
    logger.error('getAdminSites error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getAllDevices(req: Request, res: Response): Promise<void> {
  try {
    const devices = await Device.find()
      .populate('site_id', 'name admin_id')
      .sort({ created_at: -1 })
      .lean({ virtuals: true });

    const result = devices.map((d) => {
      const site = d.site_id as { name?: string; _id?: unknown } | null;
      return { ...d, site_name: site?.name ?? null };
    });

    res.json({ devices: result });
  } catch (err) {
    logger.error('getAllDevices error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

/**
 * POST /superadmin/devices/:id/ota
 * Body: { url: string }
 *
 * Publishes an MQTT command to the device's cmd topic so it begins an OTA
 * firmware flash from the supplied HTTPS URL.  The device will stream progress
 * back on AC/<device_id>/ota/progress and the final result on
 * AC/<device_id>/ota/result, which the backend forwards as Socket.IO events.
 */
export async function triggerOta(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const { url } = req.body as { url?: string };

  if (!url || typeof url !== 'string' || !url.startsWith('http')) {
    res.status(400).json({ error: 'A valid https:// firmware URL is required' });
    return;
  }

  try {
    const device = await Device.findById(id).select('device_id').lean();
    if (!device) {
      res.status(404).json({ error: 'Device not found' });
      return;
    }

    const mqttClient = getMqttClient();
    if (!mqttClient) {
      res.status(503).json({ error: 'MQTT broker not connected' });
      return;
    }

    const deviceStrId = (device as unknown as { device_id: string }).device_id;
    const payload = JSON.stringify({ cmd: 'ota_update', url });
    mqttClient.publish(`AC/${deviceStrId}/cmd`, payload, { qos: 1, retain: false });

    logger.info('OTA triggered by superadmin', { device: deviceStrId, url });
    res.json({ ok: true, device_id: deviceStrId });
  } catch (err) {
    logger.error('triggerOta error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

/**
 * POST /superadmin/firmware/upload
 * Multipart: field "firmware" must be a .bin file (≤ 4 MB).
 * Returns { url, filename, size } where url is the public download link.
 */
export async function uploadFirmware(req: Request, res: Response): Promise<void> {
  const file = req.file;
  if (!file) {
    res.status(400).json({ error: 'No firmware file received' });
    return;
  }

  /* ── Validate ESP32 firmware binary ──────────────────────────────────────
   * A valid ESP32 application image always starts with magic byte 0xE9.
   * Files like ota_data_initial.bin (OTA metadata) start with 0xFF and are
   * only 8 KB — they will corrupt the OTA partition if flashed via HTTP OTA.
   * Minimum realistic firmware size is ~200 KB; reject anything under 100 KB.
   */
  const MIN_FIRMWARE_SIZE = 100_000; // 100 KB
  const ESP32_MAGIC_BYTE  = 0xe9;

  if (file.size < MIN_FIRMWARE_SIZE) {
    fs.unlinkSync(file.path);
    res.status(400).json({
      error: `Invalid firmware: file is only ${file.size} bytes. ` +
             `Upload Config_CoolC.bin from your build/ directory, not ota_data_initial.bin.`,
    });
    return;
  }

  // Read the first byte to confirm it is a valid ESP32 image header
  let magicByte: number;
  try {
    const buf = Buffer.alloc(1);
    const fd  = fs.openSync(file.path, 'r');
    fs.readSync(fd, buf, 0, 1, 0);
    fs.closeSync(fd);
    magicByte = buf[0];
  } catch (err) {
    fs.unlinkSync(file.path);
    res.status(500).json({ error: 'Could not read uploaded file for validation.' });
    return;
  }

  if (magicByte !== ESP32_MAGIC_BYTE) {
    fs.unlinkSync(file.path);
    res.status(400).json({
      error: `Invalid firmware: first byte is 0x${magicByte.toString(16).toUpperCase().padStart(2, '0')}, ` +
             `expected 0xE9 (ESP32 image magic). ` +
             `Upload Config_CoolC.bin from your build/ directory, not ota_data_initial.bin.`,
    });
    return;
  }

  const baseUrl =
    process.env.SERVER_BASE_URL ||
    `http://localhost:${process.env.PORT ?? 3000}`;

  const url = `${baseUrl}/firmware/${file.filename}`;
  logger.info('Firmware uploaded', { filename: file.filename, size: file.size, magic: `0x${magicByte.toString(16)}` });
  res.json({ url, filename: file.filename, size: file.size });
}

/**
 * GET /superadmin/firmware
 * Lists all firmware files stored on the server.
 */
export async function listFirmwareFiles(_req: Request, res: Response): Promise<void> {
  try {
    const files = fs.readdirSync(firmwareDir).map((name) => {
      const stat = fs.statSync(path.join(firmwareDir, name));
      const baseUrl =
        process.env.SERVER_BASE_URL ||
        `http://localhost:${process.env.PORT ?? 3000}`;
      return { filename: name, size: stat.size, url: `${baseUrl}/firmware/${name}` };
    });
    res.json({ files });
  } catch (err) {
    logger.error('listFirmwareFiles error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function getAllConnectionLogs(req: Request, res: Response): Promise<void> {
  const limit = Math.min(Number(req.query.limit) || 100, 500);
  const offset = Number(req.query.offset) || 0;

  try {
    const logs = await DeviceConnectionLog.find()
      .sort({ created_at: -1 })
      .skip(offset)
      .limit(limit)
      .populate({
        path: 'device_id',
        select: 'device_id site_id',
        populate: { path: 'site_id', select: 'name' },
      })
      .lean({ virtuals: true });

    const result = logs.map((l) => {
      const dev = l.device_id as { device_id?: string; site_id?: { name?: string } } | null;
      return {
        ...l,
        device_str_id: dev?.device_id ?? '',
        site_name: dev?.site_id?.name ?? '',
      };
    });

    res.json({ logs: result });
  } catch (err) {
    logger.error('getAllConnectionLogs error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}
