import cron from 'node-cron';
import { RefreshToken } from '../models/schemas';
import { logger } from '../utils/logger';
import { checkUnstableDevices } from './lwt.service';

// Plan execution is now handled entirely by the device-side scheduler.
// The backend sends a fully-resolved plan (with pre-built IR codes) to the
// device via MQTT whenever a plan is created, updated, toggled or deleted
// (see plans.controller.ts → publishPlanToDevice).
// The device stores the plan in SPIFFS and fires IR commands autonomously at
// each slice's start time — no server polling required.

export function startPlanningCron(): void {
  // Check every 30 seconds for devices that stopped sending telemetry.
  // node-cron supports a 6-field expression where the first field is seconds.
  // With a 10 s firmware keepalive the LWT arrives within ~15 s, so the
  // "unstable" window (no telemetry but still technically connected) is caught
  // here with a 60-second threshold (see UNSTABLE_THRESHOLD_MINUTES default).
  cron.schedule('*/30 * * * * *', () => {
    checkUnstableDevices();
  });

  // Clean up expired refresh tokens daily at 03:00
  // (MongoDB TTL index on RefreshToken also handles this, belt-and-suspenders)
  cron.schedule('0 3 * * *', async () => {
    try {
      const result = await RefreshToken.deleteMany({ expires_at: { $lt: new Date() } });
      logger.info('Expired refresh tokens cleaned', { count: result.deletedCount });
    } catch (err) {
      logger.error('Refresh token cleanup error', { error: (err as Error).message });
    }
  });

  logger.info('Planning and monitoring cron jobs started');
}
