from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from threading import Thread
import time
import paho.mqtt.client as mqtt
import os


# --- Configuration ---
# Modbus Configuration
MODBUS_PORT = 502


# MQTT Configuration
MQTT_BROKER_HOST = "localhost"  # The broker is running on this same Raspberry Pi
MQTT_PORT = 1883
MQTT_TOPIC_PREFIX = "metro/signage/register" # Base topic for publishing


# How many registers to monitor and bridge to MQTT
REGISTERS_TO_BRIDGE = 10


def bridge_and_display_loop(modbus_context, mqtt_client):
    """
    Continuously checks Modbus register states, publishes changes to MQTT topics,
    and displays the current values in a table.
    """
    print("Starting Modbus-to-MQTT bridge and display loop.")
    # Initialize a list to store the last known values to detect changes
    last_known_values = [None] * REGISTERS_TO_BRIDGE


    while True:
        try:
            # --- Bridge Logic ---
            # Read the current values from the Modbus datastore's holding registers
            current_values = modbus_context[0].getValues(3, 0, count=REGISTERS_TO_BRIDGE)


            # Compare current values with the last known values
            for i in range(REGISTERS_TO_BRIDGE):
                if current_values[i] != last_known_values[i]:
                    # A value has changed, so we publish it to the correct MQTT topic
                    register_address = 40001 + i
                    topic = f"{MQTT_TOPIC_PREFIX}/{register_address}/value"
                    payload = str(current_values[i])
                   
                    print(f"\n--- CHANGE DETECTED on Register {register_address} ---")
                    print(f"Publishing '{payload}' to topic '{topic}'")
                    print("------------------------------------------")
                    mqtt_client.publish(topic, payload)
                   
                    # Update the last known value for this register
                    last_known_values[i] = current_values[i]
           
            # --- Display Logic ---
            os.system('cls' if os.name == 'nt' else 'clear')
            print("--- Modbus-MQTT Bridge Active ---")
            print(f"PLC -> Modbus (Port {MODBUS_PORT}) -> MQTT (Broker on {MQTT_BROKER_HOST}) -> ESP32")
            print("+------------+---------+------------+---------+")
            print("| Register   |  Value  | Register   |  Value  |")
            print("+------------+---------+------------+---------+")
            rows = REGISTERS_TO_BRIDGE // 2
            for i in range(rows):
                addr1 = 40001 + i
                val1 = current_values[i]
                addr2 = 40001 + i + rows
                val2 = current_values[i + rows]
                print(f"| {addr1:<10} | {val1:>7} | {addr2:<10} | {val2:>7} |")
            print("+------------+---------+------------+---------+")
            print("\nMonitoring for changes... Press Ctrl+C to stop.")


            time.sleep(1) # Poll for changes and refresh display every second
        except Exception as e:
            print(f"Error in bridge loop: {e}")
            break


# --- Main Execution ---
if __name__ == '__main__':
    try:
        # 1. Setup Modbus Datastore
        store = ModbusSlaveContext(
            hr=ModbusSequentialDataBlock(0, [0] * 100) # Initialize 100 registers to 0
        )
        context = ModbusServerContext(slaves=store, single=True)


        # 2. Setup and connect the MQTT Client
        mqtt_client = mqtt.Client("ModbusBridgeClient")
        mqtt_client.connect(MQTT_BROKER_HOST, MQTT_PORT, 60)
        mqtt_client.loop_start()
        print("Connected to MQTT Broker.")


        # 3. Start the Modbus TCP server in a separate thread
        #insert the modbus server ip address here, that is the ip address of the raspberry pi
        modbus_thread = Thread(target=StartTcpServer, kwargs={'context': context, 'address': ("", MODBUS_PORT)}, daemon=True)
        modbus_thread.start()
        print(f"Modbus TCP server started on port {MODBUS_PORT}.")


        # 4. Start the main bridge and display loop
        bridge_and_display_loop(context, mqtt_client)
       
    except KeyboardInterrupt:
        print("\nScript stopped by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        print("Shutting down...")
        mqtt_client.loop_stop()



