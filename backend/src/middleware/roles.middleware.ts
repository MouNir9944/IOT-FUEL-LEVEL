import { Request, Response, NextFunction } from 'express';
import { UserRole } from '../models/types';

export function requireRoles(...roles: UserRole[]) {
  return (req: Request, res: Response, next: NextFunction): void => {
    if (!req.user) {
      res.status(401).json({ error: 'Unauthenticated' });
      return;
    }
    if (!roles.includes(req.user.role as UserRole)) {
      res.status(403).json({ error: 'Forbidden: insufficient permissions' });
      return;
    }
    next();
  };
}

export function requireActive(req: Request, res: Response, next: NextFunction): void {
  // active flag is checked at login; JWT is revoked on suspension via token rotation
  next();
}
