import 'package:flutter/foundation.dart';
import '../core/api_client.dart';
import '../models/device.dart';

class DeviceProvider extends ChangeNotifier {
  List<Device> _devices = [];
  bool _isLoading = false;
  String? _error;

  List<Device> get devices => _devices;
  bool get isLoading => _isLoading;
  String? get error => _error;

  List<Device> get offlineDevices =>
      _devices.where((d) => d.lastStatus == 'offline').toList();
  List<Device> get unstableDevices =>
      _devices.where((d) => d.lastStatus == 'unstable').toList();

  Future<void> fetchDevices() async {
    _isLoading = true;
    _error = null;
    notifyListeners();
    try {
      final resp = await ApiClient.instance.get('/devices');
      final list = (resp.data['devices'] as List)
          .map((d) => Device.fromJson(Map<String, dynamic>.from(d as Map)))
          .toList();
      _devices = list;
    } catch (e) {
      _error = ApiClient.errorMessage(e);
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  void updateDeviceStatus(String deviceId, String status) {
    final idx = _devices.indexWhere((d) => d.deviceId == deviceId);
    if (idx != -1) {
      _devices[idx].lastStatus = status;
      notifyListeners();
    }
  }

  void updateDeviceTelemetry(String deviceId, Telemetry telemetry) {
    final idx = _devices.indexWhere((d) => d.deviceId == deviceId);
    if (idx != -1) {
      _devices[idx].lastTelemetry = telemetry;
      _devices[idx].lastTelemetryAt = telemetry.timestamp;
      notifyListeners();
    }
  }

  /// Forcefully clears a stuck loading state.
  /// Call this when the app resumes from background in case a previous
  /// fetchDevices() was left hanging by the OS suspending the network request.
  void resetLoading() {
    if (_isLoading) {
      _isLoading = false;
      notifyListeners();
    }
  }

  Device? findById(String id) {
    try {
      return _devices.firstWhere((d) => d.id == id);
    } catch (_) {
      return null;
    }
  }
}
