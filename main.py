#!/usr/bin/env python3
"""
简化版BLE到MQTT桥接器
Arduino Nano 33 IoT <-> Raspberry Pi <-> MQTT
"""

import asyncio
import logging
from typing import Optional
from bleak import BleakScanner, BleakClient, BleakDevice
import paho.mqtt.client as mqtt


class IoTBridgeConfig:
    """IoT桥接器配置类"""
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
    """Arduino IoT传感器的BLE到MQTT桥接器"""

    def __init__(self, config: IoTBridgeConfig):
        self.config = config
        self.ble_client: Optional[BleakClient] = None
        self.mqtt_client: Optional[mqtt.Client] = None
        self.running = False
        self._setup_logging()

    def _setup_logging(self):
        """设置日志配置"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger(__name__)

    def _setup_mqtt(self):
        """设置MQTT客户端和回调函数"""
        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.mqtt_client.on_message = self._on_mqtt_message
        self.mqtt_client.connect(self.config.MQTT_HOST, self.config.MQTT_PORT, 60)
        self.mqtt_client.subscribe(self.config.COMMAND_TOPIC)
        self.mqtt_client.loop_start()
        self.logger.info("✓ MQTT connected: %s", self.config.MQTT_HOST)

    def _on_mqtt_message(self, _client, _userdata, msg):
        """处理MQTT消息"""
        try:
            command = msg.payload.decode('utf-8')
            self.logger.info("MQTT命令: %s", command)
            if self.ble_client:
                asyncio.create_task(self._send_command_to_arduino(command))
        except UnicodeDecodeError as e:
            self.logger.error("MQTT消息解码失败: %s", e)

    async def _scan_for_device(self) -> Optional[BleakDevice]:
        """扫描Arduino设备"""
        self.logger.info("扫描Arduino设备...")
        devices = await BleakScanner.discover(timeout=self.config.SCAN_TIMEOUT)
        self.logger.info("发现 %d 个BLE设备", len(devices))

        for device in devices:
            if device.name:
                self.logger.debug("  - %s", device.name)
                if "Arduino" in device.name:
                    self.logger.info("    ⚠️  可能的Arduino设备: %s", device.name)

        for device in devices:
            if device.name and self.config.TARGET_NAME in device.name:
                return device

        return None

    def _sensor_data_handler(self, _sender, data: bytearray):
        """处理来自Arduino的传感器数据"""
        try:
            json_data = data.decode('utf-8')
            self.logger.info("传感器数据: %s", json_data)
            if self.mqtt_client:
                self.mqtt_client.publish(self.config.SENSOR_TOPIC, json_data)
                self.logger.info("✓ 已发布到MQTT")
        except UnicodeDecodeError as e:
            self.logger.error("传感器数据解码失败: %s", e)

    async def _send_command_to_arduino(self, command: str):
        """向Arduino发送命令"""
        try:
            if self.ble_client:
                await self.ble_client.write_gatt_char(
                    self.config.COMMAND_CHAR_UUID,
                    command.encode('utf-8')
                )
                self.logger.info("✓ 命令已发送: %s", command)
        except Exception as e:
            self.logger.error("命令发送失败: %s", e)

    async def _connect_to_device(self, device: BleakDevice):
        """连接到BLE设备并设置通知"""
        async with BleakClient(device.address) as client:
            self.ble_client = client
            self.logger.info("✓ BLE已连接")

            await client.start_notify(
                self.config.SENSOR_CHAR_UUID,
                self._sensor_data_handler
            )
            self.logger.info("✓ 已订阅传感器数据")
            self.logger.info("桥接器运行中... (Ctrl+C退出)")

            while self.running:
                await asyncio.sleep(1)

    async def _device_connection_loop(self):
        """主设备连接循环（带重试逻辑）"""
        while self.running:
            try:
                target_device = await self._scan_for_device()

                if not target_device:
                    self.logger.warning("✗ 未找到设备: %s", self.config.TARGET_NAME)
                    self.logger.info("请检查:")
                    self.logger.info("1. Arduino是否正在运行？")
                    self.logger.info("2. Arduino串口是否显示 'Arduino BLE sensor started'？")
                    self.logger.info("3. Arduino和这台设备距离是否够近？")
                    self.logger.info("%d秒后重试...", self.config.DEVICE_CHECK_DELAY)
                    await asyncio.sleep(self.config.DEVICE_CHECK_DELAY)
                    continue

                self.logger.info("✓ 找到设备: %s", target_device.name)
                await self._connect_to_device(target_device)

            except Exception as ble_error:
                self.logger.error("BLE连接失败: %s", ble_error)
                self.logger.info("%d秒后重新尝试...", self.config.RETRY_DELAY)
                self.ble_client = None
                await asyncio.sleep(self.config.RETRY_DELAY)

    async def start(self):
        """启动IoT桥接器"""
        self.logger.info("Arduino BLE-MQTT桥接器启动中...")
        self.running = True

        try:
            self._setup_mqtt()
            await self._device_connection_loop()
        except KeyboardInterrupt:
            self.logger.info("\n程序退出...")
        except Exception as e:
            self.logger.error("意外错误: %s", e)
        finally:
            await self.stop()

    async def stop(self):
        """停止IoT桥接器"""
        self.running = False
        if self.mqtt_client:
            self.mqtt_client.disconnect()
        self.logger.info("桥接器已停止")


async def main():
    """主入口点"""
    config = IoTBridgeConfig()
    bridge = IoTBridge(config)
    await bridge.start()


if __name__ == "__main__":
    asyncio.run(main())