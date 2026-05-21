import { Router } from 'express';
import { syncReadings } from '../controllers/sync.controller';

const router = Router();

// POST /api/devices/sync — no auth required, device identifies via MAC
router.post('/sync', syncReadings);

export default router;
