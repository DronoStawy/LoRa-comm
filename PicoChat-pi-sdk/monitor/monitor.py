import sys
from PySide6 import QtSerialPort
from serial import SerialException
from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import QFile
from monitor_ui import Ui_mainWindow
from datetime import datetime

serial_port = None
connected_to_serial = False
baud_rate = 0

class MainWindow(QMainWindow):
    def __init__(self):
        global baud_rate
        super(MainWindow, self).__init__()
        self.ui = Ui_mainWindow()
        self.ui.setupUi(self)
        
        # ============================= Serial connection elements =============================
        # Qt serial instance
        self.qt_serial = QtSerialPort.QSerialPort(self)
        self.qt_serial_info = QtSerialPort.QSerialPortInfo()
        # UI elements
        # ComboBox for serial ports
        self.ui.portComboBox.addItems([port.portName() for port in self.qt_serial_info.availablePorts()])
        self.ui.portComboBox.currentIndexChanged.connect(self.on_port_selected)

        # Update serial ports list
        self.ui.refreshPortsButton.clicked.connect(self.serial_ports_list_update)
        # Baudrate selection
        baud_rates = [9600, 19200, 38400, 57600, 115200]
        self.ui.baudComboBox.addItems(map(str, baud_rates))
        self.ui.baudComboBox.setCurrentText(str(baud_rates[0]))
        self.ui.baudComboBox.currentIndexChanged.connect(self.on_baud_selected)
        baud_rate = 9600
        # Functions
        # Send button sends message
        self.ui.sendButton.clicked.connect(self.on_send_button_clicked)
        # Connect button connects to port
        self.ui.connectButton.clicked.connect(self.on_connect_button_clicked)
        # When serial data is available, read it
        self.qt_serial.readyRead.connect(self.on_ready_read)
        # ==========================================================================================
        
        # =================================== Text messages window  ================================
        # Message box
        now = datetime.now()
        current_time = now.strftime("%H:%M:%S")
        self.ui.messagesBrowser.append("[{}] PicoChat Monitor started\n".format(current_time))
        # ==========================================================================================
        
        # =================================== Debug messages window ================================
        
        # ==========================================================================================
        
    def on_send_button_clicked(self):
        global connected_to_serial
        if connected_to_serial:
            message = self.ui.messageEdit.toPlainText()
            if message:
                self.qt_serial.write(message.encode('utf-8'))
                self.ui.messageEdit.clear()
        else:
            print("Not connected to any serial port.")
        
    def on_ready_read(self):
        serial_data = self.qt_serial.readAll()
        if serial_data:
            text = serial_data.data().decode()
            self.ui.messagesBrowser.append(text)
    
    def serial_ports_list_update(self):
        ports = [port.portName() for port in self.qt_serial_info.availablePorts()]
        self.ui.portComboBox.clear()
        self.ui.portComboBox.addItems(ports)
        print("Updated serial ports list:", ports)     
        
    def on_baud_selected(self):
        global baud_rate
        baud_rate = int(self.ui.baudComboBox.currentText())
        print(f"Selected baud rate: {baud_rate}")
        if connected_to_serial:
            self.qt_serial.close()
            self.qt_serial.setBaudRate(baud_rate)
            try:
                self.qt_serial.open(QtSerialPort.QSerialPort.ReadWrite)
                print(f"Reconnected to {serial_port.portName()} with baud rate {baud_rate}")
            except SerialException as e:
                print(f"Failed to reconnect: {e}")
        
    def on_port_selected(self, index):
        global serial_port
        serial_port = self.qt_serial_info.availablePorts()[index]
        self.qt_serial.setPort(serial_port)
        # Here you can add code to handle the selected port, e.g., open it or configure it
        
    def on_connect_button_clicked(self):
        global serial_port
        global connected_to_serial
        
        if connected_to_serial:
            # If already connected, disconnect
            print(f"Disconnecting from {serial_port.portName()}")
            self.ui.connectButton.setText("Connect")
            self.qt_serial.close()
            connected_to_serial = False
        else:
            try:
                # Attempt to open the serial port
                index = self.ui.portComboBox.currentIndex()
                serial_port = self.qt_serial_info.availablePorts()[index]
                self.qt_serial.setPort(serial_port)
                self.qt_serial.setBaudRate(baud_rate)
                self.qt_serial.open(QtSerialPort.QSerialPort.ReadWrite)
                print(f"Connected to {serial_port.portName()} at {baud_rate} baud")
                self.ui.connectButton.setText("Disconnect")
                connected_to_serial = True
            except SerialException as e:
                print(f"Failed to connect to {serial_port}: {e}")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()

    sys.exit(app.exec())
    


