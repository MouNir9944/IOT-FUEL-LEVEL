import http from 'http';
import dotenv from 'dotenv';
dotenv.config();

import app from './app';
import { connectDB } from './db';
import { initSocketServer } from './services/socket.service';
import { initMqttClient } from './services/mqtt.service';
import { startPlanningCron } from './services/planning.service';
import { logger } from './utils/logger';

const PORT = Number(process.env.PORT) || 3000;

async function bootstrap(): Promise<void> {
  await connectDB();

  const httpServer = http.createServer(app);

  initSocketServer(httpServer);
  try {
    initMqttClient();
  } catch (err) {
    logger.error('MQTT init failed — continuing without MQTT', { error: (err as Error).message });
  }
  startPlanningCron();

  httpServer.listen(PORT, () => {
    logger.info(`Server running on port ${PORT}`);
  });

  const shutdown = () => {
    logger.info('Shutting down...');
    httpServer.close(() => process.exit(0));
  };

  process.on('SIGTERM', shutdown);
  process.on('SIGINT', shutdown);
}

bootstrap().catch((err) => {
  logger.error('Bootstrap failed', { error: (err as Error).message });
  process.exit(1);
});
