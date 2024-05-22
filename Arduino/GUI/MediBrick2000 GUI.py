# -*- coding: utf-8 -*-
"""
Created on Tue Oct  3 13:02:50 2023

@author: Moath Alsayar
@edited by: Carmella Ocaya
    Fri Dec  29 23:59:00 2023

"""
import serial
import tkinter as tk
from tkinter import ttk
import customtkinter as ctk
import matplotlib as plt
import serial.tools.list_ports as port_list
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.backend_bases import MouseButton
import time
import datetime
from datetime import timedelta
from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk
import psutil 
import os
import threading
import logging
import numpy as np
LARGEFONT = ("Comic Sans", 45, "bold")
MidFONT = ("Comic Sans", 25, "bold")
regFont = ("Comic Sans",15,"bold" )
smallFont =("Comic Sans",13,"bold" )
ctk.set_appearance_mode("dark")

class loginPage(ctk.CTkFrame):
    def __init__(self, parent, controller):
        ctk.CTkFrame.__init__(self, parent)

        label = ctk.CTkLabel(self, text="MediBrick2000", font=LARGEFONT, fg_color="#bf2c19", corner_radius=10)
        label.pack(side="top", pady=15)
   
        userlab=ctk.CTkLabel(self, text="Enter Username", font=MidFONT, fg_color="#bf2c19", corner_radius=10)
        userlab.pack()

        self.warning_label = ctk.CTkLabel(self, text="Avoid \\ / : * ? \" < > |", fg_color="#bf2c19", font=("Comic Sans", 16), corner_radius=10)
        self.warning_label.pack(pady=5)

        self.text_input = ctk.CTkEntry(self)
        self.text_input.pack(pady=5)

        enter = ctk.CTkButton(self, text="Enter", font=MidFONT, command=lambda: self.on_enter_click(controller), fg_color="#bf2c19", corner_radius=10)
        enter.pack(pady=5)

    def show_warning_popup(self, message):
        popup = tk.Toplevel()
        popup.title("Warning")
        
        label = tk.Label(popup, text=message, fg="#bf2c19")  # Set the text color with fg
        label.pack(padx=10, pady=10)
        
        button = tk.Button(popup, text="OK", command=popup.destroy)
        button.pack(pady=10)

    def on_enter_click(self, controller):
        # Get the user input from the text box
        user_input = self.text_input.get()

        # Check if the user input is empty or contains invalid characters
        if not user_input or not self.is_valid_folder_name(user_input):
            self.show_warning_popup("Invalid folder name. Please enter a valid name")
            return
        
        # Get the directory of the current Python script
        current_directory = os.path.dirname(__file__)

        # Create the main user folder in the current directory
        user_folder_path = os.path.join(current_directory, user_input)
        os.makedirs(user_folder_path, exist_ok=True)

        # Create subfolders inside the user folder
        subfolders = ["Pulse Oximetry Data", "ECG Data", "Temperature Data", "Skin Impedance Data", "Digital Stethoscope Data"]
        for subfolder in subfolders:
            os.makedirs(os.path.join(user_folder_path, subfolder), exist_ok=True)

        # Continue with navigating to the next frame (e.g., StartPage)
        controller.show_frame(StartPage)

    def is_valid_folder_name(self, folder_name):
        # Check if the folder name contains invalid characters
        invalid_characters = set(r'\/:*?"<>|.')
        return not any(char in invalid_characters for char in folder_name)

class tkinterApp(tk.Tk):
    def __init__(self, *args, **kwargs):
        tk.Tk.__init__(self, *args, **kwargs)
        self.title("MediBrick2000")
        container = ctk.CTkFrame(self)
        container.pack(side="top", fill="both", expand=True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)
        self.frames = {}
        for F in (loginPage, StartPage, Page1, Page2, Page3, Page4, Page5):
            frame = F(container, self)
            self.frames[F] = frame
            frame.grid(row=0, column=0, sticky="nsew")
        self.show_frame(loginPage)

    def show_frame(self, cont):
        frame = self.frames[cont]
        frame.tkraise()

class StartPage(ctk.CTkFrame):
    def __init__(self, parent, controller):
        ctk.CTkFrame.__init__(self, parent)      
        
        label = ctk.CTkLabel(self, text="MediBrick2000", font=LARGEFONT, fg_color="#bf2c19", corner_radius=10)
        label.pack(pady = 5)
        
        button1 = ctk.CTkButton(self, text="Pulse Oximeter", command=lambda: controller.show_frame(Page1), fg_color="#f2a138",font = regFont)
        button1.pack(pady = 1)

        button2 = ctk.CTkButton(self, text="Electrocardiogram (ECG)", command=lambda: controller.show_frame(Page2), fg_color="#f2a138",font = regFont)
        button2.pack(pady = 1)

        button3 = ctk.CTkButton(self, text="Temperature Sensor", command=lambda: controller.show_frame(Page3), fg_color="#f2a138",font = regFont)
        button3.pack(pady = 1)
        
        button4 = ctk.CTkButton(self, text="Body Fat and Water Composition (Skin Impedance)", command=lambda: controller.show_frame(Page4), fg_color="#f2a138",font = regFont)
        button4.pack(pady = 1)
        
        button5 = ctk.CTkButton(self, text="Digital Stethoscope", command=lambda: controller.show_frame(Page5), fg_color="#f2a138",font = regFont)
        button5.pack(pady = 1)
        
        port_label = ctk.CTkLabel(self, text="Select COM Port:",font = smallFont)
        port_label.pack()
       
        # Add a dropdown to select the port
        ports = [port.device for port in port_list.comports()]
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:",font = smallFont)
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        baud_var.set(500000)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200",'500000']
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        connect_button = ctk.CTkButton(self, text="Connect", command=lambda: self.connect_to_serial(port_var, baud_var))
        connect_button.place(x=570,y=330)

        self.themeMode = "dark"
        switch_button = ctk.CTkSwitch(self, command=self.toggle_theme,text="Light/Dark Mode")
        switch_button.place(x=3,y=3)
    def toggle_theme(self):
        
        # Add logic here to toggle the theme between light and dark mode
        if self.themeMode == "dark":
         ctk.set_appearance_mode("light")
         self.themeMode = "light"
        else:
            ctk.set_appearance_mode("dark")
            self.themeMode = "dark"
    
    def connect_to_serial(self, port_var, baud_var):
       selected_port = port_var.get()
       selected_baud = baud_var.get()

       try:
           # Establish the serial connection
           serial.Serial(selected_port, selected_baud, timeout=1)
           print(f"Connected to {selected_port} at {selected_baud} baud")
           # Continue with other actions (e.g., navigate to the data acquisition page)
       except serial.SerialException as e:
           print(f"Error: {e}")
           # Handle the connection error (e.g., display an error message)
            
class Page1(ctk.CTkFrame):
    def __init__(self, parent, controller):
        self.start_time = 0  # Start the time at 0
        ctk.CTkFrame.__init__(self, parent)
        self.exit_flag = True
        self.data_counter = 0
        self.x_data = []
        self.y_data = []
        self.x_data_ir = []
        self.y_data_ir = []
        self.save_data = False  # Make save_data a class attribute
        self.line_ir = None
        
        button1 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: stop_reading_and_show_home(controller), fg_color="#ed9818")
        button1.place(x=1,y=1)
        
        # Create a label to display the received data
        data_label = ctk.CTkLabel(self, text="Pulse Oximeter Data", font=LARGEFONT)
        data_label.pack()

        # Create labels to display the initialization status, SPO2, and BPM
        status_label = ctk.CTkLabel(self, text="Initializing AFE44xx...")
        status_label.pack()

        spo2_label = ctk.CTkLabel(self, text="SPO2: ", font=LARGEFONT)
        spo2_label.pack()

        bpm_label = ctk.CTkLabel(self, text="BPM: ", font=LARGEFONT)
        bpm_label.pack()

        # Get available serial ports
        ports = [port.device for port in port_list.comports()]
        if not ports:
            print("No serial ports available. Please check your connections.")
            exit(1)

        # Create a combo box for selecting the serial port
        port_label = ctk.CTkLabel(self, text="Select COM Port:")
        
        port_label.pack()
        
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:")
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        baud_var.set(500000)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200",'500000']
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        # Create a subframe for the Matplotlib graph
        graph_frame = ttk.Frame(self)
        graph_frame.pack()

        # Create a Matplotlib figure and subplot
       # Create a Matplotlib figure and subplot
        fig = Figure(figsize=(13, 3.3), dpi=100, facecolor='dimgrey')
        self.ax = fig.add_subplot(111)  # Define self.ax here
        self.ax.set_facecolor('black')
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('Raw Values')
        self.line, = self.ax.plot([], [], 'b-')
       
        # Create a canvas for the Matplotlib figure
        canvas = FigureCanvasTkAgg(fig, master=graph_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Create an instance of the toolbar
        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.pack()
        toolbar.update()

        #x_data, y_data = [], []
        # Create buttons for starting and stopping data saving
        start_button = ctk.CTkButton(self, text="Start Saving Data",fg_color = "#3cb043")
        start_button.place(x=3,y=250)

        stop_button = ctk.CTkButton(self, text="Stop Saving Data", fg_color= "#bf2f24")
        stop_button.place(x=150,y=250)

        save_data = False  # Initialize the save flag
        
        def stop_reading_and_show_home(controller):
            stop_reading()
            controller.show_frame(StartPage)
        
        def start_saving_data():
           
            self.save_data = True

        def stop_saving_data():
            self.save_data = False
            if len(self.x_data) >= 500:  # Save data only if there are at least 1000 points
                save_to_csv()  # Save data to CSV when stop is pressed

        def save_to_csv():
            def find_folder_path(folder_name):
                for root, dirs, files in os.walk(os.path.dirname(os.path.abspath(__file__))):  # Starting from GUI directory
                    if folder_name in dirs:
                        return os.path.join(root, folder_name)

            folder_name_to_find = "Pulse Oximetry Data" #Replace with folder name
            folder_path = find_folder_path(folder_name_to_find)
           
            file_name = "PulseOxi.csv"
            file_path = os.path.join(folder_path, file_name)

            print("Saving to CSV...")

            with open(file_path, "a") as file:
             if os.path.getsize(file_path) == 0:
               file.write("Time, Red Values, IR Values\n")
           
             start_index = max(0, len(self.x_data) - 500)
           
             for i in range(start_index, len(self.x_data)):
               file.write(f"{self.x_data[i]}, {self.y_data[i]}, {self.y_data_ir[i]}\n")
               
            print(f"File saved to: {os.path.abspath(file_name)}")  # Print the absolute path of the file
                              
        # Configure button commands
        start_button.configure(command=start_saving_data)
        stop_button.configure(command=stop_saving_data) 

        # Define global variables
        exit_flag = False

        ser = None  # Initialize serial connection object

        def start_reading():
          self.exit_flag = False  # Access exit_flag using self
          
          ser_thread = threading.Thread(target=read_serial_data)
          ser_thread.start()
    
        # Function to stop reading serial data
        def stop_reading():
            self.exit_flag = True  # Access exit_flag using self
            
        # Create buttons for starting and stopping reading
        start_reading_button = ctk.CTkButton(self, text="Start Reading Data", fg_color="#3cb043", command=start_reading)
        start_reading_button.place(x=3,y=200)

        stop_reading_button = ctk.CTkButton(self, text="Stop Reading Data", fg_color="#bf2f24", command=stop_reading)
        stop_reading_button.place(x=150,y=200)

        def read_serial_data():
            global exit_flag, ser,x_data,y_data, data_counter
            ser = None  # Initialize ser here
            data_buffer_red = []  # Buffer to accumulate Red data points
            data_buffer_ir = []  # Buffer to accumulate IR data points
            batch_size = 20  # Set the size for batch processing
            start_time = time.time()  # Variable to store the start time

            while not self.exit_flag:  # Access exit_flag using self
                try:
                    # Open serial connection if not open
                    if ser is None or not ser.is_open:
                        selected_port = port_var.get()
                        selected_baud = baud_var.get()
                        ser = serial.Serial(selected_port, selected_baud, timeout=1)
                        print(f"Connected to {selected_port} at {selected_baud} baud")

                    # Read incoming serial data
                    if ser.in_waiting > 0:
                        data = ser.readline(50).decode('utf-8').strip()

                        # Check if data contains "Red Values:" or "IR Values:"
                        if "Red Values:" in data:
                         
                            # Extract Red value
                             # Extract Red value
                          try:
                           red_value = int(data.split(":")[1].strip())
                           current_time = time.time() - start_time
                           data_buffer_red.append((current_time, red_value))
                          # print(red_value)
                           
                          except ValueError:
                            print(f"Invalid Red Value: {data}")

                        elif "IR Values:" in data:
                            # Extract IR value
                            ir_value = int(data.split(":")[1].strip())
                            current_time = time.time() - start_time
                            data_buffer_ir.append((current_time, ir_value))  # Append IR data to the buffer
                            #print(ir_value)
                            
                        # Process batched data when buffer size reaches the specified batch size
                        if len(data_buffer_red) >= batch_size and len(data_buffer_ir) >= batch_size:
                            process_batch_data(data_buffer_red, data_buffer_ir)
                            data_buffer_red = []  # Clear the Red buffer after processing
                            data_buffer_ir = []  # Clear the IR buffer after processing
                            time.sleep(0.005)  # Introduce a small delay to throttle the data processing

                        # Handle initialization and other messages
                        elif "Init" in data:
                            status_label.configure(text=data)
                        elif "Sp02" in data and "Pulse rate" in data:
                            spo2 = data.split(",")[0].split(":")[1].strip()
                            bpm = int(data.split(",")[1].split(":")[1].strip().split(" ")[0])  # Convert to integer for comparison
                            if self.data_counter >= 500:  # Check after 500 data points
                                if bpm > 205:
                                    status_label.configure(text="Probe error please place oximeter on finger")
                                else:
                                    spo2_label.configure(text=f"SPO2: {spo2}")
                                    bpm_label.configure(text=f"BPM: {bpm}")

                except serial.SerialException as e:
                    print(f"Serial Exception: {e}")
                    if ser is not None:
                        ser.close()
                    time.sleep(2)
                
 
        line_ir = None  # Define line_ir globally
        
        # Function to create the plot for IR values
        def create_ir_plot():
            global line_ir
            self.line_ir, = self.ax.plot([], [], 'orange', label='IR Values')  # Create an initial line for IR values
            self.ax.legend(loc='upper left', bbox_to_anchor=(1, 1))  # Display legend outside the graph
            
        # Call the function to create the plot for IR values
        create_ir_plot()

        # Inside the process_batch_data function
        def process_batch_data(data_buffer_red, data_buffer_ir):
            
            x_batch_red, y_batch_red = zip(*data_buffer_red)  # Unzip Red data batch
            x_batch_ir, y_batch_ir = zip(*data_buffer_ir)  # Unzip IR data batch

            self.x_data.extend(x_batch_red)  # Update x_data with Red values' time
            self.y_data.extend(y_batch_red)  # Update y_data with Red values
            
            # Display the last 1000 points for Red values
            max_points = 1000
            if len(self.x_data) > max_points:
                self.x_data = self.x_data[-max_points:]
                self.y_data = self.y_data[-max_points:]

            # Plot Red values
            self.line.set_data(self.x_data, self.y_data)
            self.line.set_color('red')  # Change the color of the Red values line to red
            self.ax.relim()
            self.ax.autoscale_view()
            canvas.draw_idle()

            # Store and display the last 1000 points for IR values
            self.x_data_ir.extend(list(x_batch_ir))
            self.y_data_ir.extend(list(y_batch_ir))

            if len(self.x_data_ir) > max_points:
                self.x_data_ir = self.x_data_ir[-max_points:]
                self.y_data_ir = self.y_data_ir[-max_points:]

            # Plot IR values in the same plot
            self.line_ir.set_data(self.x_data_ir, self.y_data_ir)  # Set IR data to the plot
            self.line_ir.set_color('orange')  # Set color of IR values line to orange

            self.data_counter += len(data_buffer_red)  # Increment data counter
            
            # Update the legend with both Red and IR values
            self.ax.legend(['Red Values', 'IR Values'], loc='upper left', bbox_to_anchor=(1, 1))

            if self.data_counter >= 500:
                status_label.configure(text="AFE4490 Calculating...")
        # Start reading the serial data in a separate thread
        import threading
        thread = threading.Thread(target=read_serial_data)
        thread.start()

class Page2(ctk.CTkFrame):
    def __init__(self, parent, controller):
        ctk.CTkFrame.__init__(self, parent)
        
        self.exit_flag = True
        self.data_counter = 0
        self.x_data = []
        self.y_data = []
        self.save_data = False  # Make save_data a class attribute
        self.line_ir = None
        self.zero_ecg_time = 0
        
        label = ctk.CTkLabel(self, text="Electrocardiogram (ECG)", font=LARGEFONT)
        label.pack()
        button2 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: stop_reading_and_show_home(controller), fg_color="#ed9818")
        button2.place(x=3,y=0)
    
        # Create labels to display the initialization status, SPO2, and BPM
        status_label = ctk.CTkLabel(self, text="Initializing ECG module...")
        status_label.pack()

        ECG_label = ctk.CTkLabel(self, text="ECG Val: ", font=LARGEFONT)
        ECG_label.pack()
        
        # Get available serial ports
        ports = [port.device for port in port_list.comports()]
        if not ports:
            print("No serial ports available. Please check your connections.")
            exit(1)
            #warning_label = ctk.CTkLabel(self, text="No ports connected. Check your connection.", fg_color="#bf2c19")
            #warning_label.pack()
                
        # Custom styling for the option menu
        style = ttk.Style()
        style.theme_use('default')
        style.configure('Custom.TMenubutton', background='#fcb103')

        # Create a combo box for selecting the serial port
        port_label = ctk.CTkLabel(self, text="Select COM Port:")
        port_label.pack()
        
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:")
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200"]
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        # Create a subframe for the Matplotlib graph
        graph_frame = ttk.Frame(self)
        graph_frame.pack()

        # Create a Matplotlib figure and subplot
        fig = Figure(figsize=(14, 3.8), dpi=100, facecolor='dimgrey')
        self.ax = fig.add_subplot(111)  # Define self.ax here
        self.ax.set_facecolor('black')
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('ECG Val:')
        self.line, = self.ax.plot([], [], 'b-')
       
        # Create a canvas for the Matplotlib figure
        canvas = FigureCanvasTkAgg(fig, master=graph_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Create an instance of the toolbar
        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.pack()
        toolbar.update()

        #x_data, y_data = [], []
        # Create buttons for starting and stopping data saving
        start_button = ctk.CTkButton(self, text="Start Saving Data",fg_color = "#3cb043")
        start_button.place(x=3,y=200)

        stop_button = ctk.CTkButton(self, text="Stop Saving Data", fg_color= "#bf2f24")
        stop_button.place(x=150,y=200)

        save_data = False  # Initialize the save flag
        
        def stop_reading_and_show_home(controller):
            stop_reading()
            controller.show_frame(StartPage)
        

        def start_saving_data():          
            self.save_data = True

        def stop_saving_data():
            self.save_data = False
            if len(self.x_data) >= 300:  # Save data only if there are at least 1000 points
                save_to_csv()  # Save data to CSV when stop is pressed

        def save_to_csv():
            def find_folder_path(folder_name):
                for root, dirs, files in os.walk(os.path.dirname(os.path.abspath(__file__))):  # Starting from GUI directory
                    if folder_name in dirs:
                        return os.path.join(root, folder_name)

            folder_name_to_find = "ECG Data" #Replace with folder name
            folder_path = find_folder_path(folder_name_to_find)

            #folder_name = "ECG Data"
            #folder_path = os.path.dirname(folder_name)
            file_name = "ECG.csv"
            file_path = os.path.join(folder_path, file_name)

            print("Saving to CSV...")

            with open(file_path, "a") as file:
                if os.path.getsize(file_path) == 0:
                    file.write("Time, ECG VAL\n")

                start_index = max(0, len(self.x_data) - 1000)

                for i in range(start_index, len(self.x_data)):
                    file.write(f"{self.x_data[i]}, {self.y_data[i]}\n")

                print(f"File saved to: {os.path.abspath(file_name)}")
        
        # Configure button commands
        start_button.configure(command=start_saving_data)
        stop_button.configure(command=stop_saving_data) 

        # Define global variables
        exit_flag = False

        ser = None  # Initialize serial connection object

        def start_reading():
          self.exit_flag = False  # Access exit_flag using self
          
          ser_thread = threading.Thread(target=read_serial_data)
          ser_thread.start()
    
        # Function to stop reading serial data
        def stop_reading():
            self.exit_flag = True  # Access exit_flag using self
        

        # Create buttons for starting and stopping reading
        start_reading_button = ctk.CTkButton(self, text="Start Reading Data", fg_color="#3cb043", command=start_reading)
        start_reading_button.place(x=3,y=150)

        stop_reading_button = ctk.CTkButton(self, text="Stop Reading Data", fg_color="#bf2f24", command=stop_reading)
        stop_reading_button.place(x=150,y=150)
        #function to receive the ECG data
        def update_ecg_label(ecg_value):
         ECG_label.configure(text=f"ECG Val: {ecg_value}")  # Update label text with ECG value
       # Inside the read_serial_data function
        def read_serial_data():
         global exit_flag, ser, x_data, y_data, data_counter
         ser = None  # Initialize ser here
         data_buffer_ecg = []  # Buffer to accumulate ECG data points
         batch_size = 10  # Set the size for batch processing
         start_time = time.time()  # Variable to store the start time

         while not self.exit_flag:  # Access exit_flag directly
          try:
            # Open serial connection if not open
            if ser is None or not ser.is_open:
                selected_port = port_var.get()
                selected_baud = baud_var.get()
                ser = serial.Serial(selected_port, selected_baud, timeout=1)
                print(f"Connected to {selected_port} at {selected_baud} baud")

            # Read incoming serial data
            if ser.in_waiting > 0:
                data = ser.readline().decode('utf-8').strip()

                # Check if data contains ECG values
                if data.startswith("ECG:"):
                    # Extract ECG value
                    ecg_value = float(data.split(":")[1].strip())
                    current_time = time.time() - start_time
                    data_buffer_ecg.append((current_time, ecg_value))  # Append ECG data to the buffer
                    # Update ECG label with the received value
                    update_ecg_label(ecg_value)
                    # Check if ECG value is zero
                    if ecg_value == 0:
                       if self.zero_ecg_time == 0:
                           self.zero_ecg_time = current_time
                       elif current_time - self.zero_ecg_time >= 3:  # Check if zero for 3 seconds
                           status_label.configure(text="Check for Pulse!!")  # Update status label
                    else:
                        self.zero_ecg_time = 0  # Reset zero ECG time if value is not zero
                # Process batched data when buffer size reaches the specified batch size
                if len(data_buffer_ecg) >= batch_size:
                    process_batch_data(data_buffer_ecg)  # Process the ECG batch data
                    data_buffer_ecg = []  # Clear the ECG buffer after processing
                # Handle other messages or initialization here
          except serial.SerialException as e:
            print(f"Serial Exception: {e}")
            if ser is not None:
                ser.close()
            time.sleep(2)

    # Inside the process_batch_data function
        def process_batch_data(data_buffer_ecg):
          global x_data, y_data

          x_batch_ecg, y_batch_ecg = zip(*data_buffer_ecg)  # Unzip ECG data batch

          self.x_data.extend(x_batch_ecg)  # Update x_data with ECG values' time
          self.y_data.extend(y_batch_ecg)  # Update y_data with ECG values

    # Plot ECG values or perform further processing as needed

    # Display the last 1000 points for ECG values
          max_points = 500
          if len(self.x_data) > max_points:
           self.x_data = self.x_data[-max_points:]
           self.y_data = self.y_data[-max_points:]

    # Plot ECG values
          self.line.set_data(self.x_data, self.y_data)
          self.line.set_color('Red')  # Change the color of the ECG values line to blue
          self.ax.relim()
          self.ax.autoscale_view()
          canvas.draw_idle()

          self.data_counter += len(data_buffer_ecg)  # Increment data counter
          
          status_label.configure(text="ECG in Operation...")
     

    # Update the legend with ECG values
          self.ax.legend(['ECG Values'], loc='upper left', bbox_to_anchor=(1, 1))
          # Start reading the serial data in a separate thread
        import threading
        thread = threading.Thread(target=read_serial_data)
        thread.start()

class Page3(ctk.CTkFrame):
    def __init__(self, parent, controller):
        self.exit_flag = True
        self.data_counter = 0
        self.x_data = []
        self.y_data = []
        self.save_data = False  # Make save_data a class attribute
        self.line_ir = None
        ctk.CTkFrame.__init__(self, parent)
        self.label = ctk.CTkLabel(self, text="Temperature Sensor", font=LARGEFONT)
        self.label.pack()

        self.button3 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: stop_reading_and_show_home(controller), fg_color="#ed9818")
        self.button3.place(x=3,y=0)
        # Configure logging to a file
      #  logging.basicConfig(filename='uart_log.txt', level=logging.INFO, format='%(asctime)s - %(message)s')
        # Create labels to display the initialization status, temp
        status_label = ctk.CTkLabel(self, text="Initializing Temperature Sensor...")
        status_label.pack()

        temp_label = ctk.CTkLabel(self, text="Temperature: ", font=LARGEFONT)
        temp_label.pack()
        # Create entry fields for resistor values
        resistor1_label = ctk.CTkLabel(self, text="Resistor 1 Value:")
        resistor1_label.place(x=995, y=10)

        self.resistor1_entry = ctk.CTkEntry(self)
        self.resistor1_entry.place(x=1100, y=10)

        resistor2_label = ctk.CTkLabel(self, text="Resistor 2 Value:")
        resistor2_label.place(x=995, y=70)

        self.resistor2_entry = ctk.CTkEntry(self)
        self.resistor2_entry.place(x=1100, y=70)

        resistor3_label = ctk.CTkLabel(self, text="Resistor 3 Value:")
        resistor3_label.place(x=995, y=130)

        self.resistor3_entry = ctk.CTkEntry(self)
        self.resistor3_entry.place(x=1100, y=130)

       # Create buttons for sending resistor values
        send_resistor1_button = ctk.CTkButton(self, text="Send Resistor 1", fg_color="#3cb043", command=self.send_resistor1)
        send_resistor1_button.place(x=1100, y=40)

        send_resistor2_button = ctk.CTkButton(self, text="Send Resistor 2", fg_color="#3cb043", command=self.send_resistor2)
        send_resistor2_button.place(x=1100, y=100)

        send_resistor3_button = ctk.CTkButton(self, text="Send Resistor 3", fg_color="#3cb043", command=self.send_resistor3)
        send_resistor3_button.place(x=1100, y=160)

  
        # Get available serial ports
        ports = [port.device for port in port_list.comports()]
        if not ports:
            print("No serial ports available. Please check your connections.")
            exit(1)
                
        # Custom styling for the option menu
        style = ttk.Style()
        style.theme_use('default')
        style.configure('Custom.TMenubutton', background='#fcb103')

        # Create a combo box for selecting the serial port
        port_label = ctk.CTkLabel(self, text="Select COM Port:")
        port_label.pack()
        
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:")
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200"]
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        # Create a subframe for the Matplotlib graph
        graph_frame = ttk.Frame(self)
        graph_frame.pack()

        # Create a Matplotlib figure and subplot
        fig = Figure(figsize=(14, 3.8), dpi=100, facecolor='dimgrey')
        self.ax = fig.add_subplot(111)  # Define self.ax here
        self.ax.set_facecolor('black')
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('Temperature (°C)')
        self.line, = self.ax.plot([], [], 'b-')
       
        # Create a canvas for the Matplotlib figure
        canvas = FigureCanvasTkAgg(fig, master=graph_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Create an instance of the toolbar
        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.pack()
        toolbar.update()

        #x_data, y_data = [], []
        # Create buttons for starting and stopping data saving
        start_button = ctk.CTkButton(self, text="Start Saving Data",fg_color = "#3cb043")
        start_button.place(x=3,y=200)

        stop_button = ctk.CTkButton(self, text="Stop Saving Data", fg_color= "#bf2f24")
        stop_button.place(x=150,y=200)

        save_data = False  # Initialize the save flag
        
        def stop_reading_and_show_home(controller):
            stop_reading()
            controller.show_frame(StartPage)
        
        
        def start_saving_data():          
            self.save_data = True

        def stop_saving_data():
            self.save_data = False
            if len(self.x_data) >= 10000:  # Save data only if there are at least 1000 points
                save_to_csv()  # Save data to CSV when stop is pressed

        def save_to_csv():
            def find_folder_path(folder_name):
                for root, dirs, files in os.walk(os.path.dirname(os.path.abspath(__file__))):  # Starting from GUI directory
                    if folder_name in dirs:
                        return os.path.join(root, folder_name)

            folder_name_to_find = "Temperature Data" 
            folder_path = find_folder_path(folder_name_to_find)

            file_name = "Temp.csv"
            file_path = os.path.join(folder_path, file_name)

            print("Saving to CSV...")

            with open(file_path, "a") as file:
                if os.path.getsize(file_path) == 0:
                    file.write("Time, Temperature °C\n")

                start_index = max(0, len(self.x_data) - 10000)

                for i in range(start_index, len(self.x_data)):
                    file.write(f"{self.x_data[i]}, {self.y_data[i]}\n")

                print(f"File saved to: {os.path.abspath(file_name)}")
        
        # Configure button commands
        start_button.configure(command=start_saving_data)
        stop_button.configure(command=stop_saving_data) 

        # Define global variables
        exit_flag = False

        ser = None  # Initialize serial connection object

        def start_reading():
          self.exit_flag = False  # Access exit_flag using self
          
          ser_thread = threading.Thread(target=read_serial_data)
          ser_thread.start()
    
        # Function to stop reading serial data
        def stop_reading():
            self.exit_flag = True  # Access exit_flag using self
        
        # Create buttons for starting and stopping reading
        start_reading_button = ctk.CTkButton(self, text="Start Reading Data", fg_color="#3cb043", command=start_reading)
        start_reading_button.place(x=3,y=150)

        stop_reading_button = ctk.CTkButton(self, text="Stop Reading Data", fg_color="#bf2f24", command=stop_reading)
        stop_reading_button.place(x=150,y=150)
        #function to receive the ECG data
        def update_ecg_label(temp_value):
         temp_label.configure(text=f"Temperature: {temp_value} °C")  # Update label text with ECG value
       # Inside the read_serial_data function
        def read_serial_data():
         global exit_flag, ser, x_data, y_data, data_counter
         ser = None  # Initialize ser here
         data_buffer_temp = []  # Buffer to accumulate temp data points
         batch_size = 150  # Set the size for batch processing
         start_time = time.time()  # Variable to store the start time

         while not self.exit_flag:  # Access exit_flag directly
          try:
            # Open serial connection if not open
            if ser is None or not ser.is_open:
                selected_port = port_var.get()
                selected_baud = baud_var.get()
                ser = serial.Serial(selected_port, selected_baud, timeout=0.1)
               # print(f"Connected to {selected_port} at {selected_baud} baud")

            # Read incoming serial data
            if ser.in_waiting > 0:
                data = ser.readline(32).decode('utf-8').strip()
                #print(data)

                # Check if data contains ECG values
                if data.startswith("Temperature:"):
                    # Extract temp value
                    temp_value = float(data.split(":")[1].strip())
                    current_time = time.time() - start_time
                    data_buffer_temp.append((current_time, temp_value))  # Append ECG data to the buffer
                    # Update temp label with the received value
                    update_ecg_label(temp_value)
                    # Check if ECG value is zero
                    if temp_value < 36 :
                           status_label.configure(text="You are very cold! Are you a lizard?!")  # Update status label
                    if temp_value > 37.5:
                       status_label.configure(text="You are very hot! check for fever!")  # Update status label
                    if (temp_value > 36 and temp_value < 37.5):
                        status_label.configure(text="Perfect Temprature! you are chilling and healthy :)")  # Update status label
                # Process batched data when buffer size reaches the specified batch size
                if len(data_buffer_temp) >= batch_size:
                    process_batch_data(data_buffer_temp)  # Process the ECG batch data
                    data_buffer_temp = []  # Clear the ECG buffer after processing
                    time.sleep(0.01)  # Adjust the sleep duration
                # Handle other messages or initialization here
          except serial.SerialException as e:
            print(f"Serial Exception: {e}")
            if ser is not None:
                ser.close()
            time.sleep(2)

# Inside the process_batch_data function
        def process_batch_data(data_buffer_temp):
          global x_data, y_data

          x_batch_temp, y_batch_temp = zip(*data_buffer_temp)  # Unzip temp data batch

          self.x_data.extend(x_batch_temp)  # Update x_data with time
          self.y_data.extend(y_batch_temp)  # Update y_data with temp

    # Plot temp values or perform further processing as needed

    # Display the last temp points for temp values
          max_points = 10000
          if len(self.x_data) > max_points:
           self.x_data = self.x_data[-max_points:]
           self.y_data = self.y_data[-max_points:]

    # Plot ECG values
          self.line.set_data(self.x_data, self.y_data)
          self.line.set_color('Red')  # Change the color of the temp values line to blue
          self.ax.relim()
          self.ax.autoscale_view()
          canvas.draw_idle()

          self.data_counter += len(data_buffer_temp)  # Increment data counter        
     
    # Update the legend with ECG values
          self.ax.legend(['Temperature °C'], loc='upper left', bbox_to_anchor=(1, 1))
          # Start reading the serial data in a separate thread
        import threading
        thread = threading.Thread(target=read_serial_data)
        thread.start()
        
    def send_resistor1(self):
           resistor1_value = self.resistor1_entry.get()
           self.send_to_esp32(f"Resistor1:{resistor1_value}")

    def send_resistor2(self):
           resistor2_value = self.resistor2_entry.get()
           self.send_to_esp32(f"Resistor2:{resistor2_value}")

    def send_resistor3(self):
           resistor3_value = self.resistor3_entry.get()
           self.send_to_esp32(f"Resistor3:{resistor3_value}")

    def send_to_esp32(self, message):
        
     
        ser.write(message.encode('utf-8'))  # Send the message as bytes

       # logging.info(f"Sent to ESP32: {message}")
        
class Page4(ctk.CTkFrame):
    def __init__(self, parent, controller):
        ctk.CTkFrame.__init__(self, parent)
        self.button3 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: stop_reading_and_show_home(controller), fg_color="#ed9818")
        self.button3.place(x=3,y=0)
        self.exit_flag = True
        self.data_counter = 0
        self.x_data = []
        self.y_data = []
        self.save_data = False  # Make save_data a class attribute
        self.line_ir = None
        
        # Configure logging to a file
        logging.basicConfig(filename='uart_log.txt', level=logging.INFO, format='%(asctime)s - %(message)s')
        
        label = ctk.CTkLabel(self, text="        Body Fat and Water Composition (Skin Impedance)", font=LARGEFONT)
        label.pack()
        button2 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: controller.show_frame(StartPage), fg_color="#ed9818")
        button2.place(x=3,y=0)
    
        # Create labels to display the initialization status, SPO2, and BPM
        status_label = ctk.CTkLabel(self, text="Initializing AD5933 module...")
        status_label.pack()

        bodyFat_label = ctk.CTkLabel(self, text="Bodyfat %: ", font=LARGEFONT)
        bodyFat_label.pack()
        
        waterComp_label = ctk.CTkLabel(self, text="Water Composition: ", font=LARGEFONT)
        waterComp_label.pack()
        
        Impeadance_label = ctk.CTkLabel(self, text="Avg Impedance: ", font=MidFONT)
        Impeadance_label.pack()
    
        input_label = ctk.CTkLabel(self, text="User input: ", font=LARGEFONT)
        input_label.place(x=980,y=100)
        
        # Create a button to trigger the sending of the 'measure:1' signal
        measure_button = ctk.CTkButton(self, text="Start Measure", command=self.send_measure)
        measure_button.place(x=3, y=150)
   
        logging.warning("Serial connection is not open.")
         
        switch_button = ctk.CTkButton(self, text="Calibrate Module",command= self.send_cali)
        switch_button.place(x=3, y=103)
        # Configure button commands  
    
        # Create input boxes for Height, Bodyweight, and Sex
        self.height_entry = ctk.CTkEntry(self)
        self.height_label = ctk.CTkLabel(self, text="Height (cm): ")
        self.height_label.place(x=1000,y=150)
        self.height_entry.place(x=1080,y=150)

        self.bodyweight_entry = ctk.CTkEntry(self)
        self.bodyweight_label = ctk.CTkLabel(self, text="Bodyweight (kg): ")
        self.bodyweight_label.place(x=975,y=180)
        self.bodyweight_entry.place(x=1080,y=180)

        self.sex_label = ctk.CTkLabel(self, text="Sex: ")
        self.sex_label.place(x=1050,y=210)
        self.sex_var = ctk.StringVar(self)
        
        sex_options = ["Male", "Female"]  # Add more options if needed
        sex_combobox = ctk.CTkComboBox(self, values=sex_options, variable=self.sex_var)
        sex_combobox.place(x=1080, y=210)
        
        input_button = ctk.CTkButton(self, text='Send Input')
        input_button.place(x=1080,y=290)
 
        # Get available serial ports
        ports = [port.device for port in port_list.comports()]
        if not ports:
            print("No serial ports available. Please check your connections.")
            exit(1)
            #warning_label = ctk.CTkLabel(self, text="No ports connected. Check your connection.", fg_color="#bf2c19")
            #warning_label.pack()
                
        # Custom styling for the option menu
        style = ttk.Style()
        style.theme_use('default')
        style.configure('Custom.TMenubutton', background='#fcb103')

        # Create a combo box for selecting the serial port
        port_label = ctk.CTkLabel(self, text="Select COM Port:")
        port_label.pack()
        
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:")
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200"]
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        # Create a subframe for the Matplotlib graph
        graph_frame = ttk.Frame(self)
        graph_frame.pack()

        # Create a Matplotlib figure and subplot
        fig = Figure(figsize=(14, 3.8), dpi=100, facecolor='dimgrey')
        self.ax = fig.add_subplot(111)  # Define self.ax here
        self.ax.set_facecolor('black')
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('Impedance(ohms)')
        self.line, = self.ax.plot([], [], 'b-')
       
        # Create a canvas for the Matplotlib figure
        canvas = FigureCanvasTkAgg(fig, master=graph_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    
        #x_data, y_data = [], []
        # Create buttons for starting and stopping data saving
        start_button = ctk.CTkButton(self, text="Start Saving Data",fg_color = "#3cb043")
        start_button.place(x=3,y=260)

        stop_button = ctk.CTkButton(self, text="Stop Saving Data", fg_color= "#bf2f24")
        stop_button.place(x=150,y=260)

        save_data = False  # Initialize the save flag
        
        def stop_reading_and_show_home(controller):
            stop_reading()
            controller.show_frame(StartPage)       

        def start_saving_data():          
            self.save_data = True

        def stop_saving_data():
            self.save_data = False
            if len(self.x_data) >= 300:  # Save data only if there are at least 1000 points
                save_to_csv()  # Save data to CSV when stop is pressed

        def save_to_csv():
            def find_folder_path(folder_name):
                for root, dirs, files in os.walk(os.path.dirname(os.path.abspath(__file__))):  # Starting from GUI directory
                    if folder_name in dirs:
                        return os.path.join(root, folder_name)

            folder_name_to_find = "Skin Impedance Data" #Replace with folder name
            folder_path = find_folder_path(folder_name_to_find)

            #folder_name = "ECG Data"
            #folder_path = os.path.dirname(folder_name)
            file_name = "Skin Impedance Data.csv"
            file_path = os.path.join(folder_path, file_name)

            print("Saving to CSV...")

            with open(file_path, "a") as file:
                if os.path.getsize(file_path) == 0:
                    file.write("Time, ECG VAL\n")

                start_index = max(0, len(self.x_data) - 1000)

                for i in range(start_index, len(self.x_data)):
                    file.write(f"{self.x_data[i]}, {self.y_data[i]}\n")

                print(f"File saved to: {os.path.abspath(file_name)}")
        
        # Configure button commands
        start_button.configure(command=start_saving_data)
        stop_button.configure(command=stop_saving_data) 

        # Define global variables
        exit_flag = False

        ser = None  # Initialize serial connection object

        def start_reading():
          self.exit_flag = False  # Access exit_flag using self
          
          ser_thread = threading.Thread(target=read_serial_data)
          ser_thread.start()
    
        # Function to stop reading serial data
        def stop_reading():
            self.exit_flag = True  # Access exit_flag using self
          # Create buttons for starting and stopping reading
        start_reading_button = ctk.CTkButton(self, text="Start Reading Data", fg_color="#3cb043", command=start_reading)
        start_reading_button.place(x=3,y=230)

        stop_reading_button = ctk.CTkButton(self, text="Stop Reading Data", fg_color="#bf2f24", command=stop_reading)
        stop_reading_button.place(x=150,y=230)

      
        # Inside the read_serial_data function
        def read_serial_data():
           global exit_flag, ser, x_data, y_data, data_counter
           ser = None  # Initialize ser here
           data_buffer_imp = []  # Buffer to accumulate impedance data points
           batch_size = 2  # Set the size for batch processing

           while not self.exit_flag:  # Access exit_flag directly
               try:
                   # Open serial connection if not open
                   if ser is None or not ser.is_open:
                       selected_port = port_var.get()
                       selected_baud = baud_var.get()
                       ser = serial.Serial(selected_port, selected_baud, timeout=1)
                       print(f"Connected to {selected_port} at {selected_baud} baud")

                   # Read incoming serial data
                   if ser.in_waiting > 0:
                       data = ser.readline().decode('utf-8').strip()
                       print(data)

                       # Check if data contains impedance values
                       if data.startswith("ImpAvg:"):
                           # Extract impedance average value
                          impedance_avg = float(data.split(":")[1])
                           # Update GUI label with the received average impedance value
                          Impeadance_label.configure(text=f"Average Impedance: {impedance_avg} ohms")
                          if impedance_avg > 0:
                           # Calculate body fat percentage
                           height_cm = float(self.height_entry.get())
                           weight_kg = float(self.bodyweight_entry.get())
                           sex = 1 if self.sex_var.get() == "Male" else 0  # Male=1, Female=0
                           #age = int(self.age_entry.get())
                           #print(age)
                           print(sex)
                           print(weight_kg)
                           print(height_cm)
                        
                           # If a better equation model was discovered for Fat-Free Mass, update below
                           FFM = (0.396*((height_cm ** 2)/(impedance_avg/200)))+(0.143*weight_kg+8.399)*1.37*2
                           BFM = weight_kg - FFM
                           body_fat_percentage = (BFM / weight_kg) * 100

                    # Update bodyFat_label with the calculated body fat percentage
                           bodyFat_label.configure(text=f"Bodyfat % {body_fat_percentage:.2f}")
                           TBW = FFM * 0.73
                           
                        # Update waterComp_label with the calculated total body water
                           waterComp_label.configure(text=f"Total Body Water: {TBW:.2f} liters")

                       if data.startswith("Imp:"):
                           # Extract impedance value
                           impedance_val = float(data.split(":")[1])
                           current_time = time.time()  # Get current time
                           data_buffer_imp.append((current_time, impedance_val))  # Append impedance data to the buffer
                           print(impedance_val)
                       # Process batched data when buffer size reaches the specified batch size
                       if len(data_buffer_imp) >= batch_size:
                           process_batch_data(data_buffer_imp)  # Process the impedance batch data
                           data_buffer_imp = []  # Clear the impedance buffer after processing

               except serial.SerialException as e:
                   print(f"Serial Exception: {e}")
                   if ser is not None:
                       ser.close()
                   time.sleep(2)

     # Inside the process_batch_data function
        def process_batch_data(data_buffer_imp):
          global x_data, y_data

          x_batch_imp, y_batch_imp = zip(*data_buffer_imp)  # Unzip impedance data batch

          self.x_data.extend(x_batch_imp)  # Update x_data with impedance values' time
          self.y_data.extend(y_batch_imp)  # Update y_data with impedance values

         # Plot impedance values or perform further processing as needed

         # Display the last 1000 points for impedance values
          max_points = 500
          if len(self.x_data) > max_points:
             self.x_data = self.x_data[-max_points:]
             self.y_data = self.y_data[-max_points:]

         # Plot impedance values
          self.line.set_data(self.x_data, self.y_data)
          self.line.set_color('Green')  # Change the color of the impedance values line to green
          self.ax.relim()
          self.ax.autoscale_view()
          canvas.draw_idle()

          self.data_counter += len(data_buffer_imp)  # Increment data counter

          status_label.configure(text="Impedance Reading in Progress...")

        # Update the legend with impedance values
        self.ax.legend(['Impedance Values'], loc='upper left', bbox_to_anchor=(1, 1))
          
        # Start reading the serial data in a separate thread
        import threading
        thread = threading.Thread(target=read_serial_data)
        thread.start()
        
    def send_cali(self):
           print("cali")
           self.send_to_esp32("Cali:1")
           
    def send_measure(self):
        # Call the method to send the signal to ESP32
        self.send_to_esp32("measure:1")
           
    def send_to_esp32(self, message):
         
        
        ser.write(message.encode('utf-8'))
        print(message) 
        logging.info(f"Sent to ESP32: {message}")
   
class Page5(ctk.CTkFrame):
    def __init__(self, parent, controller):
        ctk.CTkFrame.__init__(self, parent)
        
        self.exit_flag = True
        self.data_counter = 0
        self.x_data = np.array([])
        self.y_data = np.array([])
        self.save_data = False  # Make save_data a class attribute
        self.line_ir = None
        self.zero_ecg_time = 0
        
        label = ctk.CTkLabel(self, text="Digital Stethoscope", font=LARGEFONT)
        label.pack()
        button2 = ctk.CTkButton(self, text="MediBrick2000 Home", command=lambda: stop_reading_and_show_home(controller), fg_color="#ed9818")
        button2.place(x=3,y=0)
    
        # Create labels to display the initialization status, SPO2, and BPM
        status_label = ctk.CTkLabel(self, text="Initializing digital stethoscope module...")
        status_label.pack()

        Audio_label = ctk.CTkLabel(self, text="Audio Amplituide(DB): ", font=LARGEFONT)
        Audio_label.pack()
        
        # Get available serial ports
        ports = [port.device for port in port_list.comports()]
        if not ports:
            print("No serial ports available. Please check your connections.")
            exit(1)
            #warning_label = ctk.CTkLabel(self, text="No ports connected. Check your connection.", fg_color="#bf2c19")
            #warning_label.pack()
                
        # Custom styling for the option menu
        style = ttk.Style()
        style.theme_use('default')
        style.configure('Custom.TMenubutton', background='#fcb103')

        # Create a combo box for selecting the serial port
        port_label = ctk.CTkLabel(self, text="Select COM Port:")
        port_label.pack()
        
        port_var = ctk.StringVar(self)
        port_menu = ctk.CTkComboBox(self, values=ports ,variable=port_var,corner_radius=10 )
        port_menu.pack()
        port_var.set(ports[0] if ports else "No ports available")

        baud_label = ctk.CTkLabel(self, text="Select Baud Rate:")
        baud_label.pack()
       
        baud_var = ctk.IntVar(self)
        # Convert baud rates to strings
        baud_values = ["9600", "19200", "38400", "57600", "115200"]
        baud_menu = ctk.CTkComboBox(self, values = baud_values ,variable = baud_var,corner_radius= 10)
        baud_menu.pack()

        # Create a subframe for the Matplotlib graph
        graph_frame = ttk.Frame(self)
        graph_frame.pack()
        
        self.switch_var = tk.IntVar(self)
        self.switch_var.set(0)  # Set the initial state to 0 (tissue sample)

        # Create the switch (Checkbutton)
        switch_label = ctk.CTkLabel(self, text="Live Audio Mode:")
        switch_label.place(x=979,y=100)

        self.switch_text_var = tk.StringVar(self)
        self.switch_text_var.set("Audio ON")

        switch = ctk.CTkSwitch(self, textvariable=self.switch_text_var, variable=self.switch_var, onvalue=0, offvalue=1,
                               command=self.update_switch_text)
        switch.place(x=1080,y=103)

        # Create a Matplotlib figure and subplot
        fig = Figure(figsize=(14, 3.8), dpi=100, facecolor='dimgrey')
        self.ax = fig.add_subplot(111)  # Define self.ax here
        self.ax.set_facecolor('black')
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('dB SPL')
        self.line, = self.ax.plot([], [], 'b-')
       
        # Create a canvas for the Matplotlib figure
        canvas = FigureCanvasTkAgg(fig, master=graph_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Create an instance of the toolbar
        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.pack()
        toolbar.update()
        
        # Create buttons for starting and stopping data saving
        start_rec = ctk.CTkButton(self, text="Start Recording",fg_color = "#3cb043")
        start_rec.place(x=900,y=200)

        stop_rec = ctk.CTkButton(self, text="Stop Recording", fg_color= "#bf2f24")
        stop_rec.place(x=1050,y=200)


        #x_data, y_data = [], []
        # Create buttons for starting and stopping data saving
        start_button = ctk.CTkButton(self, text="Start Saving Data",fg_color = "#3cb043")
        start_button.place(x=3,y=200)

        stop_button = ctk.CTkButton(self, text="Stop Saving Data", fg_color= "#bf2f24")
        stop_button.place(x=150,y=200)

        save_data = False  # Initialize the save flag
        
        def stop_reading_and_show_home(controller):
            stop_reading()
            controller.show_frame(StartPage)     

        def start_saving_data():          
            self.save_data = True

        def stop_saving_data():
            self.save_data = False
            if len(self.x_data) >= 300:  # Save data only if there are at least 1000 points
                save_to_csv()  # Save data to CSV when stop is pressed

        def save_to_csv():
            def find_folder_path(folder_name):
                for root, dirs, files in os.walk(os.path.dirname(os.path.abspath(__file__))):  # Starting from GUI directory
                    if folder_name in dirs:
                        return os.path.join(root, folder_name)

            folder_name_to_find = "Digital Stethoscope Data" #Replace with folder name
            folder_path = find_folder_path(folder_name_to_find)

            file_name = "Audio.csv"
            file_path = os.path.join(folder_path, file_name)

            print("Saving to CSV...")

            with open(file_path, "a") as file:
                if os.path.getsize(file_path) == 0:
                    file.write("Time, ECG VAL\n")

                start_index = max(0, len(self.x_data) - 1000)

                for i in range(start_index, len(self.x_data)):
                    file.write(f"{self.x_data[i]}, {self.y_data[i]}\n")

                print(f"File saved to: {os.path.abspath(file_name)}")
        
        # Configure button commands
        start_button.configure(command=start_saving_data)
        stop_button.configure(command=stop_saving_data) 

        # Define global variables
        exit_flag = False

        ser = None  # Initialize serial connection object

        def start_reading():
          self.exit_flag = False  # Access exit_flag using self
          
          ser_thread = threading.Thread(target=read_serial_data)
          ser_thread.start()
    
        # Function to stop reading serial data
        def stop_reading():
            self.exit_flag = True  # Access exit_flag using self
        
        # Create buttons for starting and stopping reading
        start_reading_button = ctk.CTkButton(self, text="Start Reading Data", fg_color="#3cb043", command=start_reading)
        start_reading_button.place(x=3,y=150)

        stop_reading_button = ctk.CTkButton(self, text="Stop Reading Data", fg_color="#bf2f24", command=stop_reading)
        stop_reading_button.place(x=150,y=150)
        #function to receive the ECG data
        def update_audio_label(audio_value):
         Audio_label.configure(text=f"Audio: {audio_value}")  # Update label text with ECG value
       # Inside the read_serial_data function
        def read_serial_data():
         global exit_flag, ser, x_data, y_data, data_counter
         ser = None  # Initialize ser here
         data_buffer_audio = []  # Buffer to accumulate ECG data points
         batch_size = 100  # Set the size for batch processing
         start_time = time.time()  # Variable to store the start time

         while not self.exit_flag:  # Access exit_flag directly
          try:
            # Open serial connection if not open
            if ser is None or not ser.is_open:
                selected_port = port_var.get()
                selected_baud = baud_var.get()
                ser = serial.Serial(selected_port, selected_baud, timeout=1)
                print(f"Connected to {selected_port} at {selected_baud} baud")
         
                # Read incoming serial data
            if ser.in_waiting > 0:
                    data = ser.readline().decode('utf-8').strip()
                    #print(data)
                    # Check if data contains audio values
                    if data:
                     try:
                        # Parse audio value (assuming one value per line)
                        audio_value = int(data)
                        current_time = time.time() - start_time
                        data_buffer_audio.append((current_time, audio_value))  # Append audio data to the buffer
                        # Update label with the received value
                        update_audio_label(audio_value)
                     except ValueError:
                        # Handle non-numeric data gracefully
                        print("Skipping non-numeric data:", data)
                    # Process batched data when buffer size reaches the specified batch size
                    if len(data_buffer_audio) >= batch_size:
                        process_batch_data(data_buffer_audio)  # Process the audio batch data
                        data_buffer_audio = []  # Clear the audio buffer after processing
                    # Handle other messages or initialization here
          except serial.SerialException as e:
                print(f"Serial Exception: {e}")
                if ser is not None:
                    ser.close()
                time.sleep(2)

# Inside the process_batch_data function
        def process_batch_data(data_buffer_audio):
          global x_data, y_data
# Update x_data and y_data using NumPy array concatenation
          self.x_data = np.concatenate([self.x_data, np.array(data_buffer_audio)[:, 0]])
          self.y_data = np.concatenate([self.y_data, np.array(data_buffer_audio)[:, 1]])

    # Plot ECG values or perform further processing as needed

    # Display the last 1000 points for ECG values
          max_points = 20000
          if len(self.x_data) > max_points:
           self.x_data = self.x_data[-max_points:]
           self.y_data = self.y_data[-max_points:]
           
            # Plot audio values using NumPy arrays
          self.line.set_data(self.x_data, self.y_data)

    # Plot audio values
          self.line.set_data(self.x_data, self.y_data)
          self.line.set_color('blue')  # Change the color of the audio values line to blue
          self.ax.relim()
          self.ax.autoscale_view()
          canvas.draw_idle()

          self.data_counter += len(data_buffer_audio)  # Increment data counter
          
          status_label.configure(text="audio in Operation...")
     
    # Update the legend with ECG values
          self.ax.legend(['Audio amp'], loc='upper left', bbox_to_anchor=(1, 1))
          
          # Start reading the serial data in a separate thread
        import threading
        thread = threading.Thread(target=read_serial_data)
        thread.start()
        
        
    def update_switch_text(self):
            if self.switch_var.get() == 0:
                self.switch_text_var.set("Audio ON")
                
            else:
                self.switch_text_var.set("MUTE")
                
    def get_switch_state(self):
            return self.switch_var.get()    

# Driver Code
app = tkinterApp()
app.mainloop()