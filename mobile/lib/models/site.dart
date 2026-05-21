class Site {
  final String id;
  final String name;
  final String? address;
  final String adminId;
  final int deviceCount;

  const Site({
    required this.id,
    required this.name,
    this.address,
    required this.adminId,
    this.deviceCount = 0,
  });

  factory Site.fromJson(Map<String, dynamic> j) => Site(
        id: j['id']?.toString() ?? '',
        name: j['name'] as String,
        address: j['address'] as String?,
        adminId: j['admin_id']?.toString() ?? '',
        deviceCount: int.tryParse(j['device_count']?.toString() ?? '0') ?? 0,
      );
}
