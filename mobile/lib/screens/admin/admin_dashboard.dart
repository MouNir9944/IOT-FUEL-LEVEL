import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../core/api_client.dart';
import '../../core/constants.dart';
import '../../core/app_strings.dart';
import '../../core/socket_service.dart';
import '../../models/site.dart';
import '../../models/device.dart';
import '../../providers/auth_provider.dart';
import '../../widgets/device_card.dart';
import '../profile_screen.dart';
import '../device_control_screen.dart';
import '../device_history_screen.dart';
import 'create_site_screen.dart';
import 'manage_devices_screen.dart';
import 'manage_users_screen.dart';
import '../../widgets/pill_nav_bar.dart';

class AdminDashboard extends StatefulWidget {
  const AdminDashboard({super.key});

  @override
  State<AdminDashboard> createState() => _AdminDashboardState();
}

class _AdminDashboardState extends State<AdminDashboard> {
  List<Site> _sites = [];
  List<Device> _devices = [];
  String? _selectedSiteId;
  bool _loading = false;
  int _navIndex = 0; // 0=Devices 1=Users 2=Sites 3=Profile

  @override
  void initState() {
    super.initState();
    _fetch();
  }

  @override
  void dispose() {
    SocketService.off('telemetry_update',     id: 'admin_dashboard');
    SocketService.off('device_status_change', id: 'admin_dashboard');
    super.dispose();
  }

  Future<void> _fetch() async {
    setState(() => _loading = true);
    try {
      final results = await Future.wait([
        ApiClient.instance.get('/sites'),
        ApiClient.instance.get('/devices'),
      ]);
      setState(() {
        _sites = (results[0].data['sites'] as List)
            .map((s) => Site.fromJson(Map<String, dynamic>.from(s as Map)))
            .toList();
        _devices = (results[1].data['devices'] as List)
            .map((d) => Device.fromJson(Map<String, dynamic>.from(d as Map)))
            .toList();
      });
    } catch (_) {
    } finally {
      setState(() => _loading = false);
    }
    _connectSocket();
  }

  void _connectSocket() {
    SocketService.connect().then((socket) {
      // Subscribe to real-time updates for every device
      for (final d in _devices) {
        SocketService.subscribeToDevice(d.deviceId);
      }

      // Live telemetry → update the matching device in _devices
      SocketService.on('telemetry_update', (data) {
        final map = Map<String, dynamic>.from(data as Map);
        try {
          final tel = Telemetry.fromJson(map);
          final idx = _devices.indexWhere((d) => d.deviceId == tel.deviceId);
          if (idx != -1 && mounted) {
            setState(() {
              _devices[idx].lastTelemetry   = tel;
              _devices[idx].lastTelemetryAt = tel.timestamp;
            });
          }
        } catch (_) {}
      }, id: 'admin_dashboard');

      // Online / offline status changes
      SocketService.on('device_status_change', (data) {
        final map = Map<String, dynamic>.from(data as Map);
        final deviceId = map['device_id'] as String?;
        final status   = map['status']    as String?;
        if (deviceId == null || status == null) return;
        final idx = _devices.indexWhere((d) => d.deviceId == deviceId);
        if (idx != -1 && mounted) {
          setState(() => _devices[idx].lastStatus = status);
        }
      }, id: 'admin_dashboard');
    });
  }

  List<Device> get _filtered => _selectedSiteId == null
      ? _devices
      : _devices.where((d) => d.siteId == _selectedSiteId).toList();

  int _countForSite(String? id) => id == null
      ? _devices.length
      : _devices.where((d) => d.siteId == id).length;

  // ── Build ───────────────────────────────────────────────────────────────────
  @override
  Widget build(BuildContext context) {
    final s   = AppStrings.of(context);
    final auth = context.watch<AuthProvider>();

    final pages = [
      _devicesTab(s),
      _usersShortcut(s),
      _sitesTab(s),
      const ProfileScreen(),
    ];

    final navItems = [
      PillNavItem(icon: Icons.devices_rounded,       label: s.devices),
      PillNavItem(icon: Icons.people_rounded,         label: s.users),
      PillNavItem(icon: Icons.business_rounded,       label: s.sites),
      PillNavItem(icon: Icons.person_outline_rounded, label: 'Profile'),
    ];

    return Scaffold(
      backgroundColor: AppColors.background,
      // ── Header ─────────────────────────────────────────────────────────────
      body: NestedScrollView(
        key: ValueKey(_navIndex), // re-anchor scroll when tab changes
        headerSliverBuilder: (ctx, _) => [
          SliverAppBar(
            pinned: true,
            backgroundColor: AppColors.surface,
            foregroundColor: AppColors.text,
            expandedHeight: 120,
            automaticallyImplyLeading: false,
            elevation: 0,
            flexibleSpace: FlexibleSpaceBar(
              background: Container(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    colors: [Color(0xFF0F172A), Color(0xFF1E293B)],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                ),
                padding: const EdgeInsets.fromLTRB(20, 0, 20, 0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.end,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              navItems[_navIndex].label,
                              style: const TextStyle(
                                color: AppColors.text,
                                fontSize: 24,
                                fontWeight: FontWeight.w800,
                              ),
                            ),
                            Text(
                              auth.user?.displayName ?? '',
                              style: const TextStyle(
                                color: AppColors.textMuted,
                                fontSize: 13,
                              ),
                            ),
                          ],
                        ),
                        _avatarBtn(auth.user?.displayName ?? '?', context),
                      ],
                    ),
                    const SizedBox(height: 16),
                  ],
                ),
              ),
            ),
          ),
        ],
        body: pages[_navIndex],
      ),

      // ── Floating pill bottom navigation bar ─────────────────────────────────
      bottomNavigationBar: PillNavBar(
        items:    navItems,
        selected: _navIndex,
        onTap:    (i) => setState(() => _navIndex = i),
      ),

      // ── FAB ────────────────────────────────────────────────────────────────
      floatingActionButton: _fab(_navIndex, s),
    );
  }

  // ── FABs ───────────────────────────────────────────────────────────────────
  Widget _fab(int idx, AppStrings s) {
    if (idx == 0) {
      return FloatingActionButton.extended(
        heroTag: 'fab_devices',
        onPressed: () => Navigator.of(context)
            .push(MaterialPageRoute(
                builder: (_) =>
                    ManageDevicesScreen(sites: _sites, onChanged: _fetch)))
            .then((_) => _fetch()),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        icon: const Icon(Icons.add_rounded),
        label: Text(s.newDevice),
      );
    } else if (idx == 1) {
      return FloatingActionButton.extended(
        heroTag: 'fab_users',
        onPressed: () => Navigator.of(context)
            .push(MaterialPageRoute(builder: (_) => const ManageUsersScreen()))
            .then((_) => _fetch()),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        icon: const Icon(Icons.person_add_outlined),
        label: Text(s.newUser),
      );
    } else if (idx == 2) {
      return FloatingActionButton.extended(
        heroTag: 'fab_sites',
        onPressed: () => Navigator.of(context)
            .push(MaterialPageRoute(
                builder: (_) => CreateSiteScreen(onCreated: _fetch)))
            .then((_) => _fetch()),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        icon: const Icon(Icons.add_business_rounded),
        label: Text(s.newSite),
      );
    }
    return const SizedBox.shrink();
  }

  // ── Devices tab ─────────────────────────────────────────────────────────────
  Widget _devicesTab(AppStrings s) => RefreshIndicator(
        color: AppColors.primary,
        backgroundColor: AppColors.surface,
        onRefresh: _fetch,
        child: CustomScrollView(slivers: [
          SliverToBoxAdapter(
            child: SizedBox(
              height: 54,
              child: ListView(
                scrollDirection: Axis.horizontal,
                padding:
                    const EdgeInsets.symmetric(horizontal: 16, vertical: 9),
                children: [
                  _chip(null, s.allDevices),
                  ..._sites.map((site) => _chip(site.id, site.name)),
                  _addSiteChip(),
                ],
              ),
            ),
          ),
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 12),
              child: Row(children: [
                _stat(
                    '${_filtered.where((d) => d.lastStatus == 'online').length}',
                    s.online,
                    AppColors.success),
                const SizedBox(width: 10),
                _stat(
                    '${_filtered.where((d) => d.lastStatus == 'offline').length}',
                    s.offline,
                    AppColors.error),
                const SizedBox(width: 10),
                _stat(
                    '${_filtered.where((d) => d.lastStatus == 'unstable').length}',
                    s.unstable,
                    AppColors.warning),
              ]),
            ),
          ),
          if (_loading && _devices.isEmpty)
            const SliverFillRemaining(
                child: Center(
                    child: CircularProgressIndicator(color: AppColors.primary)))
          else if (_filtered.isEmpty)
            SliverFillRemaining(
              child: Center(
                child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(Icons.devices_other_rounded,
                          color: AppColors.textMuted.withOpacity(0.3),
                          size: 60),
                      const SizedBox(height: 12),
                      Text(s.noDevices,
                          style:
                              const TextStyle(color: AppColors.textMuted)),
                    ]),
              ),
            )
          else
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 120),
              sliver: SliverList(
                delegate: SliverChildBuilderDelegate(
                  (_, i) => Padding(
                    padding: const EdgeInsets.only(bottom: 10),
                    child: DeviceCard(
                      device: _filtered[i],
                      onTap: () => Navigator.of(context).push(MaterialPageRoute(
                          builder: (_) =>
                              DeviceHistoryScreen(device: _filtered[i]))),
                      onControlTap: () =>
                          Navigator.of(context).push(MaterialPageRoute(
                              builder: (_) => DeviceControlScreen(
                                  device: _filtered[i], isAdmin: true))),
                    ),
                  ),
                  childCount: _filtered.length,
                ),
              ),
            ),
        ]),
      );

  // ── Users shortcut tab ──────────────────────────────────────────────────────
  Widget _usersShortcut(AppStrings s) => Center(
        child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
          Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(
              color: AppColors.primary.withOpacity(0.12),
              shape: BoxShape.circle,
            ),
            child: const Icon(Icons.people_outline_rounded,
                color: AppColors.primary, size: 36),
          ),
          const SizedBox(height: 16),
          Text(s.users,
              style: const TextStyle(
                  color: AppColors.text,
                  fontSize: 18,
                  fontWeight: FontWeight.w700)),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            onPressed: () => Navigator.of(context)
                .push(MaterialPageRoute(
                    builder: (_) => const ManageUsersScreen()))
                .then((_) => _fetch()),
            icon: const Icon(Icons.arrow_forward_rounded, size: 18),
            label: Text(s.users),
            style: ElevatedButton.styleFrom(
              backgroundColor: AppColors.primary,
              foregroundColor: Colors.white,
              padding:
                  const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12)),
            ),
          ),
        ]),
      );

  // ── Sites tab ───────────────────────────────────────────────────────────────
  Widget _sitesTab(AppStrings s) => RefreshIndicator(
        color: AppColors.primary,
        backgroundColor: AppColors.surface,
        onRefresh: _fetch,
        child: _sites.isEmpty
            ? ListView(padding: const EdgeInsets.all(32), children: [
                const SizedBox(height: 80),
                Center(
                  child: Column(children: [
                    Icon(Icons.business_outlined,
                        color: AppColors.textMuted.withOpacity(0.3), size: 60),
                    const SizedBox(height: 12),
                    Text(s.noSites,
                        textAlign: TextAlign.center,
                        style: const TextStyle(color: AppColors.textMuted)),
                  ]),
                ),
              ])
            : ListView.builder(
                padding: const EdgeInsets.fromLTRB(16, 16, 16, 120),
                itemCount: _sites.length,
                itemBuilder: (_, i) {
                  final site  = _sites[i];
                  final count = _countForSite(site.id);
                  return Container(
                    margin: const EdgeInsets.only(bottom: 12),
                    decoration: BoxDecoration(
                      color: AppColors.surface,
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(color: AppColors.border),
                    ),
                    child: ListTile(
                      contentPadding: const EdgeInsets.symmetric(
                          horizontal: 16, vertical: 10),
                      leading: Container(
                        width: 44,
                        height: 44,
                        decoration: BoxDecoration(
                          color: AppColors.primary.withOpacity(0.12),
                          borderRadius: BorderRadius.circular(12),
                        ),
                        child: const Icon(Icons.business_rounded,
                            color: AppColors.primary, size: 22),
                      ),
                      title: Text(site.name,
                          style: const TextStyle(
                              color: AppColors.text,
                              fontWeight: FontWeight.w700,
                              fontSize: 15)),
                      subtitle: site.address != null
                          ? Text(site.address!,
                              style: const TextStyle(
                                  color: AppColors.textMuted, fontSize: 12))
                          : null,
                      trailing:
                          Row(mainAxisSize: MainAxisSize.min, children: [
                        Container(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 10, vertical: 4),
                          decoration: BoxDecoration(
                            color: AppColors.primary.withOpacity(0.12),
                            borderRadius: BorderRadius.circular(20),
                          ),
                          child: Text(
                            '$count ${count == 1 ? 'device' : 'devices'}',
                            style: const TextStyle(
                                color: AppColors.primary,
                                fontWeight: FontWeight.w700,
                                fontSize: 12),
                          ),
                        ),
                        const SizedBox(width: 6),
                        const Icon(Icons.chevron_right_rounded,
                            color: AppColors.textMuted),
                      ]),
                      onTap: () => setState(() {
                        _selectedSiteId = site.id;
                        _navIndex = 0;
                      }),
                    ),
                  );
                },
              ),
      );

  // ── Helpers ─────────────────────────────────────────────────────────────────
  Widget _chip(String? id, String label) {
    final sel   = _selectedSiteId == id;
    final count = _countForSite(id);
    return GestureDetector(
      onTap: () => setState(() => _selectedSiteId = id),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 180),
        margin: const EdgeInsets.only(right: 8),
        padding: const EdgeInsets.symmetric(horizontal: 14),
        decoration: BoxDecoration(
          gradient: sel
              ? const LinearGradient(
                  colors: [AppColors.primary, Color(0xFF38BDF8)])
              : null,
          color: sel ? null : AppColors.surface,
          borderRadius: BorderRadius.circular(20),
          border: sel ? null : Border.all(color: AppColors.border),
        ),
        child: Row(mainAxisSize: MainAxisSize.min, children: [
          Text(label,
              style: TextStyle(
                  color: sel ? Colors.white : AppColors.textMuted,
                  fontWeight: FontWeight.w600,
                  fontSize: 13)),
          if (count > 0) ...[
            const SizedBox(width: 6),
            Container(
              width: 20,
              height: 20,
              decoration: BoxDecoration(
                color: sel
                    ? Colors.white.withOpacity(0.25)
                    : AppColors.primary.withOpacity(0.15),
                shape: BoxShape.circle,
              ),
              child: Center(
                  child: Text('$count',
                      style: TextStyle(
                          color: sel ? Colors.white : AppColors.primary,
                          fontSize: 10,
                          fontWeight: FontWeight.w800))),
            ),
          ],
        ]),
      ),
    );
  }

  Widget _addSiteChip() => GestureDetector(
        onTap: () => Navigator.of(context)
            .push(MaterialPageRoute(
                builder: (_) => CreateSiteScreen(onCreated: _fetch)))
            .then((_) => _fetch()),
        child: Container(
          margin: const EdgeInsets.only(right: 8),
          padding: const EdgeInsets.symmetric(horizontal: 12),
          decoration: BoxDecoration(
            color: AppColors.surface,
            borderRadius: BorderRadius.circular(20),
            border:
                Border.all(color: AppColors.primary.withOpacity(0.4)),
          ),
          child: const Row(mainAxisSize: MainAxisSize.min, children: [
            Icon(Icons.add_rounded, color: AppColors.primary, size: 16),
            SizedBox(width: 4),
            Icon(Icons.business_rounded, color: AppColors.primary, size: 14),
          ]),
        ),
      );

  Widget _stat(String value, String label, Color color) => Expanded(
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 10),
          decoration: BoxDecoration(
            color: color.withOpacity(0.08),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: color.withOpacity(0.2)),
          ),
          child: Column(children: [
            Text(value,
                style: TextStyle(
                    color: color, fontSize: 20, fontWeight: FontWeight.w800)),
            const SizedBox(height: 2),
            Text(label,
                style: const TextStyle(
                    color: AppColors.textMuted, fontSize: 10)),
          ]),
        ),
      );

  Widget _avatarBtn(String name, BuildContext ctx) => GestureDetector(
        onTap: () => setState(() => _navIndex = 3),
        child: Container(
          width: 42,
          height: 42,
          decoration: BoxDecoration(
            gradient: const LinearGradient(
                colors: [AppColors.primary, Color(0xFF38BDF8)]),
            shape: BoxShape.circle,
            boxShadow: [
              BoxShadow(
                  color: AppColors.primary.withOpacity(0.35),
                  blurRadius: 10,
                  offset: const Offset(0, 4))
            ],
          ),
          child: Center(
            child: Text(_initials(name),
                style: const TextStyle(
                    color: Colors.white,
                    fontWeight: FontWeight.w800,
                    fontSize: 14)),
          ),
        ),
      );

  String _initials(String name) {
    final p = name.trim().split(' ');
    if (p.length >= 2) return '${p[0][0]}${p[1][0]}'.toUpperCase();
    return name.isNotEmpty ? name[0].toUpperCase() : '?';
  }
}
