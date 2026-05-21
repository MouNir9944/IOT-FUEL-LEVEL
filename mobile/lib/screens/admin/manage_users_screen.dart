import 'package:flutter/material.dart';
import '../../core/api_client.dart';
import '../../core/constants.dart';
import '../../core/app_strings.dart';

class _UserItem {
  final String id;
  final String email;
  final String fullName;
  final String role;
  bool active;

  _UserItem({
    required this.id,
    required this.email,
    required this.fullName,
    required this.role,
    required this.active,
  });

  factory _UserItem.fromJson(Map<String, dynamic> j) => _UserItem(
        id: j['id']?.toString() ?? '',
        email: j['email'] as String,
        fullName: j['full_name'] as String? ?? '',
        role: j['role'] as String,
        active: (j['active'] as bool?) ?? true,
      );
}

class ManageUsersScreen extends StatefulWidget {
  const ManageUsersScreen({super.key});

  @override
  State<ManageUsersScreen> createState() => _ManageUsersScreenState();
}

class _ManageUsersScreenState extends State<ManageUsersScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  List<_UserItem> _users = [];
  bool _loading = false;

  // Add form
  final _nameCtrl = TextEditingController();
  final _emailCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  String _newRole = 'user';
  bool _obscure = true;
  bool _submitting = false;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 2, vsync: this);
    _fetch();
  }

  @override
  void dispose() {
    _tabs.dispose();
    _nameCtrl.dispose();
    _emailCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  Future<void> _fetch() async {
    setState(() => _loading = true);
    try {
      final resp = await ApiClient.instance.get('/admin/users');
      setState(() {
        _users = (resp.data['users'] as List)
            .map((u) => _UserItem.fromJson(Map<String, dynamic>.from(u as Map)))
            .toList();
      });
    } catch (_) {
    } finally {
      setState(() => _loading = false);
    }
  }

  Future<void> _toggle(String id) async {
    try {
      final resp =
          await ApiClient.instance.patch('/admin/users/$id/toggle');
      final updated = _UserItem.fromJson(
          Map<String, dynamic>.from(resp.data['user'] as Map));
      setState(() {
        final idx = _users.indexWhere((u) => u.id == id);
        if (idx != -1) _users[idx].active = updated.active;
      });
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    }
  }

  Future<void> _delete(String id) async {
    final s = AppStrings.read(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        backgroundColor: AppColors.surface,
        title: Text(s.deleteUser,
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700)),
        content: Text(s.cannotUndo,
            style: const TextStyle(color: AppColors.textMuted)),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: Text(s.cancel,
                  style: const TextStyle(color: AppColors.textMuted))),
          TextButton(
              onPressed: () => Navigator.pop(context, true),
              child: Text(s.delete,
                  style: const TextStyle(
                      color: AppColors.error, fontWeight: FontWeight.w700))),
        ],
      ),
    );
    if (ok != true) return;
    try {
      await ApiClient.instance.delete('/admin/users/$id');
      setState(() => _users.removeWhere((u) => u.id == id));
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    }
  }

  Future<void> _createUser() async {
    final s = AppStrings.read(context);
    if (_nameCtrl.text.trim().isEmpty ||
        _emailCtrl.text.trim().isEmpty ||
        _passCtrl.text.isEmpty) {
      _snack(s.allFields, isError: true);
      return;
    }
    setState(() => _submitting = true);
    try {
      final resp = await ApiClient.instance.post('/admin/users', data: {
        'full_name': _nameCtrl.text.trim(),
        'email': _emailCtrl.text.trim().toLowerCase(),
        'password': _passCtrl.text,
        'role': _newRole,
      });
      final newUser = _UserItem.fromJson(
          Map<String, dynamic>.from(resp.data['user'] as Map));
      setState(() {
        _users.insert(0, newUser);
        _nameCtrl.clear();
        _emailCtrl.clear();
        _passCtrl.clear();
        _newRole = 'user';
      });
      _snack(s.success);
      _tabs.animateTo(1);
    } catch (e) {
      _snack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _submitting = false);
    }
  }

  void _snack(String msg, {bool isError = false}) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: isError ? AppColors.error : AppColors.success,
      behavior: SnackBarBehavior.floating,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
    ));
  }

  @override
  Widget build(BuildContext context) {
    final s = AppStrings.of(context);
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: Text(s.users,
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700)),
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.text,
        elevation: 0,
        bottom: TabBar(
          controller: _tabs,
          labelColor: Colors.white,
          unselectedLabelColor: AppColors.textMuted,
          indicatorColor: AppColors.primary,
          tabs: [Tab(text: s.newUser), Tab(text: s.users)],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [_addTab(s), _listTab(s)],
      ),
    );
  }

  // ── Add user tab ───────────────────────────────────────────────────────────
  Widget _addTab(AppStrings s) => SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Center(
            child: Container(
              width: 64,
              height: 64,
              decoration: BoxDecoration(
                color: AppColors.primary.withOpacity(0.12),
                borderRadius: BorderRadius.circular(18),
              ),
              child: const Icon(Icons.person_add_outlined,
                  color: AppColors.primary, size: 32),
            ),
          ),
          const SizedBox(height: 24),

          _label(s.fullName),
          _field(_nameCtrl, s.fullName, Icons.person_outline_rounded),
          const SizedBox(height: 14),

          _label(s.email),
          _field(_emailCtrl, s.email, Icons.email_outlined,
              type: TextInputType.emailAddress),
          const SizedBox(height: 14),

          _label(s.password),
          TextField(
            controller: _passCtrl,
            obscureText: _obscure,
            autocorrect: false,
            style: const TextStyle(color: AppColors.text, fontSize: 15),
            decoration: InputDecoration(
              hintText: s.password,
              hintStyle:
                  const TextStyle(color: AppColors.textMuted, fontSize: 14),
              prefixIcon: const Icon(Icons.lock_outline_rounded,
                  color: AppColors.textMuted, size: 20),
              suffixIcon: IconButton(
                icon: Icon(
                  _obscure
                      ? Icons.visibility_off_outlined
                      : Icons.visibility_outlined,
                  color: AppColors.textMuted,
                  size: 20,
                ),
                onPressed: () => setState(() => _obscure = !_obscure),
              ),
              filled: true,
              fillColor: AppColors.surface,
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
          ),
          const SizedBox(height: 20),

          _label(s.role),
          Row(children: [
            Expanded(
                child: _roleChip('user', s.user,
                    Icons.person_outline_rounded, AppColors.primary)),
            const SizedBox(width: 10),
            Expanded(
                child: _roleChip('technician', s.technician,
                    Icons.build_outlined, AppColors.warning)),
          ]),
          const SizedBox(height: 28),

          _gradientBtn(s.newUser, _submitting, _createUser),
        ]),
      );

  Widget _roleChip(
      String role, String label, IconData icon, Color color) {
    final sel = _newRole == role;
    return GestureDetector(
      onTap: () => setState(() => _newRole = role),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        padding: const EdgeInsets.symmetric(vertical: 14),
        decoration: BoxDecoration(
          color: sel ? color.withOpacity(0.15) : AppColors.surface,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
              color: sel ? color : AppColors.border,
              width: sel ? 1.5 : 1),
        ),
        child: Column(children: [
          Icon(icon, color: sel ? color : AppColors.textMuted, size: 22),
          const SizedBox(height: 6),
          Text(label,
              style: TextStyle(
                  color: sel ? AppColors.text : AppColors.textMuted,
                  fontWeight:
                      sel ? FontWeight.w700 : FontWeight.normal,
                  fontSize: 13)),
        ]),
      ),
    );
  }

  // ── Users list tab ─────────────────────────────────────────────────────────
  Widget _listTab(AppStrings s) => _loading
      ? const Center(
          child: CircularProgressIndicator(color: AppColors.primary))
      : RefreshIndicator(
          color: AppColors.primary,
          backgroundColor: AppColors.surface,
          onRefresh: _fetch,
          child: _users.isEmpty
              ? ListView(padding: const EdgeInsets.all(32), children: [
                  const SizedBox(height: 80),
                  Center(
                    child: Column(children: [
                      Icon(Icons.people_outline_rounded,
                          color: AppColors.textMuted.withOpacity(0.3),
                          size: 60),
                      const SizedBox(height: 12),
                      Text(s.noUsers,
                          style:
                              const TextStyle(color: AppColors.textMuted)),
                    ]),
                  ),
                ])
              : ListView.builder(
                  padding:
                      const EdgeInsets.fromLTRB(16, 16, 16, 80),
                  itemCount: _users.length,
                  itemBuilder: (_, i) {
                    final u = _users[i];
                    final roleColor = u.role == 'technician'
                        ? AppColors.warning
                        : AppColors.primary;
                    return Container(
                      margin: const EdgeInsets.only(bottom: 10),
                      decoration: BoxDecoration(
                        color: AppColors.surface,
                        borderRadius: BorderRadius.circular(14),
                        border: Border.all(color: AppColors.border),
                      ),
                      child: ListTile(
                        contentPadding: const EdgeInsets.symmetric(
                            horizontal: 16, vertical: 10),
                        leading: Container(
                          width: 44,
                          height: 44,
                          decoration: BoxDecoration(
                            color: roleColor.withOpacity(0.12),
                            shape: BoxShape.circle,
                          ),
                          child: Center(
                            child: Text(
                              _initials(u.fullName.isEmpty ? u.email : u.fullName),
                              style: TextStyle(
                                  color: roleColor,
                                  fontWeight: FontWeight.w800,
                                  fontSize: 14),
                            ),
                          ),
                        ),
                        title: Text(
                            u.fullName.isEmpty ? u.email : u.fullName,
                            style: const TextStyle(
                                color: AppColors.text,
                                fontWeight: FontWeight.w700,
                                fontSize: 14)),
                        subtitle: Text(
                            '${u.email}  •  ${u.role}',
                            style: const TextStyle(
                                color: AppColors.textMuted,
                                fontSize: 11)),
                        trailing: Row(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              // Active toggle
                              GestureDetector(
                                onTap: () => _toggle(u.id),
                                child: Container(
                                  padding: const EdgeInsets.symmetric(
                                      horizontal: 8, vertical: 4),
                                  decoration: BoxDecoration(
                                    color: u.active
                                        ? AppColors.success.withOpacity(0.12)
                                        : AppColors.error.withOpacity(0.12),
                                    borderRadius: BorderRadius.circular(8),
                                    border: Border.all(
                                        color: u.active
                                            ? AppColors.success
                                                .withOpacity(0.3)
                                            : AppColors.error
                                                .withOpacity(0.3)),
                                  ),
                                  child: Text(
                                    u.active ? s.active : s.suspended,
                                    style: TextStyle(
                                        color: u.active
                                            ? AppColors.success
                                            : AppColors.error,
                                        fontSize: 11,
                                        fontWeight: FontWeight.w700),
                                  ),
                                ),
                              ),
                              const SizedBox(width: 8),
                              IconButton(
                                onPressed: () => _delete(u.id),
                                icon: const Icon(
                                    Icons.delete_outline_rounded,
                                    color: AppColors.error,
                                    size: 20),
                                padding: EdgeInsets.zero,
                                constraints: const BoxConstraints(),
                              ),
                            ]),
                      ),
                    );
                  },
                ),
        );

  Widget _label(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: Text(text,
            style: const TextStyle(
                color: AppColors.textMuted,
                fontSize: 12,
                fontWeight: FontWeight.w600,
                letterSpacing: 0.5)),
      );

  Widget _field(TextEditingController ctrl, String hint, IconData icon,
          {TextInputType? type}) =>
      TextField(
        controller: ctrl,
        keyboardType: type,
        autocorrect: false,
        style: const TextStyle(color: AppColors.text, fontSize: 15),
        decoration: InputDecoration(
          hintText: hint,
          hintStyle:
              const TextStyle(color: AppColors.textMuted, fontSize: 14),
          prefixIcon: Icon(icon, color: AppColors.textMuted, size: 20),
          filled: true,
          fillColor: AppColors.surface,
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

  Widget _gradientBtn(String label, bool loading, VoidCallback onTap) =>
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
                    end: Alignment.centerRight),
            color: loading ? AppColors.surfaceLight : null,
            borderRadius: BorderRadius.circular(14),
            boxShadow: loading
                ? null
                : [
                    BoxShadow(
                        color: AppColors.primary.withOpacity(0.35),
                        blurRadius: 16,
                        offset: const Offset(0, 6))
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

  String _initials(String name) {
    final p = name.trim().split(' ');
    if (p.length >= 2) return '${p[0][0]}${p[1][0]}'.toUpperCase();
    return name.isNotEmpty ? name[0].toUpperCase() : '?';
  }
}
