import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/constants.dart';
import '../models/device.dart';
import '../widgets/status_badge.dart';

class DeviceHistoryScreen extends StatefulWidget {
  final Device device;

  const DeviceHistoryScreen({super.key, required this.device});

  @override
  State<DeviceHistoryScreen> createState() => _DeviceHistoryScreenState();
}

class _DeviceHistoryScreenState extends State<DeviceHistoryScreen> {
  Device? _device;
  List<ConnectionLog> _logs = [];
  bool _loading = false;

  @override
  void initState() {
    super.initState();
    _fetch();
  }

  Future<void> _fetch() async {
    setState(() => _loading = true);
    try {
      final resp =
          await ApiClient.instance.get('/devices/${widget.device.id}/status');
      setState(() {
        _device = Device.fromJson(
            Map<String, dynamic>.from(resp.data['device'] as Map));
        _logs = (resp.data['connection_history'] as List)
            .map((l) =>
                ConnectionLog.fromJson(Map<String, dynamic>.from(l as Map)))
            .toList();
      });
    } catch (_) {} finally {
      setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final d = _device ?? widget.device;
    final t = d.lastTelemetry;

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: Text('History – ${d.displayName}'),
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.text,
      ),
      body: _loading && _logs.isEmpty
          ? const Center(
              child: CircularProgressIndicator(color: AppColors.primary))
          : RefreshIndicator(
              color: AppColors.primary,
              backgroundColor: AppColors.surface,
              onRefresh: _fetch,
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  // ── Device info card ──────────────────────────────────────
                  Container(
                    padding: const EdgeInsets.all(16),
                    margin: const EdgeInsets.only(bottom: 20),
                    decoration: BoxDecoration(
                      color: AppColors.surface,
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: AppColors.border),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          children: [
                            StatusBadge(status: d.lastStatus),
                            if (t != null)
                              Row(
                                children: [
                                  Icon(
                                    Icons.local_gas_station_rounded,
                                    size: 14,
                                    color: AppColors.fuelLevelColor(
                                        t.fuelLevelPct),
                                  ),
                                  const SizedBox(width: 4),
                                  Text(
                                    '${t.fuelLevelPct.toStringAsFixed(1)}%'
                                    ' · ${t.fuelVolumeL.toStringAsFixed(1)} L'
                                    '${t.temperatureC != null ? ' · ${t.temperatureC!.toStringAsFixed(1)}°C' : ''}',
                                    style: TextStyle(
                                      color: AppColors.fuelLevelColor(
                                          t.fuelLevelPct),
                                      fontSize: 12,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                ],
                              ),
                          ],
                        ),
                        if (d.lastTelemetryAt != null) ...[
                          const SizedBox(height: 6),
                          Text(
                            'Last telemetry: ${_fmt(d.lastTelemetryAt!)}',
                            style: const TextStyle(
                                color: AppColors.textMuted, fontSize: 11),
                          ),
                        ],
                        if (d.lastLwtAt != null) ...[
                          const SizedBox(height: 2),
                          Text(
                            'Last LWT: ${_fmt(d.lastLwtAt!)}'
                            '${d.lastLwtPayload?['reason'] != null ? ' (${d.lastLwtPayload!['reason']})' : ''}',
                            style: const TextStyle(
                                color: AppColors.textMuted, fontSize: 11),
                          ),
                        ],
                      ],
                    ),
                  ),

                  // ── Connection timeline ───────────────────────────────────
                  const Padding(
                    padding: EdgeInsets.only(bottom: 10),
                    child: Text(
                      'CONNECTION TIMELINE',
                      style: TextStyle(
                        color: AppColors.textMuted,
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                        letterSpacing: 0.5,
                      ),
                    ),
                  ),

                  if (_logs.isEmpty)
                    const Center(
                      child: Text('No connection history.',
                          style: TextStyle(color: AppColors.textMuted)),
                    )
                  else
                    ...List.generate(_logs.length, (i) {
                      final log    = _logs[i];
                      final isLast = i == _logs.length - 1;
                      return IntrinsicHeight(
                        child: Row(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            // Timeline dots
                            SizedBox(
                              width: 24,
                              child: Column(
                                children: [
                                  Container(
                                    width: 12,
                                    height: 12,
                                    margin: const EdgeInsets.only(top: 4),
                                    decoration: BoxDecoration(
                                      shape: BoxShape.circle,
                                      color: log.status == 'online'
                                          ? AppColors.success
                                          : AppColors.error,
                                    ),
                                  ),
                                  if (!isLast)
                                    Expanded(
                                      child: Container(
                                          width: 2,
                                          color: AppColors.border),
                                    ),
                                ],
                              ),
                            ),
                            const SizedBox(width: 10),
                            Expanded(
                              child: Padding(
                                padding: const EdgeInsets.only(bottom: 16),
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    Row(
                                      children: [
                                        Text(
                                          log.status.toUpperCase(),
                                          style: TextStyle(
                                            color: log.status == 'online'
                                                ? AppColors.success
                                                : AppColors.error,
                                            fontWeight: FontWeight.w700,
                                            fontSize: 13,
                                          ),
                                        ),
                                        if (log.reason != null) ...[
                                          const SizedBox(width: 8),
                                          Container(
                                            padding: const EdgeInsets.symmetric(
                                                horizontal: 8, vertical: 2),
                                            decoration: BoxDecoration(
                                              color: AppColors.surfaceLight,
                                              borderRadius:
                                                  BorderRadius.circular(6),
                                            ),
                                            child: Text(
                                              log.reason!,
                                              style: const TextStyle(
                                                  color: AppColors.textMuted,
                                                  fontSize: 10),
                                            ),
                                          ),
                                        ],
                                      ],
                                    ),
                                    const SizedBox(height: 2),
                                    Text(
                                      _fmt(log.createdAt),
                                      style: const TextStyle(
                                          color: AppColors.textMuted,
                                          fontSize: 11),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          ],
                        ),
                      );
                    }),
                ],
              ),
            ),
    );
  }

  String _fmt(DateTime dt) =>
      dt.toLocal().toString().substring(0, 19).replaceFirst('T', ' ');
}
