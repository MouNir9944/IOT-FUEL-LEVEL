import 'package:flutter/material.dart';
import '../core/constants.dart';
import '../models/device.dart';
import 'status_badge.dart';

class DeviceCard extends StatelessWidget {
  final Device device;
  final VoidCallback onTap;
  final VoidCallback? onControlTap;

  const DeviceCard({
    super.key,
    required this.device,
    required this.onTap,
    this.onControlTap,
  });

  @override
  Widget build(BuildContext context) {
    final t        = device.lastTelemetry;
    final isOnline = device.lastStatus == 'online';

    return GestureDetector(
      onTap: onTap,
      child: Container(
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(
            color: t != null && t.isCritical
                ? AppColors.fuelLow.withOpacity(0.6)
                : AppColors.border,
            width: t != null && t.isCritical ? 1.5 : 1.0,
          ),
        ),
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // ── Header ────────────────────────────────────────────────────────
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Fuel icon
                Container(
                  width: 40,
                  height: 40,
                  decoration: BoxDecoration(
                    color: (t != null
                            ? AppColors.fuelLevelColor(t.fuelLevelPct)
                            : AppColors.textMuted)
                        .withOpacity(0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Icon(
                    Icons.propane_tank_rounded,
                    color: t != null
                        ? AppColors.fuelLevelColor(t.fuelLevelPct)
                        : AppColors.textMuted,
                    size: 22,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        device.displayName,
                        style: const TextStyle(
                          color: AppColors.text,
                          fontSize: 16,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Text(
                        device.deviceId,
                        style: const TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 11,
                        ),
                      ),
                    ],
                  ),
                ),
                StatusBadge(status: device.lastStatus),
              ],
            ),

            // ── Fuel level gauge ──────────────────────────────────────────────
            if (t != null) ...[
              const SizedBox(height: 14),
              _FuelGaugeBar(telemetry: t),
              const SizedBox(height: 10),

              // Metrics row
              Container(
                padding:
                    const EdgeInsets.symmetric(vertical: 10, horizontal: 12),
                decoration: BoxDecoration(
                  color: AppColors.surfaceLight,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceAround,
                  children: [
                    _Metric(
                      '${t.fuelLevelPct.toStringAsFixed(1)}%',
                      'Fuel Level',
                      valueColor: AppColors.fuelLevelColor(t.fuelLevelPct),
                    ),
                    _Metric(
                      '${t.fuelVolumeL.toStringAsFixed(1)} L',
                      'Volume',
                    ),
                    if (t.temperatureC != null)
                      _Metric(
                        '${t.temperatureC!.toStringAsFixed(1)}°C',
                        'Temp',
                        valueColor:
                            t.temperatureC! > 50 ? AppColors.error : null,
                      ),
                    if (t.batteryMv != null)
                      _Metric(
                        t.isOnMains
                            ? 'Secteur'
                            : '${t.batteryPct!.toStringAsFixed(0)}%',
                        'Battery',
                        valueColor: t.isOnMains
                            ? AppColors.info
                            : t.batteryPct! < 20
                                ? AppColors.error
                                : null,
                      ),
                  ],
                ),
              ),

              // Critical / low fuel alert banner
              if (t.isCritical)
                _AlertBanner(
                  icon: Icons.warning_amber_rounded,
                  color: AppColors.fuelLow,
                  message:
                      'CRITICAL – Fuel below ${t.alertThresholdPct.toStringAsFixed(0)}%',
                )
              else if (t.isLow)
                _AlertBanner(
                  icon: Icons.warning_rounded,
                  color: AppColors.fuelMid,
                  message: 'Low fuel – consider refilling soon',
                ),
            ],

            // ── Monitor button ────────────────────────────────────────────────
            if (onControlTap != null) ...[
              const SizedBox(height: 10),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: onControlTap,
                  icon: Icon(
                    isOnline
                        ? Icons.monitor_rounded
                        : Icons.wifi_off_rounded,
                    size: 16,
                    color: isOnline ? Colors.white : AppColors.textMuted,
                  ),
                  label: Text(
                    isOnline ? 'Monitor Device' : 'Device Offline',
                    style: TextStyle(
                      fontWeight: FontWeight.w700,
                      color: isOnline ? Colors.white : AppColors.textMuted,
                    ),
                  ),
                  style: ElevatedButton.styleFrom(
                    backgroundColor:
                        isOnline ? AppColors.primary : AppColors.border,
                    disabledBackgroundColor: AppColors.border,
                    shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(10)),
                    padding: const EdgeInsets.symmetric(vertical: 10),
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

// ── Fuel gauge bar ────────────────────────────────────────────────────────────

class _FuelGaugeBar extends StatelessWidget {
  final Telemetry telemetry;
  const _FuelGaugeBar({required this.telemetry});

  @override
  Widget build(BuildContext context) {
    final pct   = telemetry.fuelLevelPct.clamp(0.0, 100.0);
    final color = AppColors.fuelLevelColor(pct);
    final ratio = pct / 100.0;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            const Text(
              'Fuel Level',
              style: TextStyle(color: AppColors.textMuted, fontSize: 11),
            ),
            Text(
              '${pct.toStringAsFixed(1)}%',
              style: TextStyle(
                color: color,
                fontSize: 12,
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 6),
        ClipRRect(
          borderRadius: BorderRadius.circular(6),
          child: LinearProgressIndicator(
            value: ratio,
            minHeight: 8,
            backgroundColor: color.withOpacity(0.15),
            valueColor: AlwaysStoppedAnimation<Color>(color),
          ),
        ),
      ],
    );
  }
}

// ── Alert banner ──────────────────────────────────────────────────────────────

class _AlertBanner extends StatelessWidget {
  final IconData icon;
  final Color color;
  final String message;
  const _AlertBanner(
      {required this.icon, required this.color, required this.message});

  @override
  Widget build(BuildContext context) => Container(
        margin: const EdgeInsets.only(top: 8),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
        decoration: BoxDecoration(
          color: color.withOpacity(0.1),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: color.withOpacity(0.4)),
        ),
        child: Row(
          children: [
            Icon(icon, size: 14, color: color),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                message,
                style: TextStyle(
                    color: color, fontSize: 11, fontWeight: FontWeight.w700),
              ),
            ),
          ],
        ),
      );
}

// ── Metric cell ───────────────────────────────────────────────────────────────

class _Metric extends StatelessWidget {
  final String value;
  final String label;
  final Color? valueColor;

  const _Metric(this.value, this.label, {this.valueColor});

  @override
  Widget build(BuildContext context) => Column(
        children: [
          Text(
            value,
            style: TextStyle(
              color: valueColor ?? AppColors.text,
              fontSize: 13,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 2),
          Text(label,
              style: const TextStyle(color: AppColors.textMuted, fontSize: 10)),
        ],
      );
}
