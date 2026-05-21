import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../core/constants.dart';
import '../core/app_strings.dart';
import '../providers/auth_provider.dart';

class RegisterScreen extends StatefulWidget {
  const RegisterScreen({super.key});

  @override
  State<RegisterScreen> createState() => _RegisterScreenState();
}

class _RegisterScreenState extends State<RegisterScreen>
    with SingleTickerProviderStateMixin {
  final _nameCtrl = TextEditingController();
  final _emailCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  final _confirmCtrl = TextEditingController();
  bool _obscure = true;
  bool _loading = false;
  String? _error;
  late AnimationController _anim;
  late Animation<double> _fade;

  @override
  void initState() {
    super.initState();
    _anim = AnimationController(
        vsync: this, duration: const Duration(milliseconds: 600));
    _fade = CurvedAnimation(parent: _anim, curve: Curves.easeOut);
    _anim.forward();
  }

  @override
  void dispose() {
    _anim.dispose();
    _nameCtrl.dispose();
    _emailCtrl.dispose();
    _passCtrl.dispose();
    _confirmCtrl.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final s = AppStrings.read(context);
    final name = _nameCtrl.text.trim();
    final email = _emailCtrl.text.trim();
    final pass = _passCtrl.text;
    final confirm = _confirmCtrl.text;

    if (name.isEmpty || email.isEmpty || pass.isEmpty) {
      setState(() => _error = s.allFields);
      return;
    }
    if (pass != confirm) {
      setState(() => _error = 'Passwords do not match');
      return;
    }
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      await context.read<AuthProvider>().register(email, pass, name);
      if (mounted) Navigator.of(context).pop();
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
    final s = AppStrings.of(context);
    final size = MediaQuery.of(context).size;
    final hPad = size.width > 500 ? (size.width - 440) / 2 : 24.0;

    return Scaffold(
      backgroundColor: AppColors.background,
      body: Stack(
        children: [
          Positioned(
            top: -60, left: -60,
            child: _blob(240, AppColors.primary.withOpacity(0.12)),
          ),
          Positioned(
            bottom: -80, right: -60,
            child: _blob(280, AppColors.primary.withOpacity(0.10)),
          ),
          SafeArea(
            child: SingleChildScrollView(
              padding: EdgeInsets.symmetric(horizontal: hPad),
              child: FadeTransition(
                opacity: _fade,
                child: Column(
                  children: [
                    const SizedBox(height: 20),

                    // Back button + title row
                    Row(
                      children: [
                        IconButton(
                          onPressed: () => Navigator.of(context).pop(),
                          icon: const Icon(Icons.arrow_back_ios_new_rounded,
                              color: AppColors.text, size: 20),
                        ),
                        const SizedBox(width: 4),
                        Text(s.register,
                            style: const TextStyle(
                              color: AppColors.text,
                              fontSize: 22,
                              fontWeight: FontWeight.w800,
                            )),
                      ],
                    ),

                    const SizedBox(height: 32),

                    // Form card
                    Container(
                      padding: const EdgeInsets.all(24),
                      decoration: BoxDecoration(
                        color: AppColors.surface,
                        borderRadius: BorderRadius.circular(20),
                        border: Border.all(color: AppColors.border),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black.withOpacity(0.2),
                            blurRadius: 24,
                            offset: const Offset(0, 8),
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
                            hint: s.fullName,
                            ctrl: _nameCtrl,
                            icon: Icons.person_outline_rounded,
                          ),
                          const SizedBox(height: 12),
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
                          const SizedBox(height: 12),
                          _field(
                            hint: s.confirmPassword,
                            ctrl: _confirmCtrl,
                            icon: Icons.lock_outline_rounded,
                            obscure: _obscure,
                            onSubmit: (_) => _submit(),
                          ),
                          const SizedBox(height: 24),
                          _gradientButton(
                              label: s.register,
                              loading: _loading,
                              onTap: _submit),
                        ],
                      ),
                    ),

                    const SizedBox(height: 20),

                    TextButton(
                      onPressed: () => Navigator.of(context).pop(),
                      child: Text(s.hasAccount,
                          style: const TextStyle(
                              color: AppColors.primary, fontSize: 13)),
                    ),
                    const SizedBox(height: 40),
                  ],
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

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
                child: Text(msg,
                    style: const TextStyle(
                        color: AppColors.error, fontSize: 13))),
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

  Widget _gradientButton(
          {required String label,
          required bool loading,
          required VoidCallback onTap}) =>
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
                    )
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
