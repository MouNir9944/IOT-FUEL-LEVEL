import 'package:flutter/material.dart';
import '../core/api_client.dart';
import '../core/constants.dart';
import '../models/device.dart';
import '../models/plan.dart';

class PlanningScreen extends StatefulWidget {
  final Device device;
  final bool isAdmin;

  const PlanningScreen({super.key, required this.device, this.isAdmin = false});

  @override
  State<PlanningScreen> createState() => _PlanningScreenState();
}

class _PlanningScreenState extends State<PlanningScreen> {
  List<Plan> _plans = [];
  bool _loading = false;
  bool _showCreate = false;
  Plan? _editingPlan;

  // Create / edit form state
  final _nameCtrl = TextEditingController();
  DateTime _startDate = DateTime.now();
  DateTime? _endDate;
  final Set<String> _activeDays = {};
  bool _sameForAll = true;
  final List<_SliceForm> _sharedSlices = [];
  final Map<String, List<_SliceForm>> _slices = {};

  static const _days = [
    'monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday',
  ];
  static const _dayAbbr = ['Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa', 'Su'];

  @override
  void initState() {
    super.initState();
    _fetchPlans();
  }

  @override
  void dispose() {
    _nameCtrl.dispose();
    super.dispose();
  }

  // ── API helpers ──────────────────────────────────────────────────────────────

  Future<void> _fetchPlans() async {
    setState(() => _loading = true);
    try {
      final resp = await ApiClient.instance
          .get('/devices/${widget.device.id}/plans');
      final list = resp.data['plans'] as List;
      setState(() {
        _plans = list
            .map((p) => Plan.fromJson(Map<String, dynamic>.from(p as Map)))
            .toList();
      });
    } catch (e) {
      _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _savePlan() async {
    if (_nameCtrl.text.trim().isEmpty) {
      _showSnack('Plan name is required', isError: true);
      return;
    }
    if (_activeDays.isEmpty) {
      _showSnack('Select at least one day', isError: true);
      return;
    }

    // ── End date must be after start date ────────────────────────────────
    if (_endDate != null && !_endDate!.isAfter(_startDate)) {
      _showSnack('End date must be after start date', isError: true);
      return;
    }

    // ── Build rules ───────────────────────────────────────────────────────
    final rules = <Map<String, dynamic>>[];
    for (final day in _days) {
      if (!_activeDays.contains(day)) continue;
      final daySlices = _sameForAll ? _sharedSlices : (_slices[day] ?? []);
      if (daySlices.isEmpty) continue;
      rules.add({
        'day_of_week': day,
        'slices': daySlices.map((s) => s.toJson()).toList(),
      });
    }

    if (rules.isEmpty) {
      _showSnack('Add at least one time slot', isError: true);
      return;
    }

    // ── Slice stop must be after start ────────────────────────────────────
    for (final rule in rules) {
      for (final s in (rule['slices'] as List)) {
        final slice = s as Map<String, dynamic>;
        if ((slice['stop_time'] as String).compareTo(slice['start_time'] as String) <= 0) {
          _showSnack(
            'Stop time must be after start time '
            '(${slice['start_time']} → ${slice['stop_time']})',
            isError: true,
          );
          return;
        }
      }
    }

    // ── Intra-plan slice overlap check ────────────────────────────────────
    for (final day in _days) {
      if (!_activeDays.contains(day)) continue;
      final daySlices = _sameForAll ? _sharedSlices : (_slices[day] ?? []);
      for (int i = 0; i < daySlices.length; i++) {
        for (int j = i + 1; j < daySlices.length; j++) {
          final a = daySlices[i];
          final b = daySlices[j];
          if (a.startTime.compareTo(b.stopTime) < 0 && b.startTime.compareTo(a.stopTime) < 0) {
            _showSnack(
              '${day[0].toUpperCase()}${day.substring(1)}: slots '
              '${a.startTime}–${a.stopTime} and ${b.startTime}–${b.stopTime} overlap',
              isError: true,
            );
            return;
          }
        }
      }
    }

    // ── Inter-plan conflict check ─────────────────────────────────────────
    final newStart = _startDate.toIso8601String().split('T').first;
    final newStop  = _endDate?.toIso8601String().split('T').first;
    for (final other in _plans) {
      if (_editingPlan != null && other.id == _editingPlan!.id) continue;
      // Check date range overlap
      final otherStop = other.dateStop ?? '9999-12-31';
      final selfStop  = newStop ?? '9999-12-31';
      if (newStart.compareTo(otherStop) > 0 || other.dateStart.compareTo(selfStop) > 0) continue;
      // Check day + time overlap
      for (final day in _days) {
        if (!_activeDays.contains(day)) continue;
        final mySlices = _slices[day] ?? [];
        if (mySlices.isEmpty) continue;
        final otherRule = other.rules.where((r) => r.dayOfWeek == day).toList();
        if (otherRule.isEmpty) continue;
        for (final ms in mySlices) {
          for (final os in otherRule.first.slices) {
            if (ms.startTime.compareTo(os.stopTime) < 0 && os.startTime.compareTo(ms.stopTime) < 0) {
              _showSnack(
                'Conflicts with "${other.name}" on '
                '${day[0].toUpperCase()}${day.substring(1)} '
                'at ${ms.startTime}–${ms.stopTime}',
                isError: true,
              );
              return;
            }
          }
        }
      }
    }

    final body = {
      'name':       _nameCtrl.text.trim(),
      'date_start': _startDate.toIso8601String().split('T').first,
      if (_endDate != null) 'date_stop': _endDate!.toIso8601String().split('T').first,
      'timezone':   'Africa/Tunis',
      'rules':      rules,
    };

    setState(() => _loading = true);
    try {
      if (_editingPlan != null) {
        await ApiClient.instance.put(
          '/plans/${_editingPlan!.id}',
          data: body,
        );
      } else {
        await ApiClient.instance.post(
          '/devices/${widget.device.id}/plans',
          data: body,
        );
      }
      _resetForm();
      await _fetchPlans();
      _showSnack(_editingPlan != null ? 'Plan updated' : 'Plan created');
    } catch (e) {
      if (mounted) _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _deletePlan(Plan plan) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppColors.surface,
        title: const Text('Delete plan?', style: TextStyle(color: AppColors.text)),
        content: Text(
          'Delete "${plan.name}"? This cannot be undone.',
          style: const TextStyle(color: AppColors.textMuted),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel', style: TextStyle(color: AppColors.textMuted)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: AppColors.error),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    try {
      await ApiClient.instance.delete(
        '/plans/${plan.id}',
      );
      await _fetchPlans();
      _showSnack('Plan deleted');
    } catch (e) {
      if (mounted) _showSnack(ApiClient.errorMessage(e), isError: true);
    }
  }

  Future<void> _togglePlan(Plan plan) async {
    try {
      await ApiClient.instance.patch(
        '/plans/${plan.id}/toggle',
      );
      await _fetchPlans();
    } catch (e) {
      if (mounted) _showSnack(ApiClient.errorMessage(e), isError: true);
    }
  }

  Future<void> _syncPlans() async {
    setState(() => _loading = true);
    try {
      await ApiClient.instance.post(
        '/devices/${widget.device.id}/plans/sync',
      );
      _showSnack('Plans synced to device');
    } catch (e) {
      if (mounted) _showSnack(ApiClient.errorMessage(e), isError: true);
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  // ── Form helpers ─────────────────────────────────────────────────────────────

  void _resetForm() {
    _nameCtrl.clear();
    _startDate    = DateTime.now();
    _endDate      = null;
    _activeDays.clear();
    _sharedSlices.clear();
    _slices.clear();
    _sameForAll  = true;
    _editingPlan = null;
    if (mounted) setState(() => _showCreate = false);
  }

  void _editPlan(Plan plan) {
    _nameCtrl.text = plan.name;
    _startDate     = DateTime.tryParse(plan.dateStart) ?? DateTime.now();
    _endDate       = plan.dateStop != null ? DateTime.tryParse(plan.dateStop!) : null;
    _activeDays
      ..clear()
      ..addAll(plan.rules.map((r) => r.dayOfWeek));
    _sharedSlices.clear();
    _slices.clear();

    // Detect whether all days share identical slices
    bool allSame = plan.rules.length > 1 && plan.rules.every((r) {
      if (r.slices.length != plan.rules.first.slices.length) return false;
      for (int i = 0; i < r.slices.length; i++) {
        if (r.slices[i].startTime != plan.rules.first.slices[i].startTime ||
            r.slices[i].stopTime  != plan.rules.first.slices[i].stopTime)
          return false;
      }
      return true;
    });
    _sameForAll = plan.rules.length == 1 ? false : allSame;

    if (_sameForAll) {
      _sharedSlices.addAll(plan.rules.first.slices.map((s) => _SliceForm(
        s.startTime, s.stopTime,
        targetEnabled: s.targetLiters != null,
        targetLiters:  s.targetLiters?.toString() ?? '',
      )));
    } else {
      for (final rule in plan.rules) {
        _slices[rule.dayOfWeek] = rule.slices.map((s) => _SliceForm(
          s.startTime, s.stopTime,
          targetEnabled: s.targetLiters != null,
          targetLiters:  s.targetLiters?.toString() ?? '',
        )).toList();
      }
    }

    _editingPlan = plan;
    setState(() => _showCreate = true);
  }

  void _showSnack(String msg, {bool isError = false}) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      content: Text(msg),
      backgroundColor: isError ? AppColors.error : AppColors.success,
    ));
  }

  // ── Build ─────────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        backgroundColor: AppColors.surface,
        title: Text(
          'Watering Schedule',
          style: const TextStyle(color: AppColors.text, fontWeight: FontWeight.w700),
        ),
        actions: [
          if (_plans.isNotEmpty)
            IconButton(
              icon: const Icon(Icons.sync_rounded, color: AppColors.textMuted),
              tooltip: 'Sync to device',
              onPressed: _syncPlans,
            ),
          IconButton(
            icon: const Icon(Icons.refresh_rounded, color: AppColors.textMuted),
            onPressed: _fetchPlans,
          ),
        ],
      ),
      body: _loading && _plans.isEmpty
          ? const Center(child: CircularProgressIndicator(color: AppColors.primary))
          : Column(
              children: [
                Expanded(
                  child: _plans.isEmpty && !_showCreate
                      ? _emptyState()
                      : ListView(
                          padding: const EdgeInsets.all(16),
                          children: [
                            ..._plans.map((p) => _PlanCard(
                                  plan: p,
                                  onEdit: () => _editPlan(p),
                                  onDelete: () => _deletePlan(p),
                                  onToggle: () => _togglePlan(p),
                                )),
                            if (_showCreate) _buildForm(),
                          ],
                        ),
                ),
                if (!_showCreate)
                  _AddPlanButton(onTap: () => setState(() => _showCreate = true)),
              ],
            ),
    );
  }

  Widget _emptyState() => Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: const [
            Icon(Icons.water_drop_outlined, size: 48, color: AppColors.textMuted),
            SizedBox(height: 12),
            Text(
              'No watering schedules yet',
              style: TextStyle(color: AppColors.textMuted, fontSize: 15),
            ),
            SizedBox(height: 4),
            Text(
              'Tap + to create one',
              style: TextStyle(color: AppColors.textMuted, fontSize: 12),
            ),
          ],
        ),
      );

  Widget _buildForm() {
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: AppColors.surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppColors.primary.withOpacity(0.4)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            _editingPlan != null ? 'Edit Plan' : 'New Watering Plan',
            style: const TextStyle(
                color: AppColors.text, fontWeight: FontWeight.w700, fontSize: 15),
          ),
          const SizedBox(height: 14),

          // Name
          TextField(
            controller: _nameCtrl,
            style: const TextStyle(color: AppColors.text),
            decoration: const InputDecoration(
              labelText: 'Plan name',
              labelStyle: TextStyle(color: AppColors.textMuted),
              enabledBorder: OutlineInputBorder(
                borderSide: BorderSide(color: AppColors.border),
              ),
              focusedBorder: OutlineInputBorder(
                borderSide: BorderSide(color: AppColors.primary),
              ),
            ),
          ),
          const SizedBox(height: 12),

          // Date range
          Row(
            children: [
              Expanded(
                child: _DatePickerField(
                  label: 'Start date',
                  date: _startDate,
                  firstDate: DateTime(2020),
                  onPick: (d) => setState(() {
                    _startDate = d;
                    // reset end date if it's now before start
                    if (_endDate != null && !_endDate!.isAfter(d)) _endDate = null;
                  }),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _DatePickerField(
                  label: 'End date (opt.)',
                  date: _endDate,
                  firstDate: _startDate.add(const Duration(days: 1)),
                  onPick: (d) => setState(() => _endDate = d),
                  onClear: () => setState(() => _endDate = null),
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),

          // Day selector
          const Text(
            'Active days',
            style: TextStyle(color: AppColors.textMuted, fontSize: 12),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 6,
            children: List.generate(_days.length, (i) {
              final day = _days[i];
              final sel = _activeDays.contains(day);
              return FilterChip(
                label: Text(_dayAbbr[i]),
                selected: sel,
                onSelected: (v) {
                  setState(() {
                    if (v) {
                      _activeDays.add(day);
                      if (!_slices.containsKey(day)) _slices[day] = [];
                    } else {
                      _activeDays.remove(day);
                    }
                  });
                },
                selectedColor: AppColors.primary.withOpacity(0.2),
                checkmarkColor: AppColors.primary,
                labelStyle: TextStyle(
                  color: sel ? AppColors.primary : AppColors.textMuted,
                  fontSize: 12,
                ),
                backgroundColor: AppColors.surfaceLight,
                side: BorderSide(
                    color: sel
                        ? AppColors.primary.withOpacity(0.5)
                        : AppColors.border),
              );
            }),
          ),
          const SizedBox(height: 14),

          // ── Slot schedule mode selector ───────────────────────────────────
          if (_activeDays.isNotEmpty) ...[
            Row(
              children: [
                Expanded(
                  child: _ModeButton(
                    label: 'Same for all days',
                    icon: Icons.copy_all_rounded,
                    selected: _sameForAll,
                    onTap: () => setState(() => _sameForAll = true),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: _ModeButton(
                    label: 'Per day',
                    icon: Icons.view_day_rounded,
                    selected: !_sameForAll,
                    onTap: () => setState(() => _sameForAll = false),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),

            // ── Shared editor ───────────────────────────────────────────────
            if (_sameForAll)
              _SliceListEditor(
                slices: _sharedSlices,
                label: 'Add slot',
                onChange: () => setState(() {}),
              )
            // ── Per-day editors ─────────────────────────────────────────────
            else
              for (final day in _days)
                if (_activeDays.contains(day)) ...[
                  Padding(
                    padding: const EdgeInsets.only(top: 10, bottom: 4),
                    child: Row(
                      children: [
                        Container(
                          width: 6, height: 6,
                          decoration: const BoxDecoration(
                            color: AppColors.primary,
                            shape: BoxShape.circle,
                          ),
                        ),
                        const SizedBox(width: 6),
                        Text(
                          day[0].toUpperCase() + day.substring(1),
                          style: const TextStyle(
                            color: AppColors.primary,
                            fontSize: 13,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ],
                    ),
                  ),
                  _SliceListEditor(
                    slices: _slices.putIfAbsent(day, () => []),
                    label: 'Add slot',
                    onChange: () => setState(() {}),
                  ),
                ],
          ] else
            const Padding(
              padding: EdgeInsets.symmetric(vertical: 8),
              child: Text(
                'Select at least one day above to add time slots.',
                style: TextStyle(color: AppColors.textMuted, fontSize: 12),
              ),
            ),

          const SizedBox(height: 16),

          // Save / Cancel
          Row(
            children: [
              Expanded(
                child: OutlinedButton(
                  onPressed: _resetForm,
                  style: OutlinedButton.styleFrom(
                    foregroundColor: AppColors.textMuted,
                    side: const BorderSide(color: AppColors.border),
                  ),
                  child: const Text('Cancel'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: ElevatedButton(
                  onPressed: _loading ? null : _savePlan,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: AppColors.primary,
                    foregroundColor: Colors.white,
                  ),
                  child: _loading
                      ? const SizedBox(
                          width: 16, height: 16,
                          child: CircularProgressIndicator(
                              strokeWidth: 2, color: Colors.white))
                      : Text(_editingPlan != null ? 'Update' : 'Create'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

// ── Plan card ────────────────────────────────────────────────────────────────

class _PlanCard extends StatelessWidget {
  final Plan plan;
  final VoidCallback onEdit;
  final VoidCallback onDelete;
  final VoidCallback onToggle;

  const _PlanCard({
    required this.plan,
    required this.onEdit,
    required this.onDelete,
    required this.onToggle,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: AppColors.surface,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: plan.enabled
              ? AppColors.primary.withOpacity(0.3)
              : AppColors.border,
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  plan.name,
                  style: TextStyle(
                    color: plan.enabled ? AppColors.text : AppColors.textMuted,
                    fontWeight: FontWeight.w700,
                    fontSize: 14,
                  ),
                ),
              ),
              Switch(
                value: plan.enabled,
                onChanged: (_) => onToggle(),
                activeColor: AppColors.primary,
                materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
              ),
              IconButton(
                icon: const Icon(Icons.edit_rounded, size: 18, color: AppColors.textMuted),
                onPressed: onEdit,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints(),
              ),
              const SizedBox(width: 8),
              IconButton(
                icon: const Icon(Icons.delete_outline_rounded, size: 18, color: AppColors.error),
                onPressed: onDelete,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints(),
              ),
            ],
          ),
          const SizedBox(height: 6),
          Text(
            '${plan.dateStart}${plan.dateStop != null ? " → ${plan.dateStop}" : " (no end)"}',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 11),
          ),
          if (plan.rules.isNotEmpty) ...[
            const SizedBox(height: 8),
            Wrap(
              spacing: 6,
              runSpacing: 4,
              children: plan.rules.expand((r) => r.slices.map(
                (s) => Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: AppColors.primary.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: AppColors.primary.withOpacity(0.3)),
                  ),
                  child: Text(
                    '${r.dayOfWeek.substring(0, 2).toUpperCase()}  ${s.startTime}–${s.stopTime}',
                    style: const TextStyle(
                        color: AppColors.primary,
                        fontSize: 10,
                        fontWeight: FontWeight.w600),
                  ),
                ),
              )).toList(),
            ),
          ],
        ],
      ),
    );
  }
}

// ── Slice list editor ────────────────────────────────────────────────────────

class _SliceListEditor extends StatelessWidget {
  final List<_SliceForm> slices;
  final String label;
  final VoidCallback onChange;

  const _SliceListEditor({
    required this.slices,
    required this.label,
    required this.onChange,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        ...slices.asMap().entries.map((e) => _SliceRow(
              slice: e.value,
              index: e.key,
              onRemove: () {
                slices.removeAt(e.key);
                onChange();
              },
              onChange: onChange,
            )),
        TextButton.icon(
          onPressed: () {
            slices.add(_SliceForm('06:00', '07:00'));
            onChange();
          },
          icon: const Icon(Icons.add_rounded, size: 16, color: AppColors.primary),
          label: Text(label, style: const TextStyle(color: AppColors.primary, fontSize: 13)),
        ),
      ],
    );
  }
}

// ── Slice row ─────────────────────────────────────────────────────────────────

class _SliceRow extends StatefulWidget {
  final _SliceForm slice;
  final int index;
  final VoidCallback onRemove;
  final VoidCallback onChange;

  const _SliceRow({
    required this.slice,
    required this.index,
    required this.onRemove,
    required this.onChange,
  });

  @override
  State<_SliceRow> createState() => _SliceRowState();
}

class _SliceRowState extends State<_SliceRow> {
  late final TextEditingController _targetCtrl;

  @override
  void initState() {
    super.initState();
    _targetCtrl = TextEditingController(text: widget.slice.targetLiters);
    _targetCtrl.addListener(() {
      widget.slice.targetLiters = _targetCtrl.text;
      widget.onChange();
    });
  }

  @override
  void dispose() {
    _targetCtrl.dispose();
    super.dispose();
  }

  Future<void> _pickTime({required bool isStart}) async {
    final initial = _parseTimeOfDay(
        isStart ? widget.slice.startTime : widget.slice.stopTime);
    final picked = await showTimePicker(
      context: context,
      initialTime: initial,
      builder: (ctx, child) => Theme(
        data: ThemeData.dark().copyWith(
          colorScheme: const ColorScheme.dark(primary: AppColors.primary),
        ),
        child: child!,
      ),
    );
    if (picked == null) return;
    final str = '${picked.hour.toString().padLeft(2, '0')}:'
        '${picked.minute.toString().padLeft(2, '0')}';

    // Validate stop > start
    if (!isStart && str.compareTo(widget.slice.startTime) <= 0) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
          content: Text('Stop time must be after start time'),
          backgroundColor: Colors.red,
        ));
      }
      return;
    }
    if (isStart && widget.slice.stopTime.isNotEmpty &&
        widget.slice.stopTime.compareTo(str) <= 0) {
      // Auto-advance stop time by 1 hour when start is moved past it
      final newStop = TimeOfDay(hour: (picked.hour + 1) % 24, minute: picked.minute);
      widget.slice.stopTime = '${newStop.hour.toString().padLeft(2, '0')}:'
          '${newStop.minute.toString().padLeft(2, '0')}';
    }

    setState(() {
      if (isStart) {
        widget.slice.startTime = str;
      } else {
        widget.slice.stopTime = str;
      }
    });
    widget.onChange();
  }

  static TimeOfDay _parseTimeOfDay(String s) {
    final parts = s.split(':');
    return TimeOfDay(
      hour:   int.tryParse(parts[0]) ?? 6,
      minute: int.tryParse(parts.length > 1 ? parts[1] : '0') ?? 0,
    );
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── Time pickers row ──────────────────────────────────────────────
          Row(
            children: [
              Expanded(
                child: GestureDetector(
                  onTap: () => _pickTime(isStart: true),
                  child: _TimeChip(label: 'Start', time: widget.slice.startTime),
                ),
              ),
              const Padding(
                padding: EdgeInsets.symmetric(horizontal: 6),
                child: Text('→', style: TextStyle(color: AppColors.textMuted)),
              ),
              Expanded(
                child: GestureDetector(
                  onTap: () => _pickTime(isStart: false),
                  child: _TimeChip(label: 'Stop', time: widget.slice.stopTime),
                ),
              ),
              IconButton(
                icon: const Icon(Icons.close_rounded, size: 16, color: AppColors.error),
                onPressed: widget.onRemove,
                padding: const EdgeInsets.only(left: 4),
                constraints: const BoxConstraints(),
              ),
            ],
          ),

          // ── Consumption target toggle + input ─────────────────────────────
          Row(
            children: [
              const Icon(Icons.water_drop_outlined, size: 13, color: AppColors.textMuted),
              const SizedBox(width: 4),
              const Text(
                'Water target',
                style: TextStyle(color: AppColors.textMuted, fontSize: 12),
              ),
              const Spacer(),
              Transform.scale(
                scale: 0.75,
                child: Switch(
                  value: widget.slice.targetEnabled,
                  onChanged: (v) {
                    setState(() => widget.slice.targetEnabled = v);
                    widget.onChange();
                  },
                  activeColor: AppColors.primary,
                ),
              ),
            ],
          ),

          if (widget.slice.targetEnabled)
            Padding(
              padding: const EdgeInsets.only(bottom: 4),
              child: TextField(
                controller: _targetCtrl,
                keyboardType: const TextInputType.numberWithOptions(decimal: true),
                style: const TextStyle(color: AppColors.text, fontSize: 13),
                decoration: InputDecoration(
                  isDense: true,
                  contentPadding:
                      const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                  hintText: 'e.g. 5.0',
                  hintStyle: const TextStyle(color: AppColors.textMuted, fontSize: 12),
                  suffixText: 'L',
                  suffixStyle: const TextStyle(color: AppColors.primary, fontSize: 13),
                  enabledBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppColors.border),
                  ),
                  focusedBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppColors.primary),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _TimeChip extends StatelessWidget {
  final String label;
  final String time;

  const _TimeChip({required this.label, required this.time});

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        decoration: BoxDecoration(
          color: AppColors.surfaceLight,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: AppColors.border),
        ),
        child: Row(
          children: [
            const Icon(Icons.access_time_rounded, size: 14, color: AppColors.textMuted),
            const SizedBox(width: 6),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label,
                    style: const TextStyle(color: AppColors.textMuted, fontSize: 9)),
                Text(time,
                    style: const TextStyle(
                        color: AppColors.text, fontSize: 13, fontWeight: FontWeight.w700)),
              ],
            ),
          ],
        ),
      );
}

// ── Date picker field ─────────────────────────────────────────────────────────

class _DatePickerField extends StatelessWidget {
  final String label;
  final DateTime? date;
  final DateTime? firstDate;   // null = DateTime(2020)
  final void Function(DateTime) onPick;
  final VoidCallback? onClear;

  const _DatePickerField({
    required this.label,
    required this.date,
    required this.onPick,
    this.firstDate,
    this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    final earliest = firstDate ?? DateTime(2020);
    return GestureDetector(
      onTap: () async {
        final picked = await showDatePicker(
          context: context,
          initialDate: date != null && !date!.isBefore(earliest) ? date! : earliest,
          firstDate: earliest,
          lastDate: DateTime(2035),
          builder: (ctx, child) => Theme(
            data: ThemeData.dark().copyWith(
              colorScheme: const ColorScheme.dark(primary: AppColors.primary),
            ),
            child: child!,
          ),
        );
        if (picked != null) onPick(picked);
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
        decoration: BoxDecoration(
          color: AppColors.surfaceLight,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: AppColors.border),
        ),
        child: Row(
          children: [
            const Icon(Icons.calendar_today_rounded, size: 14, color: AppColors.textMuted),
            const SizedBox(width: 6),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(label,
                      style: const TextStyle(color: AppColors.textMuted, fontSize: 9)),
                  Text(
                    date != null
                        ? '${date!.year}-${date!.month.toString().padLeft(2, '0')}-${date!.day.toString().padLeft(2, '0')}'
                        : 'Not set',
                    style: TextStyle(
                      color: date != null ? AppColors.text : AppColors.textMuted,
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ),
            if (onClear != null && date != null)
              GestureDetector(
                onTap: onClear,
                child: const Icon(Icons.close_rounded, size: 14, color: AppColors.textMuted),
              ),
          ],
        ),
      ),
    );
  }
}

// ── Add plan button ───────────────────────────────────────────────────────────

class _AddPlanButton extends StatelessWidget {
  final VoidCallback onTap;
  const _AddPlanButton({required this.onTap});

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.all(16),
        child: SizedBox(
          width: double.infinity,
          child: ElevatedButton.icon(
            onPressed: onTap,
            icon: const Icon(Icons.add_rounded),
            label: const Text('Add Watering Plan'),
            style: ElevatedButton.styleFrom(
              backgroundColor: AppColors.primary,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 14),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
              ),
            ),
          ),
        ),
      );
}

// ── Mode button (Same for all / Per day) ─────────────────────────────────────

class _ModeButton extends StatelessWidget {
  final String label;
  final IconData icon;
  final bool selected;
  final VoidCallback onTap;

  const _ModeButton({
    required this.label,
    required this.icon,
    required this.selected,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) => GestureDetector(
        onTap: onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 180),
          padding: const EdgeInsets.symmetric(vertical: 10, horizontal: 8),
          decoration: BoxDecoration(
            color: selected
                ? AppColors.primary.withOpacity(0.15)
                : AppColors.surfaceLight,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(
              color: selected ? AppColors.primary : AppColors.border,
              width: selected ? 1.5 : 1,
            ),
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon,
                  size: 15,
                  color: selected ? AppColors.primary : AppColors.textMuted),
              const SizedBox(width: 6),
              Flexible(
                child: Text(
                  label,
                  style: TextStyle(
                    color: selected ? AppColors.primary : AppColors.textMuted,
                    fontSize: 12,
                    fontWeight:
                        selected ? FontWeight.w700 : FontWeight.normal,
                  ),
                ),
              ),
            ],
          ),
        ),
      );
}

// ── SliceForm data model ──────────────────────────────────────────────────────

class _SliceForm {
  String startTime;
  String stopTime;
  bool   targetEnabled;
  String targetLiters;   // raw text input, e.g. "5.0"

  _SliceForm(this.startTime, this.stopTime,
      {this.targetEnabled = false, this.targetLiters = ''});

  Map<String, dynamic> toJson() {
    final m = <String, dynamic>{
      'start_time': startTime,
      'stop_time':  stopTime,
    };
    if (targetEnabled) {
      final v = double.tryParse(targetLiters);
      if (v != null && v > 0) m['target_liters'] = v;
    }
    return m;
  }
}
