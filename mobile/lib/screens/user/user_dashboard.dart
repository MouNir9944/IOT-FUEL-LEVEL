import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../core/constants.dart';
import '../../core/app_strings.dart';
import '../../core/socket_service.dart';
import '../../models/device.dart';
import '../../providers/auth_provider.dart';
import '../../providers/device_provider.dart';
import '../../widgets/device_card.dart';
import '../device_control_screen.dart';
import '../device_history_screen.dart';
import '../technician_logs_screen.dart';
import '../profile_screen.dart';

class UserDashboard extends StatefulWidget {
  const UserDashboard({super.key});

  @override
  State<UserDashboard> createState() => _UserDashboardState();
}

class _UserDashboardState extends State<UserDashboard> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _init());
  }

  Future<void> _init() async {
    final deviceProv = context.read<DeviceProvider>();
    await deviceProv.fetchDevices();
    _connectSocket();
  }

  void _connectSocket() {
    final deviceProv = context.read<DeviceProvider>();

    SocketService.on('device_status_change', (data) {
      final map = Map<String, dynamic>.from(data as Map);
      deviceProv.updateDeviceStatus(
        map['device_id'] as String,
        map['status'] as String,
      );
    }, id: 'user_dashboard');

    SocketService.on('telemetry_update', (data) {
      final map = Map<String, dynamic>.from(data as Map);
      try {
        final tel = Telemetry.fromJson(map);
        deviceProv.updateDeviceTelemetry(tel.deviceId, tel);
      } catch (_) {}
    }, id: 'user_dashboard');

    SocketService.addReconnectCallback(_onSocketReconnect);

    SocketService.connect().then((_) {
      for (final d in deviceProv.devices) {
        SocketService.subscribeToDevice(d.deviceId);
      }
    });
  }

  void _onSocketReconnect() {
    if (mounted) context.read<DeviceProvider>().fetchDevices();
  }

  @override
  void dispose() {
    SocketService.off('device_status_change', id: 'user_dashboard');
    SocketService.off('telemetry_update',     id: 'user_dashboard');
    SocketService.removeReconnectCallback(_onSocketReconnect);
    super.dispose();
  }

  void _goControl(Device d) {
    Navigator.of(context).push(MaterialPageRoute(
      builder: (_) => DeviceControlScreen(device: d),
    ));
  }

  void _goHistory(Device d) {
    Navigator.of(context).push(MaterialPageRoute(
      builder: (_) => DeviceHistoryScreen(device: d),
    ));
  }

  void _goLogs(Device d) {
    Navigator.of(context).push(MaterialPageRoute(
      builder: (_) => TechnicianLogsScreen(device: d),
    ));
  }

  void _goProfile() {
    Navigator.of(context)
        .push(MaterialPageRoute(builder: (_) => const ProfileScreen()));
  }

  @override
  Widget build(BuildContext context) {
    final s            = AppStrings.of(context);
    final auth         = context.watch<AuthProvider>();
    final deviceProv   = context.watch<DeviceProvider>();
    final isTechnician = auth.user?.role == 'technician';
    final firstName    = auth.user?.displayName.split(' ').first ?? '';
    final initials     = _initials(auth.user?.displayName ?? '?');

    return Scaffold(
      backgroundColor: AppColors.background,
      body: NestedScrollView(
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
                        width: 160,
                        height: 160,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          gradient: RadialGradient(colors: [
                            AppColors.primary.withOpacity(0.15),
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
                                Text(
                                  '${s.hello}, $firstName 👋',
                                  style: const TextStyle(
                                    color: AppColors.text,
                                    fontSize: 22,
                                    fontWeight: FontWeight.w800,
                                  ),
                                ),
                                const SizedBox(height: 4),
                                _roleBadge(auth.user?.role ?? '', s),
                              ],
                            ),
                            GestureDetector(
                              onTap: _goProfile,
                              child: Container(
                                width: 44,
                                height: 44,
                                decoration: BoxDecoration(
                                  gradient: const LinearGradient(
                                    colors: [
                                      AppColors.primary,
                                      Color(0xFFD97706),
                                    ],
                                    begin: Alignment.topLeft,
                                    end: Alignment.bottomRight,
                                  ),
                                  shape: BoxShape.circle,
                                  boxShadow: [
                                    BoxShadow(
                                      color:
                                          AppColors.primary.withOpacity(0.4),
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
              title: Padding(
                padding: const EdgeInsets.only(left: 4),
                child: Text(
                  s.myDevices,
                  style: const TextStyle(
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
        body: _buildBody(s, deviceProv, isTechnician),
      ),
      floatingActionButton: isTechnician && deviceProv.devices.isNotEmpty
          ? FloatingActionButton.extended(
              onPressed: () => _goLogs(deviceProv.devices.first),
              backgroundColor: AppColors.primary,
              icon: const Icon(Icons.terminal_rounded),
              label: Text(s.techLogs),
            )
          : null,
    );
  }

  Widget _buildBody(
      AppStrings s, DeviceProvider deviceProv, bool isTechnician) {
    if (deviceProv.isLoading && deviceProv.devices.isEmpty) {
      return const Center(
          child: CircularProgressIndicator(color: AppColors.primary));
    }

    final devices = deviceProv.devices;

    if (devices.isEmpty) {
      return RefreshIndicator(
        color: AppColors.primary,
        backgroundColor: AppColors.surface,
        onRefresh: deviceProv.fetchDevices,
        child: ListView(
          padding: const EdgeInsets.all(32),
          children: [
            const SizedBox(height: 80),
            Center(
              child: Column(
                children: [
                  Icon(
                    Icons.local_gas_station_rounded,
                    color: AppColors.textMuted.withOpacity(0.3),
                    size: 64,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    s.noDevices,
                    style: const TextStyle(
                        color: AppColors.textMuted, fontSize: 15),
                  ),
                ],
              ),
            ),
          ],
        ),
      );
    }

    // Summary stats
    final online   = devices.where((d) => d.lastStatus == 'online').length;
    final offline  = devices.where((d) => d.lastStatus == 'offline').length;
    final critical = devices
        .where((d) =>
            d.lastTelemetry != null && d.lastTelemetry!.isCritical)
        .length;

    return RefreshIndicator(
      color: AppColors.primary,
      backgroundColor: AppColors.surface,
      onRefresh: deviceProv.fetchDevices,
      child: ListView.builder(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 80),
        itemCount: devices.length + 1,
        itemBuilder: (_, i) {
          if (i == 0) {
            return Column(
              children: [
                // Stats row
                Row(
                  children: [
                    Expanded(
                        child: _statChip(
                            s.online, online, AppColors.success,
                            icon: Icons.wifi_rounded)),
                    const SizedBox(width: 8),
                    Expanded(
                        child: _statChip(
                            s.offline, offline, AppColors.error,
                            icon: Icons.wifi_off_rounded)),
                    const SizedBox(width: 8),
                    Expanded(
                        child: _statChip(
                            s.fuelCritical, critical, AppColors.fuelLow,
                            icon: Icons.warning_amber_rounded)),
                  ],
                ),
                const SizedBox(height: 16),
                Row(
                  children: [
                    Text(
                      '${s.myDevices} (${devices.length})',
                      style: const TextStyle(
                        color: AppColors.textMuted,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                        letterSpacing: 0.5,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 10),
              ],
            );
          }
          final d = devices[i - 1];
          return Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: DeviceCard(
              device: d,
              onTap: () => _goHistory(d),
              onControlTap: () => _goControl(d),
            ),
          );
        },
      ),
    );
  }

  Widget _statChip(String label, int count, Color color,
      {required IconData icon}) =>
      Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
        decoration: BoxDecoration(
          color: color.withOpacity(0.1),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: color.withOpacity(0.25)),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon, size: 13, color: color),
            const SizedBox(width: 5),
            Text(
              '$count',
              style: TextStyle(
                  color: color, fontWeight: FontWeight.w800, fontSize: 14),
            ),
            const SizedBox(width: 4),
            Flexible(
              child: Text(
                label,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(
                    color: color.withOpacity(0.8),
                    fontSize: 10,
                    fontWeight: FontWeight.w600),
              ),
            ),
          ],
        ),
      );

  Widget _roleBadge(String role, AppStrings s) {
    Color c;
    String label;
    switch (role) {
      case 'technician':
        c = AppColors.warning;
        label = s.technician;
        break;
      case 'admin':
        c = AppColors.primary;
        label = s.admin;
        break;
      default:
        c = AppColors.success;
        label = s.user;
    }
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 3),
      decoration: BoxDecoration(
        color: c.withOpacity(0.15),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: c.withOpacity(0.35)),
      ),
      child: Text(
        label.toUpperCase(),
        style: TextStyle(
          color: c,
          fontSize: 10,
          fontWeight: FontWeight.w700,
          letterSpacing: 0.5,
        ),
      ),
    );
  }

  String _initials(String name) {
    final parts = name.trim().split(' ');
    if (parts.length >= 2) {
      return '${parts[0][0]}${parts[1][0]}'.toUpperCase();
    }
    return name.isNotEmpty ? name[0].toUpperCase() : '?';
  }
}
