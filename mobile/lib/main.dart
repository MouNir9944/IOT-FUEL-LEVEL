import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'core/api_client.dart';
import 'core/constants.dart';
import 'core/socket_service.dart';
import 'providers/auth_provider.dart';
import 'providers/device_provider.dart';
import 'providers/language_provider.dart';
import 'screens/login_screen.dart';
import 'screens/register_screen.dart';
import 'screens/user/user_dashboard.dart';
import 'screens/admin/admin_dashboard.dart';
import 'screens/superadmin/superadmin_dashboard.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  ApiClient.init();
  final langProvider = LanguageProvider();
  await langProvider.load();
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider.value(value: langProvider),
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProvider(create: (_) => DeviceProvider()),
      ],
      child: const FuelSenseApp(),
    ),
  );
}

class FuelSenseApp extends StatelessWidget {
  const FuelSenseApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'FuelSense',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: AppColors.background,
        colorScheme: const ColorScheme.dark(
          primary: AppColors.primary,
          surface: AppColors.surface,
          error: AppColors.error,
        ),
        appBarTheme: const AppBarTheme(
          backgroundColor: AppColors.surface,
          foregroundColor: AppColors.text,
          elevation: 0,
        ),
        textTheme: const TextTheme(
          bodyMedium: TextStyle(color: AppColors.text),
        ),
        inputDecorationTheme: InputDecorationTheme(
          filled: true,
          fillColor: AppColors.surface,
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(12),
            borderSide: const BorderSide(color: AppColors.border),
          ),
          enabledBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(12),
            borderSide: const BorderSide(color: AppColors.border),
          ),
          focusedBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(12),
            borderSide: const BorderSide(color: AppColors.primary, width: 2),
          ),
          labelStyle: const TextStyle(color: AppColors.textMuted),
          hintStyle: const TextStyle(color: AppColors.textMuted),
        ),
        elevatedButtonTheme: ElevatedButtonThemeData(
          style: ElevatedButton.styleFrom(
            backgroundColor: AppColors.primary,
            foregroundColor: Colors.white,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
          ),
        ),
      ),
      home: const _RootRouter(),
      routes: {
        '/register': (_) => const RegisterScreen(),
      },
    );
  }
}

class _RootRouter extends StatefulWidget {
  const _RootRouter();

  @override
  State<_RootRouter> createState() => _RootRouterState();
}

class _RootRouterState extends State<_RootRouter> with WidgetsBindingObserver {
  bool _initialized = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _init();
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) _onAppResumed();
  }

  Future<void> _onAppResumed() async {
    if (!mounted) return;
    final auth = context.read<AuthProvider>();
    if (!auth.isAuthenticated) return;
    final deviceProv = context.read<DeviceProvider>();
    deviceProv.resetLoading();
    await SocketService.reconnect();
    deviceProv.fetchDevices();
  }

  Future<void> _init() async {
    final auth = context.read<AuthProvider>();
    ApiClient.onSessionExpired = () {
      if (mounted) auth.logout();
    };
    await auth.loadFromStorage();
    if (mounted) setState(() => _initialized = true);
  }

  @override
  Widget build(BuildContext context) {
    if (!_initialized) {
      return const Scaffold(
        backgroundColor: AppColors.background,
        body: Center(
          child: CircularProgressIndicator(color: AppColors.primary),
        ),
      );
    }

    return Consumer<AuthProvider>(
      builder: (context, auth, _) {
        if (!auth.isAuthenticated) return const LoginScreen();

        switch (auth.user?.role) {
          case 'superadmin':
            return const SuperadminDashboard();
          case 'admin':
            return const AdminDashboard();
          case 'user':
          case 'technician':
          default:
            return const UserDashboard();
        }
      },
    );
  }
}
