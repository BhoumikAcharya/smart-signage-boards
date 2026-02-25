import paho.mqtt.client as mqtt
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from threading import Thread
import time
import os
import sys

# --- Configuration ---
MODBUS_PORT = 502
MQTT_BROKER_HOST = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_PREFIX = "metro/signage/register"
REGISTERS_TO_BRIDGE = 10

# Dictionaries to store status
device_statuses = {}       # Stores ONLINE/OFFLINE
device_power_states = {}   # Stores Power Supply Status (OK/FAIL)
device_current1 = {}       # Stores Load 1 Status (OK/PARTIAL/FAIL)
device_current2 = {}       # Stores Load 2 Status (OK/PARTIAL/FAIL)

def on_message(client, userdata, msg):
    """Callback for incoming MQTT messages"""
    try:
        topic_parts = msg.topic.split('/')
        if len(topic_parts) == 5:
            register_address = int(topic_parts[3])
            msg_type = topic_parts[4]
            payload = msg.payload.decode()

            if msg_type == "status":
                device_statuses[register_address] = payload
                if payload == "OFFLINE":
                    device_power_states[register_address] = "---"
                    device_current1[register_address] = "---"
                    device_current2[register_address] = "---"
            
            elif msg_type == "power":
                device_power_states[register_address] = payload
            
            elif msg_type == "current1":
                device_current1[register_address] = payload
            
            elif msg_type == "current2":
                device_current2[register_address] = payload

    except Exception as e:
        print(f"Error processing message: {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to Broker. Subscribing...")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/status")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/power")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/current1")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/current2")
    else:
        print(f"Failed to connect, return code {rc}\n")

def perform_startup_cleanup(client):
    print("--- PERFORMING STARTUP CLEANUP ---")
    for i in range(REGISTERS_TO_BRIDGE):
        addr = 40001 + i
        client.publish(f"{MQTT_TOPIC_PREFIX}/{addr}/status", "OFFLINE", retain=True)
        client.publish(f"{MQTT_TOPIC_PREFIX}/{addr}/power", "---", retain=True)
        client.publish(f"{MQTT_TOPIC_PREFIX}/{addr}/current1", "---", retain=True)
        client.publish(f"{MQTT_TOPIC_PREFIX}/{addr}/current2", "---", retain=True)
        
        device_statuses[addr] = "OFFLINE"
        device_power_states[addr] = "---"
        device_current1[addr] = "---"
        device_current2[addr] = "---"
        
    print("Reset complete. Sending Broadcast PING...")
    client.publish("metro/signage/scan", "PING", retain=False)
    time.sleep(2)

def bridge_and_display_loop(modbus_context, mqtt_client):
    last_known_values = [None] * REGISTERS_TO_BRIDGE

    while True:
        try:
            current_values = modbus_context[0].getValues(3, 0, count=REGISTERS_TO_BRIDGE)

            for i in range(REGISTERS_TO_BRIDGE):
                if current_values[i] != last_known_values[i]:
                    addr = 40001 + i
                    topic = f"{MQTT_TOPIC_PREFIX}/{addr}/value"
                    payload = str(current_values[i])
                    mqtt_client.publish(topic, payload, retain=True)
                    last_known_values[i] = current_values[i]
            
            # --- Updated Table Display ---
            if sys.stdout.isatty():
                os.system('cls' if os.name == 'nt' else 'clear')
                print("--- Metro Signage Modbus-MQTT Bridge ---")
                print(f"PLC -> Modbus -> MQTT -> ESP32s")
                # Wider table for Current
                print("+------------+---------+----------+-----------------+----------+----------+----------+")
                print("| Register   |  Value  |  Status  |   IP Address    |  Power   | Load 1   | Load 2   |")
                print("+------------+---------+----------+-----------------+----------+----------+----------+")
                
                for i in range(REGISTERS_TO_BRIDGE):
                    addr = 40001 + i
                    val = current_values[i]
                    
                    # Process Online/Offline Status
                    raw_status = device_statuses.get(addr, "UNKNOWN")
                    if raw_status.startswith("ONLINE:"):
                        status_text = "ONLINE"
                        ip_text = raw_status.split(":")[1]
                    elif raw_status == "OFFLINE":
                        status_text = "OFFLINE"
                        ip_text = "---"
                    else:
                        status_text = raw_status
                        ip_text = "---"

                    power_text = device_power_states.get(addr, "---")
                    c1_text = device_current1.get(addr, "---")
                    c2_text = device_current2.get(addr, "---")

                    print(f"| {addr:<10} | {val:>7} | {status_text:<8} | {ip_text:<15} | {power_text:<8} | {c1_text:<8} | {c2_text:<8} |")

                print("+------------+---------+----------+-----------------+----------+----------+----------+")
                print("\nMonitoring... Press Ctrl+C to stop.")

            time.sleep(1)
        except Exception as e:
            print(f"Error in bridge loop: {e}")
            break

if __name__ == '__main__':
    mqtt_client = None
    try:
        store = ModbusSlaveContext(hr=ModbusSequentialDataBlock(0, [0] * 100))
        context = ModbusServerContext(slaves=store, single=True)

        mqtt_client = mqtt.Client("ModbusBridgeClient")
        mqtt_client.on_connect = on_connect
        mqtt_client.on_message = on_message
        mqtt_client.connect(MQTT_BROKER_HOST, MQTT_PORT, 60)
        mqtt_client.loop_start()

        perform_startup_cleanup(mqtt_client)

        modbus_thread = Thread(target=StartTcpServer, kwargs={'context': context, 'address': ("", MODBUS_PORT)}, daemon=True)
        modbus_thread.start()

        bridge_and_display_loop(context, mqtt_client)
        
    except KeyboardInterrupt:
        print("\nScript stopped by user.")
    finally:
        print("Shutting down...")
        if mqtt_client:
            mqtt_client.loop_stop()
