# IoT AC Controller — Flutter Mobile App

Android APK for the IoT AC Controller platform. Built with Flutter (Dart).

## Prerequisites

- Flutter SDK ≥ 3.19.0 (`flutter --version`)
- Android SDK ≥ API 21 (Android 5.0)
- Java 17 (bundled with Android Studio or set `JAVA_HOME`)

## Setup

### 1. Configure backend URL

Edit [`lib/core/constants.dart`](lib/core/constants.dart):

```dart
// Android emulator (default) — maps to host machine localhost
static const String apiBaseUrl = 'http://10.0.2.2:3000';

// Real device on same Wi-Fi — use your machine's local IP
static const String apiBaseUrl = 'http://192.168.x.x:3000';
```

### 2. Create android/local.properties

Copy the example and fill in your SDK paths:

```bash
cp android/local.properties.example android/local.properties
```

Edit `android/local.properties`:

```
sdk.dir=C:/Users/<you>/AppData/Local/Android/Sdk
flutter.sdk=C:/src/flutter
```

### 3. Install dependencies

```bash
flutter pub get
```

## Running

### Android emulator

```bash
flutter emulators --launch <emulator_id>
flutter run
```

### Physical device (USB debugging enabled)

```bash
flutter devices
flutter run -d <device_id>
```

## Building the APK

### Debug APK (fastest, for testing)

```bash
flutter build apk --debug
# Output: build/app/outputs/flutter-apk/app-debug.apk
```

### Release APK

```bash
flutter build apk --release
# Output: build/app/outputs/flutter-apk/app-release.apk
```

### Split APKs by ABI (smaller download)

```bash
flutter build apk --split-per-abi --release
# Outputs:
#   build/app/outputs/flutter-apk/app-arm64-v8a-release.apk   (modern devices)
#   build/app/outputs/flutter-apk/app-armeabi-v7a-release.apk (older 32-bit devices)
```

Install the split APK on a connected device:

```bash
flutter install
```

## Project Structure

```
lib/
├── main.dart                    # Entry point, MultiProvider, root router
├── core/
│   ├── api_client.dart          # Dio HTTP client with JWT auto-refresh
│   ├── socket_service.dart      # Socket.IO wrapper
│   └── constants.dart           # Colors, API URL, routes
├── models/
│   ├── user.dart
│   ├── device.dart              # Device, Telemetry, ConnectionLog
│   ├── site.dart
│   └── plan.dart                # Plan, PlanRule, PlanSlice
├── providers/
│   ├── auth_provider.dart       # Login/logout, token storage
│   └── device_provider.dart     # Device list state
├── screens/
│   ├── login_screen.dart
│   ├── register_screen.dart
│   ├── device_control_screen.dart
│   ├── device_history_screen.dart
│   ├── planning_screen.dart
│   ├── technician_logs_screen.dart
│   ├── user/user_dashboard.dart
│   ├── admin/
│   │   ├── admin_dashboard.dart
│   │   ├── add_device_screen.dart
│   │   └── manage_users_screen.dart
│   └── superadmin/superadmin_dashboard.dart
└── widgets/
    ├── status_badge.dart
    └── device_card.dart
```

## Role-Based Navigation

| Role         | Home Screen              |
|--------------|--------------------------|
| `superadmin` | SuperadminDashboard      |
| `admin`      | AdminDashboard           |
| `user`       | UserDashboard            |
| `technician` | UserDashboard (+ logs tab) |

## QR Device Pairing

The admin "Add Device" screen accepts two QR formats:

- **Pipe format**: `AC_001|mqtt.example.com`
- **JSON format**: `{"id":"AC_001","broker":"mqtt.example.com"}`

Generate a QR code for a device from the backend's `/devices/:id/qr` endpoint or any QR generator.

## Real-Time Events (Socket.IO)

| Event                 | Payload                                      |
|-----------------------|----------------------------------------------|
| `telemetry_update`    | `{device_id, temperature, humidity, ...}`    |
| `device_status_change`| `{device_id, status}`                        |
| `command_ack`         | `{device_id, cmd, success, message}`         |
| `device_log`          | `{device_id, log}`                           |
