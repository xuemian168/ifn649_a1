# Arduino BLE-MQTT Bridge

A simple IoT bridge that connects Arduino Nano 33 IoT sensors to MQTT broker via Bluetooth Low Energy (BLE) for QUT IFN649.

## Architecture

```
Arduino Nano 33 IoT  <--BLE-->  Raspberry Pi  <--MQTT-->  AWS Brooker
```

<img width="1290" height="689" alt="image" src="https://github.com/user-attachments/assets/0622e8ca-6e39-47c5-bac7-0e8ddcf65bed" />


## Hardware Requirements

- **Arduino Nano 33 IoT** with DHT11 sensor
- **Raspberry Pi** (or any device with BLE capability)
- **DHT11** temperature/humidity sensor
- **Buzzer** 

## Arduino Setup (assignment1.ino)

### Hardware Connections
- DHT11 sensor → Pin 2
- Buzzer → Pin 5
- Built-in LED for status indication

## Data Format

### Sensor Data (Arduino → MQTT)
```json
{
  "temperature": 25.6,
  "humidity": 60.2,
  "timestamp": 12345678,
  "device_id": "Arduino_hhx"
}
```

### Commands (MQTT → Arduino)
```json
{"led": "on"}
{"led": "off"}
{"led": "toggle"}
{"buzzer": "beep"}
```
