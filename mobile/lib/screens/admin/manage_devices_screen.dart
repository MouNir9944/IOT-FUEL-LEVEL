import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'dart:convert';
import '../../core/api_client.dart';
import '../../core/constants.dart';
import '../../core/app_strings.dart';
import '../../models/site.dart';
import '../../models/device.dart';


class ManageDevicesScreen extends StatefulWidget {
  final List<Site> sites;
  final VoidCallback? onChanged;

  const ManageDevicesScreen(
      {super.key, required this.sites, this.onChanged});

  @override
  State<ManageDevicesScreen> createState() => _ManageDevicesScreenState();
}

class _ManageDevicesScreenState extends State<ManageDevicesScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  List<Device> _devices = [];
  bool _loadingDevices = false;

  // Add form
  final _idCtrl = TextEditingController();
  final _nameCtrl = TextEditingController();
  String? _selectedSiteId;
  bool _scanning = false;
  bool _scanned = false;
  bool _adding = false;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 2, vsync: this);
    if (widget.sites.isNotEmpty) _selectedSiteId = widget.sites.first.id;
    _fetchDevices();
  }

  @override
  void dispose() {
    _tabs.dispose();
    _idCtrl.dispose();
    _nameCtrl.dispose();
    super.dispose();
  }

  Future<void> _fetchDevices() async {
    setState(() => _loadingDevices = true);
    try {
      final resp = await ApiClient.instance.get('/devices');
      setState(() {
        _devices = (resp.data['devices'] as List)
            .map((d) => Device.fromJson(Map<String, dynamic>.from(d as Map)))
            .toList();
      });
    } catch (_) {
    } finally {
      setState(() => _loadingDevices = false);
    }
  }

  void _onQr(BarcodeCapture capture) {
    if (_scanned) return;
    final raw = capture.barcodes.firstOrNull?.rawValue;
    if (raw == null) return;
    setState(() => _scanned = true);
    String id = raw;
    if (raw.contains('|')) {
      id = raw.split('|').first.trim();
    } else {
      try {
        final parsed = jsonDecode(raw) as Map<String, dynamic>;
        id = parsed['id'] as String? ?? raw;
      } catch (_) {}
    }
    setState(() {
      _idCtrl.text = id;
      _scanning = false;
    });
  }

  Future<void> _addDevice() async {
    final s = AppStrings.read(context);
    if (_idCtrl.text.trim().isEmpty || _selectedSiteId == null) {
      _snack(s.allFields, isError: true);
      return;
    }
    setState(() => _adding = true);
    try {
      await ApiClient.instance.post('/devices', data: {
        'device_id': _idCtrl.text.trim().toUpperCase(),
        'name': _nameCtrl.text.trim().isEmpty ? null : _nameCtrl.text.trim(),
        'site_id': _selectedSiteId,
      });
      _snack(s.success);
      _idCtrl.clear();
      _nameCtrl.clear();
      widget.onChanged?.call();
      await _fetchDevices();
      if (mounted) _tabs.animateTo(1);
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _adding = false);
    }
  }

  Future<void> _deleteDevice(String id, String name) async {
    final s = AppStrings.read(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        backgroundColor: AppColors.surface,
        title: Text(s.deleteDevice,
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700)),
        content: Text('$name\n${s.cannotUndo}',
            style: const TextStyle(color: AppColors.textMuted)),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: Text(s.cancel,
                  style: const TextStyle(color: AppColors.textMuted))),
          TextButton(
              onPressed: () => Navigator.pop(context, true),
              child: Text(s.delete,
                  style: const TextStyle(
                      color: AppColors.error, fontWeight: FontWeight.w700))),
        ],
      ),
    );
    if (ok != true) return;
    try {
      await ApiClient.instance.delete('/devices/$id');
      widget.onChanged?.call();
      await _fetchDevices();
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
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

  @override
  Widget build(BuildContext context) {
    final s = AppStrings.of(context);

    // QR scanner full-screen
    if (_scanning) return _qrScreen(s);

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: Text(s.devices,
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700)),
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.text,
        elevation: 0,
        bottom: TabBar(
          controller: _tabs,
          labelColor: Colors.white,
          unselectedLabelColor: AppColors.textMuted,
          indicatorColor: AppColors.primary,
          tabs: [Tab(text: s.addDevice), Tab(text: s.devices)],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [_addTab(s), _listTab(s)],
      ),
    );
  }

  // ── Add device tab ─────────────────────────────────────────────────────────
  Widget _addTab(AppStrings s) => SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // QR button
            GestureDetector(
              onTap: () => setState(() {
                _scanned = false;
                _scanning = true;
              }),
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.symmetric(vertical: 18),
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                      colors: [AppColors.primary, Color(0xFF38BDF8)]),
                  borderRadius: BorderRadius.circular(14),
                  boxShadow: [
                    BoxShadow(
                        color: AppColors.primary.withOpacity(0.35),
                        blurRadius: 16,
                        offset: const Offset(0, 6))
                  ],
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.qr_code_scanner_rounded,
                        color: Colors.white, size: 24),
                    SizedBox(width: 10),
                    Text('Scan QR Code',
                        style: TextStyle(
                            color: Colors.white,
                            fontWeight: FontWeight.w700,
                            fontSize: 15)),
                  ],
                ),
              ),
            ),

            Padding(
              padding: const EdgeInsets.symmetric(vertical: 16),
              child: Row(children: [
                const Expanded(child: Divider(color: AppColors.border)),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                  child: Text(s.orManual,
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 12)),
                ),
                const Expanded(child: Divider(color: AppColors.border)),
              ]),
            ),

            _label(s.deviceId),
            _field(_idCtrl, 'AC_001', Icons.memory_rounded),
            const SizedBox(height: 16),

            _label(s.deviceName),
            _field(_nameCtrl, s.deviceName, Icons.label_outline_rounded),
            const SizedBox(height: 20),

            _label(s.assignSite),
            ...widget.sites.map((site) => _siteTile(site)),
            const SizedBox(height: 28),

            _gradientBtn(s.addDevice, _adding, _addDevice),
          ],
        ),
      );

  Widget _siteTile(Site site) {
    final sel = _selectedSiteId == site.id;
    return GestureDetector(
      onTap: () => setState(() => _selectedSiteId = site.id),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        margin: const EdgeInsets.only(bottom: 8),
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        decoration: BoxDecoration(
          color: sel
              ? AppColors.primary.withOpacity(0.12)
              : AppColors.surface,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
              color: sel ? AppColors.primary : AppColors.border,
              width: sel ? 1.5 : 1),
        ),
        child: Row(children: [
          Icon(Icons.business_rounded,
              color: sel ? AppColors.primary : AppColors.textMuted, size: 18),
          const SizedBox(width: 12),
          Expanded(
              child: Text(site.name,
                  style: TextStyle(
                      color: sel ? AppColors.text : AppColors.textMuted,
                      fontWeight:
                          sel ? FontWeight.w700 : FontWeight.normal))),
          if (sel)
            const Icon(Icons.check_circle_rounded,
                color: AppColors.primary, size: 18),
        ]),
      ),
    );
  }

  // ── Device list tab ────────────────────────────────────────────────────────
  Widget _listTab(AppStrings s) => _loadingDevices
      ? const Center(
          child: CircularProgressIndicator(color: AppColors.primary))
      : RefreshIndicator(
          color: AppColors.primary,
          backgroundColor: AppColors.surface,
          onRefresh: _fetchDevices,
          child: _devices.isEmpty
              ? ListView(
                  padding: const EdgeInsets.all(32),
                  children: [
                    const SizedBox(height: 80),
                    Center(
                      child: Column(children: [
                        Icon(Icons.devices_other_rounded,
                            color: AppColors.textMuted.withOpacity(0.3),
                            size: 60),
                        const SizedBox(height: 12),
                        Text(s.noDevices,
                            style: const TextStyle(
                                color: AppColors.textMuted)),
                      ]),
                    ),
                  ],
                )
              : ListView.builder(
                  padding: const EdgeInsets.fromLTRB(16, 16, 16, 80),
                  itemCount: _devices.length,
                  itemBuilder: (_, i) {
                    final d = _devices[i];
                    final siteName = widget.sites
                        .firstWhere((s) => s.id == d.siteId,
                            orElse: () => Site(
                                id: '', name: '—', adminId: ''))
                        .name;
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
                          width: 40,
                          height: 40,
                          decoration: BoxDecoration(
                            color: _statusColor(d.lastStatus)
                                .withOpacity(0.12),
                            borderRadius: BorderRadius.circular(10),
                          ),
                          child: Icon(Icons.ac_unit_rounded,
                              color: _statusColor(d.lastStatus), size: 20),
                        ),
                        title: Text(d.displayName,
                            style: const TextStyle(
                                color: AppColors.text,
                                fontWeight: FontWeight.w700,
                                fontSize: 14)),
                        subtitle: Text(
                            '${d.deviceId}  •  $siteName',
                            style: const TextStyle(
                                color: AppColors.textMuted, fontSize: 11)),
                        trailing: Row(mainAxisSize: MainAxisSize.min, children: [
                          // IR status dot
                          Container(
                            width: 8,
                            height: 8,
                            decoration: BoxDecoration(
                              shape: BoxShape.circle,
                              color: _statusColor(d.lastStatus),
                            ),
                          ),
                          const SizedBox(width: 8),
                          IconButton(
                            onPressed: () =>
                                _deleteDevice(d.id, d.displayName),
                            icon: const Icon(Icons.delete_outline_rounded,
                                color: AppColors.error, size: 20),
                            padding: EdgeInsets.zero,
                            constraints: const BoxConstraints(),
                          ),
                        ]),
                      ),
                    );
                  },
                ),
        );

  // ── QR screen ──────────────────────────────────────────────────────────────
  Widget _qrScreen(AppStrings s) => Scaffold(
        backgroundColor: Colors.black,
        body: Stack(children: [
          MobileScanner(onDetect: _onQr),
          Positioned(
            top: 50,
            left: 20,
            child: SafeArea(
              child: IconButton(
                onPressed: () => setState(() => _scanning = false),
                icon: const Icon(Icons.close_rounded,
                    color: Colors.white, size: 28),
              ),
            ),
          ),
          Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Container(
                  width: 220,
                  height: 220,
                  decoration: BoxDecoration(
                    border: Border.all(color: AppColors.primary, width: 3),
                    borderRadius: BorderRadius.circular(16),
                  ),
                ),
                const SizedBox(height: 24),
                Container(
                  padding: const EdgeInsets.symmetric(
                      horizontal: 20, vertical: 10),
                  decoration: BoxDecoration(
                    color: Colors.black54,
                    borderRadius: BorderRadius.circular(20),
                  ),
                  child: Text(s.scanQr,
                      style: const TextStyle(
                          color: Colors.white, fontSize: 14)),
                ),
              ],
            ),
          ),
        ]),
      );

  Color _statusColor(String status) {
    switch (status) {
      case 'online':
        return AppColors.success;
      case 'offline':
        return AppColors.error;
      case 'unstable':
        return AppColors.warning;
      default:
        return AppColors.textMuted;
    }
  }

  Widget _label(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: Text(text,
            style: const TextStyle(
                color: AppColors.textMuted,
                fontSize: 12,
                fontWeight: FontWeight.w600,
                letterSpacing: 0.5)),
      );

  Widget _field(TextEditingController ctrl, String hint, IconData icon) =>
      TextField(
        controller: ctrl,
        autocorrect: false,
        style: const TextStyle(color: AppColors.text, fontSize: 15),
        decoration: InputDecoration(
          hintText: hint,
          hintStyle:
              const TextStyle(color: AppColors.textMuted, fontSize: 14),
          prefixIcon: Icon(icon, color: AppColors.textMuted, size: 20),
          filled: true,
          fillColor: AppColors.surface,
          isDense: true,
          contentPadding:
              const EdgeInsets.symmetric(vertical: 16, horizontal: 14),
          border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide: const BorderSide(color: AppColors.border)),
          enabledBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide: const BorderSide(color: AppColors.border)),
          focusedBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide:
                  const BorderSide(color: AppColors.primary, width: 2)),
        ),
      );

  Widget _gradientBtn(String label, bool loading, VoidCallback onTap) =>
      SizedBox(
        width: double.infinity,
        height: 52,
        child: DecoratedBox(
          decoration: BoxDecoration(
            gradient: loading
                ? null
                : const LinearGradient(
                    colors: [AppColors.primary, Color(0xFF38BDF8)],
                    begin: Alignment.centerLeft,
                    end: Alignment.centerRight),
            color: loading ? AppColors.surfaceLight : null,
            borderRadius: BorderRadius.circular(14),
            boxShadow: loading
                ? null
                : [
                    BoxShadow(
                        color: AppColors.primary.withOpacity(0.35),
                        blurRadius: 16,
                        offset: const Offset(0, 6))
                  ],
          ),
          child: ElevatedButton(
            onPressed: loading ? null : onTap,
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.transparent,
              shadowColor: Colors.transparent,
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(14)),
            ),
            child: loading
                ? const SizedBox(
                    width: 22,
                    height: 22,
                    child: CircularProgressIndicator(
                        color: Colors.white, strokeWidth: 2.5))
                : Text(label,
                    style: const TextStyle(
                        color: Colors.white,
                        fontSize: 16,
                        fontWeight: FontWeight.w700)),
          ),
        ),
      );
}
