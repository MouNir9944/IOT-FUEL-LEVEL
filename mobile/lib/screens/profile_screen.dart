import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../core/constants.dart';
import '../core/app_strings.dart';
import '../providers/auth_provider.dart';
import '../providers/language_provider.dart';

class ProfileScreen extends StatefulWidget {
  const ProfileScreen({super.key});

  @override
  State<ProfileScreen> createState() => _ProfileScreenState();
}

class _ProfileScreenState extends State<ProfileScreen> {
  @override
  Widget build(BuildContext context) {
    final s = AppStrings.of(context);
    final auth = context.watch<AuthProvider>();
    final lang = context.watch<LanguageProvider>();
    final user = auth.user;

    return Scaffold(
      backgroundColor: AppColors.background,
      body: CustomScrollView(
        slivers: [
          // ── Header ──────────────────────────────────────────────────────
          SliverAppBar(
            expandedHeight: 200,
            pinned: true,
            backgroundColor: AppColors.surface,
            foregroundColor: AppColors.text,
            flexibleSpace: FlexibleSpaceBar(
              background: Container(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    colors: [Color(0xFF0F172A), Color(0xFF1E293B)],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
                child: Stack(
                  children: [
                    Positioned(
                      top: -40, right: -40,
                      child: Container(
                        width: 180,
                        height: 180,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          gradient: RadialGradient(colors: [
                            AppColors.primary.withOpacity(0.15),
                            Colors.transparent,
                          ]),
                        ),
                      ),
                    ),
                    Align(
                      alignment: Alignment.center,
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const SizedBox(height: 40),
                          // Avatar
                          Container(
                            width: 80,
                            height: 80,
                            decoration: BoxDecoration(
                              gradient: const LinearGradient(
                                colors: [AppColors.primary, Color(0xFF38BDF8)],
                                begin: Alignment.topLeft,
                                end: Alignment.bottomRight,
                              ),
                              shape: BoxShape.circle,
                              boxShadow: [
                                BoxShadow(
                                  color: AppColors.primary.withOpacity(0.4),
                                  blurRadius: 20,
                                  offset: const Offset(0, 6),
                                ),
                              ],
                            ),
                            child: Center(
                              child: Text(
                                _initials(user?.displayName ?? '?'),
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontSize: 28,
                                  fontWeight: FontWeight.w800,
                                ),
                              ),
                            ),
                          ),
                          const SizedBox(height: 12),
                          Text(
                            user?.displayName ?? '',
                            style: const TextStyle(
                              color: AppColors.text,
                              fontSize: 18,
                              fontWeight: FontWeight.w700,
                            ),
                          ),
                          const SizedBox(height: 4),
                          _roleBadge(user?.role ?? ''),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),

          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // ── Account info ──────────────────────────────────────
                  _sectionTitle(s.profile),
                  _infoCard([
                    _infoRow(Icons.email_outlined, s.email,
                        user?.email ?? ''),
                    _divider(),
                    _infoRow(Icons.badge_outlined, s.role,
                        _roleLabel(user?.role ?? '', s)),
                  ]),

                  const SizedBox(height: 24),

                  // ── Language ──────────────────────────────────────────
                  _sectionTitle(s.language),
                  Container(
                    padding: const EdgeInsets.all(6),
                    decoration: BoxDecoration(
                      color: AppColors.surface,
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(color: AppColors.border),
                    ),
                    child: Row(
                      children: [
                        Expanded(
                            child: _langOption(
                                'fr', '🇫🇷  Français', lang)),
                        const SizedBox(width: 6),
                        Expanded(
                            child: _langOption(
                                'en', '🇬🇧  English', lang)),
                      ],
                    ),
                  ),

                  const SizedBox(height: 24),

                  // ── App info ──────────────────────────────────────────
                  _sectionTitle(s.settings),
                  _infoCard([
                    _infoRow(
                        Icons.info_outline_rounded, s.version, '1.0.0'),
                    _divider(),
                    _infoRow(Icons.cloud_outlined, 'API',
                        AppConstants.apiBaseUrl),
                  ]),

                  const SizedBox(height: 32),

                  // ── Logout ────────────────────────────────────────────
                  SizedBox(
                    width: double.infinity,
                    height: 52,
                    child: OutlinedButton.icon(
                      onPressed: () async {
                        final ok = await showDialog<bool>(
                          context: context,
                          builder: (_) => AlertDialog(
                            backgroundColor: AppColors.surface,
                            title: Text(s.logout,
                                style: const TextStyle(
                                    color: AppColors.text,
                                    fontWeight: FontWeight.w700)),
                            content: Text(s.areYouSure,
                                style: const TextStyle(
                                    color: AppColors.textMuted)),
                            actions: [
                              TextButton(
                                onPressed: () =>
                                    Navigator.pop(context, false),
                                child: Text(s.cancel,
                                    style: const TextStyle(
                                        color: AppColors.textMuted)),
                              ),
                              TextButton(
                                onPressed: () =>
                                    Navigator.pop(context, true),
                                child: Text(s.logout,
                                    style: const TextStyle(
                                        color: AppColors.error,
                                        fontWeight: FontWeight.w700)),
                              ),
                            ],
                          ),
                        );
                        if (ok == true && context.mounted) {
                          await context.read<AuthProvider>().logout();
                        }
                      },
                      icon: const Icon(Icons.logout_rounded,
                          color: AppColors.error, size: 20),
                      label: Text(s.logout,
                          style: const TextStyle(
                              color: AppColors.error,
                              fontWeight: FontWeight.w600,
                              fontSize: 15)),
                      style: OutlinedButton.styleFrom(
                        side: const BorderSide(color: AppColors.error),
                        shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(14)),
                      ),
                    ),
                  ),
                  const SizedBox(height: 40),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  String _initials(String name) {
    final parts = name.trim().split(' ');
    if (parts.length >= 2) {
      return '${parts[0][0]}${parts[1][0]}'.toUpperCase();
    }
    return name.isNotEmpty ? name[0].toUpperCase() : '?';
  }

  Widget _roleBadge(String role) {
    Color c;
    switch (role) {
      case 'superadmin':
        c = const Color(0xFFA855F7);
        break;
      case 'admin':
        c = AppColors.primary;
        break;
      case 'technician':
        c = AppColors.warning;
        break;
      default:
        c = AppColors.success;
    }
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
      decoration: BoxDecoration(
        color: c.withOpacity(0.18),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: c.withOpacity(0.4)),
      ),
      child: Text(role.toUpperCase(),
          style: TextStyle(
              color: c, fontSize: 11, fontWeight: FontWeight.w700)),
    );
  }

  String _roleLabel(String role, AppStrings s) {
    switch (role) {
      case 'admin':
        return s.admin;
      case 'technician':
        return s.technician;
      case 'user':
        return s.user;
      default:
        return role;
    }
  }

  Widget _sectionTitle(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 10),
        child: Text(
          text.toUpperCase(),
          style: const TextStyle(
            color: AppColors.textMuted,
            fontSize: 11,
            fontWeight: FontWeight.w700,
            letterSpacing: 0.8,
          ),
        ),
      );

  Widget _infoCard(List<Widget> children) => Container(
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: AppColors.border),
        ),
        child: Column(children: children),
      );

  Widget _infoRow(IconData icon, String label, String value) => Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        child: Row(
          children: [
            Icon(icon, color: AppColors.primary, size: 20),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(label,
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
                  const SizedBox(height: 2),
                  Text(value,
                      style: const TextStyle(
                          color: AppColors.text,
                          fontWeight: FontWeight.w600,
                          fontSize: 14)),
                ],
              ),
            ),
          ],
        ),
      );

  Widget _divider() => const Divider(
      height: 1, thickness: 1, color: AppColors.border,
      indent: 50, endIndent: 0);

  Widget _langOption(String code, String label, LanguageProvider lang) {
    final sel = lang.language == code;
    return GestureDetector(
      onTap: () => lang.setLanguage(code),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(vertical: 14),
        decoration: BoxDecoration(
          gradient: sel
              ? const LinearGradient(
                  colors: [AppColors.primary, Color(0xFF38BDF8)],
                  begin: Alignment.centerLeft,
                  end: Alignment.centerRight,
                )
              : null,
          color: sel ? null : Colors.transparent,
          borderRadius: BorderRadius.circular(12),
          boxShadow: sel
              ? [
                  BoxShadow(
                    color: AppColors.primary.withOpacity(0.3),
                    blurRadius: 12,
                    offset: const Offset(0, 4),
                  )
                ]
              : null,
        ),
        child: Center(
          child: Text(
            label,
            style: TextStyle(
              color: sel ? Colors.white : AppColors.textMuted,
              fontWeight: FontWeight.w700,
              fontSize: 13,
            ),
          ),
        ),
      ),
    );
  }
}
