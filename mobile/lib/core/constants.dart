import 'package:flutter/material.dart';

class AppColors {
  static const background   = Color(0xFF0F172A);
  static const surface      = Color(0xFF1E293B);
  static const surfaceLight = Color(0xFF334155);
  static const primary      = Color(0xFFF59E0B); // amber — fuel theme
  static const primaryDark  = Color(0xFFD97706);
  static const text         = Color(0xFFF1F5F9);
  static const textMuted    = Color(0xFF94A3B8);
  static const success      = Color(0xFF22C55E);
  static const warning      = Color(0xFFF59E0B);
  static const error        = Color(0xFFEF4444);
  static const info         = Color(0xFF38BDF8);
  static const border       = Color(0xFF334155);

  // Fuel-level gauge colours (green → amber → red as tank empties)
  static const fuelFull     = Color(0xFF22C55E); // > 60 %
  static const fuelMid      = Color(0xFFF59E0B); // 20–60 %
  static const fuelLow      = Color(0xFFEF4444); // < 20 %

  static Color fuelLevelColor(double pct) {
    if (pct > 60) return fuelFull;
    if (pct > 20) return fuelMid;
    return fuelLow;
  }

  static Color statusColor(String status) {
    switch (status) {
      case 'online':   return success;
      case 'offline':  return error;
      case 'unstable': return warning;
      default:         return textMuted;
    }
  }
}

class AppConstants {
  // flutter run -d chrome              → localhost:3000
  // flutter run -d emulator (Android)  → 10.0.2.2:3000
  // flutter run -d <physical device>   → your LAN IP, e.g. 192.168.x.x:3000
  static const apiBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: 'https://iot-fuel-level.onrender.com',
    // Local dev   : --dart-define=API_BASE_URL=http://localhost:3000
    // Android emu : --dart-define=API_BASE_URL=http://10.0.2.2:3000
    // Physical    : --dart-define=API_BASE_URL=http://192.168.x.x:3000
  );
  static const socketUrl = String.fromEnvironment(
    'SOCKET_URL',
    defaultValue: 'https://iot-fuel-level.onrender.com',
  );
}

// Route names
class AppRoutes {
  static const login         = '/login';
  static const register      = '/register';
  static const dashboard     = '/dashboard';
  static const deviceControl = '/device/control';
  static const planning      = '/device/planning';
  static const deviceHistory = '/device/history';
  static const addDevice     = '/admin/add-device';
  static const manageUsers   = '/admin/users';
  static const techLogs      = '/device/tech-logs';
}
