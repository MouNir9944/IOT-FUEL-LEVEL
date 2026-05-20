import { Request, Response } from 'express';
import { z } from 'zod';
import { User } from '../models/schemas';
import { hashPassword } from '../utils/password.utils';
import { logger } from '../utils/logger';

const createUserSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
  full_name: z.string().min(2),
  role: z.enum(['user', 'technician']),
});

export async function listUsers(req: Request, res: Response): Promise<void> {
  try {
    const adminId = req.user!.userId;
    const users = await User.find({
      created_by_admin_id: adminId,
      role: { $in: ['user', 'technician'] },
    })
      .sort({ created_at: -1 })
      .lean({ virtuals: true });

    res.json({ users });
  } catch (err) {
    logger.error('listUsers error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function createUser(req: Request, res: Response): Promise<void> {
  const parsed = createUserSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { email, password, full_name, role } = parsed.data;
  const adminId = req.user!.userId;

  try {
    const existing = await User.findOne({ email });
    if (existing) {
      res.status(409).json({ error: 'Email already registered' });
      return;
    }

    const password_hash = await hashPassword(password);
    const user = await User.create({
      email,
      password_hash,
      role,
      full_name,
      created_by_admin_id: adminId,
    });

    logger.info('User created by admin', { adminId, role });
    res.status(201).json({ user });
  } catch (err) {
    logger.error('createUser error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function toggleUser(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const adminId = req.user!.userId;

  try {
    const user = await User.findOne({
      _id: id,
      created_by_admin_id: adminId,
      role: { $in: ['user', 'technician'] },
    });

    if (!user) {
      res.status(404).json({ error: 'User not found or not owned by this admin' });
      return;
    }

    user.active = !user.active;
    await user.save();

    res.json({ user });
  } catch (err) {
    logger.error('toggleUser error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function deleteUser(req: Request, res: Response): Promise<void> {
  const { id } = req.params;
  const adminId = req.user!.userId;

  try {
    const result = await User.findOneAndDelete({
      _id: id,
      created_by_admin_id: adminId,
      role: { $in: ['user', 'technician'] },
    });

    if (!result) {
      res.status(404).json({ error: 'User not found or not owned by this admin' });
      return;
    }

    res.json({ message: 'User deleted' });
  } catch (err) {
    logger.error('deleteUser error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}
