import { Router } from 'express';
import { authenticate } from '../middleware/auth.middleware';
import { requireRoles } from '../middleware/roles.middleware';
import {
  listAdmins,
  suspendAdmin,
  getAdminSites,
  getAllDevices,
  getAllConnectionLogs,
  triggerOta,
  uploadFirmware,
  listFirmwareFiles,
  firmwareUpload,
} from '../controllers/superadmin.controller';

const router = Router();

router.use(authenticate, requireRoles('superadmin'));

router.get('/admins', listAdmins);
router.patch('/admins/:id/suspend', suspendAdmin);
router.get('/admins/:id/sites', getAdminSites);
router.get('/devices/all', getAllDevices);
router.get('/connection-logs', getAllConnectionLogs);
router.post('/devices/:id/ota', triggerOta);

/* ── OTA firmware file management ── */
router.post('/firmware/upload', firmwareUpload.single('firmware'), uploadFirmware);
router.get('/firmware', listFirmwareFiles);

export default router;
