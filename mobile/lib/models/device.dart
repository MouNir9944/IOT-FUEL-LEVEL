class Telemetry {
  final String deviceId;
  final double fuelLevelPct;   // 0–100 %
  final double fuelVolumeL;    // calculated volume in litres
  final double? temperatureC;  // °C (optional)
  final String pumpState;      // 'RUNNING' | 'STOPPED'
  final DateTime timestamp;
  final String controlMode;
  final bool? planActive;
  final String? planId;
  final String? planName;
  final String? sliceStart;
  final String? sliceStop;
  final double alertThresholdPct; // alert when fuel drops below this %

  const Telemetry({
    required this.deviceId,
    required this.fuelLevelPct,
    required this.fuelVolumeL,
    this.temperatureC,
    this.pumpState = 'STOPPED',
    required this.timestamp,
    this.controlMode = 'manual',
    this.planActive,
    this.planId,
    this.planName,
    this.sliceStart,
    this.sliceStop,
    this.alertThresholdPct = 20.0,
  });

  factory Telemetry.fromJson(Map<String, dynamic> j) => Telemetry(
        deviceId:           j['device_id']           as String? ?? '',
        fuelLevelPct:       (j['fuel_level_pct']     as num?)?.toDouble() ?? 0.0,
        fuelVolumeL:        (j['fuel_volume_l']       as num?)?.toDouble() ?? 0.0,
        temperatureC:       (j['temperature_c']       as num?)?.toDouble(),
        pumpState:          j['pump_state']           as String? ?? 'STOPPED',
        timestamp:          DateTime.tryParse(j['timestamp'] as String? ?? '') ?? DateTime.now(),
        controlMode:        j['control_mode']         as String? ?? 'manual',
        planActive:         j['plan_active']          as bool?,
        planId:             j['plan_id']              as String?,
        planName:           j['plan_name']            as String?,
        sliceStart:         j['slice_start']          as String?,
        sliceStop:          j['slice_stop']           as String?,
        alertThresholdPct:  (j['alert_threshold_pct'] as num?)?.toDouble() ?? 20.0,
      );

  Telemetry copyWith({
    double? fuelLevelPct,
    double? fuelVolumeL,
    double? temperatureC,
    String? pumpState,
    String? controlMode,
    bool? planActive,
    String? planId,
    String? planName,
    String? sliceStart,
    String? sliceStop,
    double? alertThresholdPct,
  }) =>
      Telemetry(
        deviceId:          deviceId,
        fuelLevelPct:      fuelLevelPct      ?? this.fuelLevelPct,
        fuelVolumeL:       fuelVolumeL       ?? this.fuelVolumeL,
        temperatureC:      temperatureC      ?? this.temperatureC,
        pumpState:         pumpState         ?? this.pumpState,
        timestamp:         timestamp,
        controlMode:       controlMode       ?? this.controlMode,
        planActive:        planActive        ?? this.planActive,
        planId:            planId            ?? this.planId,
        planName:          planName          ?? this.planName,
        sliceStart:        sliceStart        ?? this.sliceStart,
        sliceStop:         sliceStop         ?? this.sliceStop,
        alertThresholdPct: alertThresholdPct ?? this.alertThresholdPct,
      );

  bool get isAuto      => controlMode.toLowerCase() == 'auto';
  bool get isPumpOn    => pumpState == 'RUNNING';
  bool get isCritical  => fuelLevelPct <= alertThresholdPct;
  bool get isLow       => fuelLevelPct > alertThresholdPct && fuelLevelPct <= alertThresholdPct * 1.5;
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
  final String controlMode;
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
    this.controlMode = 'manual',
    this.swVersion,
    this.hwVersion,
  });

  String get displayName => name ?? deviceId;

  String get effectiveControlMode =>
      lastTelemetry?.controlMode ?? controlMode;

  factory Device.fromJson(Map<String, dynamic> j) {
    Telemetry? tel;
    if (j['last_telemetry'] != null) {
      try {
        tel = Telemetry.fromJson(Map<String, dynamic>.from(j['last_telemetry'] as Map));
      } catch (_) {}
    }
    return Device(
      id:       j['id']?.toString() ?? '',
      deviceId: j['device_id'] as String,
      name:     j['name'] as String?,
      siteId:   j['site_id']?.toString() ?? '',
      siteName: j['site_name'] as String?,
      lastStatus:    j['last_status']    as String? ?? 'unknown',
      lastStatusAt:  j['last_status_at'] != null
          ? DateTime.tryParse(j['last_status_at'] as String)
          : null,
      lastLwtAt: j['last_lwt_at'] != null
          ? DateTime.tryParse(j['last_lwt_at'] as String)
          : null,
      lastLwtPayload: j['last_lwt_payload'] != null
          ? Map<String, dynamic>.from(j['last_lwt_payload'] as Map)
          : null,
      lastTelemetry:   tel,
      lastTelemetryAt: j['last_telemetry_at'] != null
          ? DateTime.tryParse(j['last_telemetry_at'] as String)
          : null,
      alertOnOffline: (j['alert_on_offline'] as bool?) ?? true,
      controlMode:    j['control_mode']    as String? ?? 'manual',
      swVersion:      j['sw_version']      as String?,
      hwVersion:      j['hw_version']      as String?,
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
        status:    j['status'] as String,
        reason:    j['reason'] as String?,
        createdAt: DateTime.tryParse(j['created_at'] as String? ?? '') ?? DateTime.now(),
      );
}
