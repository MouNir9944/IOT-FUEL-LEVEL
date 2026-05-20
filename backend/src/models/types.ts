export type UserRole = 'superadmin' | 'admin' | 'user' | 'technician';
export type DeviceStatus = 'online' | 'offline' | 'unstable' | 'unknown';
export type CommandSource = 'manual' | 'planning' | 'auto';

export interface GpsData {
  lat: number;
  lng: number;
  alt: number;
  accuracy: number;
}

export interface TelemetryPayload {
  level_pct: number;
  level_cm: number;
  volume_l: number;
  capacity_l: number;
  temp_c: number;
  battery_mv: number;
  rssi: number;
  firmware_version?: string;
  hardware_version?: string;
  ts: number;
  gps?: GpsData;
}

export interface StatusPayload {
  device_id: string;
  status: 'online';
  last_seen: string;
  rssi: number;
  battery: number;
  uptime_seconds: number;
  sw_version?: string;
  hw_version?: string;
}

export interface LwtPayload {
  device_id: string;
  status: 'offline';
  last_seen: string;
  reason: 'unexpected_disconnect' | 'network_loss' | 'power_loss' | 'graceful_disconnect';
}

declare global {
  namespace Express {
    interface Request {
      user?: {
        userId: string;
        role: UserRole;
        email: string;
      };
    }
  }
}
