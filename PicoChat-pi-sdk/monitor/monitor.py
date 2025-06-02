import sys
import glob
import serial
from serial import SerialException
from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import QFile
from monitor_ui import Ui_mainWindow

from datetime import datetime

serial_port = ""
connected_to_serial = False
baud_rate = 0

ser = serial.Serial()

def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, SerialException):
            pass
    return result
    
class MainWindow(QMainWindow):
    def __init__(self):
        global baud_rate
        
        super(MainWindow, self).__init__()
        self.ui = Ui_mainWindow()
        self.ui.setupUi(self)
        
        # ComboBox for serial ports
        self.ui.portComboBox.addItems(serial_ports())
        self.ui.portComboBox.currentIndexChanged.connect(self.on_port_selected)
        
        # Connect button for connecting/disconnecting
        self.ui.connectButton.clicked.connect(self.on_connect_button_clicked)
        
        # Baudrate selection
        baud_rates = [9600, 19200, 38400, 57600, 115200]
        self.ui.baudComboBox.addItems(map(str, baud_rates))
        self.ui.baudComboBox.setCurrentText(str(baud_rates[0]))
        self.ui.baudComboBox.currentIndexChanged.connect(self.on_baud_selected)
        baud_rate = 9600
        
        # Message box
        now = datetime.now()
        current_time = now.strftime("%H:%M:%S")
        self.ui.messagesBrowser.insertPlainText("[{}] PicoChat Monitor started\n".format(current_time))
         
        
    def on_baud_selected(self):
        global baud_rate
        baud_rate = int(self.ui.baudComboBox.currentText())
        print(f"Selected baud rate: {baud_rate}")
        if connected_to_serial:
            ser.close()
            ser.baudrate = baud_rate
            try:
                ser.open()
                print(f"Reconnected to {serial_port} with baud rate {baud_rate}")
            except SerialException as e:
                print(f"Failed to reconnect: {e}")
            
        
    def on_port_selected(self, index):
        global serial_port
        serial_port = self.ui.portComboBox.currentText()
        print(f"Selected port: {serial_port}")
        # Here you can add code to handle the selected port, e.g., open it or configure it
        
    def on_connect_button_clicked(self):
        global serial_port
        global connected_to_serial
        
        if connected_to_serial:
            # If already connected, disconnect
            print(f"Disconnecting from {serial_port}")
            self.ui.connectButton.setText("Connect")
            ser.close()
            connected_to_serial = False
        else:
            try:
                # Attempt to open the serial port
                serial_port = self.ui.portComboBox.currentText()
                ser.port = serial_port
                ser.baudrate = baud_rate
                ser.open()
                print(f"Connected to {serial_port}")
                self.ui.connectButton.setText("Disconnect")
                connected_to_serial = True
            except SerialException as e:
                print(f"Failed to connect to {serial_port}: {e}")

        
        
        
        
if __name__ == "__main__":
    app = QApplication(sys.argv)

    window = MainWindow()
    window.show()

    sys.exit(app.exec())
    


