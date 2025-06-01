# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'monitor.ui'
##
## Created by: Qt User Interface Compiler version 6.9.0
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QComboBox, QGroupBox, QListWidget,
    QListWidgetItem, QMainWindow, QMenuBar, QPlainTextEdit,
    QPushButton, QSizePolicy, QStatusBar, QTextBrowser,
    QWidget)

class Ui_mainWindow(object):
    def setupUi(self, mainWindow):
        if not mainWindow.objectName():
            mainWindow.setObjectName(u"mainWindow")
        mainWindow.resize(800, 600)
        mainWindow.setMinimumSize(QSize(800, 600))
        mainWindow.setMaximumSize(QSize(800, 600))
        self.centralwidget = QWidget(mainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.plainTextEdit = QPlainTextEdit(self.centralwidget)
        self.plainTextEdit.setObjectName(u"plainTextEdit")
        self.plainTextEdit.setGeometry(QRect(10, 500, 631, 41))
        self.pushButton = QPushButton(self.centralwidget)
        self.pushButton.setObjectName(u"pushButton")
        self.pushButton.setGeometry(QRect(650, 500, 141, 41))
        self.groupBox = QGroupBox(self.centralwidget)
        self.groupBox.setObjectName(u"groupBox")
        self.groupBox.setGeometry(QRect(650, 310, 141, 181))
        self.pushButton_2 = QPushButton(self.groupBox)
        self.pushButton_2.setObjectName(u"pushButton_2")
        self.pushButton_2.setGeometry(QRect(10, 21, 121, 41))
        self.pushButton_3 = QPushButton(self.groupBox)
        self.pushButton_3.setObjectName(u"pushButton_3")
        self.pushButton_3.setGeometry(QRect(10, 60, 121, 41))
        self.pushButton_6 = QPushButton(self.groupBox)
        self.pushButton_6.setObjectName(u"pushButton_6")
        self.pushButton_6.setGeometry(QRect(10, 100, 121, 41))
        self.pushButton_7 = QPushButton(self.groupBox)
        self.pushButton_7.setObjectName(u"pushButton_7")
        self.pushButton_7.setGeometry(QRect(10, 140, 121, 41))
        self.groupBox_2 = QGroupBox(self.centralwidget)
        self.groupBox_2.setObjectName(u"groupBox_2")
        self.groupBox_2.setGeometry(QRect(9, -1, 311, 491))
        self.textBrowser = QTextBrowser(self.groupBox_2)
        self.textBrowser.setObjectName(u"textBrowser")
        self.textBrowser.setGeometry(QRect(0, 20, 311, 471))
        self.groupBox_3 = QGroupBox(self.centralwidget)
        self.groupBox_3.setObjectName(u"groupBox_3")
        self.groupBox_3.setGeometry(QRect(330, 0, 311, 491))
        self.textBrowser_2 = QTextBrowser(self.groupBox_3)
        self.textBrowser_2.setObjectName(u"textBrowser_2")
        self.textBrowser_2.setGeometry(QRect(0, 20, 311, 471))
        self.groupBox_4 = QGroupBox(self.centralwidget)
        self.groupBox_4.setObjectName(u"groupBox_4")
        self.groupBox_4.setGeometry(QRect(650, 0, 141, 171))
        self.listWidget = QListWidget(self.groupBox_4)
        self.listWidget.setObjectName(u"listWidget")
        self.listWidget.setGeometry(QRect(0, 20, 141, 151))
        self.groupBox_5 = QGroupBox(self.centralwidget)
        self.groupBox_5.setObjectName(u"groupBox_5")
        self.groupBox_5.setGeometry(QRect(650, 180, 141, 131))
        self.pushButton_4 = QPushButton(self.groupBox_5)
        self.pushButton_4.setObjectName(u"pushButton_4")
        self.pushButton_4.setGeometry(QRect(10, 60, 121, 31))
        self.pushButton_5 = QPushButton(self.groupBox_5)
        self.pushButton_5.setObjectName(u"pushButton_5")
        self.pushButton_5.setGeometry(QRect(10, 90, 121, 31))
        self.comboBox = QComboBox(self.groupBox_5)
        self.comboBox.setObjectName(u"comboBox")
        self.comboBox.setGeometry(QRect(0, 30, 141, 32))
        mainWindow.setCentralWidget(self.centralwidget)
        self.menubar = QMenuBar(mainWindow)
        self.menubar.setObjectName(u"menubar")
        self.menubar.setGeometry(QRect(0, 0, 800, 24))
        mainWindow.setMenuBar(self.menubar)
        self.statusbar = QStatusBar(mainWindow)
        self.statusbar.setObjectName(u"statusbar")
        mainWindow.setStatusBar(self.statusbar)

        self.retranslateUi(mainWindow)

        QMetaObject.connectSlotsByName(mainWindow)
    # setupUi

    def retranslateUi(self, mainWindow):
        mainWindow.setWindowTitle(QCoreApplication.translate("mainWindow", u"PicoChat Monitor", None))
        self.pushButton.setText(QCoreApplication.translate("mainWindow", u"Send", None))
        self.groupBox.setTitle(QCoreApplication.translate("mainWindow", u"Quick Actions", None))
        self.pushButton_2.setText(QCoreApplication.translate("mainWindow", u"Reboot", None))
        self.pushButton_3.setText(QCoreApplication.translate("mainWindow", u"Flash", None))
        self.pushButton_6.setText(QCoreApplication.translate("mainWindow", u"PushButton", None))
        self.pushButton_7.setText(QCoreApplication.translate("mainWindow", u"PushButton", None))
        self.groupBox_2.setTitle(QCoreApplication.translate("mainWindow", u"Text messages", None))
        self.groupBox_3.setTitle(QCoreApplication.translate("mainWindow", u"Debug", None))
        self.groupBox_4.setTitle(QCoreApplication.translate("mainWindow", u"Chat users", None))
        self.groupBox_5.setTitle(QCoreApplication.translate("mainWindow", u"Serial connection", None))
        self.pushButton_4.setText(QCoreApplication.translate("mainWindow", u"Connect", None))
        self.pushButton_5.setText(QCoreApplication.translate("mainWindow", u"Disconnect", None))
    # retranslateUi

