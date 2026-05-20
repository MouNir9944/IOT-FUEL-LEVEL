import { Router } from 'express';
import { authenticate } from '../middleware/auth.middleware';
import { requireRoles } from '../middleware/roles.middleware';
import { listUsers, createUser, toggleUser, deleteUser } from '../controllers/users.controller';

const router = Router();

router.use(authenticate, requireRoles('admin'));

router.get('/users', listUsers);
router.post('/users', createUser);
router.patch('/users/:id/toggle', toggleUser);
router.delete('/users/:id', deleteUser);

export default router;
