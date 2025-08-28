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
{"buzzer": "heartbeat"}
```

## Features

### Arduino Features
- **Startup indication**: Buzzer beeps twice on power-on
- **Connection status**: Visual and audio feedback
  - Connected: Single beep + steady LED when transmitting data
  - Disconnected: Three beeps + slow LED blink pattern
- **Automatic data transmission**: Sends sensor data every 5 seconds when connected
- **Remote control**: Responds to LED and buzzer commands via BLE

### Bridge Features
- **Auto-reconnection**: Automatic retry on connection failures
- **Comprehensive logging**: Timestamped logs with different severity levels
- **Error handling**: Robust error handling and recovery mechanisms
- **Modular design**: Clean object-oriented architecture following SOLID principles

## Configuration

### MQTT Settings
- **Broker**: `iot.qut.edu.kg:1883`
- **Topics**:
  - Sensor data: `iot/sensors/data`
  - Commands: `iot/commands/arduino`

### BLE Configuration
- **Target Device**: `Arduino_IoT_Sensor_hhx`
- **Service UUID**: `12345678-1234-1234-1234-123456789abc`
- **Characteristics**:
  - Sensor Data: `87654321-4321-4321-4321-cba987654321` (Read/Notify)
  - Commands: `11111111-2222-3333-4444-555555555555` (Write)
 
<img width="1290" height="2796" alt="image" src="https://github.com/user-attachments/assets/14b4c63a-ec7b-41fe-870b-3e52ac2c3615" />
<img width="1290" height="1659" alt="image" src="https://github.com/user-attachments/assets/37e64622-0a8c-47dc-9813-599b12789e11" />
<img width="1280" height="800" alt="image" src="https://github.com/user-attachments/assets/7df13235-cbcb-490c-ae32-8f6f5e21223a" />
<img width="1280" height="800" alt="image" src="https://github.com/user-attachments/assets/c50c4440-a8ae-4ac7-b36e-7c6a1f092bbc" />


## License

This project is developed for educational purposes as part of QUT IFN649 assignment.

## References

Hynek, H., & Ware, D. (2024). *Bleak: Bluetooth Low Energy platform agnostic client for Python* (Version 0.21.1) [Computer software]. https://github.com/hbldh/bleak

OASIS. (2019). *MQTT version 5.0 OASIS standard*. Organization for the Advancement of Structured Information Standards. https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html

Sensirion. (2024). *DHT11 humidity and temperature sensor datasheet*. Retrieved December 28, 2024, from https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf

The Internet Engineering Task Force. (2017). *The JavaScript Object Notation (JSON) data interchange format* (RFC 8259). https://tools.ietf.org/html/rfc8259
