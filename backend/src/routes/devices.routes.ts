import { Router } from 'express';
import { authenticate } from '../middleware/auth.middleware';
import { requireRoles } from '../middleware/roles.middleware';
import {
  listDevices,
  createDevice,
  getDeviceStatus,
  getOfflineDevices,
  getUnstableDevices,
  ackOffline,
} from '../controllers/devices.controller';
import { sendCommand } from '../controllers/commands.controller';

const router = Router();

router.use(authenticate);

router.get('/', listDevices);
router.post('/', requireRoles('admin'), createDevice);
router.get('/offline', getOfflineDevices);
router.get('/unstable', getUnstableDevices);
router.get('/:id/status', getDeviceStatus);
router.post('/:id/ack_offline', ackOffline);
router.post('/:id/command', requireRoles('user', 'technician', 'admin', 'superadmin'), sendCommand);

export default router;
