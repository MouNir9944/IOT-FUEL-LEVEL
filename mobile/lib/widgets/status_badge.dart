import 'package:flutter/material.dart';
import '../core/constants.dart';

class StatusBadge extends StatelessWidget {
  final String status;
  final bool small;

  const StatusBadge({super.key, required this.status, this.small = false});

  String get _label {
    switch (status) {
      case 'online':   return 'Online';
      case 'offline':  return 'Offline';
      case 'unstable': return 'Unstable';
      default:         return 'Unknown';
    }
  }

  @override
  Widget build(BuildContext context) {
    final color = AppColors.statusColor(status);
    final dotSize = small ? 6.0 : 8.0;
    final fontSize = small ? 10.0 : 12.0;

    return Container(
      padding: EdgeInsets.symmetric(
        horizontal: small ? 6 : 8,
        vertical: small ? 2 : 4,
      ),
      decoration: BoxDecoration(
        color: color.withOpacity(0.12),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color, width: 1),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: dotSize,
            height: dotSize,
            decoration: BoxDecoration(color: color, shape: BoxShape.circle),
          ),
          const SizedBox(width: 4),
          Text(
            _label,
            style: TextStyle(
              color: color,
              fontSize: fontSize,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    );
  }
}
