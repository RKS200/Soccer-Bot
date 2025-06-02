import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_classic/flutter_blue_classic.dart';
import 'package:flutter_joystick/flutter_joystick.dart';

void main() async {
  // Make sure the Flutter engine is ready
  WidgetsFlutterBinding.ensureInitialized();

  // Force landscape orientation
  await SystemChrome.setPreferredOrientations([
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  runApp(
    const MaterialApp(
      home: BluetoothJoystickApp(),
      debugShowCheckedModeBanner: false,
    ),
  );
}

class BluetoothJoystickApp extends StatefulWidget {
  const BluetoothJoystickApp({super.key});

  @override
  State<BluetoothJoystickApp> createState() => _BluetoothJoystickAppState();
}

class _BluetoothJoystickAppState extends State<BluetoothJoystickApp> {
  final FlutterBlueClassic flutterBlueClassic = FlutterBlueClassic();

  List<BluetoothDevice> _bondedDevices = [];
  BluetoothDevice? _selectedDevice;
  BluetoothConnection? _connection;
  Widget _connectionStatus = Spacer();

  double _voltage = 0;
  double _current = 0;

  final StringBuffer _buffer = StringBuffer();

  @override
  void initState() {
    super.initState();
    _loadBondedDevices();
    _connectionStatus = IconButton(
      onPressed: _selectedDevice != null ? connectToDevice : null,
      icon: Icon(Icons.link_off, color: Colors.blueGrey),
    );
  }

  Future<void> _loadBondedDevices() async {
    try {
      final devices = await flutterBlueClassic.bondedDevices ?? [];
      setState(() {
        _bondedDevices = devices;
        if (devices.isNotEmpty) {
          _selectedDevice = devices.first;
          _connectionStatus = IconButton(
      onPressed: connectToDevice,
      icon: Icon(Icons.link_off, color: Colors.blueGrey),
    );
        }
      });
    } catch (e) {
      debugPrint("Error fetching bonded devices: $e");
    }
  }

  Future<void> connectToDevice() async {
    if (_selectedDevice == null) return;
    setState(() {
      _connectionStatus = CircularProgressIndicator();
    });
    try {
      final connection = await flutterBlueClassic.connect(
        _selectedDevice!.address!,
      );
      setState(() {
        _connection = connection;
        _connectionStatus = IconButton(
          onPressed: _selectedDevice != null ? connectToDevice : null,
          icon: Icon(Icons.link, color: Colors.green),
        );
        ;
      });

      connection?.input?.listen(
        (Uint8List data) {
          final chunk = utf8.decode(data);
          _buffer.write(chunk);
          if (chunk.contains("\n")) {
            try {
              final jsonStr = _buffer.toString().trim();
              final data = jsonDecode(jsonStr);
              if (data is Map<String, dynamic>) {
                setState(() {
                  _voltage = data['voltage'].toDouble()  * (3.3/ 1023.0)*(37.6/7.5) ?? 0;
                  _current = data['current'].toDouble() ?? 0;
                });
                sendJoystickData("OK",0,0);
              }
            } catch (e) {
              debugPrint("Failed to parse incoming JSON: $e");
            } finally {
              _buffer.clear();
            }
          }
        },
        onDone: () {
          setState(() {
            _connectionStatus = IconButton(
              onPressed: _selectedDevice != null ? connectToDevice : null,
              icon: Icon(Icons.link_off, color: Colors.blueGrey),
            );
          });
        },
      );
    } catch (e) {
      debugPrint('Connection error: $e');
      setState(() {
        _connectionStatus = IconButton(
          onPressed: _selectedDevice != null ? connectToDevice : null,
          icon: Icon(Icons.link_off, color: Colors.red),
        );
        ;
      });
    }
  }

  void sendJoystickData(String cmd, double x, double y) {
    int mapX = (x * 255).clamp(-255, 255).toInt();
    int mapY = (-y * 255).clamp(-255, 255).toInt();

    final data = {"cmd": cmd, "x": mapX, "y": mapY};

    final jsonStr = jsonEncode(data) + '\n';
    _connection?.output.add(Uint8List.fromList(utf8.encode(jsonStr)));
    debugPrint("Sent: $jsonStr");
  }

  @override
  void dispose() {
    _connection?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text("S-BOT Connect"),
        actions: [
          DropdownButton<BluetoothDevice>(
            value: _selectedDevice,
            isExpanded: false,
            hint: const Text("Select bonded device"),
            items: _bondedDevices.map((device) {
              return DropdownMenuItem(
                value: device,
                child: Text("${device.name}"),
              );
            }).toList(),
            onChanged: (device) {
              setState(() {
                _selectedDevice = device;
              });
            },
          ),
          SizedBox(width: 10,),_connectionStatus,
        ],
      ),
      body: SafeArea(
        child: Row(
          children: [
            // JoystickArea with bigger area on the left
            Expanded(
              flex: 2,
              child: Container(
                child: JoystickArea(
                  listener: (details) {
                    sendJoystickData("drive", details.x, details.y);
                  },
                ),
              ),
            ),

            // Controls on right side
            Expanded(
              flex: 1,
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.start,
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(
                      "${ double.parse(_voltage.toStringAsFixed(2))} V",
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    Text(
                      "${ double.parse(_current.toStringAsFixed(2))} mA",
                      style: TextStyle(
                        fontSize: 16,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 12),
                    ElevatedButton(
                      onPressed: () {
                        sendJoystickData("pulse", 0, 0);
                      },
                      child: const Text("Pulse"),
                    ),
                  ],
                ),
              ),
            ),
            Expanded(
              flex: 2,
              child: Container(
                child: JoystickArea(
                  mode: JoystickMode.horizontal,
                  listener: (details) {
                    sendJoystickData("rotate", details.x, 0);
                    ;
                  },
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
