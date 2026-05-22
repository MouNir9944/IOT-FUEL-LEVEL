class Telemetry {
  final String deviceId;
  final double fuelLevelPct;      // 0–100 %
  final double fuelVolumeL;       // calculated volume in litres
  final double? temperatureC;     // °C (optional)
  final int?    batteryMv;        // battery voltage in mV (optional)
  final int?    rssi;             // signal strength in dBm (optional)
  final String? networkMode;      // e.g. "LTE", "4G", "GSM" (optional)
  final DateTime timestamp;
  final double alertThresholdPct; // alert when fuel drops below this %
  final double? tempAlertC;       // alert when temperature exceeds this °C

  const Telemetry({
    required this.deviceId,
    required this.fuelLevelPct,
    required this.fuelVolumeL,
    this.temperatureC,
    this.batteryMv,
    this.rssi,
    this.networkMode,
    required this.timestamp,
    this.alertThresholdPct = 20.0,
    this.tempAlertC,
  });

  factory Telemetry.fromJson(Map<String, dynamic> j) => Telemetry(
        deviceId:  j['device_id'] as String? ?? '',
        // Accept both backend-normalised names and raw firmware field names
        fuelLevelPct: ((j['fuel_level_pct'] ?? j['level_pct']) as num?)
                ?.toDouble() ??
            0.0,
        fuelVolumeL: ((j['fuel_volume_l'] ?? j['volume_l']) as num?)
                ?.toDouble() ??
            0.0,
        temperatureC:
            ((j['temperature_c'] ?? j['temp_c']) as num?)?.toDouble(),
        batteryMv:         (j['battery_mv']          as num?)?.toInt(),
        rssi:              (j['rssi']                as num?)?.toInt(),
        networkMode:       j['network_mode']         as String?,
        timestamp:         _parseTimestamp(j),
        alertThresholdPct: (j['alert_threshold_pct'] as num?)?.toDouble() ?? 20.0,
        tempAlertC:        (j['temp_alert_c']         as num?)?.toDouble(),
      );

  /// Handles both ISO-8601 string ("timestamp") and Unix-epoch int ("ts").
  static DateTime _parseTimestamp(Map<String, dynamic> j) {
    final iso = j['timestamp'];
    if (iso is String && iso.isNotEmpty) {
      return DateTime.tryParse(iso) ?? DateTime.now();
    }
    final epoch = j['ts'];
    if (epoch is num) {
      return DateTime.fromMillisecondsSinceEpoch((epoch * 1000).toInt());
    }
    return DateTime.now();
  }

  bool get isCritical => fuelLevelPct <= alertThresholdPct;
  bool get isLow      => fuelLevelPct > alertThresholdPct &&
                         fuelLevelPct <= alertThresholdPct * 1.5;

  /// True when the measured voltage suggests the device is on mains power
  /// (USB/wall adapter typically reads below 2 700 mV on the battery pin).
  bool get isOnMains => batteryMv != null && batteryMv! < 2700;

  /// Battery percentage: 0 % at 2 700 mV, 100 % at 4 500 mV.
  /// Returns null when no voltage is reported.
  /// Returns null (show mains icon instead) when [isOnMains] is true.
  double? get batteryPct {
    if (batteryMv == null) return null;
    if (isOnMains) return null;
    return ((batteryMv! - 2700) / 1800 * 100).clamp(0.0, 100.0);
  }
}

class Device {
  final String id;
  final String deviceId;
  final String? name;
  final String siteId;
  final String? siteName;
  String lastStatus;
  DateTime? lastStatusAt;
  DateTime? lastLwtAt;
  Map<String, dynamic>? lastLwtPayload;
  Telemetry? lastTelemetry;
  DateTime? lastTelemetryAt;
  final bool alertOnOffline;
  final String? swVersion;
  final String? hwVersion;

  Device({
    required this.id,
    required this.deviceId,
    this.name,
    required this.siteId,
    this.siteName,
    this.lastStatus = 'unknown',
    this.lastStatusAt,
    this.lastLwtAt,
    this.lastLwtPayload,
    this.lastTelemetry,
    this.lastTelemetryAt,
    this.alertOnOffline = true,
    this.swVersion,
    this.hwVersion,
  });

  String get displayName => name ?? deviceId;

  factory Device.fromJson(Map<String, dynamic> j) {
    Telemetry? tel;
    if (j['last_telemetry'] != null) {
      try {
        tel = Telemetry.fromJson(
            Map<String, dynamic>.from(j['last_telemetry'] as Map));
      } catch (_) {}
    }
    return Device(
      id:       j['id']?.toString() ?? '',
      deviceId: j['device_id'] as String,
      name:     j['name']      as String?,
      siteId:   j['site_id']?.toString() ?? '',
      siteName: j['site_name'] as String?,
      lastStatus:    j['last_status']    as String? ?? 'unknown',
      lastStatusAt:  j['last_status_at'] != null
          ? DateTime.tryParse(j['last_status_at'] as String) : null,
      lastLwtAt: j['last_lwt_at'] != null
          ? DateTime.tryParse(j['last_lwt_at'] as String) : null,
      lastLwtPayload: j['last_lwt_payload'] != null
          ? Map<String, dynamic>.from(j['last_lwt_payload'] as Map) : null,
      lastTelemetry:   tel,
      lastTelemetryAt: j['last_telemetry_at'] != null
          ? DateTime.tryParse(j['last_telemetry_at'] as String) : null,
      alertOnOffline: (j['alert_on_offline'] as bool?) ?? true,
      swVersion: j['sw_version'] as String?,
      hwVersion: j['hw_version'] as String?,
    );
  }
}

class ConnectionLog {
  final String id;
  final String status;
  final String? reason;
  final DateTime createdAt;

  const ConnectionLog({
    required this.id,
    required this.status,
    this.reason,
    required this.createdAt,
  });

  factory ConnectionLog.fromJson(Map<String, dynamic> j) => ConnectionLog(
        id:        j['id']?.toString() ?? '',
        status:    j['status']    as String,
        reason:    j['reason']    as String?,
        createdAt: DateTime.tryParse(j['created_at'] as String? ?? '') ??
            DateTime.now(),
      );
}
