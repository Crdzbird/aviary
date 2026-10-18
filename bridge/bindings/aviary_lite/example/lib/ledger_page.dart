import 'package:flutter/material.dart';

import 'ledger_page_state.dart';

/// One ledger round trip: fund, transfer, finalize, prove.
final class LedgerPage extends StatefulWidget {
  const LedgerPage({super.key});

  @override
  State<LedgerPage> createState() => LedgerPageState();
}
