import tkinter as tk
from tkinter import font
from pymodbus.client import ModbusTcpClient
import paho.mqtt.client as mqtt
import time

# --- CONFIGURATION ---------------------------------------------------------

# 1. MODBUS SETTINGS (PLC Connection)
MODBUS_IP = '192.168.1.10' # Localhost (Since Pi is the Server)
MODBUS_PORT = 502       # UPDATED: Standard Modbus Port

# 2. REGISTER SETTINGS
# Note: Standard Modbus maps Register 40001 to Address 0
REG_ACTUAL_VAL = 0      # Target: Holding Register 40001 (Output State)
REG_HMI_CONTROL = 1000  # Target: Holding Register 41001 (HMI Zone)
REG_FLAG_MUX = 3000     # Target: Holding Register 43001 (0=SCADA, 1=HMI)

# 3. MQTT SETTINGS (ESP32 Connection) - NOT USED DIRECTLY ANYMORE
MQTT_BROKER = '127.0.0.1'
MQTT_PORT = 1883
# Matches your ESP32 Code: "metro/signage/register/40001/value"
MQTT_TOPIC = "metro/signage/register/40001/value" 

# ---------------------------------------------------------------------------

# --- CLIENT SETUP ---
print("--- INITIALIZING HMI ---")

# Setup Modbus Client
mb_client = ModbusTcpClient(MODBUS_IP, port=MODBUS_PORT)
try:
    mb_client.connect()
    print(f"✓ Modbus Connected to Port {MODBUS_PORT}")
except Exception as e:
    print(f"✗ Modbus Failed: {e}")

# Setup MQTT Client
mqtt_client = mqtt.Client()
try:
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()
    print("✓ MQTT Connected")
except Exception as e:
    print(f"✗ MQTT Failed: {e}")

# --- GUI APPLICATION ---
class IndustrialHMI:
    def __init__(self, root):
        self.root = root
        self.root.title("Metro Signage Control")
        # Use fullscreen for the 7-inch DSI display
        self.root.attributes('-fullscreen', True) 
        self.root.configure(bg="#2c3e50")

        # Internal State
        self.manual_mode = False
        self.r1_state = False # False = OFF
        self.r2_state = False # False = OFF
        self.last_plc_val = -1
        
        # Font Styles
        self.font_header = font.Font(family="Helvetica", size=16, weight="bold")
        self.font_btn = font.Font(family="Helvetica", size=14, weight="bold")

        # --- UI LAYOUT ---
        
        # 1. Header Section
        header_frame = tk.Frame(root, bg="#34495e", height=50)
        header_frame.pack(fill='x')
        tk.Label(header_frame, text="UNIT 40001 CONTROL", font=self.font_header, 
                 bg="#34495e", fg="white").pack(pady=10)

        # 2. Mode Switch (Auto/Manual)
        self.mode_btn = tk.Button(root, text="MODE: AUTO (PLC)", bg="#27ae60", fg="white",
                                  font=self.font_btn, height=2, command=self.toggle_mode)
        self.mode_btn.pack(fill='x', padx=20, pady=20)

        # 3. Relay Controls
        ctrl_frame = tk.Frame(root, bg="#2c3e50")
        ctrl_frame.pack(expand=True, fill='both', padx=20)

        # Relay 1 Button
        self.btn_r1 = tk.Button(ctrl_frame, text="RELAY 1: OFF", bg="gray", fg="white",
                                font=self.font_btn, state="disabled", command=lambda: self.toggle_relay(1))
        self.btn_r1.pack(side='left', expand=True, fill='both', padx=5, pady=10)

        # Relay 2 Button
        self.btn_r2 = tk.Button(ctrl_frame, text="RELAY 2: OFF", bg="gray", fg="white",
                                font=self.font_btn, state="disabled", command=lambda: self.toggle_relay(2))
        self.btn_r2.pack(side='right', expand=True, fill='both', padx=5, pady=10)

        # 4. Exit Button (Optional, good for testing)
        tk.Button(root, text="EXIT APP", command=root.quit, bg="#c0392b", fg="white").pack(fill='x')

        # Start the Background Loop
        self.root.after(500, self.sync_loop)

    def toggle_mode(self):
        """Switches between Auto (SCADA) and Manual (HMI)"""
        self.manual_mode = not self.manual_mode
        
        if self.manual_mode:
            # MANUAL MODE ACTIVE
            self.mode_btn.config(text="MODE: MANUAL OVERRIDE", bg="#e67e22")
            self.btn_r1.config(state="normal", bg="#c0392b")
            self.btn_r2.config(state="normal", bg="#c0392b")
            
            # Notify Pi MUX via Modbus (Address 3000 -> Register 43001)
            # Writing 1 tells the MUX "Copy HMI Zone to Output Zone"
            mb_client.write_register(REG_FLAG_MUX, 1) 
            
        else:
            # AUTO MODE ACTIVE
            self.mode_btn.config(text="MODE: AUTO (SCADA)", bg="#27ae60")
            self.btn_r1.config(state="disabled", bg="gray")
            self.btn_r2.config(state="disabled", bg="gray")
            
            # Notify Pi MUX via Modbus (Writing 0 -> SCADA Control)
            mb_client.write_register(REG_FLAG_MUX, 0)

    def toggle_relay(self, relay_num):
        """Manual toggle logic - only works in Manual Mode"""
        if relay_num == 1:
            self.r1_state = not self.r1_state
        elif relay_num == 2:
            self.r2_state = not self.r2_state
        
        self.update_buttons() # Update colors
        self.send_command()   # Send to MQTT & Modbus

    def update_buttons(self):
        """Updates button colors based on state"""
        # Relay 1
        if self.r1_state:
            self.btn_r1.config(text="RELAY 1: ON", bg="#2ecc71") # Green
        else:
            self.btn_r1.config(text="RELAY 1: OFF", bg="#c0392b") # Red

        # Relay 2
        if self.r2_state:
            self.btn_r2.config(text="RELAY 2: ON", bg="#2ecc71") # Green
        else:
            self.btn_r2.config(text="RELAY 2: OFF", bg="#c0392b") # Red

    def send_command(self):
        """Calculates 0-3 payload and sends to HMI Modbus Zone"""
        # Calculate Logic: 0=Off/Off, 1=On/Off, 2=Off/On, 3=On/On
        command_val = 0
        if self.r1_state and not self.r2_state: command_val = 1
        if not self.r1_state and self.r2_state: command_val = 2
        if self.r1_state and self.r2_state: command_val = 3
        
        # Update Modbus Register 41001 (HMI Zone)
        # The Pi MUX will automatically route this to 40001 and publish to MQTT
        mb_client.write_register(REG_HMI_CONTROL, command_val)
        print(f"SENT MODBUS: {command_val} to HMI Zone (41001)")

    def sync_loop(self):
        """The Heartbeat: Reads from Output Zone to update UI"""
        if not self.manual_mode:
            # --- AUTO MODE: LISTEN TO ACTUAL OUTPUT ---
            try:
                # Read Register 40001 (Address 0)
                rr = mb_client.read_holding_registers(REG_ACTUAL_VAL, 1)
                
                if not rr.isError():
                    plc_val = rr.registers[0]
                    
                    # Only act if the output has changed
                    if plc_val != self.last_plc_val:
                        print(f"OUTPUT UPDATE RECEIVED: {plc_val}")
                        
                        # Update Local Variables
                        self.r1_state = (plc_val == 1 or plc_val == 3)
                        self.r2_state = (plc_val == 2 or plc_val == 3)
                        
                        self.last_plc_val = plc_val
                        
                        # Update Visuals
                        self.update_buttons()
            except Exception as e:
                print(f"Sync Error: {e}")

        # Run this function again in 200ms
        self.root.after(200, self.sync_loop)

# --- MAIN ENTRY POINT ---
if __name__ == "__main__":
    root = tk.Tk()
    app = IndustrialHMI(root)
    root.mainloop()
