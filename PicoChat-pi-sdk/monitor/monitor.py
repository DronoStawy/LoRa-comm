import sys
import random
import serial
from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtUiTools import QUiLoader

if __name__ == '__main__':
    # Some code to obtain the form file name, ui_file_name
    app = QtWidgets.QApplication(sys.argv)
    ui_file = QtCore.QFile("monitor/monitor.ui")
    if not ui_file.open(QtCore.QIODevice.ReadOnly):
        print("Cannot open {}: {}".format("monitor.ui", ui_file.errorString()))
        sys.exit(-1)
    loader = QUiLoader()
    widget = loader.load(ui_file, None)
    ui_file.close()
    if not widget:
        print(loader.errorString())
        sys.exit(-1)
    widget.show()
    sys.exit(app.exec_())