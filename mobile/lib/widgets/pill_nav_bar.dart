import 'package:flutter/material.dart';
import '../core/constants.dart';

/// A single item in the [PillNavBar].
class PillNavItem {
  final IconData icon;
  final String   label;
  const PillNavItem({required this.icon, required this.label});
}

/// Floating pill-style bottom navigation bar.
///
/// The active tab is highlighted with a filled rounded pill in [activeColor].
/// The label slides in alongside the icon only when active.
class PillNavBar extends StatelessWidget {
  final List<PillNavItem>  items;
  final int                selected;
  final ValueChanged<int>  onTap;
  final Color              activeColor;

  const PillNavBar({
    super.key,
    required this.items,
    required this.selected,
    required this.onTap,
    this.activeColor = AppColors.primary,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: AppColors.surface,
        border: Border(
          top: BorderSide(color: AppColors.border, width: 0.5),
        ),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.25),
            blurRadius: 20,
            offset: const Offset(0, -4),
          ),
        ],
      ),
      child: SafeArea(
        top: false,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: List.generate(items.length, (i) {
              final item     = items[i];
              final isActive = i == selected;
              return Expanded(
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onTap: () => onTap(i),
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 220),
                    curve: Curves.easeInOut,
                    margin: const EdgeInsets.symmetric(horizontal: 3),
                    padding: const EdgeInsets.symmetric(
                        horizontal: 10, vertical: 10),
                    decoration: BoxDecoration(
                      color: isActive ? activeColor : Colors.transparent,
                      borderRadius: BorderRadius.circular(40),
                    ),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          item.icon,
                          size: 22,
                          color:
                              isActive ? Colors.white : AppColors.textMuted,
                        ),
                        // Label slides in next to the icon when active.
                        // Wrapped in Flexible so long strings like "Utilisateurs"
                        // ellipsize instead of overflowing the pill.
                        if (isActive) ...[
                          const SizedBox(width: 6),
                          Flexible(
                            child: Text(
                              item.label,
                              overflow: TextOverflow.ellipsis,
                              maxLines: 1,
                              style: const TextStyle(
                                color: Colors.white,
                                fontWeight: FontWeight.w700,
                                fontSize: 12,
                              ),
                            ),
                          ),
                        ],
                      ],
                    ),
                  ),
                ),
              );
            }),
          ),
        ),
      ),
    );
  }
}
