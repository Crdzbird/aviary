import 'package:flutter/material.dart';

import 'records_page_state.dart';

/// An equipment record on a record chain: register, service, hand over,
/// and prove it to someone who holds only the state root.
final class RecordsPage extends StatefulWidget {
  const RecordsPage({super.key});

  @override
  State<RecordsPage> createState() => RecordsPageState();
}
