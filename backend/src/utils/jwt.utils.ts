import jwt from 'jsonwebtoken';
import crypto from 'crypto';
import { RefreshToken } from '../models/schemas';

const ACCESS_SECRET = process.env.JWT_SECRET!;
const ACCESS_EXPIRES  = process.env.JWT_ACCESS_EXPIRES_IN  || '8h';
const REFRESH_EXPIRES = process.env.JWT_REFRESH_EXPIRES_IN || '30d';

/** Parse a JWT duration string (e.g. "30d", "8h") into milliseconds. */
function parseDurationMs(dur: string): number {
  const unit = dur.slice(-1);
  const val  = parseInt(dur.slice(0, -1), 10);
  if (unit === 'd') return val * 24 * 60 * 60 * 1000;
  if (unit === 'h') return val * 60 * 60 * 1000;
  if (unit === 'm') return val * 60 * 1000;
  return 7 * 24 * 60 * 60 * 1000; // fallback: 7 d
}
const REFRESH_EXPIRES_MS = parseDurationMs(REFRESH_EXPIRES);

export interface JwtPayload {
  userId: string;
  role: string;
  email: string;
}

export function signAccessToken(payload: JwtPayload): string {
  return jwt.sign(payload, ACCESS_SECRET, { expiresIn: ACCESS_EXPIRES } as jwt.SignOptions);
}

export function signRefreshToken(payload: JwtPayload): string {
  return jwt.sign(payload, ACCESS_SECRET, { expiresIn: REFRESH_EXPIRES } as jwt.SignOptions);
}

export function verifyToken(token: string): JwtPayload {
  return jwt.verify(token, ACCESS_SECRET) as JwtPayload;
}

export async function saveRefreshToken(userId: string, token: string): Promise<void> {
  const hash = crypto.createHash('sha256').update(token).digest('hex');
  const expiresAt = new Date(Date.now() + REFRESH_EXPIRES_MS);
  await RefreshToken.create({ user_id: userId, token_hash: hash, expires_at: expiresAt });
}

export async function validateRefreshToken(token: string): Promise<boolean> {
  const hash = crypto.createHash('sha256').update(token).digest('hex');
  const doc = await RefreshToken.findOne({ token_hash: hash, expires_at: { $gt: new Date() } });
  return doc !== null;
}

export async function revokeRefreshToken(token: string): Promise<void> {
  const hash = crypto.createHash('sha256').update(token).digest('hex');
  await RefreshToken.deleteOne({ token_hash: hash });
}

export async function revokeAllUserRefreshTokens(userId: string): Promise<void> {
  await RefreshToken.deleteMany({ user_id: userId });
}
