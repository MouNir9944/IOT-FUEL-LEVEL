import tkinter as tk
from tkinter import ttk, scrolledtext
import threading
import serial
import serial.tools.list_ports
import time
import struct
from datetime import datetime
import random

class LIGOSensorSimulator:
    def __init__(self, root):
        self.root = root
        self.root.title("LIGO Fuel Level Sensor Simulator")
        self.root.geometry("900x750")
        
        # Sensor configuration
        self.address = 0x01
        self.temperature = 25
        self.level = 5000
        self.frequency = 1000
        self.mode = "slave"  # slave or master
        self.baudrate = 9600
        self.interval = 5  # seconds
        self.periodic_active = False
        self.default_mode = 0  # 0: none, 1: binary, 2: ASCII, 3: ASCII EXT
        
        # Random data update
        self.random_update_active = False
        self.random_update_interval = 10  # seconds
        
        # Serial connection
        self.serial_port = None
        self.running = False
        
        # Flag to prevent callbacks during initialization
        self.initializing = True
        
        self.setup_gui()
        self.initializing = False
        
    def setup_gui(self):
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Connection settings
        conn_frame = ttk.LabelFrame(main_frame, text="Connection Settings", padding="10")
        conn_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(conn_frame, text="COM Port:").grid(row=0, column=0, sticky=tk.W)
        self.com_port = ttk.Combobox(conn_frame, values=self.get_com_ports(), width=10)
        self.com_port.grid(row=0, column=1, padx=5)
        
        ttk.Label(conn_frame, text="Baud Rate:").grid(row=0, column=2, sticky=tk.W)
        self.baud_rate = ttk.Combobox(conn_frame, values=[2400, 4800, 9600, 19200, 38400, 115200], width=10)
        self.baud_rate.set(9600)
        self.baud_rate.grid(row=0, column=3, padx=5)
        
        ttk.Button(conn_frame, text="Connect", command=self.toggle_connection).grid(row=0, column=4, padx=10)
        
        # Sensor settings
        sensor_frame = ttk.LabelFrame(main_frame, text="Sensor Settings", padding="10")
        sensor_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(sensor_frame, text="Sensor Address (hex):").grid(row=0, column=0, sticky=tk.W)
        self.address_var = tk.StringVar(value="01")
        ttk.Entry(sensor_frame, textvariable=self.address_var, width=10).grid(row=0, column=1, padx=5)
        
        ttk.Label(sensor_frame, text="Mode:").grid(row=0, column=2, sticky=tk.W)
        self.mode_var = tk.StringVar(value="slave")
        ttk.Radiobutton(sensor_frame, text="Slave", variable=self.mode_var, value="slave", 
                       command=self.mode_changed).grid(row=0, column=3)
        ttk.Radiobutton(sensor_frame, text="Master", variable=self.mode_var, value="master", 
                       command=self.mode_changed).grid(row=0, column=4)
        
        ttk.Label(sensor_frame, text="Output Interval (s):").grid(row=1, column=0, sticky=tk.W)
        self.interval_var = tk.StringVar(value="5")
        ttk.Entry(sensor_frame, textvariable=self.interval_var, width=10).grid(row=1, column=1, padx=5)
        
        ttk.Label(sensor_frame, text="Default Mode:").grid(row=1, column=2, sticky=tk.W)
        self.default_mode_var = ttk.Combobox(sensor_frame, 
                                            values=["None", "Binary", "ASCII", "ASCII EXT"], 
                                            width=10)
        self.default_mode_var.set("None")
        self.default_mode_var.grid(row=1, column=3, padx=5)
        self.default_mode_var.bind('<<ComboboxSelected>>', self.default_mode_changed)
        
        # Data simulation with sliders
        data_frame = ttk.LabelFrame(main_frame, text="Sensor Data Simulation", padding="10")
        data_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        # Temperature slider
        temp_frame = ttk.Frame(data_frame)
        temp_frame.grid(row=0, column=0, columnspan=7, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(temp_frame, text="Temperature (°C):").grid(row=0, column=0, sticky=tk.W, padx=5)
        
        self.temp_var = tk.StringVar(value="25")
        self.temp_slider = ttk.Scale(temp_frame, from_=-40, to=100, orient=tk.HORIZONTAL, 
                                    length=400)
        self.temp_slider.set(25)
        self.temp_slider.grid(row=0, column=1, padx=5, sticky=(tk.W, tk.E))
        self.temp_slider.configure(command=self.update_temp_from_slider)
        
        self.temp_entry = ttk.Entry(temp_frame, textvariable=self.temp_var, width=8)
        self.temp_entry.grid(row=0, column=2, padx=5)
        self.temp_entry.bind('<Return>', self.update_temp_from_entry)
        
        ttk.Label(temp_frame, text="°C").grid(row=0, column=3, sticky=tk.W)
        
        # Level slider
        level_frame = ttk.Frame(data_frame)
        level_frame.grid(row=1, column=0, columnspan=7, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(level_frame, text="Level (0-65535):").grid(row=0, column=0, sticky=tk.W, padx=5)
        
        self.level_var = tk.StringVar(value="5000")
        self.level_slider = ttk.Scale(level_frame, from_=0, to=65535, orient=tk.HORIZONTAL, 
                                     length=400)
        self.level_slider.set(5000)
        self.level_slider.grid(row=0, column=1, padx=5, sticky=(tk.W, tk.E))
        self.level_slider.configure(command=self.update_level_from_slider)
        
        self.level_entry = ttk.Entry(level_frame, textvariable=self.level_var, width=8)
        self.level_entry.grid(row=0, column=2, padx=5)
        self.level_entry.bind('<Return>', self.update_level_from_entry)
        
        # Frequency
        freq_frame = ttk.Frame(data_frame)
        freq_frame.grid(row=2, column=0, columnspan=7, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(freq_frame, text="Frequency (Hz):").grid(row=0, column=0, sticky=tk.W, padx=5)
        
        self.freq_var = tk.StringVar(value="1000")
        ttk.Entry(freq_frame, textvariable=self.freq_var, width=10).grid(row=0, column=1, padx=5)
        
        ttk.Button(freq_frame, text="Update Data", command=self.update_data).grid(row=0, column=2, padx=10)
        
        # Random data update controls
        random_frame = ttk.LabelFrame(data_frame, text="Random Slider Updates", padding="5")
        random_frame.grid(row=3, column=0, columnspan=7, pady=10, sticky=(tk.W, tk.E))
        
        # Single random button
        ttk.Button(random_frame, text="Random Now", command=self.random_data).grid(row=0, column=0, padx=5)
        
        # Periodic random update button
        self.random_update_button = ttk.Button(random_frame, text="Start Auto-Update (10s)", 
                                             command=self.toggle_random_updates)
        self.random_update_button.grid(row=0, column=1, padx=5)
        
        # Update interval setting
        ttk.Label(random_frame, text="Update Interval (s):").grid(row=0, column=2, padx=5)
        self.update_interval_var = tk.StringVar(value="10")
        self.update_interval_entry = ttk.Entry(random_frame, textvariable=self.update_interval_var, width=5)
        self.update_interval_entry.grid(row=0, column=3, padx=5)
        
        # Update status
        self.update_status_var = tk.StringVar(value="Auto-Update: Stopped")
        ttk.Label(random_frame, textvariable=self.update_status_var, foreground="blue").grid(row=0, column=4, padx=10)
        
        # Current values display
        values_frame = ttk.Frame(data_frame)
        values_frame.grid(row=4, column=0, columnspan=7, pady=5)
        
        self.current_values_label = ttk.Label(values_frame, text="Current Values: Temp=25°C, Level=5000, Freq=1000Hz", 
                                             font=('Arial', 10, 'bold'))
        self.current_values_label.pack()
        
        # Communication log
        log_frame = ttk.LabelFrame(main_frame, text="Communication Log", padding="10")
        log_frame.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, width=100, height=10)
        self.log_text.grid(row=0, column=0, columnspan=2)
        
        # Control buttons
        control_frame = ttk.Frame(main_frame)
        control_frame.grid(row=6, column=0, columnspan=2, pady=10)
        
        ttk.Button(control_frame, text="Send Test Data", command=self.send_test_data).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Clear Log", command=self.clear_log).pack(side=tk.LEFT, padx=5)
        
        # Status
        self.status_var = tk.StringVar(value="Disconnected")
        status_label = ttk.Label(main_frame, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status_label.grid(row=7, column=0, columnspan=2, sticky=(tk.W, tk.E))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(5, weight=1)
        data_frame.columnconfigure(0, weight=1)
        
    def toggle_random_updates(self):
        """Toggle periodic random slider updates"""
        if not self.random_update_active:
            try:
                self.random_update_interval = int(self.update_interval_var.get())
                if self.random_update_interval < 1:
                    self.random_update_interval = 1
                    self.update_interval_var.set("1")
            except ValueError:
                self.random_update_interval = 10
                self.update_interval_var.set("10")
            
            self.random_update_active = True
            self.random_update_button.config(text="Stop Auto-Update")
            self.update_status_var.set(f"Auto-Update: Running (every {self.random_update_interval}s)")
            self.log_message(f"Automatic slider updates started (interval: {self.random_update_interval}s)")
            
            # Start update thread
            self.update_thread = threading.Thread(target=self.random_update_loop, daemon=True)
            self.update_thread.start()
        else:
            self.random_update_active = False
            self.random_update_button.config(text="Start Auto-Update (10s)")
            self.update_status_var.set("Auto-Update: Stopped")
            self.log_message("Automatic slider updates stopped")
    
    def random_update_loop(self):
        """Loop for updating sliders with random data periodically"""
        while self.random_update_active:
            self.root.after(0, self.random_data)  # Use after to run in main thread
            time.sleep(self.random_update_interval)
    
    def update_temp_from_slider(self, value):
        """Update temperature from slider movement"""
        if self.initializing:
            return
        try:
            self.temperature = int(float(value))
            self.temp_var.set(str(self.temperature))
            self.update_current_values_display()
        except ValueError:
            pass
        
    def update_temp_from_entry(self, event=None):
        """Update temperature from entry field"""
        try:
            value = int(self.temp_var.get())
            self.temperature = max(-40, min(100, value))
            self.temp_var.set(str(self.temperature))
            self.temp_slider.set(self.temperature)
            self.update_current_values_display()
        except ValueError:
            self.temp_var.set(str(self.temperature))
            
    def update_level_from_slider(self, value):
        """Update level from slider movement"""
        if self.initializing:
            return
        try:
            self.level = int(float(value))
            self.level_var.set(str(self.level))
            self.update_current_values_display()
        except ValueError:
            pass
        
    def update_level_from_entry(self, event=None):
        """Update level from entry field"""
        try:
            value = int(self.level_var.get())
            self.level = max(0, min(65535, value))
            self.level_var.set(str(self.level))
            self.level_slider.set(self.level)
            self.update_current_values_display()
        except ValueError:
            self.level_var.set(str(self.level))
            
    def update_current_values_display(self):
        """Update the current values display label"""
        if hasattr(self, 'current_values_label'):
            self.current_values_label.config(
                text=f"Current Values: Temp={self.temperature}°C, Level={self.level}, Freq={self.frequency}Hz"
            )
        
    def get_com_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]
    
    def mode_changed(self):
        self.mode = self.mode_var.get()
        self.log_message(f"Mode changed to: {self.mode}")
        
    def default_mode_changed(self, event):
        mode_map = {"None": 0, "Binary": 1, "ASCII": 2, "ASCII EXT": 3}
        self.default_mode = mode_map[self.default_mode_var.get()]
        self.log_message(f"Default mode changed to: {self.default_mode_var.get()}")
        
    def update_data(self):
        try:
            self.temperature = int(self.temp_var.get())
            self.level = int(self.level_var.get())
            self.frequency = int(self.freq_var.get())
            self.address = int(self.address_var.get(), 16)
            self.interval = int(self.interval_var.get())
            
            # Update sliders to match
            self.temp_slider.set(self.temperature)
            self.level_slider.set(self.level)
            self.update_current_values_display()
            
            self.log_message("Sensor data updated")
        except ValueError:
            self.log_message("Error: Invalid data values")
            
    def random_data(self):
        """Generate random sensor data and update sliders"""
        self.temperature = random.randint(-20, 80)
        self.level = random.randint(0, 65535)
        self.frequency = random.randint(0, 10000)
        
        # Update UI elements
        self.temp_var.set(str(self.temperature))
        self.level_var.set(str(self.level))
        self.freq_var.set(str(self.frequency))
        self.temp_slider.set(self.temperature)
        self.level_slider.set(self.level)
        self.update_current_values_display()
        
        self.log_message(f"Random slider update: Temp={self.temperature}°C, Level={self.level}, Freq={self.frequency}Hz")
        
    def toggle_connection(self):
        if not self.running:
            try:
                port = self.com_port.get()
                baud = int(self.baud_rate.get())
                
                self.serial_port = serial.Serial(
                    port=port,
                    baudrate=baud,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=1
                )
                
                self.running = True
                self.status_var.set(f"Connected to {port} at {baud} baud")
                self.log_message(f"Connected to {port} at {baud} baud")
                
                # Start communication thread
                self.comm_thread = threading.Thread(target=self.communication_loop, daemon=True)
                self.comm_thread.start()
                
            except Exception as e:
                self.log_message(f"Connection error: {str(e)}")
        else:
            self.running = False
            if self.serial_port:
                self.serial_port.close()
            self.status_var.set("Disconnected")
            self.log_message("Disconnected")
            
    def calculate_checksum(self, data):
        """Calculate CRC8 checksum according to the protocol"""
        crc = 0
        for byte in data:
            i = byte ^ crc
            crc = 0
            if i & 0x01: crc ^= 0x5e
            if i & 0x02: crc ^= 0xbc
            if i & 0x04: crc ^= 0x61
            if i & 0x08: crc ^= 0xc2
            if i & 0x10: crc ^= 0x9d
            if i & 0x20: crc ^= 0x23
            if i & 0x40: crc ^= 0x46
            if i & 0x80: crc ^= 0x8c
        return crc
    
    def process_command(self, command):
        """Process incoming commands"""
        if len(command) < 4:
            return None
            
        prefix = command[0]
        addr = command[1]
        opcode = command[2]
        checksum = command[-1]
        
        # Verify checksum
        calc_checksum = self.calculate_checksum(command[:-1])
        if calc_checksum != checksum:
            self.log_message(f"Checksum error: calculated {calc_checksum:02X}, received {checksum:02X}")
            return None
            
        # Check if command is for this sensor
        if prefix == 0x31 and addr == self.address:
            self.log_message(f"Received command: opcode=0x{opcode:02X}, addr=0x{addr:02X}")
            
            if opcode == 0x06:  # Single-stage data reading
                return self.create_response_06()
                
            elif opcode == 0x07:  # Periodic data output
                if len(command) >= 4:
                    self.periodic_active = True
                    self.log_message("Periodic output activated")
                    return self.create_response_07(0x00)
                    
            elif opcode == 0x13:  # Interval adjustment
                if len(command) >= 5:
                    new_interval = command[3]
                    self.interval = new_interval
                    self.interval_var.set(str(self.interval))
                    self.log_message(f"Interval adjusted to {self.interval} seconds")
                    return self.create_response_13(0x00)
                    
            elif opcode == 0x17:  # Default mode
                if len(command) >= 5:
                    mode = command[3]
                    self.default_mode = mode
                    mode_names = ["None", "Binary", "ASCII", "ASCII EXT"]
                    self.default_mode_var.set(mode_names[mode] if mode < 4 else "Unknown")
                    self.log_message(f"Default mode set to {mode_names[mode] if mode < 4 else mode}")
                    return self.create_response_17(0x00)
                    
        # Handle ASCII commands
        elif command.startswith(b'DO') or command.startswith(b'DP'):
            self.process_ascii_command(command)
            
        return None
    
    def create_response_06(self):
        """Create response for command 06h (data reading)"""
        response = bytearray()
        response.append(0x3E)  # Prefix
        response.append(self.address)  # Network address
        response.append(0x06)  # Operation code
        
        # Temperature (signed byte)
        temp = max(-128, min(127, self.temperature))
        response.append(temp & 0xFF)
        
        # Level (16-bit, little endian)
        response.append(self.level & 0xFF)
        response.append((self.level >> 8) & 0xFF)
        
        # Frequency (16-bit, little endian)
        response.append(self.frequency & 0xFF)
        response.append((self.frequency >> 8) & 0xFF)
        
        # Checksum
        checksum = self.calculate_checksum(response)
        response.append(checksum)
        
        self.log_message(f"Sending response: {response.hex().upper()}")
        return response
    
    def create_response_07(self, status):
        """Create response for command 07h"""
        response = bytearray()
        response.append(0x3E)
        response.append(self.address)
        response.append(0x07)
        response.append(status)
        response.append(self.calculate_checksum(response))
        
        self.log_message(f"Sending response for 07h: {response.hex().upper()}")
        return response
    
    def create_response_13(self, status):
        """Create response for command 13h"""
        response = bytearray()
        response.append(0x3E)
        response.append(self.address)
        response.append(0x13)
        response.append(status)
        response.append(self.calculate_checksum(response))
        
        self.log_message(f"Sending response for 13h: {response.hex().upper()}")
        return response
    
    def create_response_17(self, status):
        """Create response for command 17h"""
        response = bytearray()
        response.append(0x31)  # Note: protocol says 31h for this response
        response.append(self.address)
        response.append(0x17)
        response.append(status)
        response.append(self.calculate_checksum(response))
        
        self.log_message(f"Sending response for 17h: {response.hex().upper()}")
        return response
    
    def create_periodic_data(self):
        """Create periodic data packet"""
        response = bytearray()
        response.append(0x3E)
        response.append(self.address)
        response.append(0x07)
        
        # Temperature
        temp = max(-128, min(127, self.temperature))
        response.append(temp & 0xFF)
        
        # Level (16-bit, little endian)
        response.append(self.level & 0xFF)
        response.append((self.level >> 8) & 0xFF)
        
        # Frequency (16-bit, little endian)
        response.append(self.frequency & 0xFF)
        response.append((self.frequency >> 8) & 0xFF)
        
        # Checksum
        response.append(self.calculate_checksum(response))
        
        return response
    
    def create_ascii_data(self):
        """Create ASCII data string"""
        data_str = f"F={self.frequency:04X}t={self.temperature:X}N={self.level:04X}.0\r\n"
        return data_str.encode('ascii')
    
    def process_ascii_command(self, command):
        """Process ASCII commands"""
        if command.startswith(b'DO'):
            self.log_message("Received ASCII DO command")
            response = self.create_ascii_data()
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.write(response)
                self.log_message(f"Sent ASCII response: {response.decode('ascii', errors='ignore').strip()}")
                
        elif command.startswith(b'DP'):
            self.log_message("Received ASCII DP command")
            self.periodic_active = True
            
    def communication_loop(self):
        """Main communication loop"""
        last_periodic_time = time.time()
        
        while self.running:
            try:
                # Check for incoming data
                if self.serial_port and self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    self.log_message(f"Received: {data.hex().upper()}")
                    
                    # Process command
                    response = self.process_command(data)
                    if response:
                        self.serial_port.write(response)
                        
                # Handle periodic output in master mode
                if self.mode == "master" and self.periodic_active:
                    current_time = time.time()
                    if current_time - last_periodic_time >= self.interval:
                        if self.default_mode == 1:  # Binary
                            data = self.create_periodic_data()
                        else:  # ASCII
                            data = self.create_ascii_data()
                            
                        if self.serial_port and self.serial_port.is_open:
                            self.serial_port.write(data)
                            self.log_message(f"Periodic data sent: {data.hex().upper() if self.default_mode == 1 else data.decode('ascii', errors='ignore').strip()}")
                            
                        last_periodic_time = current_time
                        
                time.sleep(0.1)
                
            except Exception as e:
                self.log_message(f"Communication error: {str(e)}")
                time.sleep(1)
                
    def send_test_data(self):
        """Send test data manually"""
        if self.serial_port and self.serial_port.is_open:
            if self.default_mode == 1:  # Binary
                data = self.create_periodic_data()
            else:  # ASCII
                data = self.create_ascii_data()
                
            self.serial_port.write(data)
            self.log_message(f"Test data sent: {data.hex().upper() if self.default_mode == 1 else data.decode('ascii', errors='ignore').strip()}")
            
    def log_message(self, message):
        """Add message to log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.log_text.see(tk.END)
        
    def clear_log(self):
        """Clear the log"""
        self.log_text.delete(1.0, tk.END)

def main():
    root = tk.Tk()
    app = LIGOSensorSimulator(root)
    root.mainloop()

if __name__ == "__main__":
    main()