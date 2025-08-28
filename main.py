#!/usr/bin/env python3
"""
Simplified BLE to MQTT Bridge
Arduino Nano 33 IoT <-> Raspberry Pi <-> MQTT
"""

import asyncio
import logging
from typing import Optional
from bleak import BleakScanner, BleakClient
import paho.mqtt.client as mqtt


class IoTBridgeConfig:
    """Configuration class for IoT Bridge"""
    TARGET_NAME = "Arduino_IoT_Sensor_hhx"
    SENSOR_CHAR_UUID = "87654321-4321-4321-4321-cba987654321"
    COMMAND_CHAR_UUID = "11111111-2222-3333-4444-555555555555"
    MQTT_HOST = "iot.qut.edu.kg"
    MQTT_PORT = 1883
    SENSOR_TOPIC = "iot/sensors/data"
    COMMAND_TOPIC = "iot/commands/arduino"
    SCAN_TIMEOUT = 10.0
    RETRY_DELAY = 3
    DEVICE_CHECK_DELAY = 5


class IoTBridge:
    """BLE to MQTT Bridge for Arduino IoT sensors"""

    def __init__(self, config: IoTBridgeConfig):
        self.config = config
        self.ble_client: Optional[BleakClient] = None
        self.mqtt_client: Optional[mqtt.Client] = None
        self.running = False
        self._setup_logging()

    def _setup_logging(self):
        """Setup logging configuration"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger(__name__)

    def _setup_mqtt(self):
        """Setup MQTT client and callbacks"""
        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.mqtt_client.on_message = self._on_mqtt_message
        self.mqtt_client.connect(self.config.MQTT_HOST, self.config.MQTT_PORT, 60)
        self.mqtt_client.subscribe(self.config.COMMAND_TOPIC)
        self.mqtt_client.loop_start()
        self.logger.info("✓ MQTT connected: %s", self.config.MQTT_HOST)

    def _on_mqtt_message(self, _client, _userdata, msg):
        """Handle MQTT messages"""
        try:
            command = msg.payload.decode('utf-8')
            self.logger.info("MQTT command: %s", command)
            if self.ble_client:
                asyncio.create_task(self._send_command_to_arduino(command))
        except UnicodeDecodeError as e:
            self.logger.error("Failed to decode MQTT message: %s", e)

    async def _scan_for_device(self):
        """Scan for Arduino device"""
        self.logger.info("Scanning for Arduino device...")
        devices = await BleakScanner.discover(timeout=self.config.SCAN_TIMEOUT)
        self.logger.info("Discovered %d BLE devices", len(devices))

        for device in devices:
            if device.name:
                self.logger.debug("  - %s", device.name)
                if "Arduino" in device.name:
                    self.logger.info("    ⚠️  Possible Arduino device: %s", device.name)

        for device in devices:
            if device.name and self.config.TARGET_NAME in device.name:
                return device

        return None

    def _sensor_data_handler(self, _sender, data: bytearray):
        """Handle sensor data from Arduino"""
        try:
            json_data = data.decode('utf-8')
            self.logger.info("Sensor data: %s", json_data)
            if self.mqtt_client:
                self.mqtt_client.publish(self.config.SENSOR_TOPIC, json_data)
                self.logger.info("✓ Published to MQTT")
        except UnicodeDecodeError as e:
            self.logger.error("Failed to decode sensor data: %s", e)

    async def _send_command_to_arduino(self, command: str):
        """Send command to Arduino"""
        try:
            if self.ble_client:
                await self.ble_client.write_gatt_char(
                    self.config.COMMAND_CHAR_UUID,
                    command.encode('utf-8')
                )
                self.logger.info("✓ Command sent: %s", command)
        except Exception as e:
            self.logger.error("Failed to send command: %s", e)

    async def _connect_to_device(self, device):
        """Connect to BLE device and setup notifications"""
        async with BleakClient(device.address) as client:
            self.ble_client = client
            self.logger.info("✓ BLE connected")

            await client.start_notify(
                self.config.SENSOR_CHAR_UUID,
                self._sensor_data_handler
            )
            self.logger.info("✓ Subscribed to sensor data")
            self.logger.info("Bridge running... (Ctrl+C to exit)")

            while self.running:
                await asyncio.sleep(1)

    async def _device_connection_loop(self):
        """Main device connection loop with retry logic"""
        while self.running:
            try:
                target_device = await self._scan_for_device()

                if not target_device:
                    self.logger.warning("✗ Device not found: %s", self.config.TARGET_NAME)
                    self.logger.info("Please check:")
                    self.logger.info("1. Is Arduino running?")
                    self.logger.info("2. Does Arduino serial show 'Arduino BLE sensor started'?")
                    self.logger.info("3. Are Arduino and this device close enough?")
                    self.logger.info("Retrying in %d seconds...", self.config.DEVICE_CHECK_DELAY)
                    await asyncio.sleep(self.config.DEVICE_CHECK_DELAY)
                    continue

                self.logger.info("✓ Found device: %s", target_device.name)
                await self._connect_to_device(target_device)

            except Exception as ble_error:
                self.logger.error("BLE connection failed: %s", ble_error)
                self.logger.info("Retrying in %d seconds...", self.config.RETRY_DELAY)
                self.ble_client = None
                await asyncio.sleep(self.config.RETRY_DELAY)

    async def start(self):
        """Start the IoT bridge"""
        self.logger.info("Arduino BLE-MQTT Bridge Starting...")
        self.running = True

        try:
            self._setup_mqtt()
            await self._device_connection_loop()
        except KeyboardInterrupt:
            self.logger.info("\nProgram exiting...")
        except Exception as e:
            self.logger.error("Unexpected error: %s", e)
        finally:
            await self.stop()

    async def stop(self):
        """Stop the IoT bridge"""
        self.running = False
        if self.mqtt_client:
            self.mqtt_client.disconnect()
        self.logger.info("Bridge stopped")


async def main():
    """Main entry point"""
    config = IoTBridgeConfig()
    bridge = IoTBridge(config)
    await bridge.start()


if __name__ == "__main__":
    asyncio.run(main())
