import express from 'express';
import cors from 'cors';
import rateLimit from 'express-rate-limit';
import dotenv from 'dotenv';
import path from 'path';

dotenv.config();

import authRoutes from './routes/auth.routes';
import adminRoutes from './routes/admin.routes';
import sitesRoutes from './routes/sites.routes';
import devicesRoutes from './routes/devices.routes';
import plansRoutes from './routes/plans.routes';
import superadminRoutes from './routes/superadmin.routes';
import { logger } from './utils/logger';

const app = express();

app.use(cors({ origin: process.env.CORS_ORIGIN || '*' }));
app.use(express.json());

// Global rate limiter
app.use(
  rateLimit({
    windowMs: 60 * 1000,
    max: 100,
    standardHeaders: true,
    legacyHeaders: false,
    message: { error: 'Too many requests, please try again later.' },
  }),
);

// Health check
app.get('/health', (_req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Serve uploaded firmware files publicly (no auth required — device fetches directly)
app.use('/firmware', express.static(path.join(__dirname, '..', 'firmware')));

// Routes
app.use('/auth', authRoutes);
app.use('/admin', adminRoutes);
app.use('/sites', sitesRoutes);
app.use('/devices', devicesRoutes);
app.use('/superadmin', superadminRoutes);
app.use('/', plansRoutes);

// 404
app.use((_req, res) => {
  res.status(404).json({ error: 'Route not found' });
});

// Global error handler
app.use((err: Error, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
  logger.error('Unhandled error', { error: err.message, stack: err.stack });
  res.status(500).json({ error: 'Internal server error' });
});

export default app;
