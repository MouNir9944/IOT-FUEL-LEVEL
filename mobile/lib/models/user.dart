class User {
  final String id;
  final String email;
  final String role;
  final String? fullName;
  final bool isPrimaryAdmin;
  final bool active;

  const User({
    required this.id,
    required this.email,
    required this.role,
    this.fullName,
    this.isPrimaryAdmin = false,
    this.active = true,
  });

  factory User.fromJson(Map<String, dynamic> j) => User(
        id: j['id']?.toString() ?? '',
        email: j['email'] as String,
        role: j['role'] as String,
        fullName: j['full_name'] as String?,
        isPrimaryAdmin: (j['is_primary_admin'] as bool?) ?? false,
        active: (j['active'] as bool?) ?? true,
      );

  Map<String, dynamic> toJson() => {
        'id': id,
        'email': email,
        'role': role,
        'full_name': fullName,
        'is_primary_admin': isPrimaryAdmin,
        'active': active,
      };

  String get displayName => fullName ?? email.split('@').first;
}
