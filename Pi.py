import paho.mqtt.client as mqtt
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from threading import Thread, Lock, Event
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

# Thread safety lock for the dictionaries
data_lock = Lock()
# Event to track MQTT connection status
mqtt_connected_event = Event()

def on_message(client, userdata, msg):
    """Callback for incoming MQTT messages"""
    try:
        topic_parts = msg.topic.split('/')
        if len(topic_parts) == 5:
            register_address = int(topic_parts[3])
            msg_type = topic_parts[4]
            payload = msg.payload.decode()

            with data_lock:
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
        print(f"Error processing message on topic '{msg.topic}', payload '{msg.payload}': {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to Broker. Subscribing...")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/status")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/power")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/current1")
        client.subscribe(f"{MQTT_TOPIC_PREFIX}/+/current2")
        mqtt_connected_event.set()  # Signal that we are successfully connected
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
        
        with data_lock:
            device_statuses[addr] = "OFFLINE"
            device_power_states[addr] = "---"
            device_current1[addr] = "---"
            device_current2[addr] = "---"
        
    print("Reset complete. Sending Broadcast PING...")
    client.publish("metro/signage/scan", "PING", retain=False)
    time.sleep(2)

def bridge_and_display_loop(modbus_context, mqtt_client):
    last_known_values = [None] * REGISTERS_TO_BRIDGE

    # --- ADDED: Clear the screen entirely just ONCE before the loop starts ---
    if sys.stdout.isatty():
        sys.stdout.write('\033[2J')

    while True:
        try:
            current_values = modbus_context[0].getValues(3, 0, count=REGISTERS_TO_BRIDGE)

            for i in range(REGISTERS_TO_BRIDGE):
                if current_values[i] != last_known_values[i]:
                    addr = 40001 + i
                    topic = f"{MQTT_TOPIC_PREFIX}/{addr}/value"
                    payload = str(current_values[i])
                    # Added retain=True to hold state for offline ESP32s
                    mqtt_client.publish(topic, payload, retain=True)
                    last_known_values[i] = current_values[i]
            
            # --- Updated Table Display ---
            if sys.stdout.isatty():
                # --- CHANGED: Move cursor to top-left instead of clearing the screen ---
                sys.stdout.write('\033[H')
                
                print("--- Metro Signage Modbus-MQTT Bridge ---")
                print(f"PLC -> Modbus -> MQTT -> ESP32s")
                # Wider table for Current
                print("+------------+---------+----------+-----------------+----------+----------+----------+")
                print("| Register   |  Value  |  Status  |   IP Address    |  Power   | Load 1   | Load 2   |")
                print("+------------+---------+----------+-----------------+----------+----------+----------+")
                
                for i in range(REGISTERS_TO_BRIDGE):
                    try:
                        addr = 40001 + i
                        val = current_values[i]
                        
                        # Process Online/Offline Status safely using the lock
                        with data_lock:
                            raw_status = device_statuses.get(addr, "UNKNOWN")
                            power_text = device_power_states.get(addr, "---")
                            c1_text = device_current1.get(addr, "---")
                            c2_text = device_current2.get(addr, "---")

                        if raw_status.startswith("ONLINE:"):
                            status_text = "ONLINE"
                            # Added maxsplit=1 to safely handle IPv6 addresses or appended ports
                            parts = raw_status.split(":", 1)
                            ip_text = parts[1] if len(parts) > 1 else "---"
                        elif raw_status == "OFFLINE":
                            status_text = "OFFLINE"
                            ip_text = "---"
                        else:
                            status_text = raw_status
                            ip_text = "---"

                        print(f"| {addr:<10} | {val:>7} | {status_text:<8} | {ip_text:<15} | {power_text:<8} | {c1_text:<8} | {c2_text:<8} |")
                    
                    except Exception as row_e:
                        # If a specific row fails to parse, show an error state for that row instead of breaking the loop
                        print(f"| {addr:<10} |   ERR   | ERROR    | ERROR           | ERROR    | ERROR    | ERROR    |")

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

        # Wait for the connection to fully establish before publishing
        print("Waiting for MQTT connection to establish...")
        if mqtt_connected_event.wait(timeout=10):
            perform_startup_cleanup(mqtt_client)
        else:
            print("WARNING: MQTT connection timeout! Skipping startup cleanup.")

        modbus_thread = Thread(target=StartTcpServer, kwargs={'context': context, 'address': ("", MODBUS_PORT)}, daemon=True)
        modbus_thread.start()

        bridge_and_display_loop(context, mqtt_client)
        
    except KeyboardInterrupt:
        print("\nScript stopped by user.")
    finally:
        print("Shutting down...")
        if mqtt_client:
            mqtt_client.loop_stop()
