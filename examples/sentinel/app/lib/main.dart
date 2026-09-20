// Sentinel — device provenance recorder. A thin gRPC client (no native blockchain
// code): the device enrolls (its info + credential generate the genesis/origin
// block on the server), then streams data — photos, video chunks, GPS locations,
// sensor readings — each of which is hashed and anchored on the device's chain
// with BFT finality. Every anchored item can be independently verified against the
// chain root (proof-of-existence + integrity). All ledger work is server-side.
import 'dart:async';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import 'package:fixnum/fixnum.dart';
import 'package:flutter/material.dart';
import 'package:grpc/grpc.dart';

import 'gen/sentinel.pbgrpc.dart' as pb;

// Stable per-install credential so re-launching re-enrolls the SAME device chain
// (provisioning is idempotent on the device fingerprint).
final Uint8List _credential = Uint8List.fromList(List.generate(32, (i) => (i * 31 + 7) & 0xff));

void main() => runApp(const SentinelApp());

class SentinelApp extends StatelessWidget {
  const SentinelApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'Sentinel',
        theme: ThemeData(colorSchemeSeed: const Color(0xFF00897B), useMaterial3: true, brightness: Brightness.dark),
        home: const Recorder(),
      );
}

class _Rec {
  _Rec(this.kind, this.hash, this.summary);
  final pb.DataKind kind;
  final String hash;
  final String summary;
  bool finalized = false;
  int height = 0;
  bool? verified; // null = not yet checked
}

class Recorder extends StatefulWidget {
  const Recorder({super.key});
  @override
  State<Recorder> createState() => _RecorderState();
}

class _RecorderState extends State<Recorder> {
  final _rng = Random();
  final _host = TextEditingController(text: '127.0.0.1');
  ClientChannel? _channel;
  pb.SentinelClient? _client;
  String? _error;

  pb.DeviceInfo? _info;
  pb.Provenance? _prov;
  pb.LedgerStatus? _ledger;
  pb.Fleet? _fleet;
  pb.FleetProof? _fleetProof;
  bool _fleetBusy = false;

  StreamController<pb.DataRecord>? _out;
  StreamSubscription<pb.AnchorReceipt>? _sub;
  final List<_Rec> _recs = [];
  final Map<String, _Rec> _byHash = {};

  bool _live = false;
  Timer? _liveTimer;
  Timer? _pollTimer;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  Future<void> _connect() async {
    setState(() => _error = null);
    try {
      _channel = ClientChannel(_host.text.trim(),
          port: 50052, options: const ChannelOptions(credentials: ChannelCredentials.insecure()));
      _client = pb.SentinelClient(_channel!);

      _info = pb.DeviceInfo()
        ..deviceId = 'SENTINEL-${Platform.operatingSystem}-DEMO'
        ..model = Platform.operatingSystem == 'ios' ? 'iPhone (simulator)' : Platform.operatingSystem
        ..platform = Platform.operatingSystem
        ..osVersion = Platform.operatingSystemVersion
        ..label = 'Field Recorder'
        ..credential = _credential;

      _prov = await _client!.provisionDevice(_info!);
      _openStream();
      await _poll();
      _pollTimer = Timer.periodic(const Duration(milliseconds: 1500), (_) => _poll());
      _seedCaptures();
    } catch (e) {
      setState(() => _error = '$e');
    }
  }

  void _openStream() {
    _out = StreamController<pb.DataRecord>();
    _sub = _client!.streamData(_out!.stream).listen((r) {
      final existing = _byHash[r.contentHash];
      setState(() {
        if (existing == null) {
          final rec = _Rec(r.kind, r.contentHash, r.summary)
            ..finalized = r.finalized
            ..height = r.height.toInt();
          _byHash[r.contentHash] = rec;
          _recs.insert(0, rec);
          if (_recs.length > 60) _recs.removeLast();
        } else if (r.finalized) {
          existing.finalized = true;
          existing.height = r.height.toInt();
        }
      });
    }, onError: (e) => setState(() => _error = '$e'));
  }

  Int64 get _now => Int64(DateTime.now().millisecondsSinceEpoch);
  Int64 get _chain => _prov!.chainId;

  Uint8List _randomBytes(int n) {
    final b = Uint8List(n);
    for (var i = 0; i < n; i++) {
      b[i] = _rng.nextInt(256);
    }
    return b;
  }

  void _send(pb.DataRecord r) {
    if (_out == null || _out!.isClosed) return;
    _out!.add(r);
  }

  void _capturePhoto() => _send(pb.DataRecord()
    ..chainId = _chain
    ..kind = pb.DataKind.PHOTO
    ..payload = _randomBytes(3072 + _rng.nextInt(5120))
    ..mime = 'image/jpeg'
    ..capturedUnixMillis = _now
    ..note = 'still capture');

  void _captureVideo() => _send(pb.DataRecord()
    ..chainId = _chain
    ..kind = pb.DataKind.VIDEO
    ..payload = _randomBytes(16384)
    ..mime = 'video/mp4'
    ..capturedUnixMillis = _now
    ..note = '1s clip @30fps');

  void _captureLocation() => _send(pb.DataRecord()
    ..chainId = _chain
    ..kind = pb.DataKind.LOCATION
    ..lat = 37.7749 + (_rng.nextDouble() - 0.5) * 0.02
    ..lon = -122.4194 + (_rng.nextDouble() - 0.5) * 0.02
    ..accuracyM = 3 + _rng.nextDouble() * 6
    ..capturedUnixMillis = _now);

  void _captureSensors() => _send(pb.DataRecord()
    ..chainId = _chain
    ..kind = pb.DataKind.SENSOR
    ..sensors.addAll({
      'temp_c': 18 + _rng.nextDouble() * 8,
      'accel_g': 0.95 + _rng.nextDouble() * 0.2,
      'lux': (_rng.nextInt(900) + 50).toDouble(),
    })
    ..capturedUnixMillis = _now);

  void _toggleLive(bool on) {
    setState(() => _live = on);
    _liveTimer?.cancel();
    if (on) {
      _liveTimer = Timer.periodic(const Duration(milliseconds: 1100), (_) {
        switch (_rng.nextInt(4)) {
          case 0:
            _capturePhoto();
          case 1:
            _captureVideo();
          case 2:
            _captureLocation();
          default:
            _captureSensors();
        }
      });
    }
  }

  // One self-test capture of each kind on connect, so the provenance feed is
  // populated immediately (mirrors a device's power-on integrity check).
  void _seedCaptures() {
    _capturePhoto();
    Future.delayed(const Duration(milliseconds: 250), _captureLocation);
    Future.delayed(const Duration(milliseconds: 500), _captureSensors);
  }

  Future<void> _poll() async {
    try {
      final l = await _client!.getLedger(pb.DeviceRef()..chainId = _chain);
      if (mounted) setState(() => _ledger = l);
    } catch (_) {}
  }

  Future<void> _verify(_Rec rec) async {
    try {
      final v = await _client!.verifyRecord(pb.VerifyRequest()
        ..chainId = _chain
        ..contentHash = rec.hash);
      setState(() => rec.verified = v.anchored && v.verified);
    } catch (e) {
      _snack('$e');
    }
  }

  void _snack(String m) => ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(m)));

  @override
  void dispose() {
    _liveTimer?.cancel();
    _pollTimer?.cancel();
    _sub?.cancel();
    _out?.close();
    _channel?.shutdown();
    _host.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null && _prov == null) {
      return Scaffold(
        appBar: AppBar(title: const Text('Sentinel')),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(mainAxisSize: MainAxisSize.min, children: [
              Text('Cannot reach the Sentinel server.\n$_error', textAlign: TextAlign.center),
              const SizedBox(height: 8),
              const Text('Start it: cd examples/sentinel/server && go run .',
                  style: TextStyle(color: Colors.white54)),
              const SizedBox(height: 16),
              FilledButton(onPressed: _connect, child: const Text('Retry')),
            ]),
          ),
        ),
      );
    }
    if (_prov == null) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    return DefaultTabController(
      length: 2,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Sentinel · Device Provenance'),
          bottom: const TabBar(tabs: [
            Tab(icon: Icon(Icons.videocam), text: 'Recorder'),
            Tab(icon: Icon(Icons.device_hub), text: 'Fleet'),
          ]),
        ),
        body: TabBarView(children: [
          ListView(padding: const EdgeInsets.all(16), children: [
            _identityCard(),
            _captureCard(),
            _ledgerCard(),
            _feedCard(),
          ]),
          _fleetTab(),
        ]),
      ),
    );
  }

  // ── Fleet tab: a chain tree — the fleet root anchors one subchain per device ──
  Future<void> _fleetCall(Future<void> Function() f) async {
    setState(() => _fleetBusy = true);
    try {
      await f();
    } catch (e) {
      if (mounted) setState(() => _error = '$e');
    } finally {
      if (mounted) setState(() => _fleetBusy = false);
    }
  }

  Future<void> _fleetBuild() => _fleetCall(() async {
        final me = _info?.model.isNotEmpty == true ? _info!.model : 'this device';
        final f = await _client!.buildFleet(pb.FleetSpec()..deviceLabels.addAll([me, 'bodycam-07', 'dashcam-12']));
        if (mounted) setState(() { _fleet = f; _fleetProof = null; });
      });
  Future<void> _fleetRefresh() => _fleetCall(() async {
        final f = await _client!.getFleet(pb.FleetRef());
        if (mounted) setState(() => _fleet = f);
      });
  Future<void> _fleetProduce(int chainId) => _fleetCall(() async {
        final f = await _client!.fleetProduce(pb.FleetRef()..chainId = Int64(chainId));
        if (mounted) setState(() => _fleet = f);
      });
  Future<void> _fleetProve(int chainId) => _fleetCall(() async {
        final p = await _client!.fleetProveDevice(pb.FleetRef()..chainId = Int64(chainId));
        final f = await _client!.getFleet(pb.FleetRef());
        if (mounted) setState(() { _fleetProof = p; _fleet = f; });
      });

  Widget _fleetTab() {
    final f = _fleet;
    if (f == null && !_fleetBusy) _fleetRefresh();
    return ListView(padding: const EdgeInsets.all(16), children: [
      _section('Fleet chain tree', Icons.device_hub, [
        const Text(
            'One declaration builds the whole fleet: a root chain and a BFT subchain per '
            'device (genesis bottom-up, bindings top-down). Producing on the root anchors '
            'every device\'s finalized headers; a device\'s on-chain identity is then '
            'provable from the fleet root header alone.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        if (f == null || !f.built)
          FilledButton.icon(onPressed: _fleetBusy ? null : _fleetBuild, icon: const Icon(Icons.build), label: const Text('Build fleet (this device + 2 more)'))
        else ...[
          for (final n in f.nodes)
            ListTile(
              dense: true,
              contentPadding: EdgeInsets.zero,
              leading: Icon(n.root ? Icons.hub : Icons.smartphone, size: 18, color: n.root ? Colors.amberAccent : Colors.tealAccent),
              title: Text('${n.label} · chain ${n.chainId}', style: const TextStyle(fontSize: 13)),
              subtitle: Text('height ${n.height}${n.root ? " · pending anchors ${n.pendingAnchors}" : " · identity ${n.identityAddress.substring(0, 12)}…"}',
                  style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
              trailing: Wrap(spacing: 4, children: [
                IconButton(tooltip: n.root ? 'Produce root block (anchor devices)' : 'Produce device block', onPressed: _fleetBusy ? null : () => _fleetProduce(n.chainId.toInt()), icon: const Icon(Icons.layers, size: 18)),
                if (!n.root) IconButton(tooltip: 'Prove identity from the fleet root', onPressed: _fleetBusy ? null : () => _fleetProve(n.chainId.toInt()), icon: const Icon(Icons.verified_user, size: 18)),
              ]),
            ),
          if (f.rootSubchainRoot.isNotEmpty)
            Padding(padding: const EdgeInsets.only(top: 6), child: Text('root subchain_root ${f.rootSubchainRoot.substring(0, 24)}…', style: const TextStyle(fontFamily: 'monospace', fontSize: 11, color: Colors.white54))),
          const SizedBox(height: 8),
          OutlinedButton.icon(onPressed: _fleetBusy ? null : _fleetBuild, icon: const Icon(Icons.refresh), label: const Text('Rebuild fleet')),
        ],
      ]),
      if (_fleetProof != null)
        _section('Cross-chain proof', Icons.verified_user, [
          Row(children: [
            Icon(_fleetProof!.verified ? Icons.check_circle : Icons.error, color: _fleetProof!.verified ? Colors.lightGreenAccent : Colors.redAccent),
            const SizedBox(width: 8),
            Expanded(child: Text(_fleetProof!.verified
                ? 'Device ${_fleetProof!.chainId} identity verified from the fleet root header (${_fleetProof!.proofBytes} B proof)'
                : 'Not verifiable from the current root — produce a device block, then a root block, then prove again.')),
          ]),
          const SizedBox(height: 6),
          Text('identity ${_fleetProof!.identityAddress}', style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
          Text('against subchain_root ${_fleetProof!.rootSubchainRoot}', style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
        ]),
      _section('Fleet log', Icons.receipt_long, [
        if (f == null || f.log.isEmpty)
          const Text('No fleet activity yet.', style: TextStyle(color: Colors.white54))
        else
          for (final l in f.log.reversed)
            Padding(padding: const EdgeInsets.symmetric(vertical: 2), child: Text(l, style: const TextStyle(fontFamily: 'monospace', fontSize: 11))),
      ]),
    ]);
  }

  Widget _section(String title, IconData icon, List<Widget> children, {Widget? trailing}) => Card(
        margin: const EdgeInsets.only(bottom: 16),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              Icon(icon, size: 20),
              const SizedBox(width: 8),
              Expanded(child: Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold))),
              ?trailing,
            ]),
            const Divider(),
            ...children,
          ]),
        ),
      );

  Widget _identityCard() {
    final p = _prov!, i = _info!;
    return _section('Device identity (origin block)', Icons.verified_user, [
      _kv('Device', '${i.label} · ${i.model}'),
      _kv('Platform', '${i.platform} ${i.osVersion}'),
      _kv('Chain id', '${p.chainId}'),
      _kv('Fingerprint', p.deviceFingerprint),
      _kv('Genesis root', p.genesisHash),
      _kv('Identity addr', p.deviceAddress),
      Padding(
        padding: const EdgeInsets.only(top: 6),
        child: Row(children: [
          const Icon(Icons.check_circle, color: Colors.tealAccent, size: 16),
          const SizedBox(width: 6),
          Expanded(
            child: Text('Origin block sealed at height ${p.genesisHeight} — bound to this device + credential',
                style: const TextStyle(color: Colors.white70, fontSize: 12)),
          ),
        ]),
      ),
    ]);
  }

  Widget _captureCard() => _section(
        'Capture & anchor',
        Icons.sensors,
        [
          Wrap(spacing: 10, runSpacing: 10, children: [
            _capBtn('Photo', Icons.photo_camera, _capturePhoto),
            _capBtn('Video', Icons.videocam, _captureVideo),
            _capBtn('Location', Icons.location_on, _captureLocation),
            _capBtn('Sensors', Icons.thermostat, _captureSensors),
          ]),
        ],
        trailing: Row(mainAxisSize: MainAxisSize.min, children: [
          const Text('Live', style: TextStyle(fontSize: 13)),
          Switch(value: _live, onChanged: _toggleLive),
        ]),
      );

  Widget _capBtn(String label, IconData icon, VoidCallback onTap) =>
      FilledButton.tonalIcon(onPressed: onTap, icon: Icon(icon, size: 18), label: Text(label));

  Widget _ledgerCard() {
    final l = _ledger;
    return _section('Ledger', Icons.link, [
      if (l == null)
        const Text('…')
      else ...[
        Wrap(spacing: 28, runSpacing: 10, children: [
          _stat('Height', '${l.height}'),
          _stat('Anchored', '${l.records}'),
          _stat('Pending', '${l.mempool}'),
        ]),
        if (l.kindCounts.isNotEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: Wrap(spacing: 8, children: [
              for (final e in l.kindCounts.entries)
                Chip(label: Text('${_kindName(e.key)} ${e.value}'), visualDensity: VisualDensity.compact),
            ]),
          ),
      ],
    ]);
  }

  Widget _feedCard() => _section('Provenance feed', Icons.receipt_long, [
        if (_recs.isEmpty)
          const Text('No captures yet — tap a capture button or flip "Live".', style: TextStyle(color: Colors.white54))
        else
          ..._recs.map(_recRow),
      ]);

  Widget _recRow(_Rec r) {
    final color = r.finalized ? Colors.tealAccent : Colors.amberAccent;
    return ListTile(
      contentPadding: EdgeInsets.zero,
      dense: true,
      leading: Icon(_kindIcon(r.kind), color: color, size: 20),
      title: Text(r.summary, style: const TextStyle(fontSize: 13)),
      subtitle: Text(
        '${r.hash.substring(0, 16)}…  ·  ${r.finalized ? "finalized @${r.height}" : "anchoring…"}',
        style: const TextStyle(fontFamily: 'monospace', fontSize: 11),
      ),
      trailing: r.verified == true
          ? const Icon(Icons.gpp_good, color: Colors.tealAccent)
          : r.verified == false
              ? const Icon(Icons.gpp_bad, color: Colors.redAccent)
              : TextButton(onPressed: r.finalized ? () => _verify(r) : null, child: const Text('Verify')),
    );
  }

  IconData _kindIcon(pb.DataKind k) => switch (k) {
        pb.DataKind.PHOTO => Icons.photo_camera,
        pb.DataKind.VIDEO => Icons.videocam,
        pb.DataKind.LOCATION => Icons.location_on,
        pb.DataKind.SENSOR => Icons.thermostat,
        _ => Icons.bolt,
      };

  String _kindName(String enumName) => enumName.replaceFirst('DATA_', '').toLowerCase();

  Widget _kv(String k, String v) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 2),
        child: Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          SizedBox(width: 100, child: Text(k, style: const TextStyle(color: Colors.white54, fontSize: 12))),
          Expanded(child: SelectableText(v, style: const TextStyle(fontFamily: 'monospace', fontSize: 11))),
        ]),
      );

  Widget _stat(String label, String value) => Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(label, style: const TextStyle(fontSize: 12, color: Colors.white54)),
        Text(value, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
      ]);
}
