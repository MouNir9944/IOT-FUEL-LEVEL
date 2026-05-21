import 'dart:math' as math;
import 'package:dio/dio.dart';
import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/constants.dart';
import '../core/socket_service.dart';
import '../models/device.dart';
import '../widgets/status_badge.dart';
import 'planning_screen.dart';

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
    with SingleTickerProviderStateMixin {
  late Device _device;
  Telemetry? _telemetry;
  String _status = 'unknown';
  bool _sending  = false;
  bool _applying = false;
  bool _syncing  = false;
  String _controlMode = 'manual';
  late AnimationController _gaugeAnim;

  @override
  void initState() {
    super.initState();
    _device      = widget.device;
    _status      = _device.lastStatus;
    _telemetry   = _device.lastTelemetry;
    _controlMode = _device.effectiveControlMode;
    _gaugeAnim   = AnimationController(
        vsync: this, duration: const Duration(milliseconds: 900));
    _gaugeAnim.forward();
    _loadStatus();
    _initSocket();
  }

  @override
  void dispose() {
    _gaugeAnim.dispose();
    SocketService.off('telemetry_update',     id: 'device_control');
    SocketService.off('device_status_change', id: 'device_control');
    SocketService.removeReconnectCallback(_onSocketReconnect);
    super.dispose();
  }

  void _initSocket() {
    SocketService.addReconnectCallback(_onSocketReconnect);

    SocketService.on('telemetry_update', (data) {
      if (!mounted) return;
      final m = data as Map<String, dynamic>;
      if (m['device_id'] != _device.deviceId) return;
      setState(() => _telemetry = Telemetry.fromJson(m));
    }, id: 'device_control');

    SocketService.on('device_status_change', (data) {
      if (!mounted) return;
      final m = data as Map<String, dynamic>;
      if (m['device_id'] != _device.deviceId) return;
      setState(() => _status = m['status'] as String? ?? _status);
    }, id: 'device_control');
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
        _device      = d;
        _status      = d.lastStatus;
        _telemetry   = d.lastTelemetry;
        _controlMode = d.effectiveControlMode;
      });
      SocketService.subscribeToDevice(_device.deviceId);
    } catch (_) {}
  }

  Future<void> _sendCmd(Map<String, dynamic> cmd) async {
    if (_sending) return;
    setState(() => _sending = true);
    try {
      await ApiClient.instance.post(
          '/devices/${_device.id}/command', data: cmd);
    } on DioException catch (e) {
      if (!mounted) return;
      _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  Future<void> _togglePump() async {
    final newState = _telemetry?.isPumpOn == true ? 'STOP' : 'START';
    await _sendCmd({'cmd': 'set_pump', 'value': newState});
  }

  Future<void> _toggleControlMode() async {
    final newMode = _controlMode == 'auto' ? 'manual' : 'auto';
    setState(() => _applying = true);
    try {
      await ApiClient.instance.post(
        '/devices/${_device.id}/command',
        data: {'cmd': 'set_control_mode', 'value': newMode},
      );
      if (!mounted) return;
      setState(() => _controlMode = newMode);
    } on DioException catch (e) {
      if (!mounted) return;
      _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _applying = false);
    }
  }

  Future<void> _syncPlans() async {
    if (_syncing) return;
    setState(() => _syncing = true);
    try {
      final resp =
          await ApiClient.instance.post('/devices/${_device.id}/plans/sync');
      if (!mounted) return;
      final count = resp.data['count'] as int? ?? 0;
      _showSnack(count > 0
          ? '$count schedule(s) sent to device'
          : 'No schedules to send');
    } on DioException catch (e) {
      if (!mounted) return;
      _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _syncing = false);
    }
  }

  void _showSnack(String msg, {bool isError = false}) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: isError ? AppColors.error : AppColors.success,
    ));
  }

  bool get _isOnline => _status == 'online';

  @override
  Widget build(BuildContext context) {
    final t = _telemetry;

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        backgroundColor: AppColors.surface,
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              _device.displayName,
              style: const TextStyle(
                  color: AppColors.text,
                  fontSize: 16,
                  fontWeight: FontWeight.w700),
            ),
            StatusBadge(status: _status),
          ],
        ),
        actions: [
          if (widget.isAdmin)
            IconButton(
              icon: const Icon(Icons.calendar_month_rounded,
                  color: AppColors.textMuted),
              tooltip: 'Pump Schedule',
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (_) => PlanningScreen(
                      device: _device, isAdmin: widget.isAdmin),
                ),
              ),
            ),
          IconButton(
            icon: const Icon(Icons.refresh_rounded,
                color: AppColors.textMuted),
            onPressed: _loadStatus,
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: _loadStatus,
        color: AppColors.primary,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            // ── Fuel level gauge ─────────────────────────────────────────────
            _SectionCard(
              child: Column(
                children: [
                  const Text(
                    'Fuel Level',
                    style: TextStyle(
                        color: AppColors.textMuted,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                        letterSpacing: 0.5),
                  ),
                  const SizedBox(height: 16),
                  AnimatedBuilder(
                    animation: _gaugeAnim,
                    builder: (_, __) => _FuelGauge(
                      pct: (t?.fuelLevelPct ?? 0) * _gaugeAnim.value,
                      size: 180,
                    ),
                  ),
                  const SizedBox(height: 16),
                  // Metrics row
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      _MetricTile(
                        label: 'Volume',
                        value: t != null
                            ? '${t.fuelVolumeL.toStringAsFixed(1)} L'
                            : '—',
                        icon: Icons.opacity_rounded,
                        color: AppColors.info,
                      ),
                      if (t?.temperatureC != null)
                        _MetricTile(
                          label: 'Temperature',
                          value: '${t!.temperatureC!.toStringAsFixed(1)} °C',
                          icon: Icons.thermostat_rounded,
                          color: t.temperatureC! > 50
                              ? AppColors.error
                              : AppColors.warning,
                        ),
                      _MetricTile(
                        label: 'Status',
                        value: _status.toUpperCase(),
                        icon: _isOnline
                            ? Icons.wifi_rounded
                            : Icons.wifi_off_rounded,
                        color: AppColors.statusColor(_status),
                      ),
                    ],
                  ),
                  if (t != null) ...[
                    const SizedBox(height: 12),
                    Text(
                      'Updated ${_timeAgo(t.timestamp)}',
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 10),
                    ),
                  ],
                ],
              ),
            ),

            const SizedBox(height: 12),

            // ── Control mode ─────────────────────────────────────────────────
            _SectionCard(
              child: Row(
                children: [
                  Icon(
                    _controlMode == 'auto'
                        ? Icons.calendar_month_rounded
                        : Icons.tune_rounded,
                    color: _controlMode == 'auto'
                        ? AppColors.primary
                        : AppColors.warning,
                    size: 20,
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          _controlMode == 'auto'
                              ? 'Auto (Schedule)'
                              : 'Manual Control',
                          style: TextStyle(
                            color: _controlMode == 'auto'
                                ? AppColors.primary
                                : AppColors.warning,
                            fontWeight: FontWeight.w700,
                            fontSize: 14,
                          ),
                        ),
                        Text(
                          _controlMode == 'auto'
                              ? 'Pump follows your schedule'
                              : 'Pump controlled manually below',
                          style: const TextStyle(
                              color: AppColors.textMuted, fontSize: 11),
                        ),
                      ],
                    ),
                  ),
                  if (_applying)
                    const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(
                          strokeWidth: 2, color: AppColors.primary),
                    )
                  else
                    Switch(
                      value: _controlMode == 'auto',
                      onChanged:
                          _isOnline ? (_) => _toggleControlMode() : null,
                      activeColor: AppColors.primary,
                    ),
                ],
              ),
            ),

            // ── Pump control (manual mode only) ──────────────────────────────
            if (_controlMode != 'auto') ...[
              const SizedBox(height: 12),
              _SectionCard(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Pump Control',
                      style: TextStyle(
                          color: AppColors.text,
                          fontWeight: FontWeight.w700,
                          fontSize: 15),
                    ),
                    const SizedBox(height: 14),
                    Row(
                      children: [
                        Expanded(
                          child: _PumpButton(
                            label: 'Start Pump',
                            icon: Icons.play_circle_filled_rounded,
                            color: AppColors.success,
                            enabled: _isOnline && !_sending &&
                                t?.isPumpOn != true,
                            active: t?.isPumpOn == true,
                            onTap: _togglePump,
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: _PumpButton(
                            label: 'Stop Pump',
                            icon: Icons.stop_circle_rounded,
                            color: AppColors.error,
                            enabled: _isOnline && !_sending &&
                                t?.isPumpOn == true,
                            active: t?.isPumpOn != true,
                            onTap: _togglePump,
                          ),
                        ),
                      ],
                    ),
                    if (t != null) ...[
                      const SizedBox(height: 10),
                      Row(
                        children: [
                          _PumpStateDot(isRunning: t.isPumpOn),
                          const SizedBox(width: 6),
                          Text(
                            'Pump is ${t.isPumpOn ? "RUNNING" : "STOPPED"}',
                            style: const TextStyle(
                                color: AppColors.textMuted, fontSize: 12),
                          ),
                        ],
                      ),
                    ],
                  ],
                ),
              ),
            ],

            const SizedBox(height: 12),

            // ── Alert threshold ───────────────────────────────────────────────
            if (t != null)
              _SectionCard(
                child: Row(
                  children: [
                    Container(
                      padding: const EdgeInsets.all(8),
                      decoration: BoxDecoration(
                        color: AppColors.fuelLow.withOpacity(0.12),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: const Icon(Icons.notifications_active_rounded,
                          color: AppColors.fuelLow, size: 18),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text(
                            'Low Fuel Alert',
                            style: TextStyle(
                                color: AppColors.text,
                                fontWeight: FontWeight.w700,
                                fontSize: 14),
                          ),
                          Text(
                            'Triggers at ${t.alertThresholdPct.toStringAsFixed(0)}% fuel level',
                            style: const TextStyle(
                                color: AppColors.textMuted, fontSize: 12),
                          ),
                        ],
                      ),
                    ),
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 5),
                      decoration: BoxDecoration(
                        color: t.isCritical
                            ? AppColors.fuelLow.withOpacity(0.15)
                            : AppColors.success.withOpacity(0.1),
                        borderRadius: BorderRadius.circular(20),
                        border: Border.all(
                          color: t.isCritical
                              ? AppColors.fuelLow.withOpacity(0.5)
                              : AppColors.success.withOpacity(0.4),
                        ),
                      ),
                      child: Text(
                        t.isCritical ? 'TRIGGERED' : 'OK',
                        style: TextStyle(
                          color: t.isCritical
                              ? AppColors.fuelLow
                              : AppColors.success,
                          fontSize: 11,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                    ),
                  ],
                ),
              ),

            const SizedBox(height: 12),

            // ── Active schedule ───────────────────────────────────────────────
            if (t != null && _controlMode == 'auto')
              _SectionCard(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Active Pump Schedule',
                      style: TextStyle(
                          color: AppColors.text,
                          fontWeight: FontWeight.w700,
                          fontSize: 15),
                    ),
                    const SizedBox(height: 10),
                    if (t.planActive == true && t.planName != null)
                      Container(
                        padding: const EdgeInsets.all(10),
                        decoration: BoxDecoration(
                          color: AppColors.primary.withOpacity(0.1),
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(
                              color: AppColors.primary.withOpacity(0.3)),
                        ),
                        child: Row(
                          children: [
                            const Icon(Icons.play_circle_outline_rounded,
                                size: 18, color: AppColors.primary),
                            const SizedBox(width: 8),
                            Expanded(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    t.planName!,
                                    style: const TextStyle(
                                        color: AppColors.primary,
                                        fontWeight: FontWeight.w700,
                                        fontSize: 13),
                                  ),
                                  if (t.sliceStart != null &&
                                      t.sliceStop != null)
                                    Text(
                                      '${t.sliceStart} → ${t.sliceStop}',
                                      style: const TextStyle(
                                          color: AppColors.textMuted,
                                          fontSize: 11),
                                    ),
                                ],
                              ),
                            ),
                          ],
                        ),
                      )
                    else
                      Row(
                        children: const [
                          Icon(Icons.schedule_rounded,
                              size: 14, color: AppColors.textMuted),
                          SizedBox(width: 6),
                          Text(
                            'No active pump schedule right now',
                            style: TextStyle(
                                color: AppColors.textMuted, fontSize: 12),
                          ),
                        ],
                      ),
                    const SizedBox(height: 12),
                    SizedBox(
                      width: double.infinity,
                      child: ElevatedButton.icon(
                        onPressed:
                            _isOnline && !_syncing ? _syncPlans : null,
                        icon: _syncing
                            ? const SizedBox(
                                width: 14,
                                height: 14,
                                child: CircularProgressIndicator(
                                    strokeWidth: 2, color: Colors.white),
                              )
                            : const Icon(Icons.send_rounded, size: 16),
                        label: Text(
                            _syncing ? 'Sending…' : 'Send Schedules to Device'),
                        style: ElevatedButton.styleFrom(
                          backgroundColor: AppColors.primary,
                          foregroundColor: Colors.white,
                          padding: const EdgeInsets.symmetric(vertical: 11),
                          shape: RoundedRectangleBorder(
                              borderRadius: BorderRadius.circular(10)),
                        ),
                      ),
                    ),
                  ],
                ),
              ),

            const SizedBox(height: 24),
          ],
        ),
      ),
    );
  }

  String _timeAgo(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inSeconds < 60) return '${diff.inSeconds}s ago';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
    return '${diff.inHours}h ago';
  }
}

// ── Circular fuel gauge ───────────────────────────────────────────────────────

class _FuelGauge extends StatelessWidget {
  final double pct;
  final double size;

  const _FuelGauge({required this.pct, required this.size});

  @override
  Widget build(BuildContext context) {
    final clampedPct = pct.clamp(0.0, 100.0);
    final color      = AppColors.fuelLevelColor(clampedPct);
    final ratio      = clampedPct / 100.0;

    String label;
    if (clampedPct > 60) label = 'NORMAL';
    else if (clampedPct > 20) label = 'LOW';
    else label = 'CRITICAL';

    return SizedBox(
      width: size,
      height: size,
      child: Stack(
        alignment: Alignment.center,
        children: [
          // Background arc
          CustomPaint(
            size: Size(size, size),
            painter: _ArcPainter(
              ratio: 1.0,
              color: color.withOpacity(0.12),
              strokeWidth: 14,
            ),
          ),
          // Foreground arc
          CustomPaint(
            size: Size(size, size),
            painter: _ArcPainter(
              ratio: ratio,
              color: color,
              strokeWidth: 14,
            ),
          ),
          // Center text
          Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.local_gas_station_rounded, color: color, size: 28),
              const SizedBox(height: 4),
              Text(
                '${clampedPct.toStringAsFixed(1)}%',
                style: TextStyle(
                  color: color,
                  fontSize: 28,
                  fontWeight: FontWeight.w900,
                ),
              ),
              const SizedBox(height: 2),
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 10, vertical: 3),
                decoration: BoxDecoration(
                  color: color.withOpacity(0.15),
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(color: color.withOpacity(0.4)),
                ),
                child: Text(
                  label,
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
    );
  }
}

class _ArcPainter extends CustomPainter {
  final double ratio;
  final Color color;
  final double strokeWidth;

  const _ArcPainter(
      {required this.ratio, required this.color, required this.strokeWidth});

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Rect.fromCircle(
      center: Offset(size.width / 2, size.height / 2),
      radius: size.width / 2 - strokeWidth / 2,
    );
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth
      ..strokeCap = StrokeCap.round;

    const startAngle = math.pi * 0.75;
    final sweepAngle = math.pi * 1.5 * ratio;
    canvas.drawArc(rect, startAngle, sweepAngle, false, paint);
  }

  @override
  bool shouldRepaint(_ArcPainter old) =>
      old.ratio != ratio || old.color != color;
}

// ── Section card ──────────────────────────────────────────────────────────────

class _SectionCard extends StatelessWidget {
  final Widget child;
  const _SectionCard({required this.child});

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: AppColors.border),
        ),
        child: child,
      );
}

// ── Pump button ───────────────────────────────────────────────────────────────

class _PumpButton extends StatelessWidget {
  final String label;
  final IconData icon;
  final Color color;
  final bool enabled;
  final bool active;
  final VoidCallback onTap;

  const _PumpButton({
    required this.label,
    required this.icon,
    required this.color,
    required this.enabled,
    required this.active,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) => GestureDetector(
        onTap: enabled ? onTap : null,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 200),
          padding: const EdgeInsets.symmetric(vertical: 16),
          decoration: BoxDecoration(
            color: active ? color : color.withOpacity(0.1),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: color.withOpacity(active ? 1 : 0.4)),
          ),
          child: Column(
            children: [
              Icon(icon,
                  color: active ? Colors.white : color, size: 26),
              const SizedBox(height: 6),
              Text(
                label,
                style: TextStyle(
                  color: active ? Colors.white : color,
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
        ),
      );
}

// ── Pump state dot ────────────────────────────────────────────────────────────

class _PumpStateDot extends StatelessWidget {
  final bool isRunning;
  const _PumpStateDot({required this.isRunning});

  @override
  Widget build(BuildContext context) => Container(
        width: 8,
        height: 8,
        decoration: BoxDecoration(
          color: isRunning ? AppColors.success : AppColors.textMuted,
          shape: BoxShape.circle,
        ),
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
        padding: const EdgeInsets.all(12),
        constraints: const BoxConstraints(minWidth: 80),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: color.withOpacity(0.25)),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(icon, color: color, size: 18),
            const SizedBox(height: 6),
            Text(
              value,
              style: TextStyle(
                  color: color, fontSize: 15, fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 2),
            Text(label,
                style: const TextStyle(
                    color: AppColors.textMuted, fontSize: 11)),
          ],
        ),
      );
}
