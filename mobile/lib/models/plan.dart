class PlanSlice {
  final String startTime;
  final String stopTime;
  final double? targetLiters;   // null = no target (time-only slice)

  const PlanSlice({
    required this.startTime,
    required this.stopTime,
    this.targetLiters,
  });

  factory PlanSlice.fromJson(Map<String, dynamic> j) => PlanSlice(
        startTime:    j['start_time'] as String,
        stopTime:     j['stop_time']  as String,
        targetLiters: (j['target_liters'] as num?)?.toDouble(),
      );

  Map<String, dynamic> toJson() => {
        'start_time': startTime,
        'stop_time':  stopTime,
        if (targetLiters != null) 'target_liters': targetLiters,
      };
}

class PlanRule {
  final String dayOfWeek;
  final List<PlanSlice> slices;

  const PlanRule({required this.dayOfWeek, required this.slices});

  factory PlanRule.fromJson(Map<String, dynamic> j) => PlanRule(
        dayOfWeek: j['day_of_week'] as String,
        slices: (j['slices'] as List)
            .map((s) => PlanSlice.fromJson(Map<String, dynamic>.from(s as Map)))
            .toList(),
      );

  Map<String, dynamic> toJson() => {
        'day_of_week': dayOfWeek,
        'slices': slices.map((s) => s.toJson()).toList(),
      };
}

class Plan {
  final String id;
  final String deviceId;
  final String name;
  final String dateStart;
  final String? dateStop;
  final String timezone;
  final List<PlanRule> rules;
  bool enabled;

  Plan({
    required this.id,
    required this.deviceId,
    required this.name,
    required this.dateStart,
    this.dateStop,
    this.timezone = 'Africa/Tunis',
    required this.rules,
    this.enabled = true,
  });

  factory Plan.fromJson(Map<String, dynamic> j) => Plan(
        id:        (j['id'] ?? j['_id'])?.toString() ?? '',
        deviceId:  j['device_id']?.toString() ?? '',
        name:      j['name'] as String,
        dateStart: j['date_start'] as String,
        dateStop:  j['date_stop'] as String?,
        timezone:  j['timezone'] as String? ?? 'Africa/Tunis',
        rules:     j['rules'] != null
            ? (j['rules'] as List)
                .map((r) => PlanRule.fromJson(Map<String, dynamic>.from(r as Map)))
                .toList()
            : [],
        enabled: (j['enabled'] as bool?) ?? true,
      );
}
