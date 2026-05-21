import 'package:dio/dio.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'constants.dart';

class ApiClient {
  static const _storage = FlutterSecureStorage(
    webOptions: WebOptions(dbName: 'iot_ac', publicKey: 'iot_ac_key'),
  );
  static bool _initialized = false;

  /// Set this callback from the widget tree so that when a refresh token is
  /// expired or invalid, the app can clear the auth state and go to login.
  /// Example: ApiClient.onSessionExpired = () => authProvider.logout();
  static void Function()? onSessionExpired;

  static final Dio _dio = Dio(
    BaseOptions(
      baseUrl: AppConstants.apiBaseUrl,
      connectTimeout: const Duration(seconds: 15),
      receiveTimeout: const Duration(seconds: 15),
      headers: {'Content-Type': 'application/json'},
    ),
  );

  static void init() {
    if (_initialized) return;
    _initialized = true;

    _dio.interceptors.add(
      InterceptorsWrapper(
        onRequest: (options, handler) async {
          final token = await _storage.read(key: 'accessToken');
          if (token != null) {
            options.headers['Authorization'] = 'Bearer $token';
          }
          handler.next(options);
        },
        onError: (DioException error, handler) async {
          final isRetry = error.requestOptions.extra['_retry'] == true;
          if (error.response?.statusCode == 401 && !isRetry) {
            try {
              final refreshToken = await _storage.read(key: 'refreshToken');
              if (refreshToken == null) {
                // No refresh token stored — session is gone.
                _expireSession();
                handler.next(error);
                return;
              }

              final resp = await Dio().post(
                '${AppConstants.apiBaseUrl}/auth/refresh',
                data: {'refreshToken': refreshToken},
              );

              await _storage.write(
                key: 'accessToken',
                value: resp.data['accessToken'] as String,
              );
              await _storage.write(
                key: 'refreshToken',
                value: resp.data['refreshToken'] as String,
              );

              error.requestOptions.headers['Authorization'] =
                  'Bearer ${resp.data['accessToken']}';
              error.requestOptions.extra['_retry'] = true;

              final retryResponse = await _dio.fetch(error.requestOptions);
              handler.resolve(retryResponse);
              return;
            } catch (_) {
              // Refresh token is expired or rejected — force logout.
              _expireSession();
            }
          }
          handler.next(error);
        },
      ),
    );
  }

  /// Clear stored credentials and notify the app to show the login screen.
  static Future<void> _expireSession() async {
    await _storage.deleteAll();
    onSessionExpired?.call();
  }

  static Dio get instance => _dio;

  static String errorMessage(Object e) {
    if (e is DioException) {
      final data = e.response?.data;
      if (data is Map && data['error'] != null) return data['error'] as String;
      return e.message ?? 'Network error';
    }
    return e.toString();
  }
}
