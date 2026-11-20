import 'package:material_ui/material_ui.dart';

/// A dashboard card that explains one capability, runs it on demand and
/// lists what each step reported.
final class DemoLogCard extends StatelessWidget {
  /// A card titled [title] that calls [onRun] from a button labelled
  /// [label] and shows [lines] beneath it.
  const factory DemoLogCard({
    Key? key,
    required String title,
    required IconData icon,
    required String blurb,
    required String label,
    required List<String> lines,
    required VoidCallback onRun,
  }) = DemoLogCard._;

  const DemoLogCard._({
    super.key,
    required this.title,
    required this.icon,
    required this.blurb,
    required this.label,
    required this.lines,
    required this.onRun,
  });

  final String title;
  final IconData icon;
  final String blurb;
  final String label;
  final List<String> lines;
  final VoidCallback onRun;

  static const _mono = TextStyle(fontFamily: 'monospace', fontSize: 12);

  @override
  Widget build(BuildContext context) => Card(
    margin: const EdgeInsets.only(bottom: 16),
    child: Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(icon, size: 20),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  title,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ],
          ),
          const Divider(),
          Text(
            blurb,
            style: const TextStyle(color: Colors.white70, fontSize: 13),
          ),
          const SizedBox(height: 10),
          FilledButton.icon(
            onPressed: onRun,
            icon: Icon(icon),
            label: Text(label),
          ),
          for (final line in lines)
            Padding(
              padding: const EdgeInsets.only(top: 4),
              child: Text(line, style: _mono),
            ),
        ],
      ),
    ),
  );
}
