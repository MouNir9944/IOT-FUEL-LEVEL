import { Router } from 'express';
import { authenticate } from '../middleware/auth.middleware';
import { requireRoles } from '../middleware/roles.middleware';
import { listSites, createSite, updateSite, deleteSite } from '../controllers/sites.controller';

const router = Router();

router.use(authenticate);

router.get('/', listSites);
router.post('/', requireRoles('admin'), createSite);
router.put('/:id', requireRoles('admin'), updateSite);
router.delete('/:id', requireRoles('admin'), deleteSite);

export default router;
