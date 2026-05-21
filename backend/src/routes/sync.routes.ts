import { Router } from 'express';
import multer from 'multer';
import { syncReadings } from '../controllers/sync.controller';

const router = Router();

// Firmware sends multipart/form-data with a single "file" part (readings.json).
// memoryStorage keeps it in req.file.buffer — no disk I/O needed.
const upload = multer({ storage: multer.memoryStorage() });

// POST /api/devices/sync — no auth required, device identifies via MAC
router.post('/sync', upload.single('file'), syncReadings);

export default router;
