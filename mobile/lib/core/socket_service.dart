import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:socket_io_client/socket_io_client.dart' as IO;
import 'constants.dart';

/// Socket.IO client wrapper with per-subscriber event handling.
///
/// Design
/// ──────
/// • Multiple screens can listen to the same event independently by passing
///   a unique [id] string (e.g. 'user_dashboard', 'device_control').
/// • Calling [off] with an id removes ONLY that subscriber's handler, so one
///   screen's dispose() never silences another screen's listeners.
/// • Handlers registered before [connect] is called are stored in [_handlers]
///   and attached to the socket the moment it first connects (_attachAllDispatchers).
/// • One internal dispatcher function is registered per event on the socket; it
///   fans out to all registered subscribers for that event.
/// • Automatic reconnection (built-in socket_io_client) keeps the same socket
///   object, so dispatcher references survive reconnects.  A new socket object
///   (created by a fresh [connect] call) clears [_registeredEvents] so
///   _attachAllDispatchers re-registers every dispatcher.
class SocketService {
  static IO.Socket? _socket;
  static const _storage = FlutterSecureStorage();

  /// Device IDs that should be re-subscribed on every (re)connect.
  static final Set<String> _trackedDevices = {};

  /// Callbacks invoked on every successful (re)connect.
  static final List<void Function()> _onReconnectCallbacks = [];

  /// Event handlers indexed by [event][subscriberId].
  /// Stored even before the socket is created so [on] can be called early.
  static final Map<String, Map<String, Function(dynamic)>> _handlers = {};

  /// Tracks which events already have a dispatcher registered on [_socket].
  /// Cleared when a new socket object is created.
  static final Set<String> _registeredEvents = {};

  static IO.Socket? get socket => _socket;

  static Future<IO.Socket> connect() async {
    if (_socket != null && _socket!.connected) return _socket!;

    // Dispose the old (disconnected) socket cleanly before creating a new one.
    if (_socket != null) {
      _socket!.dispose();
      _socket = null;
    }

    final token = await _storage.read(key: 'accessToken');

    // New socket object — clear registered-event tracking so we re-attach
    // all dispatchers when onConnect fires.
    _registeredEvents.clear();

    _socket = IO.io(
      AppConstants.socketUrl,
      IO.OptionBuilder()
          .setTransports(['websocket'])
          .setAuth({'token': token ?? ''})
          .enableReconnection()
          .setReconnectionDelay(2000)
          .build(),
    );

    _socket!.onConnect((_) {
      // Re-subscribe to all tracked device rooms.
      for (final id in _trackedDevices) {
        _socket?.emit('subscribe_device', id);
      }
      // Attach dispatchers for any handlers registered before connect,
      // and for events not yet on this socket object.
      _attachAllDispatchers();
      // Notify screens so they can refresh stale state.
      for (final cb in _onReconnectCallbacks) cb();
    });

    _socket!.connect();
    return _socket!;
  }

  /// Register a single dispatcher per event on the socket.
  /// Each dispatcher reads [_handlers[event]] at call time, so adding or
  /// removing subscribers after registration is reflected automatically.
  static void _attachAllDispatchers() {
    for (final event in _handlers.keys) {
      if (!_registeredEvents.contains(event)) {
        _registeredEvents.add(event);
        _socket?.on(event, (data) {
          final subs = _handlers[event]?.values.toList() ?? [];
          for (final h in subs) h(data);
        });
      }
    }
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  /// Subscribe to [event] under the unique subscriber [id].
  ///
  /// If [id] already has a handler for this event it is replaced (idempotent).
  /// Safe to call before [connect] — the handler is stored and applied the
  /// moment the socket connects.
  static void on(String event, Function(dynamic) handler, {required String id}) {
    _handlers.putIfAbsent(event, () => {})[id] = handler;

    // If the socket already exists, register the dispatcher immediately so
    // the handler fires without waiting for the next onConnect.
    if (_socket != null && !_registeredEvents.contains(event)) {
      _registeredEvents.add(event);
      _socket!.on(event, (data) {
        final subs = _handlers[event]?.values.toList() ?? [];
        for (final h in subs) h(data);
      });
    }
  }

  /// Remove the handler registered under [id] for [event].
  ///
  /// If [id] is the last subscriber, the dispatcher is also removed from the
  /// socket so there are no dangling listeners.
  static void off(String event, {required String id}) {
    _handlers[event]?.remove(id);
    if (_handlers[event]?.isEmpty ?? true) {
      _socket?.off(event);
      _handlers.remove(event);
      _registeredEvents.remove(event);
    }
  }

  /// Subscribe to a device room and remember it for automatic re-subscription
  /// after a socket reconnect.
  static void subscribeToDevice(String deviceId) {
    _trackedDevices.add(deviceId);
    _socket?.emit('subscribe_device', deviceId);
  }

  static void unsubscribeFromDevice(String deviceId) {
    _trackedDevices.remove(deviceId);
    _socket?.emit('unsubscribe_device', deviceId);
  }

  /// Register [cb] to be called on every successful (re)connect.
  static void addReconnectCallback(void Function() cb) {
    if (!_onReconnectCallbacks.contains(cb)) _onReconnectCallbacks.add(cb);
  }

  static void removeReconnectCallback(void Function() cb) {
    _onReconnectCallbacks.remove(cb);
  }

  /// Reconnect with a fresh token after the app returns from background.
  ///
  /// Unlike [disconnect], this preserves all registered event handlers and
  /// tracked device subscriptions — they will be re-applied the moment the
  /// new socket fires onConnect.  Call this from an AppLifecycleObserver when
  /// [AppLifecycleState.resumed] is detected.
  static Future<IO.Socket> reconnect() async {
    if (_socket != null) {
      _socket!.dispose();
      _socket = null;
      _registeredEvents.clear();
      // _handlers and _trackedDevices are intentionally kept intact.
    }
    return connect();
  }

  /// Full teardown — called on logout.  Clears everything.
  static void disconnect() {
    _socket?.dispose();
    _socket = null;
    _trackedDevices.clear();
    _onReconnectCallbacks.clear();
    _handlers.clear();
    _registeredEvents.clear();
  }
}
