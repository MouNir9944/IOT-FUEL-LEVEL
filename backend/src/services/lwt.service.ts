import { Device, DeviceConnectionLog } from '../models/schemas';
import { logger } from '../utils/logger';
import { getSocketServer } from './socket.service';
import { LwtPayload, StatusPayload } from '../models/types';

export async function handleOnline(deviceStrId: string, payload: StatusPayload): Promise<void> {
  try {
    const versionUpdate: Record<string, string> = {};
    if (payload.sw_version) versionUpdate.sw_version = payload.sw_version;
    if (payload.hw_version) versionUpdate.hw_version = payload.hw_version;

    const device = await Device.findOneAndUpdate(
      { device_id: deviceStrId },
      { last_status: 'online', last_status_at: new Date(), ...versionUpdate },
      { new: true },
    ).select('_id');

    if (!device) return;

    await DeviceConnectionLog.create({
      device_id: device._id,
      status: 'online',
      reason: 'status_update',
      payload,
    });

    const io = getSocketServer();
    if (io) {
      io.to(`device:${deviceStrId}`).emit('device_status_change', {
        device_id: deviceStrId,
        status: 'online',
        timestamp: new Date().toISOString(),
        rssi: payload.rssi,
        battery: payload.battery,
      });
    }

    logger.info('Device came online', { device: deviceStrId });
  } catch (err) {
    logger.error('handleOnline error', { device: deviceStrId, error: (err as Error).message });
  }
}

export async function handleLWT(deviceStrId: string, payload: LwtPayload): Promise<void> {
  try {
    const device = await Device.findOneAndUpdate(
      { device_id: deviceStrId },
      {
        last_status: 'offline',
        last_lwt_at: new Date(),
        last_lwt_payload: payload,
      },
      { new: true },
    ).select('_id alert_on_offline');

    if (!device) return;

    await DeviceConnectionLog.create({
      device_id: device._id,
      status: 'offline',
      reason: payload.reason,
      payload,
    });

    const io = getSocketServer();
    if (io) {
      io.to(`device:${deviceStrId}`).emit('device_status_change', {
        device_id: deviceStrId,
        status: 'offline',
        reason: payload.reason,
        last_seen: payload.last_seen,
        timestamp: new Date().toISOString(),
      });
    }

    logger.warn('Device went offline (LWT)', { device: deviceStrId, reason: payload.reason });
  } catch (err) {
    logger.error('handleLWT error', { device: deviceStrId, error: (err as Error).message });
  }
}

export async function checkUnstableDevices(): Promise<void> {
  const thresholdMinutes = Number(process.env.UNSTABLE_THRESHOLD_MINUTES) || 1;
  const threshold = new Date(Date.now() - thresholdMinutes * 60 * 1000);

  try {
    const unstable = await Device.find({
      last_status: 'online',
      last_telemetry_at: { $lt: threshold, $ne: null },
    }).select('device_id');

    if (!unstable.length) return;

    await Device.updateMany(
      { last_status: 'online', last_telemetry_at: { $lt: threshold, $ne: null } },
      { last_status: 'unstable' },
    );

    const io = getSocketServer();
    for (const d of unstable) {
      logger.warn('Device marked unstable', { device: d.device_id });
      if (io) {
        io.to(`device:${d.device_id}`).emit('device_status_change', {
          device_id: d.device_id,
          status: 'unstable',
          timestamp: new Date().toISOString(),
        });
      }
    }
  } catch (err) {
    logger.error('checkUnstableDevices error', { error: (err as Error).message });
  }
}
