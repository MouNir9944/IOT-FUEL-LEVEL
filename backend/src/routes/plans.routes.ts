import { Router } from 'express';
import { authenticate } from '../middleware/auth.middleware';
import { requireRoles } from '../middleware/roles.middleware';
import { listPlans, createPlan, updatePlan, deletePlan, togglePlan, syncPlansToDevice } from '../controllers/plans.controller';

const router = Router();

const guard = [authenticate, requireRoles('user', 'technician', 'admin', 'superadmin')] as const;

router.get('/devices/:deviceId/plans',  ...guard, listPlans);
router.post('/devices/:deviceId/plans', ...guard, createPlan);
router.put('/plans/:id',                ...guard, updatePlan);
router.delete('/plans/:id',             ...guard, deletePlan);
router.patch('/plans/:id/toggle',          ...guard, togglePlan);
router.post('/devices/:deviceId/plans/sync', ...guard, syncPlansToDevice);

export default router;
