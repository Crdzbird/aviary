import 'package:flutter/material.dart';

import 'events_page.dart';
import 'home_page.dart';
import 'ledger_page.dart';
import 'network_page.dart';
import 'records_page.dart';

/// The selected tab.
final class HomePageState extends State<HomePage> {
  int _tab = 0;

  static const _pages = <Widget>[
    LedgerPage(),
    EventsPage(),
    RecordsPage(),
    NetworkPage(),
  ];

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('aviary')),
    body: _pages[_tab],
    bottomNavigationBar: NavigationBar(
      selectedIndex: _tab,
      onDestinationSelected: (i) => setState(() => _tab = i),
      destinations: const [
        NavigationDestination(
          icon: Icon(Icons.account_balance),
          label: 'Ledger',
        ),
        NavigationDestination(icon: Icon(Icons.bolt), label: 'Events'),
        NavigationDestination(icon: Icon(Icons.inventory_2), label: 'Records'),
        NavigationDestination(icon: Icon(Icons.hub), label: 'Network'),
      ],
    ),
  );
}
