import 'dart:async';
import 'dart:math' as math;
import 'package:dio/dio.dart';
import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/app_strings.dart';
import '../core/constants.dart';
import '../core/socket_service.dart';
import '../models/device.dart';
import '../widgets/status_badge.dart';

// ── Log entry ─────────────────────────────────────────────────────────────────

class _LogEntry {
  final String message;
  final DateTime time;
  _LogEntry({required this.message, required this.time});
}

// ── Shape metadata ────────────────────────────────────────────────────────────

class _Shape {
  final String id;   // firmware key
  final String name;
  final String desc;
  const _Shape(this.id, this.name, this.desc);
}

const _kShapes = <_Shape>[
  _Shape('rectangular',         'Rectangular',       'L × W × H'),
  _Shape('cylinder_vertical',   'Vert. Cylinder',    'Upright · r × H'),
  _Shape('cylinder_horizontal', 'Horiz. Cylinder',   'Lying · r × L'),
  _Shape('cone_vertical',       'Vertical Cone',     '¹⁄₃ π r² H'),
  _Shape('ellipse_vertical',    'Elliptic',          'Oval base'),
  _Shape('sphere',              'Sphere',            '⁴⁄₃ π r³'),
  _Shape('capsule',             'Capsule',           'Cyl. + 2 hemispheres'),
  _Shape('multi_section',       'Multi-section',     'Sum of sections'),
];

// ── Reporting interval presets ─────────────────────────────────────────────────
const _kIntervals = <int>[10, 30, 60, 300, 900, 3600];
String _fmtInterval(int s) {
  if (s < 60)   return '${s}s';
  if (s < 3600) return '${s ~/ 60}m';
  return '${s ~/ 3600}h';
}

// ── Main screen ───────────────────────────────────────────────────────────────

class DeviceControlScreen extends StatefulWidget {
  final Device device;
  final bool isAdmin;

  const DeviceControlScreen({
    super.key,
    required this.device,
    this.isAdmin = false,
  });

  @override
  State<DeviceControlScreen> createState() => _DeviceControlScreenState();
}

class _DeviceControlScreenState extends State<DeviceControlScreen>
    with TickerProviderStateMixin {
  late Device _device;
  Telemetry? _telemetry;
  String _status = 'unknown';
  late TabController _tabs;
  late AnimationController _fillAnim;
  late AnimationController _waveAnim;

  // ── Config-sync state ──────────────────────────────────────────────────────
  bool _configLoading = false;
  bool _configLoaded  = false;
  DateTime? _configSyncedAt;
  Timer? _configTimeoutTimer;

  // ── Tank config form state (firmware format: metres + firmware shape names) ─
  String _shape = 'cylinder_vertical';
  final _heightCtrl  = TextEditingController(text: '1.0');
  final _radiusCtrl  = TextEditingController(text: '0.5');
  final _radiusBCtrl = TextEditingController(text: '0.4'); // ellipse minor axis
  final _lengthCtrl  = TextEditingController(text: '2.0');
  final _widthCtrl   = TextEditingController(text: '1.5');
  final _thresholdCtrl = TextEditingController(text: '20');
  int  _reportingIntervalS = 30;
  int  _timezoneOffsetMin  = 0;
  bool _gpsEnabled = true;
  bool _debugMode  = false;
  bool _sending    = false;

  // ── Logs tab state ─────────────────────────────────────────────────────────
  final List<_LogEntry> _logs = [];

  // ── OTA tab state ──────────────────────────────────────────────────────────
  final _otaUrlCtrl = TextEditingController();
  bool   _otaSending = false;
  String? _otaStatus;

  @override
  void initState() {
    super.initState();
    _device    = widget.device;
    _status    = _device.lastStatus;
    _telemetry = _device.lastTelemetry;

    _tabs     = TabController(length: 4, vsync: this)
      ..addListener(_onTabChanged);
    _fillAnim = AnimationController(
        vsync: this, duration: const Duration(milliseconds: 1400))
      ..forward();
    _waveAnim = AnimationController(
        vsync: this, duration: const Duration(seconds: 2))
      ..repeat();

    _loadStatus();
    _initSocket();
  }

  @override
  void dispose() {
    _tabs.dispose();
    _fillAnim.dispose();
    _waveAnim.dispose();
    _heightCtrl.dispose();
    _radiusCtrl.dispose();
    _radiusBCtrl.dispose();
    _lengthCtrl.dispose();
    _widthCtrl.dispose();
    _thresholdCtrl.dispose();
    _otaUrlCtrl.dispose();
    _configTimeoutTimer?.cancel();
    SocketService.off('telemetry_update',     id: 'device_control');
    SocketService.off('device_status_change', id: 'device_control');
    SocketService.off('config_report',        id: 'device_control_cfg');
    SocketService.off('device_log',           id: 'device_control_logs');
    SocketService.removeReconnectCallback(_onSocketReconnect);
    super.dispose();
  }

  // ── Socket ────────────────────────────────────────────────────────────────

  void _initSocket() {
    SocketService.addReconnectCallback(_onSocketReconnect);

    SocketService.on('telemetry_update', (data) {
      if (!mounted) return;
      final m = Map<String, dynamic>.from(data as Map);
      if (m['device_id'] != _device.deviceId) return;
      setState(() {
        _telemetry = Telemetry.fromJson(m);
        _fillAnim..reset()..forward();
      });
    }, id: 'device_control');

    SocketService.on('device_status_change', (data) {
      if (!mounted) return;
      final m = Map<String, dynamic>.from(data as Map);
      if (m['device_id'] != _device.deviceId) return;
      setState(() => _status = m['status'] as String? ?? _status);
    }, id: 'device_control');

    SocketService.on('config_report', (data) {
      if (!mounted) return;
      final m = Map<String, dynamic>.from(data as Map);
      if (m['device_id'] != _device.deviceId) return;
      final cfg = m['config'];
      if (cfg is Map) _applyConfigReport(Map<String, dynamic>.from(cfg));
    }, id: 'device_control_cfg');

    // Live device logs
    SocketService.on('device_log', (data) {
      if (!mounted) return;
      final m = Map<String, dynamic>.from(data as Map);
      if (m['device_id'] != _device.deviceId) return;
      final msg = m['log']?.toString() ?? '';
      setState(() {
        _logs.insert(0, _LogEntry(message: msg, time: DateTime.now()));
        if (_logs.length > 300) _logs.removeLast();
      });
    }, id: 'device_control_logs');
  }

  void _onSocketReconnect() =>
      SocketService.subscribeToDevice(_device.deviceId);

  Future<void> _loadStatus() async {
    try {
      final resp = await ApiClient.instance.get('/devices/${_device.id}/status');
      if (!mounted) return;
      final d = Device.fromJson(
          Map<String, dynamic>.from(resp.data['device'] as Map));
      setState(() {
        _device    = d;
        _status    = d.lastStatus;
        _telemetry = d.lastTelemetry;
        _fillAnim..reset()..forward();
        // Pre-fill alert threshold from last known telemetry
        if (d.lastTelemetry != null) {
          _thresholdCtrl.text =
              d.lastTelemetry!.alertThresholdPct.toStringAsFixed(0);
        }
      });
      SocketService.subscribeToDevice(_device.deviceId);
    } catch (_) {}
  }

  // ── Config sync ───────────────────────────────────────────────────────────

  void _onTabChanged() {
    if (!_tabs.indexIsChanging &&
        _tabs.index == 1 &&
        !_configLoaded &&
        !_configLoading) {
      _requestDeviceConfig();
    }
  }

  Future<void> _requestDeviceConfig() async {
    if (_configLoading) return;
    setState(() { _configLoading = true; _configLoaded = false; });
    try {
      await ApiClient.instance.post(
        '/devices/${_device.id}/command',
        data: {'cmd': 'report_config'},
      );
      _configTimeoutTimer?.cancel();
      _configTimeoutTimer = Timer(const Duration(seconds: 10), () {
        if (mounted && _configLoading) setState(() => _configLoading = false);
      });
    } on DioException catch (_) {
      if (mounted) setState(() => _configLoading = false);
    }
  }

  /// Populates the form directly from the firmware-format config map.
  /// No unit conversion needed — device uses metres, form uses metres.
  void _applyConfigReport(Map<String, dynamic> config) {
    _configTimeoutTimer?.cancel();

    final shape = config['tank_shape'] as String?;
    if (shape != null) _shape = shape;

    final p = config['tank_shape_params'];
    if (p is Map) {
      _setCtrl(_heightCtrl,  (p['height_m']   as num?)?.toDouble());
      _setCtrl(_radiusCtrl,  (p['radius_m']   as num?)?.toDouble());
      _setCtrl(_radiusBCtrl, (p['radius_b_m'] as num?)?.toDouble());
      _setCtrl(_lengthCtrl,  (p['length_m']   as num?)?.toDouble());
      _setCtrl(_widthCtrl,   (p['width_m']    as num?)?.toDouble());
    }

    final interval = (config['reporting_interval_s'] as num?)?.toInt();
    if (interval != null) _reportingIntervalS = interval;
    final tz = (config['timezone_offset_min'] as num?)?.toInt();
    if (tz != null) _timezoneOffsetMin = tz;
    final gps = config['gps_enabled'] as bool?;
    if (gps != null) _gpsEnabled = gps;
    final dbg = config['debug_mode'] as bool?;
    if (dbg != null) _debugMode = dbg;

    final alertPct = (config['alert_threshold_pct'] as num?)?.toDouble();
    if (alertPct != null) _thresholdCtrl.text = alertPct.toStringAsFixed(0);

    setState(() {
      _configLoaded   = true;
      _configLoading  = false;
      _configSyncedAt = DateTime.now();
    });
  }

  void _setCtrl(TextEditingController ctrl, double? val) {
    if (val == null) return;
    ctrl.text = (val == val.roundToDouble())
        ? val.toStringAsFixed(1)
        : val.toStringAsFixed(4).replaceAll(RegExp(r'0+$'), '');
  }

  // ── Config send ───────────────────────────────────────────────────────────

  Future<void> _sendConfig() async {
    if (_sending) return;
    final s = AppStrings.read(context);

    // Build tank_shape_params in metres (native firmware format)
    final params = <String, double>{};
    final h  = double.tryParse(_heightCtrl.text.trim());
    final r  = double.tryParse(_radiusCtrl.text.trim());
    final rB = double.tryParse(_radiusBCtrl.text.trim());
    final l  = double.tryParse(_lengthCtrl.text.trim());
    final w  = double.tryParse(_widthCtrl.text.trim());

    switch (_shape) {
      case 'rectangular':
        if (l != null) params['length_m'] = l;
        if (w != null) params['width_m']  = w;
        if (h != null) params['height_m'] = h;
      case 'cylinder_vertical':
      case 'cone_vertical':
        if (r != null) params['radius_m'] = r;
        if (h != null) params['height_m'] = h;
      case 'cylinder_horizontal':
      case 'capsule':
        if (r != null) params['radius_m'] = r;
        if (l != null) params['length_m'] = l;
      case 'ellipse_vertical':
        if (r  != null) params['radius_m']   = r;
        if (rB != null) params['radius_b_m'] = rB;
        if (h  != null) params['height_m']   = h;
      case 'sphere':
        if (r != null) params['radius_m'] = r;
      // multi_section: no dimension fields — send shape name only
    }

    final firmwareCfg = <String, dynamic>{
      'tank_shape': _shape,
      if (params.isNotEmpty) 'tank_shape_params': params,
      'reporting_interval_s': _reportingIntervalS,
      'timezone_offset_min':  _timezoneOffsetMin,
      'gps_enabled': _gpsEnabled,
      'debug_mode':  _debugMode,
    };

    final alertPct = double.tryParse(_thresholdCtrl.text.trim());

    setState(() => _sending = true);
    try {
      // 1. Send firmware config (update_config = native device format)
      await ApiClient.instance.post(
        '/devices/${_device.id}/command',
        data: {'cmd': 'update_config', 'config': firmwareCfg},
      );

      // 2. Update alert_threshold_pct in DB (app-side only, not in firmware)
      if (alertPct != null) {
        await ApiClient.instance.post(
          '/devices/${_device.id}/command',
          data: {'cmd': 'set_config', 'value': {'alert_threshold_pct': alertPct}},
        );
      }

      if (!mounted) return;
      _snack(s.configSent);
      // Device is saving to NVS; clear loaded flag so next tab visit re-syncs
      setState(() { _configLoaded = false; });
    } on DioException catch (e) {
      if (!mounted) return;
      _snack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  void _snack(String msg, {bool isError = false}) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: isError ? AppColors.error : AppColors.success,
      behavior: SnackBarBehavior.floating,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
    ));
  }

  // ── OTA send ─────────────────────────────────────────────────────────────

  Future<void> _sendOta() async {
    final url = _otaUrlCtrl.text.trim();
    if (url.isEmpty) {
      _snack('Enter a firmware URL first', isError: true);
      return;
    }
    setState(() { _otaSending = true; _otaStatus = null; });
    try {
      await ApiClient.instance.post(
        '/devices/${_device.id}/command',
        data: {'cmd': 'ota_update', 'url': url},
      );
      if (!mounted) return;
      setState(() {
        _otaStatus = '✓  OTA request sent. Device will download, flash, and reboot.';
      });
      _snack('OTA update sent to device');
    } on DioException catch (e) {
      if (!mounted) return;
      final msg = ApiClient.errorMessage(e);
      setState(() { _otaStatus = '✗  $msg'; });
      _snack(msg, isError: true);
    } finally {
      if (mounted) setState(() => _otaSending = false);
    }
  }

  // ── Capacity calculator ───────────────────────────────────────────────────

  double _calcCapacityL() {
    final h  = double.tryParse(_heightCtrl.text)  ?? 0;
    final r  = double.tryParse(_radiusCtrl.text)  ?? 0;
    final rB = double.tryParse(_radiusBCtrl.text) ?? 0;
    final l  = double.tryParse(_lengthCtrl.text)  ?? 0;
    final w  = double.tryParse(_widthCtrl.text)   ?? 0;
    const pi = math.pi;
    switch (_shape) {
      case 'rectangular':         return l * w * h * 1000;
      case 'cylinder_vertical':   return pi * r * r * h * 1000;
      case 'cylinder_horizontal': return pi * r * r * l * 1000;
      case 'cone_vertical':       return (1 / 3) * pi * r * r * h * 1000;
      case 'ellipse_vertical':    return pi * r * rB * h * 1000;
      case 'sphere':              return (4 / 3) * pi * r * r * r * 1000;
      case 'capsule':
        return (pi * r * r * l + (4 / 3) * pi * r * r * r) * 1000;
      default:                    return 0;
    }
  }

  // ── Build ─────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    final s = AppStrings.of(context);
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        backgroundColor: AppColors.surface,
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(_device.displayName,
                style: const TextStyle(
                    color: AppColors.text,
                    fontSize: 16,
                    fontWeight: FontWeight.w700)),
            StatusBadge(status: _status),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh_rounded, color: AppColors.textMuted),
            onPressed: _loadStatus,
          ),
        ],
        bottom: TabBar(
          controller: _tabs,
          labelColor: Colors.white,
          unselectedLabelColor: AppColors.textMuted,
          indicatorColor: AppColors.primary,
          isScrollable: true,
          tabAlignment: TabAlignment.start,
          labelPadding:
              const EdgeInsets.symmetric(horizontal: 18),
          tabs: [
            Tab(icon: const Icon(Icons.water_drop_rounded, size: 18),
                text: s.fuelLevel),
            Tab(icon: const Icon(Icons.settings_rounded, size: 18),
                text: s.tankConfig),
            const Tab(
                icon: Icon(Icons.article_outlined, size: 18),
                text: 'Logs'),
            const Tab(
                icon: Icon(Icons.system_update_rounded, size: 18),
                text: 'OTA'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [_monitorTab(s), _configTab(s), _logsTab(), _otaTab()],
      ),
    );
  }

  // ── Tab 1 — Live monitoring ───────────────────────────────────────────────

  Widget _monitorTab(AppStrings s) {
    final t     = _telemetry;
    final pct   = (t?.fuelLevelPct ?? 0.0).clamp(0.0, 100.0);
    final color = AppColors.fuelLevelColor(pct);

    return RefreshIndicator(
      onRefresh: _loadStatus,
      color: AppColors.primary,
      child: ListView(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 24),
        children: [
          // Tank visualisation card
          _Card(
            child: Column(
              children: [
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 5),
                  decoration: BoxDecoration(
                    color: AppColors.surfaceLight,
                    borderRadius: BorderRadius.circular(20),
                  ),
                  child: Row(mainAxisSize: MainAxisSize.min, children: [
                    const Icon(Icons.local_gas_station_rounded,
                        color: AppColors.textMuted, size: 13),
                    const SizedBox(width: 6),
                    Text(_shapeName(_shape),
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 12)),
                  ]),
                ),
                const SizedBox(height: 20),
                AnimatedBuilder(
                  animation: Listenable.merge([_fillAnim, _waveAnim]),
                  builder: (_, __) {
                    final fillRatio =
                        (pct / 100.0) * _fillAnim.value.clamp(0.0, 1.0);
                    return _TankWidget(
                      pct: pct,
                      volumeL: t?.fuelVolumeL,
                      fillRatio: fillRatio,
                      wavePhase: _waveAnim.value * 2 * math.pi,
                      color: color,
                      shape: _shape,
                    );
                  },
                ),
                const SizedBox(height: 14),
                if (t != null)
                  Text('${s.lastSeen}: ${_timeAgo(t.timestamp)}',
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
              ],
            ),
          ),

          const SizedBox(height: 12),

          // Metrics grid
          GridView.count(
            crossAxisCount: 2,
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            crossAxisSpacing: 10,
            mainAxisSpacing: 10,
            childAspectRatio: 2.2,
            children: [
              _MetricTile(
                label: s.fuelVolume,
                value: t != null ? '${t.fuelVolumeL.toStringAsFixed(1)} L' : '—',
                icon: Icons.opacity_rounded,
                color: AppColors.info,
              ),
              _MetricTile(
                label: s.temperature,
                value: t?.temperatureC != null
                    ? '${t!.temperatureC!.toStringAsFixed(1)} °C' : '—',
                icon: Icons.thermostat_rounded,
                color: (t?.temperatureC ?? 0) > 50
                    ? AppColors.error : AppColors.warning,
              ),
              _MetricTile(
                label: s.batteryLevel,
                value: t?.batteryPct != null
                    ? '${t!.batteryPct!.toStringAsFixed(0)} %' : '—',
                icon: Icons.battery_charging_full_rounded,
                color: (t?.batteryPct ?? 100) < 20
                    ? AppColors.error : AppColors.success,
              ),
              _MetricTile(
                label: s.signalStrength,
                value: t?.rssi != null ? '${t!.rssi} dBm' : '—',
                icon: Icons.signal_wifi_4_bar_rounded,
                color: (t?.rssi ?? 0) < -80
                    ? AppColors.error : AppColors.success,
              ),
            ],
          ),

          const SizedBox(height: 12),

          // Alert card
          if (t != null)
            _Card(
              child: Row(children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: t.isCritical
                        ? AppColors.fuelLow.withOpacity(0.15)
                        : AppColors.success.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Icon(
                    t.isCritical
                        ? Icons.warning_amber_rounded
                        : Icons.check_circle_outline_rounded,
                    color: t.isCritical ? AppColors.fuelLow : AppColors.success,
                    size: 20,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        t.isCritical
                            ? 'LOW FUEL — below ${t.alertThresholdPct.toStringAsFixed(0)}%'
                            : t.isLow
                                ? 'LOW FUEL — consider refilling soon'
                                : 'Fuel level OK',
                        style: TextStyle(
                          color: t.isCritical
                              ? AppColors.fuelLow
                              : t.isLow ? AppColors.fuelMid : AppColors.success,
                          fontWeight: FontWeight.w700,
                          fontSize: 13,
                        ),
                      ),
                      Text(
                        '${s.alertThreshold}: ${t.alertThresholdPct.toStringAsFixed(0)}%',
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 11),
                      ),
                    ],
                  ),
                ),
              ]),
            ),
        ],
      ),
    );
  }

  // ── Tab 2 — Tank configuration ────────────────────────────────────────────

  Widget _configTab(AppStrings s) {
    final capL = _calcCapacityL();

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Sync status
          _ConfigSyncBar(
            loading:   _configLoading,
            loaded:    _configLoaded,
            syncedAt:  _configSyncedAt,
            onRefresh: _configLoading ? null : () {
              setState(() { _configLoaded = false; });
              _requestDeviceConfig();
            },
          ),
          const SizedBox(height: 14),

          // ── Card 1: Tank Geometry ──────────────────────────────────────
          _ConfigCard(
            icon: Icons.propane_tank_outlined,
            title: 'Tank Geometry',
            subtitle: 'Used by device to convert sensor distance → volume',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Shape grid
                const _SectionLabel('Tank Shape'),
                _TankShapeGrid(
                  selected: _shape,
                  onChanged: (v) => setState(() => _shape = v),
                ),
                const SizedBox(height: 20),

                // Dimensions
                const _SectionLabel('Dimensions'),
                _buildDimensionFields(),
                const SizedBox(height: 16),

                // Calculated capacity
                if (_shape != 'multi_section' && capL > 0)
                  Container(
                    padding: const EdgeInsets.symmetric(
                        horizontal: 14, vertical: 10),
                    decoration: BoxDecoration(
                      color: AppColors.primary.withOpacity(0.07),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                          color: AppColors.primary.withOpacity(0.20)),
                    ),
                    child: Row(children: [
                      const Icon(Icons.calculate_outlined,
                          color: AppColors.primary, size: 18),
                      const SizedBox(width: 10),
                      const Text('Calculated capacity',
                          style: TextStyle(
                              color: AppColors.textMuted, fontSize: 13)),
                      const Spacer(),
                      Text(
                        '${capL.toStringAsFixed(0)} L',
                        style: const TextStyle(
                          color: AppColors.primary,
                          fontSize: 17,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                      const SizedBox(width: 6),
                      Text(
                        '(${(capL / 1000).toStringAsFixed(2)} m³)',
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 11),
                      ),
                    ]),
                  ),

                if (_shape == 'multi_section')
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: AppColors.surfaceLight,
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: const Row(children: [
                      Icon(Icons.info_outline_rounded,
                          color: AppColors.textMuted, size: 16),
                      SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          'Multi-section tanks require configuration via the web dashboard.',
                          style: TextStyle(
                              color: AppColors.textMuted, fontSize: 12),
                        ),
                      ),
                    ]),
                  ),
              ],
            ),
          ),

          const SizedBox(height: 12),

          // ── Card 2: Reporting & Features ───────────────────────────────
          _ConfigCard(
            icon: Icons.schedule_rounded,
            title: 'Reporting & Features',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Interval
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Text('Reporting Interval',
                        style: TextStyle(
                            color: AppColors.text,
                            fontSize: 13,
                            fontWeight: FontWeight.w600)),
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 4),
                      decoration: BoxDecoration(
                        color: AppColors.primary.withOpacity(0.15),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Text(
                        _fmtInterval(_reportingIntervalS),
                        style: const TextStyle(
                          color: AppColors.primary,
                          fontWeight: FontWeight.w800,
                          fontSize: 14,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 4),
                const Text(
                  'How often the device sends telemetry (10 s – 1 hr)',
                  style: TextStyle(color: AppColors.textMuted, fontSize: 11),
                ),
                const SizedBox(height: 10),

                // Quick-select chips
                Wrap(
                  spacing: 8,
                  children: _kIntervals.map((v) {
                    final sel = _reportingIntervalS == v;
                    return GestureDetector(
                      onTap: () => setState(() => _reportingIntervalS = v),
                      child: AnimatedContainer(
                        duration: const Duration(milliseconds: 150),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 14, vertical: 7),
                        decoration: BoxDecoration(
                          color: sel
                              ? AppColors.primary
                              : AppColors.surfaceLight,
                          borderRadius: BorderRadius.circular(20),
                          border: Border.all(
                              color: sel
                                  ? AppColors.primary
                                  : AppColors.border),
                        ),
                        child: Text(
                          _fmtInterval(v),
                          style: TextStyle(
                            color: sel ? Colors.white : AppColors.textMuted,
                            fontSize: 12,
                            fontWeight: sel
                                ? FontWeight.w700
                                : FontWeight.normal,
                          ),
                        ),
                      ),
                    );
                  }).toList(),
                ),

                const SizedBox(height: 20),

                // Timezone
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Timezone',
                            style: TextStyle(
                                color: AppColors.text,
                                fontSize: 13,
                                fontWeight: FontWeight.w600)),
                        Text(
                          'Offset for device timestamps',
                          style: TextStyle(
                              color: AppColors.textMuted, fontSize: 11),
                        ),
                      ],
                    ),
                    Text(
                      _fmtTz(_timezoneOffsetMin),
                      style: const TextStyle(
                        color: AppColors.primary,
                        fontSize: 14,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ],
                ),
                SliderTheme(
                  data: SliderTheme.of(context).copyWith(
                    activeTrackColor: AppColors.primary,
                    inactiveTrackColor: AppColors.border,
                    thumbColor: AppColors.primary,
                    overlayColor: AppColors.primary.withOpacity(0.15),
                    trackHeight: 3,
                  ),
                  child: Slider(
                    min: -720,
                    max: 720,
                    divisions: 48, // 30-min steps
                    value: _timezoneOffsetMin.clamp(-720, 720).toDouble(),
                    onChanged: (v) =>
                        setState(() => _timezoneOffsetMin = v.toInt()),
                  ),
                ),

                const SizedBox(height: 8),

                // GPS toggle
                _ToggleRow(
                  icon: Icons.gps_fixed_rounded,
                  iconColor: AppColors.success,
                  title: 'GPS Location',
                  subtitle: 'Include GPS in telemetry',
                  value: _gpsEnabled,
                  onChanged: (v) => setState(() => _gpsEnabled = v),
                ),

                const SizedBox(height: 8),

                // Debug toggle
                _ToggleRow(
                  icon: Icons.bug_report_outlined,
                  iconColor: AppColors.warning,
                  title: 'Debug Mode',
                  subtitle: 'Verbose device logging',
                  value: _debugMode,
                  onChanged: (v) => setState(() => _debugMode = v),
                ),
              ],
            ),
          ),

          const SizedBox(height: 12),

          // ── Card 3: Alert Settings ─────────────────────────────────────
          _ConfigCard(
            icon: Icons.notifications_active_rounded,
            title: 'Alert Settings',
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Alert Threshold (%)',
                  style: TextStyle(
                      color: AppColors.textMuted,
                      fontSize: 12,
                      fontWeight: FontWeight.w600),
                ),
                const SizedBox(height: 6),
                TextField(
                  controller: _thresholdCtrl,
                  keyboardType:
                      const TextInputType.numberWithOptions(decimal: true),
                  style: const TextStyle(
                      color: AppColors.text, fontSize: 15),
                  decoration: InputDecoration(
                    prefixIcon: const Icon(
                        Icons.warning_amber_rounded,
                        color: AppColors.fuelLow,
                        size: 20),
                    suffixText: '%',
                    suffixStyle: const TextStyle(
                        color: AppColors.textMuted),
                    hintText: '20',
                    hintStyle:
                        const TextStyle(color: AppColors.textMuted),
                    filled: true,
                    fillColor: AppColors.surfaceLight,
                    isDense: true,
                    contentPadding: const EdgeInsets.symmetric(
                        vertical: 14, horizontal: 14),
                    border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(10),
                        borderSide:
                            const BorderSide(color: AppColors.border)),
                    enabledBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(10),
                        borderSide:
                            const BorderSide(color: AppColors.border)),
                    focusedBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(10),
                        borderSide: const BorderSide(
                            color: AppColors.fuelLow, width: 2)),
                  ),
                ),
                const SizedBox(height: 6),
                const Text(
                  'App shows a warning when fuel drops below this level.',
                  style: TextStyle(
                      color: AppColors.textMuted, fontSize: 11),
                ),
              ],
            ),
          ),

          const SizedBox(height: 28),

          // ── Send button ────────────────────────────────────────────────
          SizedBox(
            width: double.infinity,
            height: 52,
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: _sending
                    ? null
                    : const LinearGradient(
                        colors: [AppColors.primary, Color(0xFFD97706)],
                        begin: Alignment.centerLeft,
                        end: Alignment.centerRight,
                      ),
                color: _sending ? AppColors.surfaceLight : null,
                borderRadius: BorderRadius.circular(14),
                boxShadow: _sending
                    ? null
                    : [
                        BoxShadow(
                          color: AppColors.primary.withOpacity(0.35),
                          blurRadius: 16,
                          offset: const Offset(0, 6),
                        ),
                      ],
              ),
              child: ElevatedButton.icon(
                onPressed: _sending ? null : _sendConfig,
                icon: _sending
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(
                            color: Colors.white, strokeWidth: 2.5))
                    : const Icon(Icons.send_rounded,
                        color: Colors.white, size: 18),
                label: Text(
                  _sending ? 'Sending…' : 'Send Config to Device',
                  style: const TextStyle(
                      color: Colors.white,
                      fontSize: 15,
                      fontWeight: FontWeight.w700),
                ),
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.transparent,
                  shadowColor: Colors.transparent,
                  shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(14)),
                ),
              ),
            ),
          ),

          const SizedBox(height: 32),
        ],
      ),
    );
  }

  // ── Tab 3 — Live device logs ──────────────────────────────────────────────

  Widget _logsTab() {
    return Column(
      children: [
        // Header bar
        Container(
          padding:
              const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
          color: AppColors.surface,
          child: Row(children: [
            Container(
              width: 8,
              height: 8,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: _status == 'online'
                    ? AppColors.success
                    : AppColors.textMuted,
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                _logs.isEmpty
                    ? 'Waiting for device logs…'
                    : '${_logs.length} entries — newest first',
                style: const TextStyle(
                    color: AppColors.text,
                    fontSize: 13,
                    fontWeight: FontWeight.w600),
              ),
            ),
            if (_logs.isNotEmpty)
              GestureDetector(
                onTap: () => setState(() => _logs.clear()),
                child: Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 10, vertical: 5),
                  decoration: BoxDecoration(
                    color: AppColors.surfaceLight,
                    borderRadius: BorderRadius.circular(6),
                  ),
                  child: const Text('Clear',
                      style: TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
                ),
              ),
          ]),
        ),
        const Divider(height: 1, color: AppColors.border),

        // Log list
        Expanded(
          child: _logs.isEmpty
              ? Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const CircularProgressIndicator(
                          color: AppColors.primary),
                      const SizedBox(height: 16),
                      const Text('Waiting for device logs…',
                          style: TextStyle(color: AppColors.textMuted)),
                      const SizedBox(height: 6),
                      Text(
                        _status == 'online'
                            ? 'Device is online — logs appear when it sends debug output.'
                            : 'Device appears offline.',
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 11),
                        textAlign: TextAlign.center,
                      ),
                    ],
                  ),
                )
              : ListView.builder(
                  padding: const EdgeInsets.all(10),
                  itemCount: _logs.length,
                  itemBuilder: (_, i) {
                    final l = _logs[i];
                    // Colour-code by severity keywords
                    final msg = l.message;
                    final Color msgColor = msg.contains('ERROR') ||
                            msg.contains('error') ||
                            msg.contains('ERR')
                        ? AppColors.error
                        : msg.contains('WARN') || msg.contains('warn')
                            ? AppColors.warning
                            : AppColors.text;
                    return Container(
                      margin: const EdgeInsets.only(bottom: 3),
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 6),
                      decoration: BoxDecoration(
                        color: AppColors.surface,
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            l.time
                                .toLocal()
                                .toString()
                                .substring(11, 19),
                            style: const TextStyle(
                              color: AppColors.primary,
                              fontSize: 11,
                              fontFamily: 'monospace',
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: Text(
                              msg,
                              style: TextStyle(
                                color: msgColor,
                                fontSize: 11,
                                fontFamily: 'monospace',
                              ),
                            ),
                          ),
                        ],
                      ),
                    );
                  },
                ),
        ),
      ],
    );
  }

  // ── Tab 4 — OTA firmware update ───────────────────────────────────────────

  Widget _otaTab() {
    final bool hasStatus = _otaStatus != null;
    final bool isSuccess = hasStatus && _otaStatus!.startsWith('✓');

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Warning banner
          Container(
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              color: AppColors.warning.withOpacity(0.08),
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: AppColors.warning.withOpacity(0.35)),
            ),
            child: const Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(Icons.warning_amber_rounded,
                    color: AppColors.warning, size: 22),
                SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('OTA Firmware Update',
                          style: TextStyle(
                              color: AppColors.warning,
                              fontWeight: FontWeight.w800,
                              fontSize: 14)),
                      SizedBox(height: 4),
                      Text(
                        'The device will download and flash the new firmware, '
                        'then reboot automatically. Ensure the URL is reachable '
                        'from the device\'s cellular network.',
                        style: TextStyle(
                            color: AppColors.textMuted, fontSize: 12),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),

          const SizedBox(height: 20),

          // Device info
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: AppColors.surfaceLight,
              borderRadius: BorderRadius.circular(10),
            ),
            child: Row(children: [
              const Icon(Icons.memory_rounded,
                  color: AppColors.textMuted, size: 16),
              const SizedBox(width: 10),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(_device.displayName,
                        style: const TextStyle(
                            color: AppColors.text,
                            fontSize: 13,
                            fontWeight: FontWeight.w700)),
                    Text(_device.deviceId,
                        style: const TextStyle(
                            color: AppColors.textMuted,
                            fontSize: 11,
                            fontFamily: 'monospace')),
                  ],
                ),
              ),
              StatusBadge(status: _status),
            ]),
          ),

          const SizedBox(height: 20),

          // Firmware URL input
          const _SectionLabel('Firmware Binary URL'),
          TextField(
            controller: _otaUrlCtrl,
            keyboardType: TextInputType.url,
            autocorrect: false,
            style: const TextStyle(
                color: AppColors.text,
                fontSize: 13,
                fontFamily: 'monospace'),
            decoration: InputDecoration(
              hintText: 'https://…/firmware.bin',
              hintStyle: const TextStyle(
                  color: AppColors.textMuted, fontFamily: 'monospace'),
              prefixIcon: const Icon(Icons.link_rounded,
                  color: AppColors.textMuted, size: 20),
              filled: true,
              fillColor: AppColors.surface,
              isDense: true,
              contentPadding: const EdgeInsets.symmetric(
                  vertical: 14, horizontal: 14),
              border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide: const BorderSide(color: AppColors.border)),
              enabledBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide: const BorderSide(color: AppColors.border)),
              focusedBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide: const BorderSide(
                      color: AppColors.warning, width: 2)),
            ),
          ),

          const SizedBox(height: 24),

          // Send button
          SizedBox(
            width: double.infinity,
            height: 52,
            child: ElevatedButton.icon(
              onPressed: _otaSending ? null : _sendOta,
              icon: _otaSending
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(
                          color: Colors.white, strokeWidth: 2.5))
                  : const Icon(Icons.system_update_rounded,
                      color: Colors.white, size: 20),
              label: Text(
                _otaSending
                    ? 'Sending OTA request…'
                    : 'Send OTA Update to Device',
                style: const TextStyle(
                    color: Colors.white,
                    fontSize: 15,
                    fontWeight: FontWeight.w700),
              ),
              style: ElevatedButton.styleFrom(
                backgroundColor:
                    _otaSending ? AppColors.surfaceLight : AppColors.warning,
                foregroundColor: Colors.white,
                shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(14)),
              ),
            ),
          ),

          // Status message
          if (hasStatus) ...[
            const SizedBox(height: 16),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(14),
              decoration: BoxDecoration(
                color: isSuccess
                    ? AppColors.success.withOpacity(0.08)
                    : AppColors.error.withOpacity(0.08),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(
                  color: isSuccess
                      ? AppColors.success.withOpacity(0.35)
                      : AppColors.error.withOpacity(0.35),
                ),
              ),
              child: Text(
                _otaStatus!,
                style: TextStyle(
                  color: isSuccess ? AppColors.success : AppColors.error,
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            if (isSuccess) ...[
              const SizedBox(height: 10),
              const Text(
                'Monitor the Logs tab to track download progress and reboot.',
                style: TextStyle(color: AppColors.textMuted, fontSize: 12),
              ),
            ],
          ],

          const SizedBox(height: 32),
        ],
      ),
    );
  }

  // ── Dimension fields builder (changes per shape) ──────────────────────────

  Widget _buildDimensionFields() {
    switch (_shape) {
      case 'rectangular':
        return Column(children: [
          _DimField(label: 'Length',   ctrl: _lengthCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Width',    ctrl: _widthCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Height',   ctrl: _heightCtrl),
        ]);
      case 'cylinder_vertical':
      case 'cone_vertical':
        return Column(children: [
          _DimField(label: 'Radius',  ctrl: _radiusCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Height',  ctrl: _heightCtrl),
        ]);
      case 'cylinder_horizontal':
      case 'capsule':
        return Column(children: [
          _DimField(label: 'Radius',        ctrl: _radiusCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Length (tube)', ctrl: _lengthCtrl),
        ]);
      case 'ellipse_vertical':
        return Column(children: [
          _DimField(label: 'Radius A (equatorial)', ctrl: _radiusCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Radius B (polar)',      ctrl: _radiusBCtrl),
          const SizedBox(height: 10),
          _DimField(label: 'Height',                ctrl: _heightCtrl),
        ]);
      case 'sphere':
        return _DimField(label: 'Radius', ctrl: _radiusCtrl);
      default: // multi_section
        return const SizedBox.shrink();
    }
  }

  // ── Helpers ───────────────────────────────────────────────────────────────

  String _shapeName(String firmware) {
    for (final s in _kShapes) {
      if (s.id == firmware) return s.name;
    }
    return firmware;
  }

  static String _fmtTz(int offsetMin) {
    final h = offsetMin ~/ 60;
    final m = (offsetMin.abs()) % 60;
    final sign = h >= 0 ? '+' : '';
    return m == 0 ? 'UTC$sign$h' : 'UTC$sign$h:${m.toString().padLeft(2, '0')}';
  }

  String _timeAgo(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inSeconds < 60)  return '${diff.inSeconds}s ago';
    if (diff.inMinutes < 60)  return '${diff.inMinutes}m ago';
    return '${diff.inHours}h ago';
  }
}

// ── Tank widget ───────────────────────────────────────────────────────────────

class _TankWidget extends StatelessWidget {
  final double pct;
  final double? volumeL;
  final double fillRatio;
  final double wavePhase;
  final Color color;
  final String shape;

  const _TankWidget({
    required this.pct,
    this.volumeL,
    required this.fillRatio,
    required this.wavePhase,
    required this.color,
    required this.shape,
  });

  @override
  Widget build(BuildContext context) {
    final isHorizontal = shape == 'cylinder_horizontal';
    final double tankW = isHorizontal ? 300 : 160;
    final double tankH = isHorizontal ? 150 : 280;
    final statusLabel = pct > 60 ? 'NORMAL' : pct > 20 ? 'LOW' : 'CRITICAL';

    return Center(
      child: SizedBox(
        width: tankW,
        height: tankH,
        child: Stack(alignment: Alignment.center, children: [
          CustomPaint(
            painter: _TankPainter(
              fillRatio: fillRatio,
              wavePhase: wavePhase,
              color: color,
              shape: shape,
            ),
            child: SizedBox(width: tankW, height: tankH),
          ),
          Column(mainAxisSize: MainAxisSize.min, children: [
            Text(
              '${pct.toStringAsFixed(1)}%',
              style: TextStyle(
                color: Colors.white,
                fontSize: isHorizontal ? 30 : 38,
                fontWeight: FontWeight.w900,
                shadows: [Shadow(color: Colors.black.withOpacity(0.6), blurRadius: 10)],
              ),
            ),
            if (volumeL != null)
              Text('${volumeL!.toStringAsFixed(1)} L',
                  style: TextStyle(
                    color: Colors.white.withOpacity(0.9),
                    fontSize: isHorizontal ? 13 : 15,
                    fontWeight: FontWeight.w600,
                    shadows: [Shadow(color: Colors.black.withOpacity(0.5), blurRadius: 8)],
                  )),
            const SizedBox(height: 6),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 3),
              decoration: BoxDecoration(
                color: Colors.black.withOpacity(0.35),
                borderRadius: BorderRadius.circular(20),
                border: Border.all(color: color.withOpacity(0.6)),
              ),
              child: Text(statusLabel,
                  style: TextStyle(
                      color: color,
                      fontSize: 10,
                      fontWeight: FontWeight.w800,
                      letterSpacing: 0.8)),
            ),
          ]),
        ]),
      ),
    );
  }
}

// ── Tank painter ──────────────────────────────────────────────────────────────

class _TankPainter extends CustomPainter {
  final double fillRatio;
  final double wavePhase;
  final Color color;
  final String shape; // firmware shape name

  const _TankPainter({
    required this.fillRatio,
    required this.wavePhase,
    required this.color,
    required this.shape,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width;
    final h = size.height;
    final tank = _tankPath(w, h);

    // 1 ── Empty background
    canvas.drawPath(tank,
        Paint()..color = color.withOpacity(0.08)..style = PaintingStyle.fill);

    // 2 ── Level markers
    _drawMarkers(canvas, w, h, tank);

    // 3 ── Animated fuel fill
    if (fillRatio > 0.002) {
      final fuelTop  = h * (1.0 - fillRatio.clamp(0.0, 1.0));
      const waveAmp  = 5.0;
      const cycles   = 2.0;

      final fuel = Path()
        ..moveTo(-1, h + 1)
        ..lineTo(-1, fuelTop);
      for (double x = 0; x <= w + 1; x++) {
        fuel.lineTo(x,
            fuelTop + waveAmp * math.sin(x / w * cycles * 2 * math.pi + wavePhase));
      }
      fuel..lineTo(w + 1, h + 1)..close();

      canvas.save();
      canvas.clipPath(tank);

      canvas.drawPath(fuel,
          Paint()..color = color.withOpacity(0.50)..style = PaintingStyle.fill);

      final shimRect = Rect.fromLTWH(0, fuelTop, w, (h - fuelTop) * 0.35);
      canvas.drawRect(shimRect, Paint()
        ..style = PaintingStyle.fill
        ..shader = LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [color.withOpacity(0.22), color.withOpacity(0.0)],
        ).createShader(shimRect));

      final wave = Path()
        ..moveTo(-1, fuelTop + waveAmp * math.sin(wavePhase));
      for (double x = 0; x <= w + 1; x++) {
        wave.lineTo(x,
            fuelTop + waveAmp * math.sin(x / w * cycles * 2 * math.pi + wavePhase));
      }
      canvas.drawPath(wave, Paint()
        ..color = color.withOpacity(0.90)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.2);

      canvas.restore();
    }

    // 4 ── Glass highlight
    canvas.save();
    canvas.clipPath(tank);
    final hl = Rect.fromLTWH(0, 0, w * 0.09, h);
    canvas.drawRect(hl, Paint()
      ..style = PaintingStyle.fill
      ..shader = LinearGradient(
        begin: Alignment.centerLeft,
        end: Alignment.centerRight,
        colors: [Colors.white.withOpacity(0.07), Colors.transparent],
      ).createShader(hl));
    canvas.restore();

    // 5 ── Outline
    canvas.drawPath(tank, Paint()
      ..color = color.withOpacity(0.65)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.5
      ..strokeJoin = StrokeJoin.round
      ..strokeCap = StrokeCap.round);
  }

  Path _tankPath(double w, double h) {
    switch (shape) {
      case 'cylinder_horizontal':
        return Path()..addRRect(RRect.fromRectAndRadius(
            Rect.fromLTWH(0, 0, w, h), Radius.circular(h / 2)));
      case 'rectangular':
        return Path()..addRRect(RRect.fromRectAndRadius(
            Rect.fromLTWH(0, 0, w, h), const Radius.circular(12)));
      case 'sphere':
        return Path()..addOval(Rect.fromLTWH(0, 0, w, h));
      case 'cone_vertical':
        return Path()
          ..moveTo(w / 2, 0)
          ..lineTo(w, h)
          ..lineTo(0, h)
          ..close();
      default: // cylinder_vertical, ellipse_vertical, capsule, multi_section
        return Path()..addRRect(RRect.fromRectAndRadius(
            Rect.fromLTWH(0, 0, w, h), Radius.circular(w / 2)));
    }
  }

  void _drawMarkers(Canvas canvas, double w, double h, Path tank) {
    final paint = Paint()
      ..color = Colors.white.withOpacity(0.13)
      ..strokeWidth = 1.0
      ..style = PaintingStyle.stroke;
    canvas.save();
    canvas.clipPath(tank);
    for (final level in [0.25, 0.50, 0.75]) {
      final y = h * (1 - level);
      canvas.drawLine(Offset(0, y), Offset(w, y), paint);
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(_TankPainter old) =>
      old.fillRatio != fillRatio ||
      old.wavePhase != wavePhase ||
      old.shape != shape ||
      old.color != color;
}

// ── Tank shape grid ───────────────────────────────────────────────────────────

class _TankShapeGrid extends StatelessWidget {
  final String selected;
  final ValueChanged<String> onChanged;

  const _TankShapeGrid({required this.selected, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 4,
        crossAxisSpacing: 8,
        mainAxisSpacing: 8,
        childAspectRatio: 0.78,
      ),
      itemCount: _kShapes.length,
      itemBuilder: (_, i) {
        final s = _kShapes[i];
        final sel = selected == s.id;
        return GestureDetector(
          onTap: () => onChanged(s.id),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 150),
            padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 8),
            decoration: BoxDecoration(
              color: sel
                  ? AppColors.primary.withOpacity(0.12)
                  : AppColors.surfaceLight,
              borderRadius: BorderRadius.circular(10),
              border: Border.all(
                  color: sel ? AppColors.primary : AppColors.border,
                  width: sel ? 1.8 : 1),
            ),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                SizedBox(
                  width: 36,
                  height: 36,
                  child: CustomPaint(
                    painter: _ShapeIconPainter(
                        shape: s.id,
                        color: sel ? AppColors.primary : AppColors.textMuted),
                  ),
                ),
                const SizedBox(height: 5),
                Text(
                  s.name,
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    color: sel ? AppColors.text : AppColors.textMuted,
                    fontSize: 9,
                    fontWeight:
                        sel ? FontWeight.w700 : FontWeight.normal,
                    height: 1.2,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  s.desc,
                  textAlign: TextAlign.center,
                  style: const TextStyle(
                      color: AppColors.textMuted,
                      fontSize: 8,
                      height: 1.1),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

// ── Shape icon painter ────────────────────────────────────────────────────────

class _ShapeIconPainter extends CustomPainter {
  final String shape;
  final Color color;
  const _ShapeIconPainter({required this.shape, required this.color});

  @override
  void paint(Canvas canvas, Size sz) {
    final w = sz.width;
    final h = sz.height;
    final fill = Paint()
      ..color = color.withOpacity(0.25)
      ..style = PaintingStyle.fill;
    final stroke = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.8
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    switch (shape) {
      case 'rectangular':
        final r = RRect.fromRectAndRadius(
            Rect.fromLTRB(3, 5, w - 3, h - 5), const Radius.circular(3));
        canvas.drawRRect(r, fill);
        canvas.drawRRect(r, stroke);

      case 'cylinder_vertical':
        final top = RRect.fromRectAndRadius(
            Rect.fromLTRB(6, 2, w - 6, 10), const Radius.circular(4));
        canvas.drawRect(Rect.fromLTRB(6, 6, w - 6, h - 6), fill);
        canvas.drawRRect(top, fill);
        canvas.drawLine(Offset(6, 6), Offset(6, h - 6), stroke);
        canvas.drawLine(Offset(w - 6, 6), Offset(w - 6, h - 6), stroke);
        canvas.drawRRect(top, stroke);
        final bot = RRect.fromRectAndRadius(
            Rect.fromLTRB(6, h - 10, w - 6, h - 2), const Radius.circular(4));
        canvas.drawRRect(bot, fill);
        canvas.drawRRect(bot, stroke);

      case 'cylinder_horizontal':
        final r = RRect.fromRectAndRadius(
            Rect.fromLTRB(2, 9, w - 2, h - 9), Radius.circular((h - 18) / 2));
        canvas.drawRRect(r, fill);
        canvas.drawRRect(r, stroke);

      case 'cone_vertical':
        final path = Path()
          ..moveTo(w / 2, 2)
          ..lineTo(w - 3, h - 4)
          ..lineTo(3, h - 4)
          ..close();
        canvas.drawPath(path, fill);
        canvas.drawPath(path, stroke);

      case 'ellipse_vertical':
        final rect = Rect.fromLTRB(6, 2, w - 6, h - 2);
        canvas.drawOval(rect, fill);
        canvas.drawOval(rect, stroke);

      case 'sphere':
        canvas.drawCircle(Offset(w / 2, h / 2), math.min(w, h) / 2 - 3, fill);
        canvas.drawCircle(Offset(w / 2, h / 2), math.min(w, h) / 2 - 3, stroke);

      case 'capsule':
        final r = RRect.fromRectAndRadius(
            Rect.fromLTRB(8, 2, w - 8, h - 2), Radius.circular((h - 4) / 2));
        canvas.drawRRect(r, fill);
        canvas.drawRRect(r, stroke);

      case 'multi_section':
        const gap = 3.0;
        final bh = (h - gap * 3) / 3;
        for (int i = 0; i < 3; i++) {
          final top = gap + i * (bh + gap);
          final r2 = RRect.fromRectAndRadius(
              Rect.fromLTRB(4, top, w - 4, top + bh),
              const Radius.circular(3));
          canvas.drawRRect(r2, fill);
          canvas.drawRRect(r2, stroke);
        }
    }
  }

  @override
  bool shouldRepaint(_ShapeIconPainter old) =>
      old.shape != shape || old.color != color;
}

// ── Dimension input field ─────────────────────────────────────────────────────

class _DimField extends StatelessWidget {
  final String label;
  final TextEditingController ctrl;

  const _DimField({required this.label, required this.ctrl});

  @override
  Widget build(BuildContext context) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label,
              style: const TextStyle(
                  color: AppColors.textMuted,
                  fontSize: 12,
                  fontWeight: FontWeight.w600)),
          const SizedBox(height: 5),
          TextField(
            controller: ctrl,
            keyboardType:
                const TextInputType.numberWithOptions(decimal: true),
            style: const TextStyle(color: AppColors.text, fontSize: 15),
            decoration: InputDecoration(
              suffixText: 'm',
              suffixStyle: const TextStyle(
                  color: AppColors.textMuted, fontWeight: FontWeight.w600),
              filled: true,
              fillColor: AppColors.surface,
              isDense: true,
              contentPadding: const EdgeInsets.symmetric(
                  vertical: 14, horizontal: 14),
              border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide: const BorderSide(color: AppColors.border)),
              enabledBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide: const BorderSide(color: AppColors.border)),
              focusedBorder: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(10),
                  borderSide:
                      const BorderSide(color: AppColors.primary, width: 2)),
            ),
          ),
        ],
      );
}

// ── Toggle row ────────────────────────────────────────────────────────────────

class _ToggleRow extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String title;
  final String subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;

  const _ToggleRow({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.subtitle,
    required this.value,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) => Row(children: [
        Container(
          padding: const EdgeInsets.all(7),
          decoration: BoxDecoration(
            color: iconColor.withOpacity(0.12),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Icon(icon, color: iconColor, size: 18),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title,
                  style: const TextStyle(
                      color: AppColors.text,
                      fontSize: 13,
                      fontWeight: FontWeight.w600)),
              Text(subtitle,
                  style: const TextStyle(
                      color: AppColors.textMuted, fontSize: 11)),
            ],
          ),
        ),
        Switch(
          value: value,
          onChanged: onChanged,
          activeColor: AppColors.primary,
        ),
      ]);
}

// ── Config card ───────────────────────────────────────────────────────────────

class _ConfigCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final String? subtitle;
  final Widget child;

  const _ConfigCard({
    required this.icon,
    required this.title,
    this.subtitle,
    required this.child,
  });

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: AppColors.border),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(icon, color: AppColors.primary, size: 18),
              const SizedBox(width: 8),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title,
                        style: const TextStyle(
                          color: AppColors.text,
                          fontSize: 15,
                          fontWeight: FontWeight.w700,
                        )),
                    if (subtitle != null)
                      Text(subtitle!,
                          style: const TextStyle(
                              color: AppColors.textMuted, fontSize: 11)),
                  ],
                ),
              ),
            ]),
            const SizedBox(height: 16),
            child,
          ],
        ),
      );
}

// ── Config sync bar ───────────────────────────────────────────────────────────

class _ConfigSyncBar extends StatelessWidget {
  final bool loading;
  final bool loaded;
  final DateTime? syncedAt;
  final VoidCallback? onRefresh;

  const _ConfigSyncBar({
    required this.loading,
    required this.loaded,
    required this.syncedAt,
    required this.onRefresh,
  });

  @override
  Widget build(BuildContext context) {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 250),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: loading
            ? AppColors.primary.withOpacity(0.07)
            : loaded
                ? AppColors.success.withOpacity(0.08)
                : AppColors.surfaceLight,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(
          color: loading
              ? AppColors.primary.withOpacity(0.30)
              : loaded
                  ? AppColors.success.withOpacity(0.35)
                  : AppColors.border,
        ),
      ),
      child: Row(children: [
        SizedBox(
          width: 20,
          height: 20,
          child: loading
              ? const CircularProgressIndicator(
                  color: AppColors.primary, strokeWidth: 2.2)
              : Icon(
                  loaded
                      ? Icons.check_circle_outline_rounded
                      : Icons.sync_rounded,
                  color: loaded ? AppColors.success : AppColors.textMuted,
                  size: 20),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Text(
            loading
                ? 'Fetching config from device…'
                : loaded
                    ? 'Synced from device${syncedAt != null ? '  ·  ${syncedAt!.toLocal().toString().substring(11, 19)}' : ''}'
                    : 'Not synced yet',
            style: TextStyle(
              color: loading
                  ? AppColors.primary
                  : loaded
                      ? AppColors.success
                      : AppColors.textMuted,
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
          ),
        ),
        if (!loading)
          GestureDetector(
            onTap: onRefresh,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
              decoration: BoxDecoration(
                color: AppColors.primary.withOpacity(0.12),
                borderRadius: BorderRadius.circular(6),
              ),
              child: const Row(mainAxisSize: MainAxisSize.min, children: [
                Icon(Icons.sync_rounded, color: AppColors.primary, size: 13),
                SizedBox(width: 4),
                Text('Sync',
                    style: TextStyle(
                        color: AppColors.primary,
                        fontSize: 11,
                        fontWeight: FontWeight.w700)),
              ]),
            ),
          ),
      ]),
    );
  }
}

// ── Section label ─────────────────────────────────────────────────────────────

class _SectionLabel extends StatelessWidget {
  final String text;
  const _SectionLabel(this.text);

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(bottom: 10),
        child: Text(
          text.toUpperCase(),
          style: const TextStyle(
            color: AppColors.textMuted,
            fontSize: 11,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.6,
          ),
        ),
      );
}

// ── Card wrapper ──────────────────────────────────────────────────────────────

class _Card extends StatelessWidget {
  final Widget child;
  const _Card({required this.child});

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: AppColors.border),
        ),
        child: child,
      );
}

// ── Metric tile ───────────────────────────────────────────────────────────────

class _MetricTile extends StatelessWidget {
  final String label;
  final String value;
  final IconData icon;
  final Color color;

  const _MetricTile({
    required this.label,
    required this.value,
    required this.icon,
    required this.color,
  });

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: color.withOpacity(0.25)),
        ),
        child: Row(children: [
          Icon(icon, color: color, size: 20),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(value,
                    style: TextStyle(
                        color: color,
                        fontSize: 15,
                        fontWeight: FontWeight.w800)),
                Text(label,
                    style: const TextStyle(
                        color: AppColors.textMuted, fontSize: 10)),
              ],
            ),
          ),
        ]),
      );
}
