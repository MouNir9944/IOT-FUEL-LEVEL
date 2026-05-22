import 'dart:convert';
import 'package:dio/dio.dart';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../core/api_client.dart';
import '../../core/constants.dart';
import '../../core/app_strings.dart';
import '../../core/socket_service.dart';
import '../../providers/auth_provider.dart';
import '../profile_screen.dart';
import '../../widgets/pill_nav_bar.dart';

// â”€â”€ Data models â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

class _AdminItem {
  final String id;
  final String email;
  final String fullName;
  bool active;
  final int siteCount;
  final int deviceCount;

  _AdminItem({
    required this.id,
    required this.email,
    required this.fullName,
    required this.active,
    required this.siteCount,
    required this.deviceCount,
  });

  factory _AdminItem.fromJson(Map<String, dynamic> j) => _AdminItem(
        id: j['id']?.toString() ?? '',
        email: j['email'] as String? ?? '',
        fullName: j['full_name'] as String? ?? '',
        active: (j['active'] as bool?) ?? true,
        siteCount: int.tryParse(j['site_count']?.toString() ?? '0') ?? 0,
        deviceCount: int.tryParse(j['device_count']?.toString() ?? '0') ?? 0,
      );
}

class _LogItem {
  final String id;
  final String deviceStrId;
  final String siteName;
  final String status;
  final String? reason;
  final DateTime createdAt;

  _LogItem({
    required this.id,
    required this.deviceStrId,
    required this.siteName,
    required this.status,
    this.reason,
    required this.createdAt,
  });

  factory _LogItem.fromJson(Map<String, dynamic> j) => _LogItem(
        id: j['id']?.toString() ?? '',
        deviceStrId: j['device_str_id'] as String? ?? '',
        siteName: j['site_name'] as String? ?? '',
        status: j['status'] as String? ?? '',
        reason: j['reason'] as String?,
        createdAt: DateTime.tryParse(j['created_at'] as String? ?? '') ?? DateTime.now(),
      );
}


// ── Live MQTT device log entry ────────────────────────────────────────────────
// The firmware sends JSON: {"level":"Failed","code":"NO_CHANGE","message":"...","ts":…}
// or plain strings.  We parse both formats and infer severity.

class _LiveLog {
  final String deviceId;
  final String raw;
  final String level;    // normalised lowercase
  final String code;
  final String message;
  final DateTime time;

  _LiveLog({
    required this.deviceId,
    required this.raw,
    required this.level,
    required this.code,
    required this.message,
    required this.time,
  });

  factory _LiveLog.parse(String deviceId, String raw) {
    String level = 'info', code = '', message = raw;
    try {
      final j = jsonDecode(raw) as Map<String, dynamic>;
      level   = (j['level']   as String?)?.toLowerCase() ?? 'info';
      code    = (j['code']    as String?) ?? '';
      message = (j['message'] as String?) ?? raw;
    } catch (_) {
      // Plain string — infer level from ESP-IDF prefix (I/W/E/D)
      if (raw.length > 2 && raw[1] == ' ') {
        switch (raw[0]) {
          case 'E': level = 'error';   break;
          case 'W': level = 'warn';    break;
          case 'D': level = 'debug';   break;
          case 'I': level = 'info';    break;
        }
      }
    }
    return _LiveLog(
      deviceId: deviceId,
      raw:      raw,
      level:    level,
      code:     code,
      message:  message,
      time:     DateTime.now(),
    );
  }

  Color get levelColor {
    switch (level) {
      case 'error':
      case 'failed':
      case 'err':    return AppColors.error;
      case 'warn':
      case 'warning':return AppColors.warning;
      case 'success':return AppColors.success;
      case 'debug':  return AppColors.textMuted;
      default:       return AppColors.primary;
    }
  }

  String get levelBadge {
    switch (level) {
      case 'failed': return 'FAIL';
      case 'error':
      case 'err':    return 'ERR';
      case 'warn':
      case 'warning':return 'WARN';
      case 'success':return 'OK';
      case 'debug':  return 'DBG';
      default:       return 'INFO';
    }
  }
}

class _DeviceItem {
  final String id;         // MongoDB _id as string
  final String deviceId;   // e.g. "AABBCCDDEEFF"
  final String name;
  final String siteName;
  final String lastStatus; // 'online' | 'offline' | 'unstable' | ''
  final String? swVersion;
  final String? hwVersion;

  _DeviceItem({
    required this.id,
    required this.deviceId,
    required this.name,
    required this.siteName,
    required this.lastStatus,
    this.swVersion,
    this.hwVersion,
  });

  factory _DeviceItem.fromJson(Map<String, dynamic> j) => _DeviceItem(
        id: (j['_id'] ?? j['id'])?.toString() ?? '',
        deviceId: j['device_id'] as String? ?? '',
        name: j['name'] as String? ?? '',
        siteName: j['site_name'] as String? ?? '',
        lastStatus: j['last_status'] as String? ?? 'offline',
        swVersion: j['sw_version'] as String?,
        hwVersion: j['hw_version'] as String?,
      );
}

// â”€â”€ Dashboard â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

class SuperadminDashboard extends StatefulWidget {
  const SuperadminDashboard({super.key});

  @override
  State<SuperadminDashboard> createState() => _SuperadminDashboardState();
}

class _SuperadminDashboardState extends State<SuperadminDashboard> {
  int _navIndex = 0;

  // Admins tab
  List<_AdminItem> _admins = [];

  // LWT logs tab
  List<_LogItem> _logs = [];

  // Devices tab
  List<_DeviceItem> _devices = [];

  // Live device MQTT logs tab
  final List<_LiveLog> _liveLogs = [];
  String? _logFilterDeviceId; // null = show all devices

  bool _loading = false;

  static const _purple = Color(0xFFA855F7);

  @override
  void initState() {
    super.initState();
    _fetch();
    // Listen to all device MQTT log events.
    // Device rooms are joined after _fetch() completes.
    SocketService.on('device_log', _onDeviceLog, id: 'superadmin_live_logs');
  }

  @override
  void dispose() {
    SocketService.off('device_log', id: 'superadmin_live_logs');
    super.dispose();
  }

  void _onDeviceLog(dynamic data) {
    if (!mounted) return;
    final m         = Map<String, dynamic>.from(data as Map);
    final deviceId  = m['device_id']  as String? ?? '';
    final rawMsg    = m['log']?.toString() ?? '';
    setState(() {
      _liveLogs.insert(0, _LiveLog.parse(deviceId, rawMsg));
      if (_liveLogs.length > 600) _liveLogs.removeLast();
    });
  }

  // â”€â”€ Fetch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Future<void> _fetch() async {
    setState(() => _loading = true);
    try {
      final results = await Future.wait([
        ApiClient.instance.get('/superadmin/admins'),
        ApiClient.instance.get('/superadmin/connection-logs?limit=50'),
        ApiClient.instance.get('/superadmin/devices/all'),
      ]);
      final devData = results[2].data as Map<String, dynamic>;
      setState(() {
        _admins = (results[0].data['admins'] as List)
            .map((a) => _AdminItem.fromJson(Map<String, dynamic>.from(a as Map)))
            .toList();
        _logs = (results[1].data['logs'] as List)
            .map((l) => _LogItem.fromJson(Map<String, dynamic>.from(l as Map)))
            .toList();
        _devices = (devData['devices'] as List)
            .map((d) => _DeviceItem.fromJson(Map<String, dynamic>.from(d as Map)))
            .toList();
      });
    } catch (_) {
    } finally {
      setState(() => _loading = false);
    }
    // Ensure the socket is connected, then subscribe to every device room so
    // the Logs tab receives device_log events.  Using connect() here (not just
    // subscribeToDevice) because the superadmin screen may be opened before any
    // other screen that establishes the socket connection.
    SocketService.connect().then((_) {
      for (final d in _devices) {
        SocketService.subscribeToDevice(d.deviceId);
      }
    });
  }

  // â”€â”€ Admin suspend/restore â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Future<void> _suspend(String id, String name) async {
    final s = AppStrings.read(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        backgroundColor: AppColors.surface,
        title: Text(name,
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700)),
        content: Text(s.areYouSure,
            style: const TextStyle(color: AppColors.textMuted)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(s.cancel,
                style: const TextStyle(color: AppColors.textMuted)),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(s.confirm,
                style: const TextStyle(
                    color: AppColors.primary, fontWeight: FontWeight.w700)),
          ),
        ],
      ),
    );
    if (ok != true) return;
    try {
      final resp =
          await ApiClient.instance.patch('/superadmin/admins/$id/suspend');
      final updated = resp.data['admin'] as Map<String, dynamic>;
      setState(() {
        final idx = _admins.indexWhere((a) => a.id == id);
        if (idx != -1) _admins[idx].active = updated['active'] as bool;
      });
    } catch (_) {}
  }

  // â”€â”€ Build â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  @override
  Widget build(BuildContext context) {
    final s = AppStrings.of(context);
    final auth = context.watch<AuthProvider>();
    final initials = _initials(auth.user?.displayName ?? '?');

    return Scaffold(
      backgroundColor: AppColors.background,
      bottomNavigationBar: PillNavBar(
        items: [
          PillNavItem(icon: Icons.admin_panel_settings_rounded, label: 'Admins'),
          PillNavItem(icon: Icons.history_rounded,               label: 'LWT'),
          PillNavItem(icon: Icons.memory_rounded,                label: 'Devices'),
          PillNavItem(icon: Icons.terminal_rounded,              label: 'Logs'),
        ],
        selected:  _navIndex,
        onTap:     (i) => setState(() => _navIndex = i),
        activeColor: _purple,
      ),
      body: NestedScrollView(
        key: ValueKey(_navIndex),
        headerSliverBuilder: (_, __) => [
          SliverAppBar(
            expandedHeight: 130,
            pinned: true,
            backgroundColor: AppColors.surface,
            foregroundColor: AppColors.text,
            automaticallyImplyLeading: false,
            elevation: 0,
            flexibleSpace: FlexibleSpaceBar(
              collapseMode: CollapseMode.pin,
              background: Container(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    colors: [Color(0xFF0F172A), Color(0xFF1E293B)],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
                child: Stack(
                  children: [
                    Positioned(
                      top: -30,
                      right: -30,
                      child: Container(
                        width: 150,
                        height: 150,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          gradient: RadialGradient(colors: [
                            _purple.withOpacity(0.2),
                            Colors.transparent,
                          ]),
                        ),
                      ),
                    ),
                    SafeArea(
                      child: Padding(
                        padding: const EdgeInsets.fromLTRB(20, 16, 20, 0),
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          crossAxisAlignment: CrossAxisAlignment.center,
                          children: [
                            Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                const Text(
                                  'Superadmin Panel',
                                  style: TextStyle(
                                    color: AppColors.text,
                                    fontSize: 22,
                                    fontWeight: FontWeight.w800,
                                  ),
                                ),
                                const SizedBox(height: 4),
                                _superBadge(),
                              ],
                            ),
                            GestureDetector(
                              onTap: () => Navigator.of(context).push(
                                MaterialPageRoute(
                                    builder: (_) => const ProfileScreen()),
                              ),
                              child: Container(
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  gradient: const LinearGradient(
                                    colors: [
                                      Color(0xFFA855F7),
                                      Color(0xFF7C3AED)
                                    ],
                                    begin: Alignment.topLeft,
                                    end: Alignment.bottomRight,
                                  ),
                                  shape: BoxShape.circle,
                                  boxShadow: [
                                    BoxShadow(
                                      color: _purple.withOpacity(0.4),
                                      blurRadius: 12,
                                      offset: const Offset(0, 4),
                                    ),
                                  ],
                                ),
                                child: Center(
                                  child: Text(
                                    initials,
                                    style: const TextStyle(
                                      color: Colors.white,
                                      fontWeight: FontWeight.w800,
                                      fontSize: 15,
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              title: const Padding(
                padding: EdgeInsets.only(left: 4),
                child: Text(
                  'Superadmin',
                  style: TextStyle(
                    color: AppColors.text,
                    fontWeight: FontWeight.w700,
                    fontSize: 16,
                  ),
                ),
              ),
              titlePadding:
                  const EdgeInsetsDirectional.fromSTEB(20, 0, 0, 16),
            ),
          ),
        ],
        body: _loading && _admins.isEmpty
            ? const Center(
                child: CircularProgressIndicator(color: Color(0xFFA855F7)))
            : IndexedStack(
                index: _navIndex,
                children: [
                  _adminsTab(s),
                  _logsTab(s),
                  _devicesTab(),
                  _liveLogsTab(),
                ],
              ),
      ),
    );
  }

  // â”€â”€ Admins tab â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Widget _adminsTab(AppStrings s) => RefreshIndicator(
        color: _purple,
        backgroundColor: AppColors.surface,
        onRefresh: _fetch,
        child: _admins.isEmpty
            ? ListView(
                padding: const EdgeInsets.all(32),
                children: [
                  const SizedBox(height: 80),
                  Center(
                    child: Column(children: [
                      Icon(Icons.admin_panel_settings_rounded,
                          color: AppColors.textMuted.withOpacity(0.3),
                          size: 60),
                      const SizedBox(height: 12),
                      const Text('No admins yet.',
                          style: TextStyle(color: AppColors.textMuted)),
                    ]),
                  ),
                ],
              )
            : ListView.builder(
                padding: const EdgeInsets.fromLTRB(16, 16, 16, 80),
                itemCount: _admins.length,
                itemBuilder: (_, i) {
                  final a = _admins[i];
                  final initials = _initials(a.fullName);
                  return Container(
                    margin: const EdgeInsets.only(bottom: 10),
                    decoration: BoxDecoration(
                      color: AppColors.surface,
                      borderRadius: BorderRadius.circular(14),
                      border: Border.all(color: AppColors.border),
                    ),
                    child: ListTile(
                      contentPadding: const EdgeInsets.symmetric(
                          horizontal: 16, vertical: 8),
                      leading: Container(
                        width: 44,
                        height: 44,
                        decoration: BoxDecoration(
                          gradient: LinearGradient(
                            colors: a.active
                                ? [
                                    AppColors.primary,
                                    const Color(0xFF38BDF8)
                                  ]
                                : [
                                    AppColors.textMuted,
                                    AppColors.textMuted
                                  ],
                          ),
                          shape: BoxShape.circle,
                        ),
                        child: Center(
                          child: Text(
                            initials,
                            style: const TextStyle(
                              color: Colors.white,
                              fontWeight: FontWeight.w700,
                              fontSize: 14,
                            ),
                          ),
                        ),
                      ),
                      title: Text(a.fullName,
                          style: const TextStyle(
                              color: AppColors.text,
                              fontWeight: FontWeight.w700,
                              fontSize: 14)),
                      subtitle: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(a.email,
                              style: const TextStyle(
                                  color: AppColors.textMuted, fontSize: 11)),
                          const SizedBox(height: 2),
                          Text(
                            '${a.siteCount} sites  â€¢  ${a.deviceCount} devices',
                            style: const TextStyle(
                                color: AppColors.primary, fontSize: 11),
                          ),
                        ],
                      ),
                      trailing: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Container(
                              width: 8,
                              height: 8,
                              decoration: BoxDecoration(
                                shape: BoxShape.circle,
                                color: a.active
                                    ? AppColors.success
                                    : AppColors.error,
                              ),
                            ),
                            const SizedBox(width: 10),
                            GestureDetector(
                              onTap: () => _suspend(a.id, a.fullName),
                              child: Container(
                                padding: const EdgeInsets.symmetric(
                                    horizontal: 12, vertical: 6),
                                decoration: BoxDecoration(
                                  color: a.active
                                      ? AppColors.error.withOpacity(0.12)
                                      : AppColors.success.withOpacity(0.12),
                                  borderRadius: BorderRadius.circular(8),
                                  border: Border.all(
                                      color: a.active
                                          ? AppColors.error.withOpacity(0.3)
                                          : AppColors.success
                                              .withOpacity(0.3)),
                                ),
                                child: Text(
                                  a.active ? s.suspend : s.restore,
                                  style: TextStyle(
                                    color: a.active
                                        ? AppColors.error
                                        : AppColors.success,
                                    fontSize: 12,
                                    fontWeight: FontWeight.w700,
                                  ),
                                ),
                              ),
                            ),
                          ]),
                    ),
                  );
                },
              ),
      );

  // â”€â”€ LWT Logs tab â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Widget _logsTab(AppStrings s) => RefreshIndicator(
        color: _purple,
        backgroundColor: AppColors.surface,
        onRefresh: _fetch,
        child: _logs.isEmpty
            ? ListView(
                padding: const EdgeInsets.all(32),
                children: [
                  const SizedBox(height: 80),
                  Center(
                    child: Column(children: [
                      Icon(Icons.history_rounded,
                          color: AppColors.textMuted.withOpacity(0.3),
                          size: 60),
                      const SizedBox(height: 12),
                      Text(s.noData,
                          style:
                              const TextStyle(color: AppColors.textMuted)),
                    ]),
                  ),
                ],
              )
            : ListView.builder(
                padding: const EdgeInsets.fromLTRB(16, 16, 16, 80),
                itemCount: _logs.length,
                itemBuilder: (_, i) {
                  final l = _logs[i];
                  final isOnline = l.status == 'online';
                  return Container(
                    margin: const EdgeInsets.only(bottom: 8),
                    padding: const EdgeInsets.all(14),
                    decoration: BoxDecoration(
                      color: AppColors.surface,
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: AppColors.border),
                    ),
                    child: Row(
                      children: [
                        Container(
                          width: 36,
                          height: 36,
                          decoration: BoxDecoration(
                            color: (isOnline
                                    ? AppColors.success
                                    : AppColors.error)
                                .withOpacity(0.12),
                            borderRadius: BorderRadius.circular(10),
                          ),
                          child: Icon(
                            isOnline
                                ? Icons.wifi_rounded
                                : Icons.wifi_off_rounded,
                            color: isOnline
                                ? AppColors.success
                                : AppColors.error,
                            size: 18,
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Row(children: [
                                Text(l.deviceStrId,
                                    style: const TextStyle(
                                        color: AppColors.text,
                                        fontWeight: FontWeight.w700,
                                        fontSize: 13)),
                                const SizedBox(width: 6),
                                Text('@ ${l.siteName}',
                                    style: const TextStyle(
                                        color: AppColors.textMuted,
                                        fontSize: 11)),
                              ]),
                              const SizedBox(height: 2),
                              if (l.reason != null)
                                Text(l.reason!,
                                    style: const TextStyle(
                                        color: AppColors.textMuted,
                                        fontSize: 11)),
                              Text(
                                l.createdAt
                                    .toLocal()
                                    .toString()
                                    .substring(0, 19),
                                style: const TextStyle(
                                    color: AppColors.textMuted, fontSize: 10),
                              ),
                            ],
                          ),
                        ),
                        Container(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 8, vertical: 3),
                          decoration: BoxDecoration(
                            color: (isOnline
                                    ? AppColors.success
                                    : AppColors.error)
                                .withOpacity(0.12),
                            borderRadius: BorderRadius.circular(6),
                          ),
                          child: Text(
                            l.status.toUpperCase(),
                            style: TextStyle(
                              color: isOnline
                                  ? AppColors.success
                                  : AppColors.error,
                              fontSize: 10,
                              fontWeight: FontWeight.w700,
                            ),
                          ),
                        ),
                      ],
                    ),
                  );
                },
              ),
      );

  // â”€â”€ Devices tab â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Widget _devicesTab() {
    if (_devices.isEmpty) {
      return RefreshIndicator(
        color: _purple,
        backgroundColor: AppColors.surface,
        onRefresh: _fetch,
        child: ListView(
          padding: const EdgeInsets.all(32),
          children: [
            const SizedBox(height: 80),
            Center(
              child: Column(children: [
                Icon(Icons.memory_rounded,
                    color: AppColors.textMuted.withOpacity(0.3), size: 60),
                const SizedBox(height: 12),
                const Text('No devices found.',
                    style: TextStyle(color: AppColors.textMuted)),
              ]),
            ),
          ],
        ),
      );
    }

    return RefreshIndicator(
      color: _purple,
      backgroundColor: AppColors.surface,
      onRefresh: _fetch,
      child: ListView.builder(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 80),
        itemCount: _devices.length,
        itemBuilder: (_, i) => _deviceCard(_devices[i]),
      ),
    );
  }

  Widget _deviceCard(_DeviceItem d) {
    final statusColor = d.lastStatus == 'online'
        ? AppColors.success
        : d.lastStatus == 'unstable'
            ? AppColors.warning
            : AppColors.error;

    return Container(
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: AppColors.surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppColors.border),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // â”€â”€ Header row â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            Row(children: [
              Container(
                width: 38,
                height: 38,
                decoration: BoxDecoration(
                  color: statusColor.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: statusColor.withOpacity(0.25)),
                ),
                child: Icon(Icons.water_drop_rounded,
                    color: statusColor, size: 20),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(d.name,
                        style: const TextStyle(
                            color: AppColors.text,
                            fontWeight: FontWeight.w700,
                            fontSize: 14)),
                    const SizedBox(height: 2),
                    Text(d.deviceId,
                        style: const TextStyle(
                            color: AppColors.textMuted,
                            fontSize: 11,
                            fontFamily: 'monospace')),
                  ],
                ),
              ),
              Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                decoration: BoxDecoration(
                  color: statusColor.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(6),
                ),
                child: Text(
                  d.lastStatus.toUpperCase(),
                  style: TextStyle(
                      color: statusColor,
                      fontSize: 10,
                      fontWeight: FontWeight.w700),
                ),
              ),
            ]),
            // â”€â”€ Meta row (site + versions) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if (d.siteName.isNotEmpty ||
                d.swVersion != null ||
                d.hwVersion != null) ...[
              const SizedBox(height: 8),
              Row(children: [
                if (d.siteName.isNotEmpty) ...[
                  const Icon(Icons.location_on_rounded,
                      color: AppColors.textMuted, size: 12),
                  const SizedBox(width: 3),
                  Flexible(
                    child: Text(d.siteName,
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 11),
                        overflow: TextOverflow.ellipsis),
                  ),
                  const SizedBox(width: 10),
                ],
                if (d.swVersion != null) ...[
                  const Icon(Icons.memory_rounded,
                      color: AppColors.textMuted, size: 12),
                  const SizedBox(width: 3),
                  Text('SW ${d.swVersion}',
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
                  const SizedBox(width: 8),
                ],
                if (d.hwVersion != null)
                  Text('HW ${d.hwVersion}',
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
              ]),
            ],
            // â”€â”€ OTA button â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            const SizedBox(height: 10),
            GestureDetector(
              onTap: () => showDialog(
                context: context,
                barrierDismissible: false,
                builder: (_) => _OtaDialog(device: d),
              ),
              child: Container(
                height: 36,
                decoration: BoxDecoration(
                  color: _purple.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(8),
                  border:
                      Border.all(color: _purple.withOpacity(0.3)),
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.system_update_rounded,
                        color: _purple, size: 16),
                    SizedBox(width: 6),
                    Text('OTA Update',
                        style: TextStyle(
                            color: _purple,
                            fontSize: 12,
                            fontWeight: FontWeight.w700)),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  // ── Live device MQTT logs tab ─────────────────────────────────────────────

  Widget _liveLogsTab() {
    final deviceIds = _devices.map((d) => d.deviceId).toSet().toList();

    final filtered = _logFilterDeviceId == null
        ? _liveLogs
        : _liveLogs.where((l) => l.deviceId == _logFilterDeviceId).toList();

    return Column(
      children: [
        // ── Toolbar ──────────────────────────────────────────────────────
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
          color: AppColors.surface,
          child: Row(children: [
            Container(
              width: 8, height: 8,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: _liveLogs.isEmpty ? AppColors.textMuted : AppColors.success,
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                filtered.isEmpty
                    ? 'Waiting for device logs…'
                    : '${filtered.length} entries — newest first',
                style: const TextStyle(
                    color: AppColors.text,
                    fontSize: 13,
                    fontWeight: FontWeight.w600),
              ),
            ),
            // Device filter
            if (deviceIds.isNotEmpty) ...[
              GestureDetector(
                onTap: () async {
                  final chosen = await showModalBottomSheet<String?>(
                    context: context,
                    backgroundColor: AppColors.surface,
                    shape: const RoundedRectangleBorder(
                      borderRadius: BorderRadius.vertical(
                          top: Radius.circular(16)),
                    ),
                    builder: (_) => _DeviceFilterSheet(
                      deviceIds: deviceIds,
                      devices:   _devices,
                      selected:  _logFilterDeviceId,
                    ),
                  );
                  if (!mounted || chosen == null) return;
                  setState(() =>
                      _logFilterDeviceId = chosen == 'ALL' ? null : chosen);
                },
                child: Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 10, vertical: 5),
                  decoration: BoxDecoration(
                    color: _logFilterDeviceId != null
                        ? _purple.withOpacity(0.15)
                        : AppColors.surfaceLight,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                        color: _logFilterDeviceId != null
                            ? _purple.withOpacity(0.40)
                            : AppColors.border),
                  ),
                  child: Row(mainAxisSize: MainAxisSize.min, children: [
                    Icon(Icons.filter_list_rounded,
                        color: _logFilterDeviceId != null
                            ? _purple
                            : AppColors.textMuted,
                        size: 14),
                    const SizedBox(width: 4),
                    Text(
                      _logFilterDeviceId != null
                          ? (_logFilterDeviceId!.length > 11
                              ? '…${_logFilterDeviceId!.substring(_logFilterDeviceId!.length - 11)}'
                              : _logFilterDeviceId!)
                          : 'All devices',
                      style: TextStyle(
                          color: _logFilterDeviceId != null
                              ? _purple
                              : AppColors.textMuted,
                          fontSize: 11,
                          fontWeight: FontWeight.w600),
                    ),
                  ]),
                ),
              ),
              const SizedBox(width: 8),
            ],
            // Clear button
            if (_liveLogs.isNotEmpty)
              GestureDetector(
                onTap: () => setState(() {
                  _liveLogs.clear();
                }),
                child: Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 10, vertical: 5),
                  decoration: BoxDecoration(
                    color: AppColors.surfaceLight,
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: const Text('Clear',
                      style: TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 11,
                          fontWeight: FontWeight.w600)),
                ),
              ),
          ]),
        ),
        const Divider(height: 1, color: AppColors.border),

        // ── Log rows ─────────────────────────────────────────────────────
        Expanded(
          child: filtered.isEmpty
              ? Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(Icons.terminal_rounded,
                          color: AppColors.textMuted.withOpacity(0.22),
                          size: 52),
                      const SizedBox(height: 14),
                      const Text('No device logs yet',
                          style: TextStyle(
                              color: AppColors.textMuted,
                              fontWeight: FontWeight.w600)),
                      const SizedBox(height: 6),
                      Text(
                        _devices.isEmpty
                            ? 'Refresh the Devices tab first.'
                            : 'Logs appear as devices publish\nto device/{mac}/logs.',
                        style: const TextStyle(
                            color: AppColors.textMuted, fontSize: 11),
                        textAlign: TextAlign.center,
                      ),
                    ],
                  ),
                )
              : ListView.builder(
                  padding:
                      const EdgeInsets.fromLTRB(10, 6, 10, 80),
                  itemCount: filtered.length,
                  itemBuilder: (_, i) => _liveLogRow(filtered[i]),
                ),
        ),
      ],
    );
  }

  Widget _liveLogRow(_LiveLog l) {
    final color   = l.levelColor;
    // Show last segment of MAC (e.g. “35:13:90”) as a short device tag
    final parts   = l.deviceId.split(':');
    final shortId = parts.length >= 3
        ? parts.sublist(parts.length - 3).join(':')
        : l.deviceId;
    // Find display name from the device list
    final devName = _devices
        .firstWhere((d) => d.deviceId == l.deviceId,
            orElse: () => _DeviceItem(
                id: '', deviceId: l.deviceId, name: '',
                siteName: '', lastStatus: ''))
        .name;
    final timeStr = l.time.toLocal().toString().substring(11, 23);

    return Container(
      margin: const EdgeInsets.only(bottom: 3),
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      decoration: BoxDecoration(
        color: color.withOpacity(0.04),
        borderRadius: BorderRadius.circular(7),
        border: Border.all(color: color.withOpacity(0.12)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Level badge
          Container(
            width: 38,
            padding: const EdgeInsets.symmetric(vertical: 2),
            decoration: BoxDecoration(
              color: color.withOpacity(0.15),
              borderRadius: BorderRadius.circular(5),
            ),
            alignment: Alignment.center,
            child: Text(
              l.levelBadge,
              style: TextStyle(
                  color: color,
                  fontSize: 9,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 0.4),
            ),
          ),
          const SizedBox(width: 8),

          // Message + meta row
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  l.message,
                  style: TextStyle(
                    color: color == AppColors.textMuted
                        ? AppColors.textMuted
                        : AppColors.text,
                    fontSize: 12,
                    fontFamily: 'monospace',
                    height: 1.3,
                  ),
                ),
                const SizedBox(height: 3),
                Row(children: [
                  // Device tag
                  Container(
                    padding: const EdgeInsets.symmetric(
                        horizontal: 5, vertical: 1),
                    decoration: BoxDecoration(
                      color: _purple.withOpacity(0.12),
                      borderRadius: BorderRadius.circular(4),
                    ),
                    child: Text(
                      devName.isNotEmpty ? devName : shortId,
                      style: TextStyle(
                          color: _purple,
                          fontSize: 9,
                          fontFamily: 'monospace',
                          fontWeight: FontWeight.w700),
                    ),
                  ),
                  // Code tag
                  if (l.code.isNotEmpty) ...[
                    const SizedBox(width: 5),
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 5, vertical: 1),
                      decoration: BoxDecoration(
                        color: AppColors.surfaceLight,
                        borderRadius: BorderRadius.circular(4),
                      ),
                      child: Text(
                        l.code,
                        style: const TextStyle(
                            color: AppColors.textMuted,
                            fontSize: 9,
                            fontFamily: 'monospace'),
                      ),
                    ),
                  ],
                  const Spacer(),
                  // Timestamp
                  Text(timeStr,
                      style: const TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 9,
                          fontFamily: 'monospace')),
                ]),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _superBadge() => Container(
        padding:
            const EdgeInsets.symmetric(horizontal: 10, vertical: 3),
        decoration: BoxDecoration(
          color: _purple.withOpacity(0.15),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: _purple.withOpacity(0.35)),
        ),
        child: const Text(
          'SUPERADMIN',
          style: TextStyle(
            color: _purple,
            fontSize: 10,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.5,
          ),
        ),
      );

  String _initials(String name) {
    final parts = name.trim().split(' ');
    if (parts.length >= 2) {
      return '${parts[0][0]}${parts[1][0]}'.toUpperCase();
    }
    return name.isNotEmpty ? name[0].toUpperCase() : '?';
  }
}

// â”€â”€ OTA Update Dialog â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//
// Self-contained StatefulWidget that:
//  â€¢ Registers socket listeners for ota_progress / ota_result in initState
//  â€¢ Subscribes to the device's socket room
//  â€¢ Shows URL input â†’ Start button â†’ live progress bar â†’ success/fail result
//  â€¢ Removes listeners in dispose

class _OtaDialog extends StatefulWidget {
  final _DeviceItem device;
  const _OtaDialog({required this.device});

  @override
  State<_OtaDialog> createState() => _OtaDialogState();
}

class _OtaDialogState extends State<_OtaDialog> {
  static const _purple = Color(0xFFA855F7);

  final _urlCtrl = TextEditingController();

  // File-pick upload state
  String? _pickedFileName;
  bool  _uploading    = false;
  double _uploadPct   = 0;
  String? _uploadErr;

  // OTA progress state
  bool _sending     = false;
  bool _started     = false;
  bool _done        = false;
  bool _success     = false;
  int  _progress    = 0;
  String _statusTxt = '';
  String? _error;

  @override
  void initState() {
    super.initState();
    SocketService.on('ota_progress', _onProgress, id: 'ota_screen');
    SocketService.on('ota_result',   _onResult,   id: 'ota_screen');
    SocketService.connect().then((_) {
      SocketService.subscribeToDevice(widget.device.deviceId);
    });
  }

  @override
  void dispose() {
    _urlCtrl.dispose();
    SocketService.off('ota_progress', id: 'ota_screen');
    SocketService.off('ota_result',   id: 'ota_screen');
    super.dispose();
  }

  // â”€â”€ File pick + upload â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

  Future<void> _pickAndUpload() async {
    setState(() {
      _uploadErr  = null;
      _uploadPct  = 0;
    });

    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['bin'],
      allowMultiple: false,
    );
    if (result == null || result.files.isEmpty) return;

    final pf = result.files.single;
    final path = pf.path;
    if (path == null) {
      setState(() => _uploadErr = 'Could not read file path.');
      return;
    }

    // â”€â”€ Pre-upload sanity check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // A real ESP32 application binary is always at least ~200 KB.
    // Files like ota_data_initial.bin are only 8 KB and will fail OTA.
    const int _minFirmwareBytes = 100 * 1024; // 100 KB
    if ((pf.size) < _minFirmwareBytes) {
      setState(() => _uploadErr =
          'File too small (${pf.size} B). '
          'Upload the Droppy firmware .bin from your build/ directory, '
          'not ota_data_initial.bin.');
      return;
    }

    setState(() {
      _pickedFileName = pf.name;
      _uploading      = true;
      _uploadPct      = 0;
      _urlCtrl.clear();
    });

    try {
      final formData = FormData.fromMap({
        'firmware': await MultipartFile.fromFile(path, filename: pf.name),
      });

      final resp = await ApiClient.instance.post(
        '/superadmin/firmware/upload',
        data: formData,
        options: Options(contentType: 'multipart/form-data'),
        onSendProgress: (sent, total) {
          if (total > 0 && mounted) {
            setState(() => _uploadPct = sent / total);
          }
        },
      );

      final filename = resp.data['filename'] as String? ?? '';
      final base = AppConstants.apiBaseUrl.replaceAll(RegExp(r'/+$'), '');
      final url  = '$base/firmware/$filename';

      setState(() {
        _urlCtrl.text = url;
        _uploading    = false;
        _uploadPct    = 1.0;
      });
    } catch (e) {
      setState(() {
        _uploading = false;
        _uploadErr = ApiClient.errorMessage(e);
      });
    }
  }

  // ── Socket callbacks ──────────────────────────────────────────────────────

  void _onProgress(dynamic data) {
    if (!mounted) return;
    final map = Map<String, dynamic>.from(data as Map);
    if (map['device_id'] != widget.device.deviceId) return;
    setState(() {
      _progress  = (map['progress'] as num?)?.toInt() ?? _progress;
      _statusTxt = map['status'] as String? ?? _statusTxt;
    });
  }

  void _onResult(dynamic data) {
    if (!mounted) return;
    final map = Map<String, dynamic>.from(data as Map);
    if (map['device_id'] != widget.device.deviceId) return;
    final ok = map['status'] == 'success';
    setState(() {
      _done      = true;
      _success   = ok;
      _progress  = ok ? 100 : _progress;
      _statusTxt = ok
          ? 'Firmware updated successfully!\nDevice is rebooting…'
          : 'Failed: ${map['reason'] ?? 'unknown error'}';
    });
  }

  // ── Start OTA ─────────────────────────────────────────────────────────────

  Future<void> _startOta() async {
    final url = _urlCtrl.text.trim();
    if (url.isEmpty) {
      setState(() => _error = 'Firmware URL is required — pick a file or paste a URL');
      return;
    }
    setState(() {
      _sending = true;
      _error   = null;
    });
    try {
      await ApiClient.instance.post(
        '/superadmin/devices/${widget.device.id}/ota',
        data: {'url': url},
      );
      setState(() {
        _started   = true;
        _sending   = false;
        _statusTxt = 'starting…';
      });
    } catch (e) {
      setState(() {
        _error   = ApiClient.errorMessage(e);
        _sending = false;
      });
    }
  }

  // ── Build ──────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: AppColors.surface,
      shape:
          RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      contentPadding:
          const EdgeInsets.fromLTRB(20, 0, 20, 16),
      titlePadding:
          const EdgeInsets.fromLTRB(20, 16, 20, 12),
      title: Row(children: [
        Container(
          width: 36,
          height: 36,
          decoration: BoxDecoration(
            color: _purple.withOpacity(0.15),
            borderRadius: BorderRadius.circular(10),
          ),
          child: const Icon(Icons.system_update_rounded,
              color: _purple, size: 20),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text('OTA Update',
                  style: TextStyle(
                      color: AppColors.text,
                      fontSize: 15,
                      fontWeight: FontWeight.w700)),
              Text(widget.device.name,
                  style: const TextStyle(
                      color: AppColors.textMuted, fontSize: 11),
                  overflow: TextOverflow.ellipsis),
            ],
          ),
        ),
      ]),
      content: SizedBox(
        width: double.maxFinite,
        child: _started ? _progressBody() : _inputBody(),
      ),
      actions: _done
          ? [
              TextButton(
                onPressed: () => Navigator.pop(context),
                child: const Text('Close',
                    style: TextStyle(color: AppColors.primary)),
              )
            ]
          : _started
              ? const []
              : [
                  TextButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text('Cancel',
                        style: TextStyle(color: AppColors.textMuted)),
                  ),
                  TextButton(
                    onPressed: (_sending || _uploading) ? null : _startOta,
                    child: (_sending || _uploading)
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(
                                color: _purple, strokeWidth: 2))
                        : const Text('Start OTA',
                            style: TextStyle(
                                color: _purple,
                                fontWeight: FontWeight.w700)),
                  ),
                ],
    );
  }

  // ── Before OTA starts: URL + file upload input ────────────────────────────

  Widget _inputBody() => Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _infoRow(Icons.memory_rounded, 'ID: ${widget.device.deviceId}'),
          if (widget.device.swVersion != null)
            _infoRow(Icons.code_rounded,
                'Current SW: ${widget.device.swVersion}'),
          const SizedBox(height: 14),

          // ── File picker row ───────────────────────────────────────────
          GestureDetector(
            onTap: _uploading ? null : _pickAndUpload,
            child: Container(
              padding: const EdgeInsets.symmetric(
                  horizontal: 12, vertical: 10),
              decoration: BoxDecoration(
                color: _purple.withOpacity(0.08),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: _purple.withOpacity(0.3)),
              ),
              child: Row(
                children: [
                  Icon(
                    _uploading
                        ? Icons.hourglass_top_rounded
                        : (_pickedFileName != null && _uploadPct == 1.0
                            ? Icons.check_circle_rounded
                            : Icons.upload_file_rounded),
                    color: (_pickedFileName != null && _uploadPct == 1.0)
                        ? AppColors.success
                        : _purple,
                    size: 18,
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      _uploading
                          ? 'Uploading…  ${(_uploadPct * 100).toInt()}%'
                          : (_pickedFileName != null
                              ? _pickedFileName!
                              : 'Pick .bin file from device'),
                      style: TextStyle(
                        color: _uploading
                            ? AppColors.textMuted
                            : (_pickedFileName != null
                                ? AppColors.text
                                : _purple),
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  if (_uploading)
                    SizedBox(
                      width: 14,
                      height: 14,
                      child: CircularProgressIndicator(
                        value: _uploadPct,
                        strokeWidth: 2,
                        color: _purple,
                        backgroundColor: _purple.withOpacity(0.2),
                      ),
                    ),
                ],
              ),
            ),
          ),

          if (_uploadErr != null) ...[
            const SizedBox(height: 6),
            Text(_uploadErr!,
                style: const TextStyle(
                    color: AppColors.error, fontSize: 11)),
          ],

          const SizedBox(height: 12),
          const Row(children: [
            Expanded(child: Divider(color: AppColors.border)),
            Padding(
              padding: EdgeInsets.symmetric(horizontal: 8),
              child: Text('or paste URL',
                  style: TextStyle(
                      color: AppColors.textMuted, fontSize: 11)),
            ),
            Expanded(child: Divider(color: AppColors.border)),
          ]),
          const SizedBox(height: 10),

          const Text('Firmware URL',
              style: TextStyle(
                  color: AppColors.textMuted,
                  fontSize: 12,
                  fontWeight: FontWeight.w600)),
          const SizedBox(height: 6),
          Container(
            decoration: BoxDecoration(
              color: AppColors.background,
              borderRadius: BorderRadius.circular(10),
              border: Border.all(color: AppColors.border),
            ),
            child: TextField(
              controller: _urlCtrl,
              style: const TextStyle(color: AppColors.text, fontSize: 12),
              keyboardType: TextInputType.url,
              decoration: const InputDecoration(
                hintText: 'https://example.com/firmware.bin',
                hintStyle: TextStyle(
                    color: AppColors.textMuted, fontSize: 12),
                border: InputBorder.none,
                contentPadding: EdgeInsets.symmetric(
                    horizontal: 12, vertical: 10),
              ),
            ),
          ),
          if (_error != null) ...[
            const SizedBox(height: 8),
            Text(_error!,
                style: const TextStyle(
                    color: AppColors.error, fontSize: 11)),
          ],
        ],
      );

  // ── While / after OTA: progress body ─────────────────────────────────────

  Widget _progressBody() => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const SizedBox(height: 8),
          if (!_done) ...[
            ClipRRect(
              borderRadius: BorderRadius.circular(6),
              child: LinearProgressIndicator(
                value: _progress / 100.0,
                minHeight: 8,
                backgroundColor: AppColors.border,
                color: _purple,
              ),
            ),
            const SizedBox(height: 14),
            Text(
              '$_progress%',
              style: const TextStyle(
                  color: AppColors.text,
                  fontSize: 28,
                  fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 6),
            Text(_statusTxt,
                style: const TextStyle(
                    color: AppColors.textMuted, fontSize: 12)),
            const SizedBox(height: 12),
            const Text(
              'Do not power off the device during OTA.',
              style: TextStyle(color: AppColors.warning, fontSize: 11),
              textAlign: TextAlign.center,
            ),
          ] else ...[
            Icon(
              _success
                  ? Icons.check_circle_rounded
                  : Icons.error_rounded,
              color: _success ? AppColors.success : AppColors.error,
              size: 52,
            ),
            const SizedBox(height: 14),
            Text(
              _statusTxt,
              style: TextStyle(
                  color: _success ? AppColors.success : AppColors.error,
                  fontSize: 13,
                  fontWeight: FontWeight.w600),
              textAlign: TextAlign.center,
            ),
          ],
          const SizedBox(height: 4),
        ],
      );

  Widget _infoRow(IconData icon, String text) => Padding(
        padding: const EdgeInsets.only(bottom: 4),
        child: Row(children: [
          Icon(icon, color: AppColors.textMuted, size: 12),
          const SizedBox(width: 4),
          Text(text,
              style: const TextStyle(
                  color: AppColors.textMuted, fontSize: 11)),
        ]),
      );
}

// ── Device filter bottom sheet ─────────────────────────────────────────────────
//
// Shown when the user taps the "All devices" / filter chip in the Live Logs tab.
// Returns the chosen deviceId via Navigator.pop, or 'ALL' for "show everything".

class _DeviceFilterSheet extends StatelessWidget {
  final List<String>      deviceIds;
  final List<_DeviceItem> devices;
  final String?           selected; // null means 'ALL'

  const _DeviceFilterSheet({
    required this.deviceIds,
    required this.devices,
    required this.selected,
  });

  static const _purple = Color(0xFFA855F7);

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // ── Handle bar ───────────────────────────────────────────────────
          Container(
            width: 40, height: 4,
            margin: const EdgeInsets.symmetric(vertical: 10),
            decoration: BoxDecoration(
              color: AppColors.border,
              borderRadius: BorderRadius.circular(2),
            ),
          ),

          // ── Title ────────────────────────────────────────────────────────
          const Padding(
            padding: EdgeInsets.fromLTRB(16, 0, 16, 10),
            child: Align(
              alignment: Alignment.centerLeft,
              child: Text(
                'Filter by device',
                style: TextStyle(
                  color: AppColors.text,
                  fontSize: 15,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ),
          ),

          const Divider(height: 1, color: AppColors.border),

          // ── "All devices" option ─────────────────────────────────────────
          _SheetTile(
            label: 'All devices',
            sublabel: '${deviceIds.length} device${deviceIds.length == 1 ? '' : 's'}',
            icon: Icons.devices_rounded,
            selected: selected == null,
            purple: _purple,
            onTap: () => Navigator.pop(context, 'ALL'),
          ),

          // ── Per-device options ───────────────────────────────────────────
          ...deviceIds.map((id) {
            final dev = devices.firstWhere(
              (d) => d.deviceId == id,
              orElse: () => _DeviceItem(
                  id: '', deviceId: id, name: id,
                  siteName: '', lastStatus: ''),
            );
            final isOnline = dev.lastStatus == 'online';
            return _SheetTile(
              label: dev.name.isNotEmpty ? dev.name : id,
              sublabel: dev.siteName.isNotEmpty ? dev.siteName : id,
              icon: Icons.memory_rounded,
              selected: selected == id,
              purple: _purple,
              statusColor: isOnline ? AppColors.success : AppColors.textMuted,
              onTap: () => Navigator.pop(context, id),
            );
          }),

          const SizedBox(height: 8),
        ],
      ),
    );
  }
}

// Reusable tile used only inside _DeviceFilterSheet
class _SheetTile extends StatelessWidget {
  final String    label;
  final String    sublabel;
  final IconData  icon;
  final bool      selected;
  final Color     purple;
  final Color?    statusColor;
  final VoidCallback onTap;

  const _SheetTile({
    required this.label,
    required this.sublabel,
    required this.icon,
    required this.selected,
    required this.purple,
    required this.onTap,
    this.statusColor,
  });

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        color: selected ? purple.withOpacity(0.08) : Colors.transparent,
        child: Row(children: [
          Container(
            width: 36, height: 36,
            decoration: BoxDecoration(
              color: selected
                  ? purple.withOpacity(0.15)
                  : AppColors.surfaceLight,
              borderRadius: BorderRadius.circular(9),
            ),
            child: Stack(
              children: [
                Center(
                  child: Icon(icon,
                      color: selected ? purple : AppColors.textMuted,
                      size: 18),
                ),
                if (statusColor != null)
                  Positioned(
                    right: 3, bottom: 3,
                    child: Container(
                      width: 8, height: 8,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        color: statusColor,
                        border: Border.all(
                            color: AppColors.surface, width: 1.5),
                      ),
                    ),
                  ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  label,
                  style: TextStyle(
                    color: selected ? AppColors.text : AppColors.text,
                    fontSize: 13,
                    fontWeight: selected
                        ? FontWeight.w700
                        : FontWeight.w500,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
                if (sublabel.isNotEmpty)
                  Text(
                    sublabel,
                    style: const TextStyle(
                        color: AppColors.textMuted, fontSize: 11),
                    overflow: TextOverflow.ellipsis,
                  ),
              ],
            ),
          ),
          if (selected)
            Icon(Icons.check_rounded, color: purple, size: 18),
        ]),
      ),
    );
  }
}
