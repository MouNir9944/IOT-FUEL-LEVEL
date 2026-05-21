import 'dart:math' as math;
import 'package:dio/dio.dart';
import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/app_strings.dart';
import '../core/constants.dart';
import '../core/socket_service.dart';
import '../models/device.dart';
import '../widgets/status_badge.dart';

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
  late AnimationController _fillAnim;   // 0 → 1 on first load
  late AnimationController _waveAnim;   // repeating wave

  // ── Tank config form state ─────────────────────────────────────────────────
  String _shape = 'cylindrical_vertical';
  final _heightCtrl    = TextEditingController(text: '200');
  final _diameterCtrl  = TextEditingController(text: '100');
  final _lengthCtrl    = TextEditingController(text: '150');
  final _widthCtrl     = TextEditingController(text: '100');
  final _offsetCtrl    = TextEditingController(text: '5');
  final _thresholdCtrl = TextEditingController(text: '20');
  bool _sending = false;

  @override
  void initState() {
    super.initState();
    _device    = widget.device;
    _status    = _device.lastStatus;
    _telemetry = _device.lastTelemetry;

    _tabs     = TabController(length: 2, vsync: this);
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
    _diameterCtrl.dispose();
    _lengthCtrl.dispose();
    _widthCtrl.dispose();
    _offsetCtrl.dispose();
    _thresholdCtrl.dispose();
    SocketService.off('telemetry_update',     id: 'device_control');
    SocketService.off('device_status_change', id: 'device_control');
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
        _fillAnim
          ..reset()
          ..forward();
      });
    }, id: 'device_control');

    SocketService.on('device_status_change', (data) {
      if (!mounted) return;
      final m = Map<String, dynamic>.from(data as Map);
      if (m['device_id'] != _device.deviceId) return;
      setState(() => _status = m['status'] as String? ?? _status);
    }, id: 'device_control');
  }

  void _onSocketReconnect() =>
      SocketService.subscribeToDevice(_device.deviceId);

  Future<void> _loadStatus() async {
    try {
      final resp =
          await ApiClient.instance.get('/devices/${_device.id}/status');
      if (!mounted) return;
      final d = Device.fromJson(
          Map<String, dynamic>.from(resp.data['device'] as Map));
      setState(() {
        _device    = d;
        _status    = d.lastStatus;
        _telemetry = d.lastTelemetry;
        _fillAnim
          ..reset()
          ..forward();
      });
      SocketService.subscribeToDevice(_device.deviceId);
    } catch (_) {}
  }

  // ── Config send ───────────────────────────────────────────────────────────

  Future<void> _sendConfig() async {
    if (_sending) return;
    final s = AppStrings.read(context);

    final Map<String, dynamic> cfg = {
      'shape':               _shape,
      'sensor_offset_cm':    double.tryParse(_offsetCtrl.text.trim())    ?? 5.0,
      'alert_threshold_pct': double.tryParse(_thresholdCtrl.text.trim()) ?? 20.0,
    };

    if (_shape == 'cylindrical_vertical' ||
        _shape == 'cylindrical_horizontal') {
      cfg['height_cm']   = double.tryParse(_heightCtrl.text.trim())   ?? 0;
      cfg['diameter_cm'] = double.tryParse(_diameterCtrl.text.trim()) ?? 0;
    } else {
      cfg['height_cm'] = double.tryParse(_heightCtrl.text.trim())  ?? 0;
      cfg['length_cm'] = double.tryParse(_lengthCtrl.text.trim())  ?? 0;
      cfg['width_cm']  = double.tryParse(_widthCtrl.text.trim())   ?? 0;
    }

    setState(() => _sending = true);
    try {
      await ApiClient.instance.post(
        '/devices/${_device.id}/command',
        data: {'cmd': 'set_config', 'value': cfg},
      );
      if (!mounted) return;
      _snack(s.configSent);
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
          tabs: [
            Tab(icon: const Icon(Icons.water_drop_rounded, size: 18),
                text: s.fuelLevel),
            Tab(icon: const Icon(Icons.settings_rounded, size: 18),
                text: s.tankConfig),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          _monitorTab(s),
          _configTab(s),
        ],
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
          // ── Tank visualisation card ──────────────────────────────────────
          _Card(
            child: Column(
              children: [
                // Shape chip
                Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 12, vertical: 5),
                  decoration: BoxDecoration(
                    color: AppColors.surfaceLight,
                    borderRadius: BorderRadius.circular(20),
                  ),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const Icon(Icons.local_gas_station_rounded,
                          color: AppColors.textMuted, size: 13),
                      const SizedBox(width: 6),
                      Text(
                        _shapeLabel(_shape, s),
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 12),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 20),

                // Animated tank
                AnimatedBuilder(
                  animation: Listenable.merge([_fillAnim, _waveAnim]),
                  builder: (_, __) {
                    final fillRatio =
                        (pct / 100.0) * _fillAnim.value.clamp(0.0, 1.0);
                    final wavePhase = _waveAnim.value * 2 * math.pi;
                    return _TankWidget(
                      pct: pct,
                      volumeL: t?.fuelVolumeL,
                      fillRatio: fillRatio,
                      wavePhase: wavePhase,
                      color: color,
                      shape: _shape,
                    );
                  },
                ),

                const SizedBox(height: 14),
                if (t != null)
                  Text(
                    '${s.lastSeen}: ${_timeAgo(t.timestamp)}',
                    style: const TextStyle(
                        color: AppColors.textMuted, fontSize: 11),
                  ),
              ],
            ),
          ),

          const SizedBox(height: 12),

          // ── Metrics grid ─────────────────────────────────────────────────
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
                value: t != null
                    ? '${t.fuelVolumeL.toStringAsFixed(1)} L'
                    : '—',
                icon: Icons.opacity_rounded,
                color: AppColors.info,
              ),
              _MetricTile(
                label: s.temperature,
                value: t?.temperatureC != null
                    ? '${t!.temperatureC!.toStringAsFixed(1)} °C'
                    : '—',
                icon: Icons.thermostat_rounded,
                color: (t?.temperatureC ?? 0) > 50
                    ? AppColors.error
                    : AppColors.warning,
              ),
              _MetricTile(
                label: s.batteryLevel,
                value: t?.batteryPct != null
                    ? '${t!.batteryPct!.toStringAsFixed(0)} %'
                    : '—',
                icon: Icons.battery_charging_full_rounded,
                color: (t?.batteryPct ?? 100) < 20
                    ? AppColors.error
                    : AppColors.success,
              ),
              _MetricTile(
                label: s.signalStrength,
                value: t?.rssi != null ? '${t!.rssi} dBm' : '—',
                icon: Icons.signal_wifi_4_bar_rounded,
                color: (t?.rssi ?? 0) < -80
                    ? AppColors.error
                    : AppColors.success,
              ),
            ],
          ),

          const SizedBox(height: 12),

          // ── Alert status card ─────────────────────────────────────────────
          if (t != null)
            _Card(
              child: Row(
                children: [
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
                      color: t.isCritical
                          ? AppColors.fuelLow
                          : AppColors.success,
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
                                : t.isLow
                                    ? AppColors.fuelMid
                                    : AppColors.success,
                            fontWeight: FontWeight.w700,
                            fontSize: 13,
                          ),
                        ),
                        Text(
                          '${s.alertThreshold}: '
                          '${t.alertThresholdPct.toStringAsFixed(0)}%',
                          style: const TextStyle(
                              color: AppColors.textMuted, fontSize: 11),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }

  // ── Tab 2 — Tank configuration ────────────────────────────────────────────

  Widget _configTab(AppStrings s) => SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Hint banner
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: AppColors.primary.withOpacity(0.08),
                borderRadius: BorderRadius.circular(10),
                border:
                    Border.all(color: AppColors.primary.withOpacity(0.25)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.info_outline_rounded,
                      color: AppColors.primary, size: 18),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Text(
                      s.tankConfigHint,
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: 20),

            // Shape selector
            _SectionLabel(s.tankShape),
            _ShapeSelector(
              selected: _shape,
              onChanged: (v) => setState(() => _shape = v),
              labels: {
                'cylindrical_vertical':   s.cylindricalVertical,
                'cylindrical_horizontal': s.cylindricalHorizontal,
                'rectangular':            s.rectangular,
              },
            ),

            const SizedBox(height: 20),

            // Dimensions
            _SectionLabel('Dimensions'),
            _numField(
                label: s.heightCm,
                ctrl: _heightCtrl,
                icon: Icons.height_rounded),
            const SizedBox(height: 12),

            if (_shape != 'rectangular') ...[
              _numField(
                  label: s.diameterCm,
                  ctrl: _diameterCtrl,
                  icon: Icons.circle_outlined),
            ] else ...[
              _numField(
                  label: s.lengthCm,
                  ctrl: _lengthCtrl,
                  icon: Icons.straighten_rounded),
              const SizedBox(height: 12),
              _numField(
                  label: s.widthCm,
                  ctrl: _widthCtrl,
                  icon: Icons.straighten_rounded),
            ],

            const SizedBox(height: 20),

            // Sensor settings
            _SectionLabel('Sensor Settings'),
            _numField(
                label: s.sensorOffsetCm,
                ctrl: _offsetCtrl,
                icon: Icons.vertical_align_bottom_rounded),
            const SizedBox(height: 12),
            _numField(
                label: s.alertThresholdPct,
                ctrl: _thresholdCtrl,
                icon: Icons.notifications_active_rounded,
                color: AppColors.fuelLow),

            const SizedBox(height: 28),

            // Send button
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
                    _sending ? s.loading : s.sendConfig,
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

  // ── Helpers ───────────────────────────────────────────────────────────────

  String _shapeLabel(String shape, AppStrings s) {
    switch (shape) {
      case 'cylindrical_horizontal': return s.cylindricalHorizontal;
      case 'rectangular':            return s.rectangular;
      default:                       return s.cylindricalVertical;
    }
  }

  Widget _numField({
    required String label,
    required TextEditingController ctrl,
    required IconData icon,
    Color? color,
  }) =>
      Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label,
              style: const TextStyle(
                  color: AppColors.textMuted,
                  fontSize: 12,
                  fontWeight: FontWeight.w600)),
          const SizedBox(height: 6),
          TextField(
            controller: ctrl,
            keyboardType:
                const TextInputType.numberWithOptions(decimal: true),
            style: const TextStyle(color: AppColors.text, fontSize: 15),
            decoration: InputDecoration(
              prefixIcon:
                  Icon(icon, color: color ?? AppColors.textMuted, size: 20),
              filled: true,
              fillColor: AppColors.surface,
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
                  borderSide: BorderSide(
                      color: color ?? AppColors.primary, width: 2)),
            ),
          ),
        ],
      );

  String _timeAgo(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inSeconds < 60) return '${diff.inSeconds}s ago';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
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
    final bool isHorizontal = shape == 'cylindrical_horizontal';

    // Tank canvas dimensions
    final double tankW = isHorizontal ? 300 : 160;
    final double tankH = isHorizontal ? 150 : 280;

    final String statusLabel =
        pct > 60 ? 'NORMAL' : pct > 20 ? 'LOW' : 'CRITICAL';

    return Center(
      child: SizedBox(
        width: tankW,
        height: tankH,
        child: Stack(
          alignment: Alignment.center,
          children: [
            // Tank drawing
            CustomPaint(
              painter: _TankPainter(
                fillRatio: fillRatio,
                wavePhase: wavePhase,
                color: color,
                shape: shape,
              ),
              child: SizedBox(width: tankW, height: tankH),
            ),

            // Overlay text
            Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  '${pct.toStringAsFixed(1)}%',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: isHorizontal ? 30 : 38,
                    fontWeight: FontWeight.w900,
                    shadows: [
                      Shadow(
                          color: Colors.black.withOpacity(0.6),
                          blurRadius: 10),
                    ],
                  ),
                ),
                if (volumeL != null)
                  Text(
                    '${volumeL!.toStringAsFixed(1)} L',
                    style: TextStyle(
                      color: Colors.white.withOpacity(0.9),
                      fontSize: isHorizontal ? 13 : 15,
                      fontWeight: FontWeight.w600,
                      shadows: [
                        Shadow(
                            color: Colors.black.withOpacity(0.5),
                            blurRadius: 8),
                      ],
                    ),
                  ),
                const SizedBox(height: 6),
                Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 10, vertical: 3),
                  decoration: BoxDecoration(
                    color: Colors.black.withOpacity(0.35),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: color.withOpacity(0.6)),
                  ),
                  child: Text(
                    statusLabel,
                    style: TextStyle(
                      color: color,
                      fontSize: 10,
                      fontWeight: FontWeight.w800,
                      letterSpacing: 0.8,
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

// ── Tank painter ──────────────────────────────────────────────────────────────

class _TankPainter extends CustomPainter {
  final double fillRatio;  // 0.0 – 1.0 (pre-animated)
  final double wavePhase;  // 0 – 2π  (from repeating controller)
  final Color color;
  final String shape;

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

    // 1 ── Empty tank background
    canvas.drawPath(tank, Paint()
      ..color = color.withOpacity(0.08)
      ..style = PaintingStyle.fill);

    // 2 ── Level marker lines (25 / 50 / 75 %)
    _drawMarkers(canvas, w, h, tank);

    // 3 ── Fuel fill with animated wave surface
    if (fillRatio > 0.002) {
      final fuelTop = h * (1.0 - fillRatio.clamp(0.0, 1.0));
      const waveAmp = 5.0;
      const cycles  = 2.0;

      // Build wave-top fill path
      final fuel = Path()
        ..moveTo(-1, h + 1)
        ..lineTo(-1, fuelTop);
      for (double x = 0; x <= w + 1; x++) {
        fuel.lineTo(x,
            fuelTop +
                waveAmp *
                    math.sin(
                        x / w * cycles * 2 * math.pi + wavePhase));
      }
      fuel
        ..lineTo(w + 1, h + 1)
        ..close();

      canvas.save();
      canvas.clipPath(tank);

      // Base fill
      canvas.drawPath(fuel, Paint()
        ..color = color.withOpacity(0.50)
        ..style = PaintingStyle.fill);

      // Top-of-fill shimmer (lighter near wave line)
      final shimRect = Rect.fromLTWH(0, fuelTop, w, (h - fuelTop) * 0.35);
      canvas.drawRect(shimRect, Paint()
        ..style = PaintingStyle.fill
        ..shader = LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [
            color.withOpacity(0.22),
            color.withOpacity(0.0),
          ],
        ).createShader(shimRect));

      // Wave surface line
      final wave = Path()
        ..moveTo(-1, fuelTop + waveAmp * math.sin(wavePhase));
      for (double x = 0; x <= w + 1; x++) {
        wave.lineTo(x,
            fuelTop +
                waveAmp *
                    math.sin(
                        x / w * cycles * 2 * math.pi + wavePhase));
      }
      canvas.drawPath(wave, Paint()
        ..color = color.withOpacity(0.90)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.2);

      canvas.restore();
    }

    // 4 ── Left-edge highlight (glass / metal reflection)
    canvas.save();
    canvas.clipPath(tank);
    final highlightRect = Rect.fromLTWH(0, 0, w * 0.09, h);
    canvas.drawRect(highlightRect, Paint()
      ..style = PaintingStyle.fill
      ..shader = LinearGradient(
        begin: Alignment.centerLeft,
        end: Alignment.centerRight,
        colors: [
          Colors.white.withOpacity(0.07),
          Colors.transparent,
        ],
      ).createShader(highlightRect));
    canvas.restore();

    // 5 ── Tank outline
    canvas.drawPath(tank, Paint()
      ..color = color.withOpacity(0.65)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.5
      ..strokeJoin = StrokeJoin.round
      ..strokeCap = StrokeCap.round);
  }

  Path _tankPath(double w, double h) {
    switch (shape) {
      case 'cylindrical_horizontal':
        // Horizontal pill — corner radius = half the height
        return Path()
          ..addRRect(RRect.fromRectAndRadius(
              Rect.fromLTWH(0, 0, w, h), Radius.circular(h / 2)));
      case 'rectangular':
        // Box with small rounded corners
        return Path()
          ..addRRect(RRect.fromRectAndRadius(
              Rect.fromLTWH(0, 0, w, h), const Radius.circular(12)));
      default: // cylindrical_vertical
        // Vertical pill — corner radius = half the width
        return Path()
          ..addRRect(RRect.fromRectAndRadius(
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

// ── Shape selector ────────────────────────────────────────────────────────────

class _ShapeSelector extends StatelessWidget {
  final String selected;
  final ValueChanged<String> onChanged;
  final Map<String, String> labels;

  const _ShapeSelector({
    required this.selected,
    required this.onChanged,
    required this.labels,
  });

  @override
  Widget build(BuildContext context) => Column(
        children: labels.entries.map((e) {
          final sel = selected == e.key;
          return GestureDetector(
            onTap: () => onChanged(e.key),
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 150),
              margin: const EdgeInsets.only(bottom: 8),
              padding: const EdgeInsets.symmetric(
                  horizontal: 16, vertical: 13),
              decoration: BoxDecoration(
                color: sel
                    ? AppColors.primary.withOpacity(0.12)
                    : AppColors.surface,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                    color: sel ? AppColors.primary : AppColors.border,
                    width: sel ? 1.5 : 1),
              ),
              child: Row(
                children: [
                  Icon(
                    e.key == 'rectangular'
                        ? Icons.crop_square_rounded
                        : e.key == 'cylindrical_horizontal'
                            ? Icons.panorama_fish_eye_rounded
                            : Icons.circle_outlined,
                    color: sel ? AppColors.primary : AppColors.textMuted,
                    size: 20,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      e.value,
                      style: TextStyle(
                        color:
                            sel ? AppColors.text : AppColors.textMuted,
                        fontWeight:
                            sel ? FontWeight.w700 : FontWeight.normal,
                        fontSize: 14,
                      ),
                    ),
                  ),
                  if (sel)
                    const Icon(Icons.check_circle_rounded,
                        color: AppColors.primary, size: 18),
                ],
              ),
            ),
          );
        }).toList(),
      );
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
        padding:
            const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: color.withOpacity(0.25)),
        ),
        child: Row(
          children: [
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
          ],
        ),
      );
}
