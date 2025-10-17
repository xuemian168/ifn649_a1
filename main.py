#!/usr/bin/env python3
"""
BLE to MQTT Bridge - Laser Sensor Data Reporter
Arduino → Raspberry Pi → MQTT
"""

import asyncio
import logging
from bleak import BleakScanner, BleakClient
import paho.mqtt.client as mqtt


# === Configuration ===
ARDUINO_NAME = "Arduino_Laser_Receiver"
SENSOR_UUID = "87654321-4321-4321-4321-cba987654321"
COMMAND_UUID = "11111111-2222-3333-4444-555555555555"

MQTT_BROKER = "iot.qut.edu.kg"
MQTT_PORT = 1883
SENSOR_TOPIC = "iot/sensors/laser_data"
COMMAND_TOPIC = "iot/commands/laser"

ENABLE_MQTT = True  # Set to False to disable MQTT


class LaserBridge:
    """Laser Sensor BLE-MQTT Bridge"""

    def __init__(self):
        self.ble_client = None
        self.mqtt_client = None
        self.running = False

        # Setup logging
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(message)s'
        )
        self.log = logging.getLogger(__name__)

    def setup_mqtt(self):
        """Initialize MQTT"""
        if not ENABLE_MQTT:
            self.log.info("MQTT disabled")
            return

        try:
            self.log.info(f"Connecting to MQTT: {MQTT_BROKER}:{MQTT_PORT}")

            # Create client
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message

            # Connect asynchronously
            self.mqtt_client.loop_start()
            self.mqtt_client.connect_async(MQTT_BROKER, MQTT_PORT, 60)

            self.log.info("✓ MQTT started")

        except Exception as e:
            self.log.warning(f"MQTT connection failed: {e}")
            self.mqtt_client = None

    def on_mqtt_connect(self, client, userdata, flags, rc, properties=None):
        """MQTT connection callback"""
        if rc == 0:
            self.log.info("✓ MQTT connected")
            client.subscribe(COMMAND_TOPIC)
        else:
            self.log.error(f"✗ MQTT connection failed: {rc}")

    def on_mqtt_message(self, client, userdata, msg):
        """Received MQTT command"""
        command = msg.payload.decode('utf-8')
        self.log.info(f"Received command: {command}")

        # Forward to Arduino
        if self.ble_client:
            asyncio.create_task(self.send_command(command))

    async def send_command(self, command):
        """Send command to Arduino"""
        try:
            await self.ble_client.write_gatt_char(
                COMMAND_UUID,
                command.encode('utf-8')
            )
            self.log.info(f"✓ Command sent: {command}")
        except Exception as e:
            self.log.error(f"Send failed: {e}")

    def on_sensor_data(self, sender, data):
        """Received sensor data"""
        try:
            json_data = data.decode('utf-8')
            self.log.info(f"📡 {json_data}")

            # Publish to MQTT
            if self.mqtt_client:
                self.mqtt_client.publish(SENSOR_TOPIC, json_data, qos=1)
                self.log.info("✓ Published to MQTT")

        except Exception as e:
            self.log.error(f"Data processing failed: {e}")

    async def find_arduino(self):
        """Scan for Arduino device"""
        self.log.info("Scanning for Arduino...")

        devices = await BleakScanner.discover(timeout=10.0)
        self.log.info(f"Found {len(devices)} BLE devices")

        for device in devices:
            if device.name and ARDUINO_NAME in device.name:
                self.log.info(f"✓ Found: {device.name}")
                return device

        return None

    async def connect_arduino(self, device):
        """Connect to Arduino"""
        async with BleakClient(device.address) as client:
            self.ble_client = client
            self.log.info("✓ BLE connected")

            # Subscribe to sensor data
            await client.start_notify(SENSOR_UUID, self.on_sensor_data)
            self.log.info("✓ Started receiving data")
            self.log.info("=" * 60)

            # Keep connection alive
            while self.running:
                await asyncio.sleep(1)

    async def run(self):
        """Main loop"""
        self.log.info("=" * 60)
        self.log.info("Laser Sensor Bridge Started")
        self.log.info("=" * 60)

        self.running = True
        self.setup_mqtt()

        # Main loop: scan → connect → retry
        while self.running:
            try:
                device = await self.find_arduino()

                if not device:
                    self.log.warning("Arduino not found, retrying in 5 seconds...")
                    await asyncio.sleep(5)
                    continue

                await self.connect_arduino(device)

            except Exception as e:
                self.log.error(f"Connection failed: {e}")
                await asyncio.sleep(3)

    def stop(self):
        """Stop bridge"""
        self.running = False
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        self.log.info("Stopped")


async def main():
    """Entry point"""
    bridge = LaserBridge()
    try:
        await bridge.run()
    except KeyboardInterrupt:
        print("\nProgram exiting")
        bridge.stop()


if __name__ == "__main__":
    asyncio.run(main())
