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
        
        # Make the keyboard modal
        self.transient(parent)
        self.grab_set()
        
        self.setup_ui()
        
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
        
        # Container to hold all frames
        self.container = tk.Frame(self, bg=COLORS["bg_main"])
        self.container.pack(fill="both", expand=True)
        
        # Dictionary to store references to our pages
        self.frames = {}
        
        # Initialize the Dashboard Frame
        self.frames["Dashboard"] = DashboardFrame(parent=self.container, controller=self)
        self.frames["Dashboard"].place(x=0, y=0, relwidth=1, relheight=1)
        
        # Initialize Node Detail Frame (starts empty/hidden)
        self.frames["NodeDetail"] = NodeDetailFrame(parent=self.container, controller=self)
        self.frames["NodeDetail"].place(x=0, y=0, relwidth=1, relheight=1)
        
        # Start on Dashboard
        self.show_frame("Dashboard")

    def show_frame(self, page_name, context=None):
        """Bring a specific frame to the front."""
        frame = self.frames[page_name]
        if page_name == "NodeDetail" and context:
            frame.load_node_data(context) # Pass the specific node data
        frame.tkraise()


class DashboardFrame(tk.Frame):
    def __init__(self, parent, controller):
        super().__init__(parent, bg=COLORS["bg_main"])
        self.controller = controller
        self.current_page = 0
        self.nodes_per_page = 10
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
                 
        # Search Box (Using Label to prevent OS Keyboard focus)
        self.search_var = tk.StringVar(value="")
        self.search_var.trace("w", self.on_search)
        
        self.display_var = tk.StringVar(value="Search Node or IP...")
        
        self.search_btn = tk.Label(top_bar, textvariable=self.display_var, font=self.controller.font_body,
                                   bg=COLORS["bg_panel"], fg=COLORS["text_dim"], 
                                   highlightbackground=COLORS["border"], highlightthickness=1,
                                   width=25, anchor="w", padx=10)
        self.search_btn.pack(side="right", padx=24, pady=12, fill="y")
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
        bottom_bar = tk.Frame(self, bg=COLORS["bg_main"], height=80)
        bottom_bar.pack(fill="x", side="bottom")
        bottom_bar.pack_propagate(False)
        
        # Paginator text
        self.lbl_page_info = tk.Label(bottom_bar, text="1 - 10 / 100", font=self.controller.font_mono, bg=COLORS["bg_main"], fg=COLORS["text_dim"])
        self.lbl_page_info.pack(side="right", padx=24)
        
        # Pagination Buttons
        self.btn_next = tk.Button(bottom_bar, text="NEXT PAGE ➔", font=self.controller.font_body,
                                  bg=COLORS["bg_panel"], fg=COLORS["text_main"], relief="flat",
                                  activebackground=COLORS["bg_hover"], activeforeground=COLORS["primary"],
                                  command=self.next_page)
        self.btn_next.pack(side="left", padx=(24, 10), pady=10, ipadx=10, ipady=5)

        self.btn_prev = tk.Button(bottom_bar, text="🡨 PREV PAGE", font=self.controller.font_body,
                                  bg=COLORS["bg_panel"], fg=COLORS["text_main"], relief="flat",
                                  activebackground=COLORS["bg_hover"], activeforeground=COLORS["primary"],
                                  command=self.prev_page, state="disabled")
        self.btn_prev.pack(side="left", padx=10, pady=10, ipadx=10, ipady=5)

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
        
        # Header
        self.header_var = tk.StringVar(value="NODE DETAILS")
        top_bar = tk.Frame(self, bg=COLORS["bg_lowest"], height=64)
        top_bar.pack(fill="x", side="top")
        top_bar.pack_propagate(False)
        
        btn_back = tk.Button(top_bar, text="🡨 BACK", font=self.controller.font_body,
                             bg=COLORS["bg_panel"], fg=COLORS["text_main"], relief="flat",
                             activebackground=COLORS["bg_hover"],
                             command=lambda: self.controller.show_frame("Dashboard"))
        btn_back.pack(side="left", padx=24, pady=10)
        
        tk.Label(top_bar, textvariable=self.header_var, font=self.controller.font_h2, 
                 bg=COLORS["bg_lowest"], fg=COLORS["primary"]).pack(side="left", padx=24)
                 
        # Specific Diagnostics Button
        btn_diag = tk.Button(top_bar, text="NODE DIAGNOSTICS ⚙", font=self.controller.font_body,
                             bg=COLORS["bg_panel"], fg=COLORS["alert"], relief="flat",
                             activebackground=COLORS["bg_hover"])
        btn_diag.pack(side="right", padx=24, pady=10)

        # Placeholder Body
        self.lbl_info = tk.Label(self, text="Loading Node Data...", font=self.controller.font_h2, 
                                 bg=COLORS["bg_main"], fg=COLORS["text_main"])
        self.lbl_info.pack(expand=True)

    def load_node_data(self, node_data):
        """Called when switching to this screen to update the specific node info."""
        self.header_var.set(f"NODE DETAILS: {node_data['id']}")
        self.lbl_info.config(text=f"Selected Node: {node_data['id']}\nIP: {node_data['ip']}\nStatus: {node_data['status']}\n\n(Relay Controls and Battery Math will go here)")


if __name__ == "__main__":
    app = HMIApp()
    app.mainloop()