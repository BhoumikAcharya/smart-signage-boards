import tkinter as tk
from tkinter import font

# --- DESIGN SYSTEM VARIABLES ---
COLORS = {
    "bg_lowest": "#0e0e0e",
    "bg_main": "#131313",
    "bg_panel": "#1c1b1b",
    "bg_hover": "#2a2a2a",
    "text_main": "#e5e2e1",
    "text_dim": "#bac9cc",
    "primary": "#00daf3",      # Cyan
    "alert": "#ff8a00",        # Orange
    "success": "#6cec00",      # Lime
    "error": "#ffb4ab",        # Red
    "border": "#353534"
}

# Mock Data for UI testing
MOCK_NODES = []
for i in range(1, 101):
    status = "ONLINE" if i % 5 != 0 else "OFFLINE"
    status_color = COLORS["success"] if status == "ONLINE" else COLORS["error"]
    if i % 7 == 0: 
        status = "DEGRADED"
        status_color = COLORS["alert"]
        
    MOCK_NODES.append({
        "id": f"ND-{8090+i}",
        "ip": f"192.168.10.{100+i}",
        "status": status,
        "color": status_color
    })

class VirtualKeyboard(tk.Toplevel):
    def __init__(self, parent, target_var):
        super().__init__(parent)
        self.title("Virtual Keyboard")
        
        # Sized for 1024x600 display - centered lower on the screen
        self.geometry("900x380+62+180")
        self.configure(bg=COLORS["bg_lowest"], highlightbackground=COLORS["border"], highlightthickness=2)
        self.target_var = target_var
        
        # Remove OS window decorations for a seamless kiosk feel
        self.overrideredirect(True)
        
        # Build the UI widgets FIRST
        self.setup_ui()
        
        # Make the keyboard modal
        self.transient(parent)
        
        # CRITICAL FIX for Raspberry Pi Linux Window Managers:
        # Wait until the window is actually drawn on the screen before trying to grab it.
        self.update_idletasks()
        self.wait_visibility()
        self.grab_set()
        
    def setup_ui(self):
        # Keyboard Layout
        keys = [
            ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'],
            ['Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'],
            ['A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '-'],
            ['Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', 'CLR', 'DEL'],
            ['SPACE', 'CLOSE']
        ]
        
        key_frame = tk.Frame(self, bg=COLORS["bg_lowest"])
        key_frame.pack(expand=True, fill="both", padx=10, pady=10)
        
        btn_font = font.Font(family="Helvetica", size=16, weight="bold")
        
        for r, row in enumerate(keys):
            row_frame = tk.Frame(key_frame, bg=COLORS["bg_lowest"])
            row_frame.pack(fill="x", pady=4)
            
            for key in row:
                # Custom styling based on key type
                bg_col = COLORS["bg_panel"]
                fg_col = COLORS["text_main"]
                btn_width = 4
                
                if key in ['CLR', 'DEL']:
                    fg_col = COLORS["alert"]
                elif key == 'CLOSE':
                    bg_col = COLORS["primary"]
                    fg_col = COLORS["bg_lowest"]
                    btn_width = 15
                elif key == 'SPACE':
                    btn_width = 30
                    
                btn = tk.Button(row_frame, text=key, width=btn_width, height=2, 
                                font=btn_font, bg=bg_col, fg=fg_col, relief="flat",
                                activebackground=COLORS["bg_hover"],
                                command=lambda k=key: self.press(k))
                btn.pack(side="left", padx=4, expand=True, fill="both")
                
    def press(self, key):
        current = self.target_var.get()
        if key == 'DEL':
            self.target_var.set(current[:-1])
        elif key == 'CLR':
            self.target_var.set("")
        elif key == 'CLOSE':
            self.destroy()
        elif key == 'SPACE':
            self.target_var.set(current + " ")
        else:
            self.target_var.set(current + key)

class HMIApp(tk.Tk):
    def __init__(self):
        super().__init__()
        
        # Window Setup
        self.title("Metro Signage SCADA HMI")
        self.geometry("1024x600")
        self.configure(bg=COLORS["bg_main"])
        # self.attributes('-fullscreen', True) # Uncomment for Pi Deployment
        
        # Fonts
        self.font_h2 = font.Font(family="Helvetica", size=20, weight="bold")
        self.font_body = font.Font(family="Helvetica", size=14)
        self.font_mono = font.Font(family="Courier", size=14, weight="bold")
        self.font_large = font.Font(family="Helvetica", size=36, weight="bold")
        
        # Container to hold all frames
        self.container = tk.Frame(self, bg=COLORS["bg_main"])
        self.container.pack(fill="both", expand=True)
        
        # Dictionary to store references to our pages
        self.frames = {}
        
        # Initialize the Frames
        self.frames["Dashboard"] = DashboardFrame(parent=self.container, controller=self)
        self.frames["NodeDetail"] = NodeDetailFrame(parent=self.container, controller=self)
        self.frames["Diagnostics"] = DiagnosticsFrame(parent=self.container, controller=self)
        
        for frame in self.frames.values():
            frame.place(x=0, y=0, relwidth=1, relheight=1)
        
        # Start on Dashboard
        self.show_frame("Dashboard")

    def show_frame(self, page_name, context=None):
        """Bring a specific frame to the front."""
        frame = self.frames[page_name]
        if page_name == "NodeDetail" and context:
            frame.load_node_data(context) # Pass the specific node data
        elif page_name == "Diagnostics":
            frame.refresh_diagnostics() # Ensure data is fresh when opening
        frame.tkraise()


class DashboardFrame(tk.Frame):
    def __init__(self, parent, controller):
        super().__init__(parent, bg=COLORS["bg_main"])
        self.controller = controller
        self.current_page = 0
        self.nodes_per_page = 8  # Changed from 10 to 8 per user request
        self.filtered_nodes = MOCK_NODES.copy()
        
        self.setup_ui()
        self.render_page()

    def setup_ui(self):
        # --- TOP BAR ---
        top_bar = tk.Frame(self, bg=COLORS["bg_lowest"], height=64)
        top_bar.pack(fill="x", side="top")
        top_bar.pack_propagate(False)
        
        tk.Label(top_bar, text="DASHBOARD", font=self.controller.font_h2, 
                 bg=COLORS["bg_lowest"], fg=COLORS["primary"]).pack(side="left", padx=24)
        
        # Gear icon for settings/diagnostics
        lbl_diag = tk.Label(top_bar, text="⚙", font=("Helvetica", 20), 
                            bg=COLORS["bg_lowest"], fg=COLORS["text_dim"], cursor="hand2")
        lbl_diag.pack(side="right", padx=24)
        lbl_diag.bind("<Button-1>", lambda e: self.controller.show_frame("Diagnostics"))
        
        # Search Box (Using Label to prevent OS Keyboard focus)
        self.search_var = tk.StringVar(value="")
        self.search_var.trace("w", self.on_search)
        
        self.display_var = tk.StringVar(value="Search Node or IP...")
        
        self.search_btn = tk.Label(top_bar, textvariable=self.display_var, font=self.controller.font_body,
                                   bg=COLORS["bg_panel"], fg=COLORS["text_dim"], 
                                   highlightbackground=COLORS["border"], highlightthickness=1,
                                   width=25, anchor="w", padx=10)
        self.search_btn.pack(side="right", padx=10, pady=12, fill="y")
        self.search_btn.bind("<Button-1>", self.open_keyboard)
        
        # --- TABLE HEADER ---
        header_frame = tk.Frame(self, bg=COLORS["bg_panel"], height=40)
        header_frame.pack(fill="x", padx=24, pady=(24, 0))
        header_frame.pack_propagate(False)
        
        tk.Label(header_frame, text="NODE NUMBER", font=self.controller.font_body, bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).place(x=20, y=10)
        tk.Label(header_frame, text="IP ADDRESS", font=self.controller.font_body, bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).place(x=300, y=10)
        tk.Label(header_frame, text="CONNECTION STATUS", font=self.controller.font_body, bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).place(x=700, y=10)

        # --- ROWS CONTAINER ---
        self.rows_container = tk.Frame(self, bg=COLORS["bg_main"])
        self.rows_container.pack(fill="both", expand=True, padx=24, pady=8)
        
        # --- BOTTOM BAR (PAGINATION) ---
        bottom_bar = tk.Frame(self, bg=COLORS["bg_main"], height=90) # Increased height to ensure buttons fit comfortably
        bottom_bar.pack(fill="x", side="bottom")
        bottom_bar.pack_propagate(False)
        
        # Paginator text
        self.lbl_page_info = tk.Label(bottom_bar, text="1 - 8 / 100", font=self.controller.font_mono, bg=COLORS["bg_main"], fg=COLORS["text_dim"])
        self.lbl_page_info.pack(side="right", padx=24)
        
        # Pagination Buttons
        self.btn_next = tk.Button(bottom_bar, text="NEXT PAGE ➔", font=self.controller.font_body,
                                  bg=COLORS["bg_panel"], fg=COLORS["text_main"], relief="flat",
                                  activebackground=COLORS["bg_hover"], activeforeground=COLORS["primary"],
                                  width=14, height=2,
                                  command=self.next_page)
        self.btn_next.pack(side="left", padx=(24, 10), pady=10)

        self.btn_prev = tk.Button(bottom_bar, text="🡨 PREV PAGE", font=self.controller.font_body,
                                  bg=COLORS["bg_panel"], fg=COLORS["text_main"], relief="flat",
                                  activebackground=COLORS["bg_hover"], activeforeground=COLORS["primary"],
                                  width=14, height=2,
                                  command=self.prev_page, state="disabled")
        self.btn_prev.pack(side="left", padx=10, pady=10)

    def open_keyboard(self, event):
        # Open the custom touch keyboard
        VirtualKeyboard(self.controller, self.search_var)

    def on_search(self, *args):
        query = self.search_var.get()
        
        # Update placeholder visuals
        if query == "":
            self.display_var.set("Search Node or IP...")
            self.search_btn.config(fg=COLORS["text_dim"])
        else:
            self.display_var.set(query)
            self.search_btn.config(fg=COLORS["primary"])
            
        # Filter logic
        query_lower = query.lower()
        self.filtered_nodes = [n for n in MOCK_NODES if query_lower in n['id'].lower() or query_lower in n['ip'].lower()]
        self.current_page = 0
        self.render_page()

    def next_page(self):
        max_page = (len(self.filtered_nodes) - 1) // self.nodes_per_page
        if self.current_page < max_page:
            self.current_page += 1
            self.render_page()

    def prev_page(self):
        if self.current_page > 0:
            self.current_page -= 1
            self.render_page()

    def render_page(self):
        # Clear existing rows
        for widget in self.rows_container.winfo_children():
            widget.destroy()
            
        start_idx = self.current_page * self.nodes_per_page
        end_idx = min(start_idx + self.nodes_per_page, len(self.filtered_nodes))
        current_view = self.filtered_nodes[start_idx:end_idx]
        
        # Update Pagination Info
        total = len(self.filtered_nodes)
        if total == 0:
            self.lbl_page_info.config(text="0 - 0 / 0")
        else:
            self.lbl_page_info.config(text=f"{start_idx + 1} - {end_idx} / {total}")
        
        # Update Button States
        self.btn_prev.config(state="normal" if self.current_page > 0 else "disabled")
        self.btn_next.config(state="normal" if end_idx < total else "disabled")
        
        # Draw Rows
        for i, node in enumerate(current_view):
            # Alternating background color for readability
            bg_color = COLORS["bg_main"] if i % 2 == 0 else COLORS["bg_lowest"]
            
            row = tk.Frame(self.rows_container, bg=bg_color, height=48)
            row.pack(fill="x", pady=1)
            row.pack_propagate(False)
            
            # Row bindings for touch/click
            row.bind("<Button-1>", lambda e, n=node: self.controller.show_frame("NodeDetail", n))
            
            lbl_id = tk.Label(row, text=node["id"], font=self.controller.font_mono, bg=bg_color, fg=COLORS["text_main"])
            lbl_id.place(x=20, y=10)
            lbl_id.bind("<Button-1>", lambda e, n=node: self.controller.show_frame("NodeDetail", n))
            
            lbl_ip = tk.Label(row, text=node["ip"], font=self.controller.font_mono, bg=bg_color, fg=COLORS["primary"])
            lbl_ip.place(x=300, y=10)
            lbl_ip.bind("<Button-1>", lambda e, n=node: self.controller.show_frame("NodeDetail", n))
            
            # Fake LED Canvas
            led = tk.Canvas(row, width=16, height=16, bg=bg_color, highlightthickness=0)
            led.create_oval(2, 2, 14, 14, fill=node["color"], outline="")
            led.place(x=700, y=16)
            led.bind("<Button-1>", lambda e, n=node: self.controller.show_frame("NodeDetail", n))
            
            lbl_status = tk.Label(row, text=node["status"], font=self.controller.font_mono, bg=bg_color, fg=node["color"])
            lbl_status.place(x=725, y=10)
            lbl_status.bind("<Button-1>", lambda e, n=node: self.controller.show_frame("NodeDetail", n))


class NodeDetailFrame(tk.Frame):
    def __init__(self, parent, controller):
        super().__init__(parent, bg=COLORS["bg_main"])
        self.controller = controller
        
        self.mux_manual = False
        self.r1_on = False
        self.r2_on = False
        self.current_node_id = "ND-XXXX"
        
        self.setup_ui()

    def setup_ui(self):
        # --- HEADER ---
        self.header_var = tk.StringVar(value="ND-XXXX")
        top_bar = tk.Frame(self, bg=COLORS["bg_lowest"], height=64)
        top_bar.pack(fill="x", side="top")
        top_bar.pack_propagate(False)
        
        # Back button (Minimalist text link matching image)
        lbl_back = tk.Label(top_bar, text="← BACK", font=("Helvetica", 12, "bold"),
                            bg=COLORS["bg_lowest"], fg=COLORS["primary"], cursor="hand2")
        lbl_back.pack(side="left", padx=24)
        lbl_back.bind("<Button-1>", lambda e: self.controller.show_frame("Dashboard"))
        
        # Center Title
        tk.Label(top_bar, textvariable=self.header_var, font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_lowest"], fg=COLORS["primary"]).pack(side="left", expand=True)
                 
        # Diagnostics
        lbl_diag = tk.Label(top_bar, text="diagnostics ⚙", font=("Helvetica", 12, "bold"),
                            bg=COLORS["bg_lowest"], fg=COLORS["primary"], cursor="hand2")
        lbl_diag.pack(side="right", padx=24)
        lbl_diag.bind("<Button-1>", lambda e: self.controller.show_frame("Diagnostics"))

        # --- CONTENT AREA ---
        content_frame = tk.Frame(self, bg=COLORS["bg_main"])
        content_frame.pack(fill="both", expand=True, padx=32, pady=32)
        
        # LEFT COLUMN: TELEMETRY (Borderless to match image)
        self.left_col = tk.Frame(content_frame, bg=COLORS["bg_main"])
        self.left_col.pack(side="left", fill="both", expand=True, padx=(0, 20))
        
        # RIGHT COLUMN: OVERRIDE CONTROLS (Borderless to match image)
        self.right_col = tk.Frame(content_frame, bg=COLORS["bg_main"])
        self.right_col.pack(side="right", fill="both", expand=True, padx=(20, 0))

        self.setup_telemetry_panel()
        self.setup_control_panel()

    def setup_telemetry_panel(self):
        # Section Title
        tk.Label(self.left_col, text="❖ HARDWARE TELEMETRY", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w", pady=(0, 16))
        
        # IP Address Row (Custom row without LED)
        ip_row = tk.Frame(self.left_col, bg=COLORS["bg_panel"], height=52)
        ip_row.pack(fill="x", pady=4)
        ip_row.pack_propagate(False)
        tk.Label(ip_row, text="IP Address", font=("Helvetica", 12), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_main"]).pack(side="left", padx=16)
        self.lbl_ip = tk.Label(ip_row, text="192.168.10.X", font=("Helvetica", 12, "bold"), 
                               bg=COLORS["bg_panel"], fg=COLORS["primary"])
        self.lbl_ip.pack(side="right", padx=16)
        
        # Status Rows
        self.lbl_conn, self.led_conn = self.create_status_row(self.left_col, "Connection Status", "ONLINE", COLORS["success"])
        self.lbl_pwr, self.led_pwr = self.create_status_row(self.left_col, "Main Power Supply", "OK", COLORS["success"])
        self.lbl_l1, self.led_l1 = self.create_status_row(self.left_col, "Load 1 Current Sensor", "OK", COLORS["success"])
        self.lbl_l2, self.led_l2 = self.create_status_row(self.left_col, "Load 2 Current Sensor", "OK", COLORS["success"])
        
        # Bottom Battery Area
        batt_container = tk.Frame(self.left_col, bg=COLORS["bg_main"])
        batt_container.pack(side="bottom", fill="x", pady=10)
        
        # Battery Left Stack
        batt_left = tk.Frame(batt_container, bg=COLORS["bg_main"])
        batt_left.pack(side="left")
        
        tk.Label(batt_left, text="BACKUP BATTERY:", font=("Helvetica", 10, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w")
                 
        val_frame = tk.Frame(batt_left, bg=COLORS["bg_main"])
        val_frame.pack(anchor="w", pady=4)
        
        self.batt_icon = tk.Canvas(val_frame, width=18, height=28, bg=COLORS["bg_main"], highlightthickness=0)
        self.batt_icon.pack(side="left", padx=(0, 8))
        self.draw_battery_icon(100, COLORS["success"])
        
        self.lbl_batt = tk.Label(val_frame, text="100%", font=("Helvetica", 28, "bold"), 
                                 bg=COLORS["bg_main"], fg=COLORS["success"])
        self.lbl_batt.pack(side="left")

    def draw_battery_icon(self, pct, color):
        self.batt_icon.delete("all")
        # Terminal
        self.batt_icon.create_rectangle(5, 0, 13, 3, fill=COLORS["text_dim"], outline="")
        # Body outline
        self.batt_icon.create_rectangle(0, 3, 18, 28, fill="", outline=COLORS["text_dim"], width=2)
        # Dynamic fill
        fill_h = max(1, int(23 * (pct / 100.0)))
        self.batt_icon.create_rectangle(2, 26 - fill_h, 16, 26, fill=color, outline="")

    def create_status_row(self, parent, label_text, status_text, color):
        row = tk.Frame(parent, bg=COLORS["bg_panel"], height=52)
        row.pack(fill="x", pady=4)
        row.pack_propagate(False)
        
        tk.Label(row, text=label_text, font=("Helvetica", 12), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_main"]).pack(side="left", padx=16)
                 
        led = tk.Canvas(row, width=12, height=12, bg=COLORS["bg_panel"], highlightthickness=0)
        led.create_oval(1, 1, 11, 11, fill=color, outline="")
        led.pack(side="right", padx=(10, 16), pady=20)
        
        lbl = tk.Label(row, text=status_text, font=("Helvetica", 10, "bold"), 
                       bg=COLORS["bg_panel"], fg=color)
        lbl.pack(side="right")
        
        return lbl, led

    def setup_control_panel(self):
        # Section Title
        tk.Label(self.right_col, text="≢ MANUAL OVERRIDE CONTROL", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w", pady=(0, 16))
        
        # MUX Mode Box
        mode_frame = tk.Frame(self.right_col, bg=COLORS["bg_panel"], height=80)
        mode_frame.pack(fill="x", pady=(0, 16))
        mode_frame.pack_propagate(False)
        
        mode_text_frame = tk.Frame(mode_frame, bg=COLORS["bg_panel"])
        mode_text_frame.pack(side="left", padx=20, pady=16, fill="y")
        
        tk.Label(mode_text_frame, text="CURRENT MODE", font=("Helvetica", 10, "bold"), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).pack(anchor="w")
        self.lbl_mode_val = tk.Label(mode_text_frame, text="SCADA (AUTO)", font=("Helvetica", 16, "bold"), 
                                     bg=COLORS["bg_panel"], fg=COLORS["primary"])
        self.lbl_mode_val.pack(anchor="w")
        
        # Custom Canvas Toggle Switch
        self.toggle_canvas = tk.Canvas(mode_frame, width=64, height=32, bg=COLORS["bg_panel"], highlightthickness=0)
        self.toggle_canvas.pack(side="right", padx=20, pady=24)
        self.toggle_canvas.bind("<Button-1>", self.toggle_mux)
        self.draw_toggle(False)
        
        # Relay 1 Custom Button
        self.r1_frame = tk.Frame(self.right_col, bg=COLORS["bg_main"], highlightbackground=COLORS["border"], highlightthickness=1, height=80)
        self.r1_frame.pack(fill="x", pady=8)
        self.r1_frame.pack_propagate(False)
        self.r1_lbl = tk.Label(self.r1_frame, text="RELAY 1: OFF", font=("Helvetica", 14, "bold"), bg=COLORS["bg_main"], fg=COLORS["border"])
        self.r1_lbl.place(relx=0.5, rely=0.5, anchor="center")
        self.r1_frame.bind("<Button-1>", lambda e: self.toggle_relay(1))
        self.r1_lbl.bind("<Button-1>", lambda e: self.toggle_relay(1))
        
        # Relay 2 Custom Button
        self.r2_frame = tk.Frame(self.right_col, bg=COLORS["bg_main"], highlightbackground=COLORS["border"], highlightthickness=1, height=80)
        self.r2_frame.pack(fill="x", pady=8)
        self.r2_frame.pack_propagate(False)
        self.r2_lbl = tk.Label(self.r2_frame, text="RELAY 2: OFF", font=("Helvetica", 14, "bold"), bg=COLORS["bg_main"], fg=COLORS["border"])
        self.r2_lbl.place(relx=0.5, rely=0.5, anchor="center")
        self.r2_frame.bind("<Button-1>", lambda e: self.toggle_relay(2))
        self.r2_lbl.bind("<Button-1>", lambda e: self.toggle_relay(2))
        
        # Warning Box
        self.warn_frame = tk.Frame(self.right_col, bg="#361a00", highlightthickness=0)
        self.warn_frame.pack(side="bottom", fill="x", pady=(20, 0), ipady=12)
        
        warn_left = tk.Frame(self.warn_frame, bg=COLORS["alert"], width=4)
        warn_left.pack(side="left", fill="y")
        
        warn_text = "⚠️  Overrides are locked while in SCADA Auto mode. Engage local physical\nswitch to enable manual HMI control."
        tk.Label(self.warn_frame, text=warn_text, font=("Helvetica", 10), justify="left", 
                 bg="#361a00", fg=COLORS["text_dim"]).pack(side="left", padx=16)

    def draw_toggle(self, is_on):
        self.toggle_canvas.delete("all")
        w, h = 64, 32
        r = h / 2
        pad = 4
        
        if is_on:
            track_color = COLORS["alert"]
            knob_x = w - h + pad
            knob_color = COLORS["bg_main"]
        else:
            track_color = "#2a2a2a"
            knob_x = pad
            knob_color = COLORS["primary"]
            
        # Draw track pill
        self.toggle_canvas.create_oval(0, 0, h, h, fill=track_color, outline="")
        self.toggle_canvas.create_oval(w-h, 0, w, h, fill=track_color, outline="")
        self.toggle_canvas.create_rectangle(r, 0, w-r, h, fill=track_color, outline="")
        
        # Draw knob
        self.toggle_canvas.create_oval(knob_x, pad, knob_x+h-2*pad, h-pad, fill=knob_color, outline="")

    def toggle_mux(self, event=None):
        self.mux_manual = not self.mux_manual
        if self.mux_manual:
            self.lbl_mode_val.config(text="HMI (MANUAL)", fg=COLORS["alert"])
            self.draw_toggle(True)
            self.warn_frame.pack_forget() # Hide warning box
            self.update_relay_buttons()
        else:
            self.lbl_mode_val.config(text="SCADA (AUTO)", fg=COLORS["primary"])
            self.draw_toggle(False)
            self.warn_frame.pack(side="bottom", fill="x", pady=(20, 0), ipady=12) # Show warning box
            # Lock relays visually
            self.r1_frame.config(highlightbackground=COLORS["border"], bg=COLORS["bg_main"])
            self.r1_lbl.config(fg=COLORS["border"], bg=COLORS["bg_main"])
            self.r2_frame.config(highlightbackground=COLORS["border"], bg=COLORS["bg_main"])
            self.r2_lbl.config(fg=COLORS["border"], bg=COLORS["bg_main"])

    def toggle_relay(self, relay_num):
        if not self.mux_manual:
            return # Locked out

        if relay_num == 1:
            self.r1_on = not self.r1_on
        else:
            self.r2_on = not self.r2_on
        self.update_relay_buttons()

    def update_relay_buttons(self):
        if not self.mux_manual:
            return
            
        # Relay 1 logic
        if self.r1_on:
            self.r1_frame.config(bg=COLORS["primary"], highlightbackground=COLORS["primary"])
            self.r1_lbl.config(text="RELAY 1: ON", fg=COLORS["bg_lowest"], bg=COLORS["primary"])
        else:
            self.r1_frame.config(bg=COLORS["bg_panel"], highlightbackground=COLORS["border"])
            self.r1_lbl.config(text="RELAY 1: OFF", fg=COLORS["text_main"], bg=COLORS["bg_panel"])
            
        # Relay 2 logic
        if self.r2_on:
            self.r2_frame.config(bg=COLORS["primary"], highlightbackground=COLORS["primary"])
            self.r2_lbl.config(text="RELAY 2: ON", fg=COLORS["bg_lowest"], bg=COLORS["primary"])
        else:
            self.r2_frame.config(bg=COLORS["bg_panel"], highlightbackground=COLORS["border"])
            self.r2_lbl.config(text="RELAY 2: OFF", fg=COLORS["text_main"], bg=COLORS["bg_panel"])

    def load_node_data(self, node_data):
        self.current_node_id = node_data['id']
        self.header_var.set(self.current_node_id)
        
        # Update IP and Connection Status
        self.lbl_ip.config(text=node_data['ip'])
        self.lbl_conn.config(text=node_data['status'], fg=node_data['color'])
        self.led_conn.itemconfig(1, fill=node_data['color'])
        
        # Calculate Mock Battery state
        batt_pct = 100 if node_data['status'] == "ONLINE" else 0 if node_data['status'] == "OFFLINE" else 50
        batt_color = COLORS["success"] if batt_pct == 100 else COLORS["error"] if batt_pct == 0 else COLORS["alert"]
        
        self.lbl_batt.config(text=f"{batt_pct}%", fg=batt_color)
        self.draw_battery_icon(batt_pct, batt_color)
        
        # Enforce reset to SCADA (Auto) upon entering any new node view
        self.mux_manual = False
        self.r1_on = False
        self.r2_on = False
        self.lbl_mode_val.config(text="SCADA (AUTO)", fg=COLORS["primary"])
        self.draw_toggle(False)
        self.warn_frame.pack(side="bottom", fill="x", pady=(20, 0), ipady=12)
        
        self.r1_frame.config(highlightbackground=COLORS["border"], bg=COLORS["bg_main"])
        self.r1_lbl.config(fg=COLORS["border"], bg=COLORS["bg_main"])
        self.r2_frame.config(highlightbackground=COLORS["border"], bg=COLORS["bg_main"])
        self.r2_lbl.config(fg=COLORS["border"], bg=COLORS["bg_main"])


class DiagnosticsFrame(tk.Frame):
    def __init__(self, parent, controller):
        super().__init__(parent, bg=COLORS["bg_main"])
        self.controller = controller
        self.setup_ui()

    def setup_ui(self):
        # --- HEADER ---
        top_bar = tk.Frame(self, bg=COLORS["bg_lowest"], height=64)
        top_bar.pack(fill="x", side="top")
        top_bar.pack_propagate(False)
        
        lbl_back = tk.Label(top_bar, text="← BACK", font=("Helvetica", 12, "bold"),
                            bg=COLORS["bg_lowest"], fg=COLORS["primary"], cursor="hand2")
        lbl_back.pack(side="left", padx=24)
        lbl_back.bind("<Button-1>", lambda e: self.controller.show_frame("Dashboard"))
        
        tk.Label(top_bar, text="SYSTEM DIAGNOSTICS", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_lowest"], fg=COLORS["primary"]).pack(side="left", expand=True)

        # --- CONTENT AREA ---
        content = tk.Frame(self, bg=COLORS["bg_main"])
        content.pack(fill="both", expand=True, padx=32, pady=32)
        
        # Left Column
        left_col = tk.Frame(content, bg=COLORS["bg_main"])
        left_col.pack(side="left", fill="both", expand=True, padx=(0, 20))
        
        # Right Column
        right_col = tk.Frame(content, bg=COLORS["bg_main"])
        right_col.pack(side="right", fill="both", expand=True, padx=(20, 0))
        
        # --- LEFT COL: STATUS & METRICS ---
        tk.Label(left_col, text="❖ GATEWAY STATUS", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w", pady=(0, 16))
                 
        self.create_info_row(left_col, "MQTT Broker (Paho)", "CONNECTED", COLORS["success"])
        self.create_info_row(left_col, "Modbus TCP Server", "RUNNING (Port 502)", COLORS["success"])
        self.create_info_row(left_col, "Master Gateway IP", "192.168.10.10", COLORS["primary"])
        
        tk.Label(left_col, text="❖ NETWORK HEALTH", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w", pady=(32, 16))
                 
        # Metric Counters
        count_frame = tk.Frame(left_col, bg=COLORS["bg_main"])
        count_frame.pack(fill="x")
        
        self.lbl_online = self.create_counter_box(count_frame, "ONLINE NODES", "80", COLORS["success"])
        self.lbl_offline = self.create_counter_box(count_frame, "OFFLINE / FAULT", "20", COLORS["error"])

        # --- RIGHT COL: MASTER ACTIONS ---
        tk.Label(right_col, text="≢ MASTER ACTIONS", font=("Helvetica", 14, "bold"), 
                 bg=COLORS["bg_main"], fg=COLORS["text_dim"]).pack(anchor="w", pady=(0, 16))
                 
        # Broadcast Ping Box
        ping_frame = tk.Frame(right_col, bg=COLORS["bg_panel"], highlightbackground=COLORS["border"], highlightthickness=1)
        ping_frame.pack(fill="x", pady=8, ipady=16)
        
        tk.Label(ping_frame, text="NETWORK HEARTBEAT", font=("Helvetica", 12, "bold"), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_main"]).pack(pady=(10, 5))
        tk.Label(ping_frame, text="Broadcast PING to reset 5-minute\nfail-safe timers across the fiber ring.", 
                 font=("Helvetica", 10), justify="center", bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).pack(pady=(0, 16))
                 
        self.btn_ping = tk.Button(ping_frame, text="BROADCAST PING", font=("Helvetica", 14, "bold"),
                                  bg=COLORS["primary"], fg=COLORS["bg_lowest"], relief="flat",
                                  width=20, height=2, activebackground=COLORS["bg_hover"],
                                  command=self.send_ping)
        self.btn_ping.pack()

    def create_info_row(self, parent, label_text, value_text, color):
        row = tk.Frame(parent, bg=COLORS["bg_panel"], height=52)
        row.pack(fill="x", pady=4)
        row.pack_propagate(False)
        
        tk.Label(row, text=label_text, font=("Helvetica", 12), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_main"]).pack(side="left", padx=16)
                 
        lbl = tk.Label(row, text=value_text, font=("Helvetica", 12, "bold"), 
                       bg=COLORS["bg_panel"], fg=color)
        lbl.pack(side="right", padx=16)
        return lbl

    def create_counter_box(self, parent, label_text, value_text, color):
        box = tk.Frame(parent, bg=COLORS["bg_panel"], highlightbackground=COLORS["border"], highlightthickness=1)
        box.pack(side="left", expand=True, fill="both", padx=4)
        
        lbl_val = tk.Label(box, text=value_text, font=("Helvetica", 36, "bold"), 
                           bg=COLORS["bg_panel"], fg=color)
        lbl_val.pack(pady=(16, 0))
        
        tk.Label(box, text=label_text, font=("Helvetica", 10, "bold"), 
                 bg=COLORS["bg_panel"], fg=COLORS["text_dim"]).pack(pady=(0, 16))
        return lbl_val

    def send_ping(self):
        # Placeholder for Phase 2 integration
        self.btn_ping.config(text="PING SENT!", bg=COLORS["success"])
        # Reset button visually after 2 seconds
        self.after(2000, lambda: self.btn_ping.config(text="BROADCAST PING", bg=COLORS["primary"]))

    def refresh_diagnostics(self):
        # In Phase 2, this will count actual live nodes instead of mock data
        online_count = sum(1 for node in MOCK_NODES if node["status"] == "ONLINE")
        offline_count = len(MOCK_NODES) - online_count
        
        self.lbl_online.config(text=str(online_count))
        self.lbl_offline.config(text=str(offline_count))


if __name__ == "__main__":
    app = HMIApp()
    app.mainloop()