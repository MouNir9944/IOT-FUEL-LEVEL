import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/constants.dart';
import '../core/socket_service.dart';
import '../models/device.dart';
import 'dart:convert';

class _LogEntry {
  final String message;
  final DateTime time;

  _LogEntry({required this.message, required this.time});
}

class TechnicianLogsScreen extends StatefulWidget {
  final Device device;

  const TechnicianLogsScreen({super.key, required this.device});

  @override
  State<TechnicianLogsScreen> createState() => _TechnicianLogsScreenState();
}

class _TechnicianLogsScreenState extends State<TechnicianLogsScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  final List<_LogEntry> _logs = [];
  Map<String, dynamic>? _config;
  final _calibCtrl = TextEditingController(
      text: '{\n  "tank_capacity_l": 1000,\n  "alert_threshold_pct": 20\n}');
  bool _sending = false;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 3, vsync: this);
    _loadConfig();
    _initSocket();
  }

  @override
  void dispose() {
    _tabs.dispose();
    _calibCtrl.dispose();
    SocketService.off('device_log', id: 'tech_logs');
    super.dispose();
  }

  Future<void> _loadConfig() async {
    try {
      final resp =
          await ApiClient.instance.get('/devices/${widget.device.id}/status');
      final d = Device.fromJson(
          Map<String, dynamic>.from(resp.data['device'] as Map));
      if (mounted && d.lastTelemetry != null) {
        final tel = d.lastTelemetry!;
        setState(() {
          _config = {
            'fuel_level_pct':      tel.fuelLevelPct,
            'fuel_volume_l':       tel.fuelVolumeL,
            'temperature_c':       tel.temperatureC,
            'battery_mv':          tel.batteryMv,
            'rssi':                tel.rssi,
            'alert_threshold_pct': tel.alertThresholdPct,
          };
        });
      }
    } catch (_) {}
  }

  void _initSocket() {
    SocketService.connect().then((socket) {
      SocketService.subscribeToDevice(widget.device.deviceId);
      SocketService.on('device_log', (data) {
        final map = Map<String, dynamic>.from(data as Map);
        if (map['device_id'] != widget.device.deviceId) return;
        if (mounted) {
          setState(() {
            _logs.insert(0, _LogEntry(
              message: map['log']?.toString() ?? '',
              time: DateTime.now(),
            ));
            if (_logs.length > 200) _logs.removeLast();
          });
        }
      }, id: 'tech_logs');
    });
  }

  Future<void> _sendCalibration() async {
    Map<String, dynamic> parsed;
    try {
      parsed = jsonDecode(_calibCtrl.text) as Map<String, dynamic>;
    } catch (_) {
      _snack('Invalid JSON', isError: true);
      return;
    }
    setState(() => _sending = true);
    try {
      await ApiClient.instance.post(
        '/devices/${widget.device.id}/command',
        data: {'cmd': 'set_config', 'value': parsed},
      );
      _snack('Calibration sent to device');
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    } finally {
      setState(() => _sending = false);
    }
  }

  void _snack(String msg, {bool isError = false}) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: isError ? AppColors.error : AppColors.success,
    ));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: Text('Tech – ${widget.device.displayName}'),
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.text,
        bottom: TabBar(
          controller: _tabs,
          labelColor: Colors.white,
          unselectedLabelColor: AppColors.textMuted,
          indicatorColor: AppColors.primary,
          tabs: const [
            Tab(text: 'Logs'),
            Tab(text: 'Config'),
            Tab(text: 'Calibration'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          // Logs tab
          _logs.isEmpty
              ? const Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CircularProgressIndicator(color: AppColors.primary),
                    SizedBox(height: 16),
                    Text('Waiting for live logs...',
                        style: TextStyle(color: AppColors.textMuted)),
                  ],
                )
              : ListView.builder(
                  padding: const EdgeInsets.all(12),
                  itemCount: _logs.length,
                  itemBuilder: (_, i) {
                    final l = _logs[i];
                    return Container(
                      margin: const EdgeInsets.only(bottom: 4),
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 7),
                      decoration: BoxDecoration(
                        color: AppColors.surface,
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            l.time.toLocal().toString().substring(11, 19),
                            style: const TextStyle(
                              color: AppColors.primary,
                              fontSize: 11,
                              fontFamily: 'monospace',
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: Text(
                              l.message,
                              style: const TextStyle(
                                color: AppColors.text,
                                fontSize: 12,
                                fontFamily: 'monospace',
                              ),
                            ),
                          ),
                        ],
                      ),
                    );
                  },
                ),

          // Config tab
          SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Last Known State',
                    style: TextStyle(
                        color: AppColors.text,
                        fontWeight: FontWeight.w700,
                        fontSize: 15)),
                const SizedBox(height: 12),
                _config == null
                    ? const Text('No data yet.',
                        style: TextStyle(color: AppColors.textMuted))
                    : Container(
                        width: double.infinity,
                        padding: const EdgeInsets.all(14),
                        decoration: BoxDecoration(
                          color: AppColors.surface,
                          borderRadius: BorderRadius.circular(10),
                        ),
                        child: Text(
                          const JsonEncoder.withIndent('  ').convert(_config),
                          style: const TextStyle(
                            color: AppColors.text,
                            fontSize: 12,
                            fontFamily: 'monospace',
                          ),
                        ),
                      ),
              ],
            ),
          ),

          // Calibration tab
          SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Send Calibration Config',
                    style: TextStyle(
                        color: AppColors.text,
                        fontWeight: FontWeight.w700,
                        fontSize: 15)),
                const SizedBox(height: 6),
                const Text('Edit the JSON payload to send to the device.',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 13)),
                const SizedBox(height: 14),
                TextField(
                  controller: _calibCtrl,
                  style: const TextStyle(
                      color: AppColors.text,
                      fontSize: 13,
                      fontFamily: 'monospace'),
                  maxLines: null,
                  minLines: 6,
                  autocorrect: false,
                  decoration: InputDecoration(
                    filled: true,
                    fillColor: AppColors.surface,
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(10),
                      borderSide: const BorderSide(color: AppColors.border),
                    ),
                    enabledBorder: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(10),
                      borderSide: const BorderSide(color: AppColors.border),
                    ),
                    focusedBorder: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(10),
                      borderSide:
                          const BorderSide(color: AppColors.primary, width: 2),
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton(
                    onPressed: _sending ? null : _sendCalibration,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.primary,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 14),
                      shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(10)),
                    ),
                    child: _sending
                        ? const CircularProgressIndicator(
                            color: Colors.white, strokeWidth: 2)
                        : const Text('Send to Device',
                            style: TextStyle(
                                fontWeight: FontWeight.w700, fontSize: 15)),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
