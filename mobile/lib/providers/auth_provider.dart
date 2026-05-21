import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'dart:convert';
import '../core/api_client.dart';
import '../core/socket_service.dart';
import '../models/user.dart';

class AuthProvider extends ChangeNotifier {
  static const _storage = FlutterSecureStorage(
    webOptions: WebOptions(dbName: 'iot_ac', publicKey: 'iot_ac_key'),
  );

  User? _user;
  bool _isLoading = true;
  String? _error;

  User? get user => _user;
  bool get isAuthenticated => _user != null;
  bool get isLoading => _isLoading;
  String? get error => _error;

  Future<void> loadFromStorage() async {
    try {
      final userStr = await _storage.read(key: 'user');
      final token = await _storage.read(key: 'accessToken');
      if (userStr != null && token != null) {
        _user = User.fromJson(jsonDecode(userStr) as Map<String, dynamic>);
      }
    } catch (_) {
      _user = null;
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  Future<void> login(String email, String password) async {
    _isLoading = true;
    _error = null;
    notifyListeners();
    try {
      final resp = await ApiClient.instance.post(
        '/auth/login',
        data: {'email': email, 'password': password},
      );
      final data = resp.data as Map<String, dynamic>;
      _user = User.fromJson(data['user'] as Map<String, dynamic>);
      await _storage.write(key: 'accessToken', value: data['accessToken'] as String);
      await _storage.write(key: 'refreshToken', value: data['refreshToken'] as String);
      await _storage.write(key: 'user', value: jsonEncode(_user!.toJson()));
    } catch (e) {
      _error = ApiClient.errorMessage(e);
      rethrow;
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  Future<void> register(String email, String password, String fullName) async {
    _isLoading = true;
    _error = null;
    notifyListeners();
    try {
      await ApiClient.instance.post(
        '/auth/register',
        data: {'email': email, 'password': password, 'full_name': fullName},
      );
    } catch (e) {
      _error = ApiClient.errorMessage(e);
      rethrow;
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  Future<void> logout() async {
    try {
      final refreshToken = await _storage.read(key: 'refreshToken');
      try {
        await ApiClient.instance
            .post('/auth/logout', data: {'refreshToken': refreshToken});
      } catch (_) {}
    } catch (_) {}
    await _storage.deleteAll();
    SocketService.disconnect();
    _user = null;
    notifyListeners();
  }

  void clearError() {
    _error = null;
    notifyListeners();
  }
}
