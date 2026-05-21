import { Request, Response } from 'express';
import { z } from 'zod';
import { Device } from '../models/schemas';
import { getSocketServer } from '../services/socket.service';
import { logger } from '../utils/logger';

// Multer adds req.file when the request is multipart/form-data
declare module 'express' {
  interface Request {
    file?: Express.Multer.File;
  }
}

// ── Telemetry normalisation (mirrors mqtt.service.ts) ─────────────────────────
// The firmware sends compact names (level_pct, volume_l, temp_c, ts …).
// We store and emit the canonical names so the mobile app sees a consistent shape.
function normalizeTelemetry(raw: Record<string, unknown>): Record<string, unknown> {
  let timestamp: string;
  if (typeof raw.timestamp === 'string' && raw.timestamp.length > 0) {
    timestamp = raw.timestamp;
  } else if (typeof raw.ts === 'number') {
    timestamp = new Date(raw.ts * 1000).toISOString();
  } else {
    timestamp = new Date().toISOString();
  }

  return {
    ...raw,
    fuel_level_pct:      (raw.fuel_level_pct ?? raw.level_pct  ?? 0)  as number,
    fuel_volume_l:       (raw.fuel_volume_l  ?? raw.volume_l   ?? 0)  as number,
    temperature_c:       (raw.temperature_c  ?? raw.temp_c)            as number | undefined,
    alert_threshold_pct: (raw.alert_threshold_pct ?? 20)               as number,
    timestamp,
  };
}

// ── Validation ────────────────────────────────────────────────────────────────

const syncSchema = z.object({
  mac:      z.string().min(1),
  readings: z.array(z.record(z.unknown())).min(1).max(256),
});

// ── Controller ────────────────────────────────────────────────────────────────

/**
 * POST /api/devices/sync
 *
 * Called by firmware when MQTT reconnects after an offline period.
 * The device batches all SD-buffered readings into one POST so we can
 * catch up without replaying over MQTT.
 *
 * No JWT required — the device identifies itself via its MAC address.
 *
 * Request body:
 *   { "mac": "1C:69:20:35:13:90", "readings": [ {...telemetry}, ... ] }
 *
 * Response:
 *   { "accepted": N, "duplicates": N, "rejected": N }
 */
export async function syncReadings(req: Request, res: Response): Promise<void> {
  // ── Body extraction ───────────────────────────────────────────────────────
  // The SIM7600 firmware sends multipart/form-data with a single "file" part
  // (name="file", filename="readings.json") that contains the raw JSON.
  // multer populates req.file.buffer for multipart requests.
  // Plain application/json callers (e.g. curl / Postman tests) land in req.body.
  let rawBody: unknown;
  if (req.file?.buffer) {
    try {
      rawBody = JSON.parse(req.file.buffer.toString('utf8'));
    } catch {
      res.status(400).json({ error: 'Invalid JSON in multipart file part' });
      return;
    }
  } else {
    rawBody = req.body;
  }

  const parsed = syncSchema.safeParse(rawBody);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { mac, readings } = parsed.data;

  try {
    const device = await Device.findOne({ device_id: mac })
      .select('last_telemetry_at last_telemetry sw_version hw_version')
      .lean();

    if (!device) {
      logger.warn('syncReadings: unknown device', { mac });
      // Return 200 with all-rejected so the firmware deletes its file rather
      // than retrying forever for a device that will never be registered.
      res.json({ accepted: 0, duplicates: 0, rejected: readings.length });
      return;
    }

    // ── Normalise & sort oldest-first ────────────────────────────────────────
    const normalised = readings.map(r => normalizeTelemetry(r as Record<string, unknown>));
    normalised.sort(
      (a, b) =>
        new Date(a.timestamp as string).getTime() -
        new Date(b.timestamp as string).getTime(),
    );

    // ── Deduplicate against what is already stored ───────────────────────────
    // A reading is a duplicate if its timestamp is at or before the device's
    // current last_telemetry_at.  This handles the common case where the same
    // reading was already delivered over MQTT in a previous session.
    const baseTs = device.last_telemetry_at
      ? new Date(device.last_telemetry_at).getTime()
      : 0;

    let accepted   = 0;
    let duplicates = 0;
    const rejected  = 0; // we never reject valid JSON readings from a known device

    // Track the newest payload in this batch so we only do one DB write
    let newestTs      = baseTs;
    let newestPayload: Record<string, unknown> | null = null;

    const versionFields: Record<string, string> = {};

    const io = getSocketServer();

    for (const payload of normalised) {
      const payloadTs = new Date(payload.timestamp as string).getTime();

      if (payloadTs <= baseTs) {
        duplicates++;
        continue;
      }

      accepted++;

      // Track version strings from any accepted reading
      if (typeof payload.firmware_version === 'string')
        versionFields.sw_version = payload.firmware_version;
      if (typeof payload.hardware_version === 'string')
        versionFields.hw_version = payload.hardware_version;

      // Emit live update for each accepted reading so connected app clients
      // see them stream in if the screen is open during resync
      if (io) {
        io.to(`device:${mac}`).emit('telemetry_update', {
          ...payload,
          device_id: mac,
        });
      }

      if (payloadTs > newestTs) {
        newestTs      = payloadTs;
        newestPayload = payload;
      }
    }

    // ── Persist the newest accepted reading ──────────────────────────────────
    if (newestPayload) {
      await Device.findOneAndUpdate(
        { device_id: mac },
        {
          last_telemetry:    newestPayload,
          last_telemetry_at: new Date(newestPayload.timestamp as string),
          last_status:       'online',
          last_status_at:    new Date(),
          ...versionFields,
        },
      );

      // Notify app that the device came online (if it was showing offline)
      if (io) {
        io.to(`device:${mac}`).emit('device_status_change', {
          device_id: mac,
          status:    'online',
          timestamp: new Date().toISOString(),
        });
      }
    }

    logger.info('HTTP sync processed', { mac, accepted, duplicates, rejected });
    res.json({ accepted, duplicates, rejected });
  } catch (err) {
    logger.error('syncReadings error', { error: (err as Error).message, mac });
    res.status(500).json({ error: 'Internal server error' });
  }
}
