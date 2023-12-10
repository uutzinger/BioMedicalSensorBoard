################################################################################################
# Serial Communication GUI
# ========================
# Provides serial interface to send and receive text to/from serial port similar to Arduino IDE.
# Plot of numbers on up to 4 traces with zoom, save and clear
# This framework was setup can visualize signals at high data rates.
# 
# Urs Utzinger, 2022, 2023
# University of Arizona
################################################################################################

# QT imports
from PyQt5 import QtCore, QtWidgets, QtGui, uic
from PyQt5.QtCore import QThread
from PyQt5.QtWidgets import QMainWindow, QLineEdit, QSlider, QMessageBox
from PyQt5.QtGui import QIcon

# System
import logging
import os

# Custom imports
from helpers.Qserial_helper     import QSerial, QSerialUI
from helpers.Qgraph_helper      import QChartUI, MAX_ROWS

# QT
# Deal with high resolution displays
if hasattr(QtCore.Qt, 'AA_EnableHighDpiScaling'):
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)

if hasattr(QtCore.Qt, 'AA_UseHighDpiPixmaps'):
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)

###########################################################################################
# Main Window
###########################################################################################

class mainWindow(QMainWindow):
    """
    Create the main window that stores all of the widgets necessary for the application.
    """
        
    #-------------------------------------------------------------------------------------
    # Initialize
    #-------------------------------------------------------------------------------------

    def __init__(self, parent=None):
        """
        Initialize the components of the main window.
        This will create the connections between slots and signals in both directions.
        
        Serial:
        Create serial worker and move it to separate thread.
        
        """
        super(mainWindow, self).__init__(parent) # parent constructor
        
        self.logger = logging.getLogger("Main___")

        main_dir = os.path.dirname(os.path.abspath(__file__))
        
        #----------------------------------------------------------------------------------------------------------------------
        # User Interface
        #----------------------------------------------------------------------------------------------------------------------
        self.ui = uic.loadUi('assets/mainWindow.ui', self)
        icon_path = os.path.join(main_dir, 'assets', 'serial_48.png')
        window_icon = QIcon(icon_path)
        self.setWindowIcon(QIcon(window_icon))
        self.setWindowTitle("Serial GUI")

        #----------------------------------------------------------------------------------------------------------------------
        # Serial
        #----------------------------------------------------------------------------------------------------------------------
       
        # Serial Thread 
        self.serialThread = QThread()                                                                      # create QThread object
        self.serialThread.start()                                                                          # start thread which will start worker

        # Create user interface hook for serial
        self.serialUI     = QSerialUI(ui=self.ui)                                                          # create serial user interface object

        # Create serial worker
        self.serialWorker = QSerial()                                                                      # create serial worker object

        # Connect worker / thread
        self.serialWorker.finished.connect(                 self.serialThread.quit                       ) # if worker emits finished quite worker thread
        self.serialWorker.finished.connect(                 self.serialWorker.deleteLater                ) # delete worker at some time
        self.serialThread.finished.connect(                 self.serialThread.deleteLater                ) # delete thread at some time

        # Signals from Serial to Serial-UI
        # ---------------------------------
        #self.serialWorker.textReceived.connect(             self.serialUI.on_SerialReceivedText         ) # connect text display to serial receiver signal
        self.serialWorker.linesReceived.connect(            self.serialUI.on_SerialReceivedLines         ) # connect text display to serial receiver signal
        self.serialWorker.newPortListReady.connect(         self.serialUI.on_newPortListReady            ) # connect new port list to its ready signal
        self.serialWorker.newBaudListReady.connect(         self.serialUI.on_newBaudListReady            ) # connect new baud list to its ready signal
        self.serialWorker.serialStatusReady.connect(        self.serialUI.on_serialStatusReady           ) # connect display serial status to ready signal
        self.serialWorker.throughputReady.connect(          self.serialUI.on_throughputReceived          ) # connect display throughput status 

        # Signals from Serial-UI to Serial
        # ---------------------------------
        self.serialUI.changePortRequest.connect(            self.serialWorker.on_changePortRequest       ) # connect changing port
        self.serialUI.closePortRequest.connect(             self.serialWorker.on_closePortRequest        ) # connect close port
        self.serialUI.changeBaudRequest.connect(            self.serialWorker.on_changeBaudRateRequest   ) # connect changing baudrate
        self.serialUI.changeLineTerminationRequest.connect( self.serialWorker.on_changeLineTerminationRequest) # connect changing baudrate
        self.serialUI.scanPortsRequest.connect(             self.serialWorker.on_scanPortsRequest        ) # connect request to scan ports
        self.serialUI.scanBaudRatesRequest.connect(         self.serialWorker.on_scanBaudRatesRequest    ) # connect request to scan baudrates
        self.serialUI.setupReceiverRequest.connect(         self.serialWorker.on_setupReceiverRequest    ) # connect start receiver
        self.serialUI.startReceiverRequest.connect(         self.serialWorker.on_startReceiverRequest    ) # connect start receiver
        self.serialUI.stopReceiverRequest.connect(          self.serialWorker.on_stopReceiverRequest     ) # connect start receiver
        self.serialUI.sendTextRequest.connect(              self.serialWorker.on_sendTextRequest         ) # connect sending text
        self.serialUI.sendLineRequest.connect(              self.serialWorker.on_sendLineRequest         ) # connect sending text
        self.serialUI.sendLinesRequest.connect(             self.serialWorker.on_sendLinesRequest        ) # connect sending lines of text
        self.serialUI.serialStatusRequest.connect(          self.serialWorker.on_serialStatusRequest     ) # connect request for serial status
        self.serialUI.finishWorkerRequest.connect(          self.serialWorker.on_stopWorkerRequest       ) # connect finish request
        self.serialUI.startThroughputRequest.connect(       self.serialWorker.on_startThroughputRequest  ) # start throughput
        self.serialUI.stopThroughputRequest.connect(        self.serialWorker.on_stopThroughputRequest   ) # stop throughput

        # Prepare the Serial Worker and User Interface
        # --------------------------------------------
        self.serialWorker.moveToThread(                     self.serialThread                             ) # move worker to thread
        self.serialUI.scanPortsRequest.emit()                                                               # request to scan for serial ports
        self.serialUI.setupReceiverRequest.emit()                                                           # establishes QTimer in the QThread above
        self.serialUI.on_comboBoxDropDown_LineTermination()                                                 # initialize line termination, use current text from comboBox
        # do not initialize baudrate or serial port, user will need to select at startup

        # Signals from User Interface to Serial-UI
        # ----------------------------------------
        # User selected port or baud 
        self.ui.comboBoxDropDown_SerialPorts.currentIndexChanged.connect(
                                                            self.serialUI.on_comboBoxDropDown_SerialPorts )
        self.ui.comboBoxDropDown_BaudRates.currentIndexChanged.connect(
                                                            self.serialUI.on_comboBoxDropDown_BaudRates   )
        # User changed line termination
        self.ui.comboBoxDropDown_LineTermination.currentIndexChanged.connect( 
                                                            self.serialUI.on_comboBoxDropDown_LineTermination )
        # User clicked scan ports, start send, clear or save
        self.ui.pushButton_SerialScan.clicked.connect(      self.serialUI.on_pushButton_SerialScan        ) # Scan for ports
        self.ui.pushButton_SerialStartStop.clicked.connect( self.serialUI.on_pushButton_SerialStartStop   ) # Start/Stop serial receive
        self.ui.pushButton_SerialSend.clicked.connect(      self.serialUI.on_serialMonitorSend            ) # Send text to serial port
        self.ui.lineEdit_SerialText.returnPressed.connect(  self.serialUI.on_serialMonitorSend            ) # Send text as soon as enter key is pressed
        self.ui.pushButton_SerialClearOutput.clicked.connect(
                                                            self.serialUI.on_pushButton_SerialClearOutput ) # Clear serial receive window
        self.ui.pushButton_SerialSave.clicked.connect(      self.serialUI.on_pushButton_SerialSave        ) # Save text from serial receive window

        # User hit up/down arrow in serial lineEdit
        self.shortcutUpArrow   = QtWidgets.QShortcut(QtGui.QKeySequence.MoveToPreviousLine, self.ui.lineEdit_SerialText, self.serialUI.on_serialMonitorSendUpArrowPressed)
        self.shortcutDownArrow = QtWidgets.QShortcut(QtGui.QKeySequence.MoveToNextLine,     self.ui.lineEdit_SerialText, self.serialUI.on_serialMonitorSendDownArrowPressed)

        # Done with Serial
        self.logger.log(logging.INFO, "[{}]: serial initialized.".format(int(QThread.currentThreadId())))

        #----------------------------------------------------------------------------------------------------------------------
        # Serial Plotter
        #----------------------------------------------------------------------------------------------------------------------
        # Create user interface hook for chart plotting
        self.chartUI  = QChartUI(ui=self.ui, serialUI=self.serialUI, serialWorker = self.serialWorker) # create chart user interface object

        self.ui.pushButton_ChartStartStop.clicked.connect(  self.chartUI.on_pushButton_StartStop         )
        self.ui.pushButton_ChartClear.clicked.connect(      self.chartUI.on_pushButton_Clear             )
        self.ui.pushButton_ChartSave.clicked.connect(       self.chartUI.on_pushButton_Save              )

        self.ui.comboBoxDropDown_DataSeparator.currentIndexChanged.connect(
                                                            self.chartUI.on_changeDataSeparator          )        

        # Horizontal Zoom
        self.horizontalSlider_Zoom = self.ui.findChild(QSlider, "horizontalSlider_Zoom")
        self.horizontalSlider_Zoom.setMinimum(0)  
        self.horizontalSlider_Zoom.setMaximum(MAX_ROWS)
        self.horizontalSlider_Zoom.valueChanged.connect(    self.chartUI.on_HorizontalSliderChanged     )

        self.lineEdit_Zoom = self.ui.findChild(QLineEdit, "lineEdit_Horizontal")
        self.lineEdit_Zoom.returnPressed.connect( self.chartUI.on_HorizontalLineEditChanged )

        # Done with Plotter
        self.logger.log(logging.INFO, "[{}]: plotter initialized.".format(int(QThread.currentThreadId())))
        
        #----------------------------------------------------------------------------------------------------------------------
        # Menu Bar
        #----------------------------------------------------------------------------------------------------------------------
        # Connect the action_about action to the show_about_dialog slot
        self.ui.action_About.triggered.connect(self.show_about_dialog)
        
        #----------------------------------------------------------------------------------------------------------------------
        # Finish up
        #----------------------------------------------------------------------------------------------------------------------
        self.show() 


    def show_about_dialog(self):
        # Information to be displayed
        info_text = "Serial Terminal & Plotter\nVersion: 1.0\nAuthor: Urs Utzinger\n2022,2023"
        # Create and display the MessageBox
        QMessageBox.about(self, "About Program", info_text)
        
###########################################################################################
# Testing Main Window
###########################################################################################

if __name__ == "__main__":
    import sys
    from PyQt5 import QtWidgets
    import logging
    
    # set logging level
    # CRITICAL  50
    # ERROR     40
    # WARNING   30
    # INFO      20
    # DEBUG     10
    # NOTSET     0
    logging.basicConfig(level=logging.INFO)
    
    app = QtWidgets.QApplication(sys.argv)
    
    win = mainWindow()
    d = app.desktop()
    scalingX = d.logicalDpiX()/96.0
    scalingY = d.logicalDpiY()/96.0
    win.resize(int(1200*scalingX), int(620*scalingY))
    win.show()
    sys.exit(app.exec_())
    
