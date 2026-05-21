import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import '../../core/api_client.dart';
import '../../core/constants.dart';
import '../../models/site.dart';
import 'dart:convert';

class AddDeviceScreen extends StatefulWidget {
  const AddDeviceScreen({super.key});

  @override
  State<AddDeviceScreen> createState() => _AddDeviceScreenState();
}

class _AddDeviceScreenState extends State<AddDeviceScreen> {
  final _idCtrl = TextEditingController();
  final _nameCtrl = TextEditingController();
  List<Site> _sites = [];
  String? _selectedSiteId;
  bool _scanning = false;
  bool _loading = false;
  bool _scanned = false;

  @override
  void initState() {
    super.initState();
    _loadSites();
  }

  @override
  void dispose() {
    _idCtrl.dispose();
    _nameCtrl.dispose();
    super.dispose();
  }

  Future<void> _loadSites() async {
    try {
      final resp = await ApiClient.instance.get('/sites');
      setState(() {
        _sites = (resp.data['sites'] as List)
            .map((s) => Site.fromJson(Map<String, dynamic>.from(s as Map)))
            .toList();
      });
    } catch (_) {}
  }

  void _onQrDetected(BarcodeCapture capture) {
    if (_scanned) return;
    final raw = capture.barcodes.firstOrNull?.rawValue;
    if (raw == null) return;

    setState(() => _scanned = true);

    String deviceId = raw;
    if (raw.contains('|')) {
      deviceId = raw.split('|').first.trim();
    } else {
      try {
        final parsed = jsonDecode(raw) as Map<String, dynamic>;
        deviceId = parsed['id'] as String? ?? raw;
      } catch (_) {}
    }

    setState(() {
      _idCtrl.text = deviceId;
      _scanning = false;
    });
  }

  Future<void> _addDevice() async {
    if (_idCtrl.text.trim().isEmpty) {
      _snack('Device ID is required', isError: true);
      return;
    }
    if (_selectedSiteId == null) {
      _snack('Please select a site', isError: true);
      return;
    }

    setState(() => _loading = true);
    try {
      await ApiClient.instance.post('/devices', data: {
        'device_id': _idCtrl.text.trim().toUpperCase(),
        'name': _nameCtrl.text.trim().isEmpty ? null : _nameCtrl.text.trim(),
        'site_id': _selectedSiteId,
      });
      if (mounted) {
        _snack('Device added successfully');
        Navigator.of(context).pop();
      }
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _loading = false);
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
    if (_scanning) {
      return Scaffold(
        backgroundColor: Colors.black,
        body: Stack(
          children: [
            MobileScanner(
              onDetect: _onQrDetected,
            ),
            // Overlay
            Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Container(
                    width: 220,
                    height: 220,
                    decoration: BoxDecoration(
                      border: Border.all(color: AppColors.primary, width: 3),
                      borderRadius: BorderRadius.circular(12),
                    ),
                  ),
                  const SizedBox(height: 24),
                  const Text(
                    'Point at the device QR code',
                    style: TextStyle(color: Colors.white, fontSize: 14),
                  ),
                  const SizedBox(height: 24),
                  ElevatedButton(
                    onPressed: () => setState(() => _scanning = false),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.surface,
                      foregroundColor: AppColors.text,
                    ),
                    child: const Text('Cancel'),
                  ),
                ],
              ),
            ),
          ],
        ),
      );
    }

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Add Device'),
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.text,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // QR Scanner button
            GestureDetector(
              onTap: () => setState(() {
                _scanned = false;
                _scanning = true;
              }),
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.symmetric(vertical: 18),
                decoration: BoxDecoration(
                  color: AppColors.surface,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: AppColors.primary, width: 2),
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.qr_code_scanner, color: AppColors.primary, size: 24),
                    SizedBox(width: 10),
                    Text(
                      'Scan QR Code',
                      style: TextStyle(
                        color: AppColors.primary,
                        fontWeight: FontWeight.w700,
                        fontSize: 15,
                      ),
                    ),
                  ],
                ),
              ),
            ),

            const Padding(
              padding: EdgeInsets.symmetric(vertical: 14),
              child: Row(
                children: [
                  Expanded(child: Divider(color: AppColors.border)),
                  Padding(
                    padding: EdgeInsets.symmetric(horizontal: 12),
                    child: Text('or enter manually',
                        style: TextStyle(color: AppColors.textMuted, fontSize: 12)),
                  ),
                  Expanded(child: Divider(color: AppColors.border)),
                ],
              ),
            ),

            _label('Device ID'),
            _input(_idCtrl, 'e.g. AC_001'),
            const SizedBox(height: 16),
            _label('Device Name (optional)'),
            _input(_nameCtrl, 'e.g. Office AC Unit'),
            const SizedBox(height: 20),

            _label('Assign to Site'),
            ..._sites.map((s) => GestureDetector(
                  onTap: () => setState(() => _selectedSiteId = s.id),
                  child: Container(
                    margin: const EdgeInsets.only(bottom: 8),
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
                    decoration: BoxDecoration(
                      color: _selectedSiteId == s.id
                          ? AppColors.primary.withOpacity(0.15)
                          : AppColors.surface,
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: _selectedSiteId == s.id ? AppColors.primary : AppColors.border,
                      ),
                    ),
                    child: Row(
                      children: [
                        if (_selectedSiteId == s.id)
                          const Padding(
                            padding: EdgeInsets.only(right: 8),
                            child: Icon(Icons.check, color: AppColors.primary, size: 16),
                          ),
                        Text(
                          s.name,
                          style: TextStyle(
                            color: _selectedSiteId == s.id ? AppColors.text : AppColors.textMuted,
                            fontWeight: _selectedSiteId == s.id
                                ? FontWeight.w700
                                : FontWeight.normal,
                          ),
                        ),
                      ],
                    ),
                  ),
                )),

            const SizedBox(height: 24),
            SizedBox(
              width: double.infinity,
              height: 52,
              child: ElevatedButton(
                onPressed: _loading ? null : _addDevice,
                style: ElevatedButton.styleFrom(
                  backgroundColor: AppColors.primary,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                ),
                child: _loading
                    ? const CircularProgressIndicator(color: Colors.white, strokeWidth: 2)
                    : const Text('Add Device',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w700)),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _label(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 6),
        child: Text(text,
            style: const TextStyle(
              color: AppColors.textMuted,
              fontSize: 13,
              fontWeight: FontWeight.w600,
            )),
      );

  Widget _input(TextEditingController ctrl, String hint) => TextField(
        controller: ctrl,
        style: const TextStyle(color: AppColors.text),
        autocorrect: false,
        decoration: InputDecoration(
          hintText: hint,
          hintStyle: const TextStyle(color: AppColors.textMuted),
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
            borderSide: const BorderSide(color: AppColors.primary, width: 2),
          ),
        ),
      );
}
