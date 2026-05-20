import { Request, Response } from 'express';
import { z } from 'zod';
import { User } from '../models/schemas';
import { hashPassword, comparePassword } from '../utils/password.utils';
import {
  signAccessToken,
  signRefreshToken,
  saveRefreshToken,
  validateRefreshToken,
  revokeRefreshToken,
  verifyToken,
} from '../utils/jwt.utils';
import { logger } from '../utils/logger';

const registerSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
  full_name: z.string().min(2),
});

const loginSchema = z.object({
  email: z.string().email(),
  password: z.string().min(1),
});

export async function register(req: Request, res: Response): Promise<void> {
  const parsed = registerSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { email, password, full_name } = parsed.data;

  try {
    const existing = await User.findOne({ email });
    if (existing) {
      res.status(409).json({ error: 'Email already registered' });
      return;
    }

    const password_hash = await hashPassword(password);
    const user = await User.create({ email, password_hash, role: 'admin', full_name });

    logger.info('Admin registered', { email });
    res.status(201).json({ user });
  } catch (err) {
    logger.error('Register error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function login(req: Request, res: Response): Promise<void> {
  const parsed = loginSchema.safeParse(req.body);
  if (!parsed.success) {
    res.status(400).json({ error: 'Validation failed', details: parsed.error.flatten() });
    return;
  }

  const { email, password } = parsed.data;

  try {
    const user = await User.findOne({ email }).select('+password_hash');

    if (!user) {
      res.status(401).json({ error: 'Invalid credentials' });
      return;
    }

    if (!user.active) {
      res.status(403).json({ error: 'Account suspended' });
      return;
    }

    const valid = await comparePassword(password, user.password_hash);
    if (!valid) {
      res.status(401).json({ error: 'Invalid credentials' });
      return;
    }

    const userId = (user._id as { toString(): string }).toString();
    const payload = { userId, role: user.role, email: user.email };
    const accessToken = signAccessToken(payload);
    const refreshToken = signRefreshToken(payload);
    await saveRefreshToken(userId, refreshToken);

    logger.info('User logged in', { userId, role: user.role });
    res.json({
      accessToken,
      refreshToken,
      user: {
        id: userId,
        email: user.email,
        role: user.role,
        full_name: user.full_name,
        is_primary_admin: user.is_primary_admin,
      },
    });
  } catch (err) {
    logger.error('Login error', { error: (err as Error).message });
    res.status(500).json({ error: 'Internal server error' });
  }
}

export async function refresh(req: Request, res: Response): Promise<void> {
  const { refreshToken } = req.body;
  if (!refreshToken) {
    res.status(400).json({ error: 'refreshToken required' });
    return;
  }

  try {
    const payload = verifyToken(refreshToken);
    const valid = await validateRefreshToken(refreshToken);
    if (!valid) {
      res.status(401).json({ error: 'Invalid or expired refresh token' });
      return;
    }

    await revokeRefreshToken(refreshToken);
    const newPayload = { userId: payload.userId, role: payload.role, email: payload.email };
    const accessToken = signAccessToken(newPayload);
    const newRefreshToken = signRefreshToken(newPayload);
    await saveRefreshToken(payload.userId, newRefreshToken);

    res.json({ accessToken, refreshToken: newRefreshToken });
  } catch {
    res.status(401).json({ error: 'Invalid or expired refresh token' });
  }
}

export async function logout(req: Request, res: Response): Promise<void> {
  const { refreshToken } = req.body;
  if (refreshToken) {
    await revokeRefreshToken(refreshToken).catch(() => null);
  }
  res.json({ message: 'Logged out' });
}
