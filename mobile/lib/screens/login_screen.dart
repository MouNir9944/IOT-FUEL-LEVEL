import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../core/constants.dart';
import '../core/app_strings.dart';
import '../providers/auth_provider.dart';
import '../providers/language_provider.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen>
    with SingleTickerProviderStateMixin {
  final _emailCtrl = TextEditingController();
  final _passCtrl  = TextEditingController();
  bool _obscure  = true;
  bool _loading  = false;
  String? _error;
  late AnimationController _anim;
  late Animation<double> _fade;
  late Animation<Offset> _slide;

  @override
  void initState() {
    super.initState();
    _anim = AnimationController(
        vsync: this, duration: const Duration(milliseconds: 700));
    _fade  = CurvedAnimation(parent: _anim, curve: Curves.easeOut);
    _slide = Tween<Offset>(begin: const Offset(0, 0.08), end: Offset.zero)
        .animate(CurvedAnimation(parent: _anim, curve: Curves.easeOut));
    _anim.forward();
  }

  @override
  void dispose() {
    _anim.dispose();
    _emailCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final s     = AppStrings.read(context);
    final email = _emailCtrl.text.trim();
    final pass  = _passCtrl.text;
    if (email.isEmpty || pass.isEmpty) {
      setState(() => _error = s.allFields);
      return;
    }
    setState(() { _loading = true; _error = null; });
    try {
      await context.read<AuthProvider>().login(email, pass);
    } catch (e) {
      if (mounted) {
        setState(() => _error = e.toString().replaceFirst('Exception: ', ''));
      }
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final s    = AppStrings.of(context);
    final lang = context.watch<LanguageProvider>();
    final size = MediaQuery.of(context).size;
    final hPad = size.width > 500 ? (size.width - 440) / 2 : 24.0;

    return Scaffold(
      backgroundColor: AppColors.background,
      body: Stack(
        children: [
          // ── Decorative blobs ───────────────────────────────────────────────
          Positioned(
            top: -80, right: -60,
            child: _blob(260, AppColors.primary.withOpacity(0.15)),
          ),
          Positioned(
            bottom: -100, left: -80,
            child: _blob(300, AppColors.primary.withOpacity(0.08)),
          ),

          SafeArea(
            child: SingleChildScrollView(
              padding: EdgeInsets.symmetric(horizontal: hPad),
              child: FadeTransition(
                opacity: _fade,
                child: SlideTransition(
                  position: _slide,
                  child: Column(
                    children: [
                      const SizedBox(height: 56),

                      // ── Logo / icon ───────────────────────────────────────
                      Container(
                        width: 80,
                        height: 80,
                        decoration: BoxDecoration(
                          gradient: const LinearGradient(
                            colors: [AppColors.primary, Color(0xFFD97706)],
                            begin: Alignment.topLeft,
                            end: Alignment.bottomRight,
                          ),
                          borderRadius: BorderRadius.circular(24),
                          boxShadow: [
                            BoxShadow(
                              color: AppColors.primary.withOpacity(0.45),
                              blurRadius: 28,
                              offset: const Offset(0, 10),
                            ),
                          ],
                        ),
                        child: const Icon(
                          Icons.local_gas_station_rounded,
                          color: Colors.white,
                          size: 44,
                        ),
                      ),
                      const SizedBox(height: 20),

                      // ── App name ──────────────────────────────────────────
                      Text(
                        s.appName,
                        style: const TextStyle(
                          color: AppColors.text,
                          fontSize: 28,
                          fontWeight: FontWeight.w900,
                          letterSpacing: -0.5,
                        ),
                      ),
                      const SizedBox(height: 6),
                      Text(
                        s.appTagline,
                        style: const TextStyle(
                          color: AppColors.textMuted,
                          fontSize: 14,
                        ),
                      ),

                      const SizedBox(height: 44),

                      // ── Form card ─────────────────────────────────────────
                      Container(
                        padding: const EdgeInsets.all(24),
                        decoration: BoxDecoration(
                          color: AppColors.surface,
                          borderRadius: BorderRadius.circular(20),
                          border: Border.all(color: AppColors.border),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withOpacity(0.28),
                              blurRadius: 30,
                              offset: const Offset(0, 10),
                            ),
                          ],
                        ),
                        child: Column(
                          children: [
                            if (_error != null) ...[
                              _errorBanner(_error!),
                              const SizedBox(height: 16),
                            ],
                            _field(
                              hint: s.email,
                              ctrl: _emailCtrl,
                              icon: Icons.email_outlined,
                              type: TextInputType.emailAddress,
                            ),
                            const SizedBox(height: 12),
                            _field(
                              hint: s.password,
                              ctrl: _passCtrl,
                              icon: Icons.lock_outline_rounded,
                              obscure: _obscure,
                              onSubmit: (_) => _submit(),
                              suffix: IconButton(
                                icon: Icon(
                                  _obscure
                                      ? Icons.visibility_off_outlined
                                      : Icons.visibility_outlined,
                                  color: AppColors.textMuted,
                                  size: 20,
                                ),
                                onPressed: () =>
                                    setState(() => _obscure = !_obscure),
                              ),
                            ),
                            const SizedBox(height: 22),
                            _gradientButton(
                              label: s.login,
                              loading: _loading,
                              onTap: _submit,
                            ),
                          ],
                        ),
                      ),

                      const SizedBox(height: 18),

                      // Register link
                      TextButton(
                        onPressed: () =>
                            Navigator.of(context).pushNamed('/register'),
                        child: Text(
                          s.noAccount,
                          style: const TextStyle(
                            color: AppColors.primary,
                            fontSize: 13,
                          ),
                        ),
                      ),

                      const SizedBox(height: 16),

                      // Language switcher
                      Container(
                        decoration: BoxDecoration(
                          color: AppColors.surface,
                          borderRadius: BorderRadius.circular(30),
                          border: Border.all(color: AppColors.border),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            _langChip('fr', '🇫🇷  FR', lang),
                            _langChip('en', '🇬🇧  EN', lang),
                          ],
                        ),
                      ),

                      const SizedBox(height: 40),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  // ── Private helpers ────────────────────────────────────────────────────────

  Widget _blob(double size, Color color) => Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: RadialGradient(colors: [color, Colors.transparent]),
        ),
      );

  Widget _errorBanner(String msg) => Container(
        width: double.infinity,
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: AppColors.error.withOpacity(0.12),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: AppColors.error.withOpacity(0.3)),
        ),
        child: Row(
          children: [
            const Icon(Icons.error_outline, color: AppColors.error, size: 18),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                msg,
                style: const TextStyle(color: AppColors.error, fontSize: 13),
              ),
            ),
          ],
        ),
      );

  Widget _field({
    required String hint,
    required TextEditingController ctrl,
    required IconData icon,
    bool obscure = false,
    TextInputType? type,
    Widget? suffix,
    ValueChanged<String>? onSubmit,
  }) =>
      TextField(
        controller: ctrl,
        obscureText: obscure,
        keyboardType: type,
        autocorrect: false,
        onSubmitted: onSubmit,
        style: const TextStyle(color: AppColors.text, fontSize: 15),
        decoration: InputDecoration(
          hintText: hint,
          hintStyle:
              const TextStyle(color: AppColors.textMuted, fontSize: 14),
          prefixIcon: Icon(icon, color: AppColors.textMuted, size: 20),
          suffixIcon: suffix,
          filled: true,
          fillColor: AppColors.background,
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

  Widget _gradientButton({
    required String label,
    required bool loading,
    required VoidCallback onTap,
  }) =>
      SizedBox(
        width: double.infinity,
        height: 52,
        child: DecoratedBox(
          decoration: BoxDecoration(
            gradient: loading
                ? null
                : const LinearGradient(
                    colors: [AppColors.primary, Color(0xFFD97706)],
                    begin: Alignment.centerLeft,
                    end: Alignment.centerRight,
                  ),
            color: loading ? AppColors.surfaceLight : null,
            borderRadius: BorderRadius.circular(14),
            boxShadow: loading
                ? null
                : [
                    BoxShadow(
                      color: AppColors.primary.withOpacity(0.35),
                      blurRadius: 16,
                      offset: const Offset(0, 6),
                    ),
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
                : Text(
                    label,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 16,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
          ),
        ),
      );

  Widget _langChip(String code, String label, LanguageProvider lang) {
    final sel = lang.language == code;
    return GestureDetector(
      onTap: () => lang.setLanguage(code),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 9),
        decoration: BoxDecoration(
          color: sel ? AppColors.primary : Colors.transparent,
          borderRadius: BorderRadius.circular(30),
        ),
        child: Text(
          label,
          style: TextStyle(
            color: sel ? Colors.white : AppColors.textMuted,
            fontSize: 12,
            fontWeight: FontWeight.w600,
          ),
        ),
      ),
    );
  }
}
