import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

class LanguageProvider extends ChangeNotifier {
  static const _storage = FlutterSecureStorage(
    webOptions: WebOptions(dbName: 'iot_ac', publicKey: 'iot_ac_key'),
  );

  String _language = 'fr'; // default: French

  String get language => _language;
  bool get isFrench => _language == 'fr';

  Future<void> load() async {
    final stored = await _storage.read(key: 'lang');
    if (stored != null) {
      _language = stored;
      notifyListeners();
    }
  }

  Future<void> setLanguage(String lang) async {
    if (_language == lang) return;
    _language = lang;
    await _storage.write(key: 'lang', value: lang);
    notifyListeners();
  }
}
