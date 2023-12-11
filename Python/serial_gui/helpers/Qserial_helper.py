############################################################################################
# QT Serial Helper
############################################################################################
# July 2022: initial work
# December 2023: moved to serial only example, implemented line reading and line termination
# ------------------------------------------------------------------------------------------
# Urs Utzinger
# University of Arizona 2023
############################################################################################

############################################################################################
# Helpful readings:
# ------------------------------------------------------------------------------------------
# Signals and Slots and Threads
#      https://realpython.com/python-pyqt-qthread/
#      https://www.tutorialspoint.com/pyqt/pyqt_signals_and_slots.htm
#      http://blog.debao.me/2013/08/how-to-use-qthread-in-the-right-way-part-1/
#   Examples
#      https://stackoverflow.com/questions/41026032/pyqt5-how-to-send-a-signal-to-a-worker-thread
#      https://stackoverflow.com/questions/68163578/stopping-an-infinite-loop-in-a-worker-thread-in-pyqt5-the-simplest-way
#      https://stackoverflow.com/questions/61625043/threading-with-qrunnable-proper-manner-of-sending-bi-directional-callbacks
#      https://stackoverflow.com/questions/52973090/pyqt5-signal-communication-between-worker-thread-and-main-window-is-not-working
#      https://stackoverflow.com/questions/61625043/threading-with-qrunnable-proper-manner-of-sending-bi-directional-callbacks
#
# Timer, infinite loop
#      https://stackoverflow.com/questions/55651718/how-to-use-a-qtimer-in-a-separate-qthread
#      https://programmer.ink/think/no-event-loop-or-use-of-qtimer-in-non-gui-qt-threads.html
#      https://stackoverflow.com/questions/47661854/use-qtimer-to-run-functions-in-an-infinte-loop
#      https://stackoverflow.com/questions/10492480/starting-qtimer-in-a-qthread
#      https://www.pythonfixing.com/2022/03/fixed-how-to-use-qtimer-inside-qthread.html
#      https://stackoverflow.com/questions/23607294/qtimer-in-worker-thread
#      https://stackoverflow.com/questions/60649644/how-to-properly-stop-qtimer-from-another-thread
#
# Serial
#   Examples using pySerial
#      https://programmer.group/python-uses-pyqt5-to-write-a-simple-serial-assistant.html
#      https://github.com/mcagriaksoy/Serial-Communication-GUI-Program/blob/master/qt.py
#      https://hl4rny.tistory.com/433
#      https://iosoft.blog/pyqt-serial-terminal-code/
#   Examples using QSerialPort
#      https://stackoverflow.com/questions/55070483/connect-to-serial-from-a-pyqt-gui
#      https://ymt-lab.com/en/post/2021/pyqt5-serial-monitor/
#
# textIOWrapper for end of line character handling
#      http://pyserial.readthedocs.io/en/latest/shortintro.html#eol
#      https://docs.python.org/3/library/io.html#io.TextIOWrapper
############################################################################################

from serial import Serial as sp
from serial import EIGHTBITS, PARITY_NONE, STOPBITS_ONE
from serial.tools import list_ports 

import io
import time, logging
from math import ceil
from enum import Enum

try:
    import debugpy
    DEBUGPY_ENABLED = True
except ImportError:
    DEBUGPY_ENABLED = False
    

from PyQt5.QtCore import QObject, QTimer, QThread, pyqtSignal, pyqtSlot, QStandardPaths
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QTextCursor

from PyQt5.QtWidgets import QFileDialog

# Constants
########################################################################################
DEFAULT_BAUDRATE          = 115200
MAX_TEXTBROWSER_LENGTH    = 1024*1024   # display window character length is trimmed to this length
                                        # lesser value results in better performance
MAX_LINE_LENGTH           = 1024        # number of characters after which an end of line characters is expected
RECEIVER_FINISHCOUNT      = 10          # [times] If we encountered a timeout 10 times we slow down serial polling
NUM_LINES_COLLATE         = 10          # [lines] estimated number of lines to collate before emitting signal
                                        #   this results in collating about NUM_LINES_COLLATE * 32 bytes in a list of lines
                                        #   plotting and processing large amounts of data is more efficient for display and plotting 

NEW_READLINE = True

class SerialReceiverState(Enum):
    """ 
    When data is expected on the serial input we use a QT timer to read line by line.
    When no data is expected we are in stopped state
    When data is expected but has not yet arrived we are in awaiting state
    When data has arrived and there might be more data arriving we are in receiving state
    """
    stopped               =  0
    awaitingData          =  1
    receivingData         =  2
    
############################################################################################
# QSerial interaction with Graphical User Interface
# This section contains routines that can not be moved to a separate thread 
# because it interacts with the QT User Interface.
# The Serial Worker is in a separate thread and receives data through signals from this class
############################################################################################

class QSerialUI(QObject):
    """
    Serial Interface for QT
    
    Signals (to be emitted by UI)
        scanPortsRequest                 request that QSerial is scanning for ports
        scanBaudRatesRequest             request that QSerial is scanning for baudrates
        changePortRequest                request that QSerial is changing port
        changeLineTerminationRequest     request that QSerial line termination is changed
        sendTextRequest                  request that provided text is transmitted over serial TX
        sendLineRequest                  request that provided line of text is transmitted over serial TX
        sendLinesRequest                 request that provided lines of text are transmitted over serial TX
        setupReceiverRequest             request that QTimer for receiver and QTimer for throughput is created
        startReceiverRequest             request that QTimer for receiver is started
        stopReceiverRequest              request that QTimer for receiver is stopped
        startThroughputRequest           request that QTimer for throughput is started
        stopThroughputRequest            request that QTimer for throughput is stopped
        serialStatusRequest              request that QSerial reports current port, baudrate, line termination, encoding, timeout
        finishWorkerRequest              request that QSerial worker is finished
        closePortRequest                 request that QSerial closes current port
        
    Slots (functions available to respond to external signals)
        on_serialMonitorSend                 transmit text from UI to serial TX line
        on_serialMonitorSendUpArrowPressed   recall previous line of text from serial TX line buffer
        on_serialMonitorSendDownArrowPressed recall next line of text from serial TX line buffer
        on_pushButton_SerialClearOutput      clear the text display window
        on_pushButton_SerialStartStop        start/stop serial receiver and throughput timer
        on_pushButton_SerialSave             save text from display window into text file
        on_pushButton_SerialScan             update serial port list
        on_comboBoxDropDown_SerialPorts      user selected a new port on the drop down list
        on_comboBoxDropDown_BaudRates        user selected a different baudrate on drop down list
        on_comboBoxDropDown_LineTermination  user selected a different line termination from drop down menu
        on_serialStatusReady(str, int, str, str, float) pickup QSerial status on port, baudrate, line termination, encoding, timeout
        on_newPortListReady(list, list)      pickup new list of serial ports
        on_newBaudListReady(tuple)           pickup new list of baudrates
        on_SerialReceivedText(bytearray)     pickup text from serial port
        on_SerialReceivedLines(list)         pickup lines of text from serial port
        on_throughputReceived(int, int)      pickup throughput data from QSerial
    """
    
    # Signals
    ########################################################################################

    scanPortsRequest             = pyqtSignal()                                            # port scan
    scanBaudRatesRequest         = pyqtSignal()                                            # baudrates scan
    changePortRequest            = pyqtSignal(str, int)                                    # port and baudrate to change
    changeBaudRequest            = pyqtSignal(int)                                         # request serial baud rate to change
    changeLineTerminationRequest = pyqtSignal(str)                                         # request line termination to change
    sendTextRequest              = pyqtSignal(str)                                         # request to transmit text to TX
    sendLineRequest              = pyqtSignal(str)                                         # request to transmit one line of text to TX
    sendLinesRequest             = pyqtSignal(list)                                        # request to transmit lines of text to TX
    startReceiverRequest         = pyqtSignal()                                            # start serial receiver, expecting text
    stopReceiverRequest          = pyqtSignal()                                            # stop serial receiver
    setupReceiverRequest         = pyqtSignal()                                            # start serial receiver, expecting text
    startThroughputRequest       = pyqtSignal()                                            # start timer to report throughput
    stopThroughputRequest        = pyqtSignal()                                            # stop timer to report throughput
    serialStatusRequest          = pyqtSignal()                                            # request serial port and baudrate status
    finishWorkerRequest          = pyqtSignal()                                            # request worker to finish
    closePortRequest             = pyqtSignal()                                            # close the current serial Port
    serialSendFileRequest        = pyqtSignal(str)                                         # request to open file and send over serial port
           
    def __init__(self, parent=None, ui=None, worker=None):

        super(QSerialUI, self).__init__(parent)

        # state variables, populated by service routines
        self.defaultBaudRate = DEFAULT_BAUDRATE
        self.BaudRates             = []                                                    # e.g. (1200, 2400, 9600, 115200)
        self.serialPortNames       = []                                                    # human readable
        self.serialPorts           = []                                                    # e.g. COM6
        self.serialPort            = ""                                                    # e.g. COM6
        self.serialBaudRate        = -1                                                    # e.g. 115200
        self.serialSendHistory     = []                                                    # previously sent text (e.g. commands)
        self.serialSendHistoryIndx = -1                                                    # init history
        self.lastNumReceived       = 0                                                     # init throughput            
        self.lastNumSent           = 0                                                     # init throughput
        self.rx                    = 0                                                     # init throughput
        self.tx                    = 0                                                     # init throughput 
        self.receiverIsRunning     = False                                                 # keep track of worker state
        self.textLineTerminator    = ''                                                    # default line termination
        self.encoding              = 'utf-8'                                               # default encoding
        self.serialTimeout         = 0                                                     # default timeout    
        
        self.logger = logging.getLogger("QSerUI_")           
                   
        if ui is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to User Interface".format(int(QThread.currentThreadId())))
        self.ui = ui

        if worker is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to serial worker signals".format(int(QThread.currentThreadId())))
        self.serialWorker =  worker

        # Text display window on serial text display
        self.textScrollbar = self.ui.plainTextEdit_SerialTextDisplay.verticalScrollBar()
        self.ui.plainTextEdit_SerialTextDisplay.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)
            
        # Limit the amount of text retained in the serial text display window
        # Execute a text trim function every minute
        self.textTrimTimer = QTimer(self)
        self.textTrimTimer.timeout.connect(self.serialTextDisplay_trim)
        self.textTrimTimer.start(10000)  # Trigger every 10 seconds
      
        self.logger.log(logging.INFO, "[{}]: QSerialUI initialized.".format(int(QThread.currentThreadId())))

    # Response Functions to User Interface Signals
    ########################################################################################

    @pyqtSlot()
    def on_serialMonitorSend(self):
        """
        Transmitting text from UI to serial TX line
        """
        text = self.ui.lineEdit_SerialText.text()                                          # obtain text from send input window
        self.serialSendHistory.append(text)                                                # keep history of previously sent commands
        if self.receiverIsRunning == False:
            self.serialWorker.linesReceived.connect(self.on_SerialReceivedLines) # connect text display to serial receiver signal
            self.serialWorker.textReceived.connect(self.on_SerialReceivedText) # connect text display to serial receiver signal            
            self.startReceiverRequest.emit()
            self.startThroughputRequest.emit()
            self.ui.pushButton_SerialStartStop.setText("Stop")
        self.sendLineRequest.emit(text)
        self.ui.lineEdit_SerialText.clear()

    @pyqtSlot()
    def on_serialSendFile(self):
        """
        Transmitting file to serial TX line
        """
        stdFileName = QStandardPaths.writableLocation(QStandardPaths.DocumentsLocation) + "/QSerial.txt"
        fname, _ = QFileDialog.getOpenFileName(self.ui, 'Open', stdFileName, "Text files (*.txt)")
        if fname:
            self.serialSendFileRequest.emit(fname)

    @pyqtSlot()
    def on_serialMonitorSendUpArrowPressed(self):
        """ 
        Handle special keys on lineEdit: UpArrow
        """
        # increment history pointer
        self.serialSendHistoryIndx += 1
        # if pointer at end of buffer restart at -1
        if self.serialSendHistoryIndx == len(self.serialSendHistory):
            self.serialSendHistoryIndx = -1
        # populate with previous sent command from history buffer
        # if index is -1, use empty string as previously sent command
        if self.serialSendHistoryIndx == -1:
            self.ui.lineEdit_SerialText.setText("")
        else: 
            self.ui.lineEdit_SerialText.setText(self.serialSendHistory[self.serialSendHistoryIndx])

    @pyqtSlot()
    def on_serialMonitorSendDownArrowPressed(self):
        """ 
        Handle special keys on lineEdit: DownArrow
        """
        # increment history pointer
        self.serialSendHistoryIndx -= 1
        # if pointer at start of buffer reset index to end of buffer 
        if self.serialSendHistoryIndx == -2:
            self.serialSendHistoryIndx = len(self.serialSendHistory) - 1
        # populate with previous sent command from history buffer
        # if index is -1, use empty string as previously sent command
        if self.serialSendHistoryIndx == -1:
            self.ui.lineEdit_SerialText.setText("")
        else: 
            self.ui.lineEdit_SerialText.setText(self.serialSendHistory[self.serialSendHistoryIndx])

    @pyqtSlot()
    def on_pushButton_SerialClearOutput(self):
        """ 
        Clearing text display window 
        """
        self.ui.plainTextEdit_SerialTextDisplay.clear()

    @pyqtSlot()
    def on_pushButton_SerialStartStop(self):
        """
        Start serial receiver
        """

        if self.ui.pushButton_SerialStartStop.text() == "Start":
            self.ui.pushButton_SerialStartStop.setText("Stop")
            self.serialWorker.linesReceived.connect(self.on_SerialReceivedLines) # connect text display to serial receiver signal
            self.serialWorker.textReceived.connect(self.on_SerialReceivedText) # connect text display to serial receiver signal
            self.startReceiverRequest.emit()
            self.startThroughputRequest.emit()
        else:
            self.ui.pushButton_SerialStartStop.setText("Start")
            self.serialWorker.linesReceived.disconnect(self.on_SerialReceivedLines) # connect text display to serial receiver signal
            self.serialWorker.textReceived.disconnect(self.on_SerialReceivedText) # connect text display to serial receiver signal
            self.stopReceiverRequest.emit()
            self.stopThroughputRequest.emit()

    @pyqtSlot()
    def on_pushButton_SerialSave(self):
        """ 
        Saving text from display window into text file 
        """
        stdFileName = QStandardPaths.writableLocation(QStandardPaths.DocumentsLocation) + "/QSerial.txt"
        fname, _ = QFileDialog.getSaveFileName(self.ui, 'Save as', stdFileName, "Text files (*.txt)")

        if fname:
            # check if fname is valid, user can select cancel
            with open(fname, 'w') as f: f.write(self.ui.plainTextEdit_SerialTextDisplay.toPlainText())

    @pyqtSlot()
    def on_pushButton_SerialScan(self):
        """ 
        Updating serial port list
        
        Sends signal to serial worker to scan for ports
        Serial worker will create newPortList signal when completed which 
          is handled by the function on_newPortList below 
        """
        self.scanPortsRequest.emit()
        self.logger.log(logging.DEBUG, "[{}]: scanning for serial ports.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_comboBoxDropDown_SerialPorts(self):
        """ 
        User selected a new port on the drop down list 
        """
        lenSerialPorts     = len(self.serialPorts)
        lenBaudRates       = len(self.BaudRates)
        if lenSerialPorts > 0:                                                               # only continue if we have recognized serial ports
            index = self.ui.comboBoxDropDown_SerialPorts.currentIndex()
            if index == lenSerialPorts:                                                      # "None" was selected so close the port
                self.closePortRequest.emit()
                return                                                                       # do not continue
            else:
                port = self.serialPorts[index]                                               # we have valid port
                
            if lenBaudRates > 0:                                                             # if we have recognized serial baudrates
                index = self.ui.comboBoxDropDown_BaudRates.currentIndex()
                if index < lenBaudRates:                                                     # last entry is -1
                    baudrate = self.BaudRates[index]
                else:
                    baudrate = self.defaultBaudRate                                          # use default baud rate
            else: 
                baudrate = self.defaultBaudRate                                              # use default baud rate, user can change later
                
            # change port if port or baudrate changed                    
            if (port != self.serialPort):
                self.serialBaudRate = baudrate
                self.serialPort = port
                QTimer.singleShot(0,    lambda: self.changePortRequest.emit(port, baudrate)) # takes 11ms to open
                QTimer.singleShot(50,   lambda: self.scanBaudRatesRequest.emit())            # request to scan serial baudrates
                QTimer.singleShot(100,  lambda: self.serialStatusRequest.emit())             # request to report serial port status
                self.logger.log(logging.INFO, "[{}]: port {} baud {}".format(int(QThread.currentThreadId()), port, baudrate))
            elif (port == self.serialPort):
                self.logger.log(logging.INFO, "[{}]: port {} baud {}".format(int(QThread.currentThreadId()), port, baudrate))                
            elif (baudrate != self.serialBaudRate):
                self.changeBaudRequest.emit(baudrate)
                self.logger.log(logging.INFO, "[{}]: baudrate {}".format(int(QThread.currentThreadId()), baudrate))
            else: 
                self.logger.log(logging.INFO, "[{}]: port and baudrate remain the same".format(int(QThread.currentThreadId()), port, baudrate))
        else: 
            self.logger.log(logging.ERROR, "[{}]: no ports available".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_comboBoxDropDown_BaudRates(self):
        """ 
        User selected a different baudrate on drop down list 
        """
        lenBaudRates = len(self.BaudRates)
        if lenBaudRates > 0:                                                               # if we have recognized serial baudrates
            index = self.ui.comboBoxDropDown_BaudRates.currentIndex()
            if index < lenBaudRates:                                                       # last entry is -1
                baudrate = self.BaudRates[index]
            else:
                baudrate = self.defaultBaudRate                                            # use default baud rate                            
            if (baudrate != self.serialBaudRate):                                          # change baudrate if different from current
                self.changeBaudRequest.emit(baudrate)
                self.logger.log(logging.INFO, "[{}]: baudrate {}".format(int(QThread.currentThreadId()), baudrate))
            else: 
                self.logger.log(logging.INFO, "[{}]: baudrate remains the same".format(int(QThread.currentThreadId())))
        else: 
            self.logger.log(logging.ERROR, "[{}]: no baudrates available".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_comboBoxDropDown_LineTermination(self):
        """ 
        User selected a different line termination from drop down menu
        """
        _tmp = self.ui.comboBoxDropDown_LineTermination.currentText()
        if _tmp == "newline (\\n)":
            self.textLineTerminator = '\n'
        elif _tmp == "return (\\r)":
            self.textLineTerminator = '\r'
        elif _tmp == "newline return (\\n\\r)":
            self.textLineTerminator = '\n\r'
        elif _tmp == "none":
            self.textLineTerminator = ''
        else:
            self.textLineTerminator = '\r\n'
        self.changeLineTerminationRequest.emit(self.textLineTerminator)
        self.logger.log(logging.INFO, "[{}]: line termination {}".format(int(QThread.currentThreadId()), repr(self.textLineTerminator)))

    # Response to Serial Signals
    ########################################################################################

    @pyqtSlot(str, int, str, str, float)
    def on_serialStatusReady(self, port: str, baud: int, newline: str, encoding: str, timeout: float):
        """ 
        Serial status report available 
        """
        self.serialPort = port
        self.serialBaudRate = baud
        self.textLineTerminator = newline
        self.encoding = encoding
        self.serialTimeout = timeout
        
        # adjust the combobox current item to match the current port
        try:
            if self.serialPort == "":
                index = self.serialPorts.index("None")                                     # find current port in serial port list
            else:
                index = self.serialPorts.index(self.serialPort)                            # find current port in serial port list
            self.ui.comboBoxDropDown_SerialPorts.setCurrentIndex(index)                    # update serial port combobox
            self.logger.log(logging.DEBUG, "[{}]: port {}.".format(int(QThread.currentThreadId()),self.serialPortNames[index]))
        except:
            self.logger.log(logging.ERROR, "[{}]: port not available.".format(int(QThread.currentThreadId())))
        # adjust the combobox current item to match the current baudrate
        try: 
            index = self.BaudRates.index(self.serialBaudRate)                              # find baud rate in serial baud rate list
            self.ui.comboBoxDropDown_BaudRates.setCurrentIndex(index)                      #  baud combobox
            self.logger.log(logging.DEBUG, "[{}]: baudrate {}.".format(int(QThread.currentThreadId()),self.BaudRates[index]))
        except:
            self.logger.log(logging.ERROR, "[{}]: no baudrate available.".format(int(QThread.currentThreadId())))

        # adjust the combobox current item to match the current line termination
        if   newline== '\n':    _tmp = "newline (\\n)"
        elif newline == '\r':   _tmp = "return (\\r)"
        elif newline == '\n\r': _tmp = "newline return (\\n\\r)"
        else:                   _tmp = "return newline (\\r\\n)"
        try:
            index = self.ui.comboBoxDropDown_LineTermination.findText(_tmp)
            self.ui.comboBoxDropDown_LineTermination.setCurrentIndex(index)
            self.logger.log(logging.DEBUG, "[{}]: line termination {}.".format(int(QThread.currentThreadId()),_tmp))
        except:
            self.logger.log(logging.ERROR, "[{}]: line termination not available.".format(int(QThread.currentThreadId())))
        
        # handle timeout and encoding
        #  not implemented as currently not selectable in the UI
        #  encoding is fixed to utf-8
        #  timeout can be computed from baud rate and longest expected line of text
        #  however is set to zero resulting in non blocking reads

    @pyqtSlot(list, list)
    def on_newPortListReady(self, ports: list, portNames: list):
        """ 
        New serial port list available 
        """
        self.logger.log(logging.DEBUG, "[{}]: port list received.".format(int(QThread.currentThreadId())))
        self.serialPorts = ports
        self.serialPortNames = portNames
        lenPortNames = len(self.serialPortNames)
        # block the box from emitting changed index signal when items are added
        self.ui.comboBoxDropDown_SerialPorts.blockSignals(True)
        # what is currently selected in the box?
        selected = self.ui.comboBoxDropDown_SerialPorts.currentText()
        # populate new items
        self.ui.comboBoxDropDown_SerialPorts.clear()
        self.ui.comboBoxDropDown_SerialPorts.addItems(self.serialPortNames+['None'])
        # search for the previously selected item
        index = self.ui.comboBoxDropDown_SerialPorts.findText(selected)
        if index > -1: # if we found previously selected item
            self.ui.comboBoxDropDown_SerialPorts.setCurrentIndex(index)
        else:  # if we did not find previous item set box to last item (None)
            self.ui.comboBoxDropDown_SerialPorts.setCurrentIndex(lenPortNames)
        # enable signals again
        self.ui.comboBoxDropDown_SerialPorts.blockSignals(False)

    @pyqtSlot(tuple)
    def on_newBaudListReady(self, baudrates):
        """ 
        New baud rate list available
        For logic and sequence of commands refer to newPortList
        """
        self.logger.log(logging.DEBUG, "[{}]: baud list received.".format(int(QThread.currentThreadId())))
        self.BaudRates = list(baudrates)
        lenBaudRates = len(self.BaudRates)
        self.ui.comboBoxDropDown_BaudRates.blockSignals(True)
        selected = self.ui.comboBoxDropDown_BaudRates.currentText()
        self.ui.comboBoxDropDown_BaudRates.clear()
        self.ui.comboBoxDropDown_BaudRates.addItems([str(x) for x in self.BaudRates + [-1]])
        if (selected == '-1' or selected == ''):
            index = self.ui.comboBoxDropDown_BaudRates.findText(str(self.serialBaudRate))
        else: 
            index = self.ui.comboBoxDropDown_BaudRates.findText(selected)
        if index > -1:
            self.ui.comboBoxDropDown_BaudRates.setCurrentIndex(index)
        else:
            self.ui.comboBoxDropDown_BaudRates.setCurrentIndex(lenBaudRates)        
        self.ui.comboBoxDropDown_BaudRates.blockSignals(False)

    @pyqtSlot(bytearray)
    def on_SerialReceivedText(self, byte_array: bytearray):
        """ 
        Received text () on serial port 
        Display it in the text display window
        """
        self.logger.log(logging.DEBUG, "[{}]: text received.".format(int(QThread.currentThreadId())))
        try:
            text = byte_array.decode(self.encoding)
            self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),text))
            # Move cursor to the end of the document
            cursor = self.ui.plainTextEdit_SerialTextDisplay.textCursor()
            cursor.movePosition(QTextCursor.End)
            cursor.insertText(text)
            self.ui.plainTextEdit_SerialTextDisplay.setTextCursor(cursor)
        except:
            self.logger.log(logging.ERROR, "[{}]: could not decode text in {}.".format(int(QThread.currentThreadId()),repr(byte_array)))

    @pyqtSlot(list)
    def on_SerialReceivedLines(self, lines: list):
        """ 
        Received lines of text on serial port 
        Display the lines in the text display window
        """
        self.logger.log(logging.DEBUG, "[{}]: text received.".format(int(QThread.currentThreadId())))
        for line in lines:
            if NEW_READLINE:
                self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),line.decode(self.encoding)))
                self.ui.plainTextEdit_SerialTextDisplay.appendPlainText("{}".format(line))
            else:
                self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),line))
                self.ui.plainTextEdit_SerialTextDisplay.appendPlainText("{}".format(line))

    @pyqtSlot(bool)
    def on_serialWorkerStateChanged(self, running: bool):
        """ 
        Serial worker was started or stopped
        """
        self.logger.log(logging.DEBUG, "[{}]: serial worker is {}.".format(int(QThread.currentThreadId()), "on" if running else "off"))
        self.receiverIsRunning = running
        if running: 
            self.ui.pushButton_SerialStartStop.setText("Stop")
            self.ui.pushButton_ChartStartStop.setText("Stop")
        else:
            self.ui.pushButton_SerialStartStop.setText("Start")
            self.ui.pushButton_ChartStartStop.setText("Start")

    def on_throughputReceived(self, numReceived, numSent):
        """
        Report throughput
        """
        rx = numReceived - self.lastNumReceived
        tx = numSent - self.lastNumSent
        self.rx = 0.5*self.rx + 0.5*rx # poor man's low pass
        self.tx = 0.5*self.tx + 0.5*tx # poor man's low pass
        self.ui.throughput.setText("{:<5.1f} {:<5.1f} kB/s".format(self.rx/1000, self.tx/1000))
        self.lastNumReceived = numReceived
        self.lastNumSent = numSent

    def serialTextDisplay_trim(self):
        """
        Reduce the amount of text kept in the text display window
        Attempt to keep the current relative scrollbar location 
        """
        proportion = self.textScrollbar.value() / self.textScrollbar.maximum() if self.textScrollbar.maximum() != 0 else 0
        current_text = self.ui.plainTextEdit_SerialTextDisplay.toPlainText()
        len_current_text = len(current_text)
        if len_current_text > MAX_TEXTBROWSER_LENGTH:
            current_text = current_text[-MAX_TEXTBROWSER_LENGTH:]
            self.ui.plainTextEdit_SerialTextDisplay.setPlainText(current_text)
            new_max = self.textScrollbar.maximum()            
            self.textScrollbar.setValue(int(proportion * new_max))
        
############################################################################################
# Q Serial
#
# separate thread handling serial input and output
# these routines have no access to the user interface, 
# communication occurs through signals
############################################################################################

class QSerial(QObject):
    """
    Serial Interface for QT

    Worker Signals
        textReceived                     received text on serial RX
        linesReceived                    received multiple lines on serial RX
        newPortListReady                 completed a port scan
        newBaudListReady                 completed a baud scan
        throughputReady                  throughput data is available
        serialStatusReady                report on port and baudrate available
        serialWorkerStateChanged         worker started or stopped

    Worker Slots
        on_startReceiverRequest()        start timer that reads input port
        on_stopReceiverRequest()         stop  timer that reads input port
        on_stopWorkerRequest()           stop  timer and close serial port
        on_sendTextRequest(str)          worker received request to transmit text
        on_sendLineRequest(str)          worker received request to transmit text, adds eol characters
        on_sendLinesRequest(list)        worker received request to transmit multiple lines of text
        on_changePortRequest(str, int)   worker received request to change port
        on_changeLineTerminationRequest(str)
                                         worker received request to change line termination
        on_closePortRequest()            worker received request to close current port
        on_changeBaudRequest(int)        worker received request to change baud rate
        on_scanPortsRequest()            worker received request to scan for serial ports
        on_scanBaudRatesRequest()        worker received request to scan for serial baudrates
        on_serialStatusRequest()         worker received request to report current port and baudrate 
        on_startThroughputRequest()      start timer to report throughput
        on_stopThroughputRequest()       stop timer to report throughput
        on_throughputTimer()             emit throughput data
    """
        
    # Signals
    ########################################################################################
    textReceived             = pyqtSignal(bytearray)                                       # text received on serial port
    linesReceived            = pyqtSignal(list)                                            # lines of text received on serial port
    newPortListReady         = pyqtSignal(list, list)                                      # updated list of serial ports is available
    newBaudListReady         = pyqtSignal(tuple)                                           # updated list of baudrates is available
    serialStatusReady        = pyqtSignal(str, int, str, str, float)                       # serial status is available
    throughputReady          = pyqtSignal(int,int)                                         # number of characters received/sent on serial port
    serialWorkerStateChanged = pyqtSignal(bool)                                            # worker started or stopped
    finished                 = pyqtSignal() 
        
    def __init__(self, parent=None):

        super(QSerial, self).__init__(parent)

        self.logger = logging.getLogger("QSerial")           

        self.PSer = PSerial()
        self.PSer.scanports()
        self.serialPorts     = [sublist[0] for sublist in self.PSer.ports]                  # COM3 ...
        self.serialPortNames = [sublist[1] for sublist in self.PSer.ports]                  # USB ... (COM3)
        self.serialBaudRates = self.PSer.baudrates
        
        self.textLineTerminator = '\r\n' # default line termination

        # Adjust response time
        # Fastest serial baud rate is 5,000,000 bits per second 
        # Regular serial baud rate is   115,200 bits per second
        # Slow serial baud rate is        9,600 bits per second
        # Transmitting one byte with 8N1 (8 data bits, no stop bit, one stop bit) might take up to 10 bits
        # Transmitting two int16 like "-8192, -8191\r\n" takes 14 bytes (3 times more than the actual numbers)
        # This would result in receiving 1k lines/second with 115200 and 40k lines/second with 5,000,000
        # These numbers are now updated with function based on baud rate further below        
        self.receiverInterval          = 5                                                 # in milliseconds 
        self.receiverIntervalStandby   = 50                                                # in milliseconds
        self.readLineTimeOut           = 0                                                 # in seconds
        self.serialReceiverCountDown   = 0                                                 # initialize

        self.logger.log(logging.INFO, "[{}]: QSerial initialized.".format(int(QThread.currentThreadId())))
        
    # Slots
    ########################################################################################

    @pyqtSlot()
    def on_setupReceiverRequest(self):
        """ 
        Set up a QTimer for reading data from serial input line at predefined interval.
        This does not start the timer.
        We can not create the timer in the init function because we will not move QSerial 
         to a new thread and the timer would not move with it.
         
        Set up QTimer for throughput measurements
        """
        if DEBUGPY_ENABLED:
            debugpy.debug_this_thread() # this should enable debugging of all methods QSerial methods
        
        # setup the receiver timer
        self.serialReceiverState = SerialReceiverState.stopped # initialize state machine
        self.receiverTimer = QTimer()
        self.receiverTimer.timeout.connect(self.updateReceiver)
        self.logger.log(logging.INFO, "[{}]: setup receiver timer.".format(int(QThread.currentThreadId())))

        # setup the receiver timer
        self.throughputTimer = QTimer(self)
        self.throughputTimer.setInterval(1000) 
        self.throughputTimer.timeout.connect(self.on_throughputTimer)
        self.logger.log(logging.INFO, "[{}]: setup throughput timer.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_throughputTimer(self):
        """
        Report throughput
        """
        if self.PSer.ser_open:
            self.throughputReady.emit(self.PSer.totalCharsReceived, self.PSer.totalCharsSent)
        else:
            self.throughputReady.emit(0,0)

    @pyqtSlot()
    def on_startReceiverRequest(self):
        """ 
        Start QTimer for reading data from serial input line (RX)
        Response will need to be analyzed in the main task.
        """
        # clear serial buffers
        self.PSer.clear()
        # start the receiver timer
        self.receiverTimer.setInterval(self.receiverIntervalStandby) 
        self.receiverTimer.start()        
        self.serialReceiverState = SerialReceiverState.awaitingData
        self.serialWorkerStateChanged.emit(True)
        self.logger.log(logging.INFO, "[{}]: started receiver.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopReceiverRequest(self):
        """ 
        Stop the receiver timer
        """
        self.receiverTimer.stop()
        self.serialReceiverState = SerialReceiverState.stopped
        self.serialWorkerStateChanged.emit(False)
        self.logger.log(logging.INFO, "[{}]: stopped receiver.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_startThroughputRequest(self):
        """ 
        Stop QTimer for reading through put from PSer)
        """
        self.throughputTimer.start()
        self.logger.log(logging.INFO, "[{}]: started throughput timer.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopThroughputRequest(self):
        """ 
        Stop QTimer for reading through put from PSer)
        """
        self.throughputTimer.stop()
        self.logger.log(logging.INFO, "[{}]: stopped throughput timer.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def updateReceiver(self):
        """ 
        Reading lines of text from serial RX 
        """
        if (self.serialReceiverState != SerialReceiverState.stopped):
            startTime = time.perf_counter()
            
            # check if end of line handling is wanted
            if self.PSer.newline != "": 
                # use the sio interface and handle line termination
                lines = self.PSer.readlines() # read lines until buffer empty
                endTime = time.perf_counter()
                
                if lines: 
                    self.logger.log(logging.DEBUG, "[{}]: {} lines {:.3f} [ms] per line.".format(int(QThread.currentThreadId()),len(lines), 1000*(endTime-startTime)/len(lines)))
                    if self.serialReceiverState == SerialReceiverState.awaitingData:
                        self.receiverTimer.setInterval(self.receiverInterval)
                        self.serialReceiverState == SerialReceiverState.receivingData
                    self.linesReceived.emit(lines)

                else:
                    if self.serialReceiverState == SerialReceiverState.receivingData:
                        self.serialReceiverCountDown += 1
                        if self.serialReceiverCountDown >= RECEIVER_FINISHCOUNT:    
                            # switch to awaiting data
                            self.serialReceiverState = SerialReceiverState.awaitingData
                            # slow down timer
                            self.receiverTimer.setInterval(self.receiverIntervalStandby)
                            self.serialReceiverCountDown = 0
                            self.logger.log(logging.DEBUG, "[{}]: receiving finished, set slower update rate.".format(int(QThread.currentThreadId())))

            else:
                # use the ser interface directly
                byte_array = self.PSer.read()
                endTime = time.perf_counter()
                self.logger.log(logging.DEBUG, "[{}]: {} bytes {:.3f} [ms] per line.".format(int(QThread.currentThreadId()),len(byte_array), 1000*(endTime-startTime)/len(byte_array)))
                self.textReceived.emit(byte_array)
        
        else:
            self.logger.log(logging.ERROR, "[{}]: checking input, receiver is stopped or port is not open.".format(int(QThread.currentThreadId())))

        self.logger.log(logging.DEBUG, "[{}]: completed receiving lines.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopWorkerRequest(self): 
        """ 
        Worker received request to stop
        We want to stop QTimer and close serial port and then let subscribers know that serial worker is no longer available
        """
        self.receiverTimer.stop()
        self.serialWorkerStateChanged.emit(False)
        self.PSer.close() # close sio and ser
        self.serialPort = ""
        self.serialBaudRate = -1
        self.logger.log(logging.INFO, "[{}]: stopped timer, closed port.".format(int(QThread.currentThreadId())))
        self.finished.emit()
            
    @pyqtSlot(str)
    def on_sendTextRequest(self, text: str):
        """ 
        Request to transmit text to serial TX line 
        """
        if self.PSer.ser_open:
            res = self.PSer.write(text.encode(self.PSer.encoding))
            self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),text, res))
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(str)
    def on_sendLineRequest(self, text: str):
        """ 
        Request to transmit a line of text to serial TX line 
        Terminate the text with eol characters.
        """
        if self.PSer.ser_open:
            res = self.PSer.writeline(text)
            self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),text, res))
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(list)
    def on_sendLinesRequest(self, lines: list):
        """ 
        Request to transmit multiple lines of text to serial TX line
        """
        if self.PSer.ser_open:
            for text in lines:
                res = self.PSer.writeline(text)
                self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),text, res))
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(str)
    def on_sendFileRequest(self, fname: str):
        """ 
        Request to transmit file to serial TX line 
        """
        if DEBUGPY_ENABLED: debugpy.debug_this_thread()
        
        if self.PSer.ser_open:
            if fname:
                with open(fname, 'rb') as f: # open file in binary read mode
                    try: 
                        file_content = f.read()
                        res = self.PSer.write(file_content)
                        self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),fname, res))
                    except:
                        self.logger.log(logging.ERROR, "[{}]: error transmitting \"{}\".".format(int(QThread.currentThreadId()),fname))
            else:
                self.logger.log(logging.WARNING, "[{}]: no file name provided.".format(int(QThread.currentThreadId())))        
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(str, int)
    def on_changePortRequest(self, port: str, baud: int):
        """ 
        Request to change port received 
        """
        self.PSer.close()
        if port != "":
            readLineTimeOut, receiverInterval, receiverIntervalStandby = compute_timeouts(baud)
            if self.PSer.open(port=port, baud=baud, lineTerminator=self.textLineTerminator, encoding=self.PSer.encoding, timeout=readLineTimeOut):
                self.serialPort = port
                self.serialBaudRate = baud
                self.readLineTimeOut = readLineTimeOut
                self.receiverInterval = receiverInterval 
                self.receiverIntervalStandby = receiverIntervalStandby
                self.receiverTimer.setInterval(self.receiverIntervalStandby) 
                self.logger.log(logging.INFO, "[{}]: port {} opened with newline {} encoding {} timeout {}.".format(
                    int(QThread.currentThreadId()), port, repr(self.textLineTerminator), self.PSer.encoding, self.PSer.timeout))

    @pyqtSlot()
    def on_closePortRequest(self):
        """ 
        Request to close port received 
        """
        self.PSer.close()

    @pyqtSlot(int)
    def on_changeBaudRateRequest(self, baud: int):
        """ 
        New baudrate received 
        """
        if (baud is None) or (baud <= 0):
            self.logger.log(logging.WARNING, "[{}]: range error, baudrate not changed to {},".format(int(QThread.currentThreadId()), baud))
        else:                
            readLineTimeOut, receiverInterval, receiverIntervalStandby = compute_timeouts(baud)    
            if self.PSer.ser_open:
                if self.serialBaudRates.index(baud) >= 0: # check if baud rate is available by searching for its index in the baud rate list
                    self.PSer.changeport(self.PSer.port, baud, lineTerminator=self.textLineTerminator, encoding=self.PSer.encoding, timeout=readLineTimeOut)
                    if self.PSer.baud == baud: # check if new value matches desired value
                        self.readLineTimeOut = readLineTimeOut
                        self.serialBaudRate = baud  # update local variable
                        self.receiverInterval = receiverInterval
                        self.receiverIntervalStandby = receiverIntervalStandby
                        self.receiverTimer.setInterval(self.receiverIntervalStandby) 
                    else:
                        self.serialBaudRate = self.PSer.baud
                        self.logger.log(logging.ERROR, "[{}]: failed to set baudrate to {}.".format(int(QThread.currentThreadId()), baud))
                else: 
                    self.logger.log(logging.ERROR, "[{}]: baudrate {} no available.".format(int(QThread.currentThreadId()), baud))
                    self.serialBaudRate = -1
            else:
                self.logger.log(logging.ERROR, "[{}]: failed to set baudrate, serial port not open!".format(int(QThread.currentThreadId())))
            
    @pyqtSlot(str)
    def on_changeLineTerminationRequest(self, lineTermination: str):
        """ 
        New LineTermination received
        """
        if (lineTermination is None):
            self.logger.log(logging.WARNING, "[{}]: line termination not changed, line termination string not provided.".format(int(QThread.currentThreadId())))
            return
        else:            
            self.PSer.newline = lineTermination
            self.textLineTerminator = lineTermination
            self.logger.log(logging.INFO, "[{}]: changed line termination to {}.".format(int(QThread.currentThreadId()), repr(self.textLineTerminator)))

    @pyqtSlot()
    def on_scanPortsRequest(self):
        """ 
        Request to scan for serial ports received 
        """            
        if self.PSer.scanports() > 0 :
            self.serialPorts =     [sublist[0] for sublist in self.PSer.ports]
            self.serialPortNames = [sublist[1] for sublist in self.PSer.ports]
        else :
            self.serialPorts = []
            self.serialPortNames = []        
        self.logger.log(logging.INFO, "[{}]: Port(s) {} available.".format(int(QThread.currentThreadId()),self.serialPortNames))
        self.newPortListReady.emit(self.serialPorts, self.serialPortNames)
        
    @pyqtSlot()
    def on_scanBaudRatesRequest(self):
        """ 
        Request to report serial baud rates received 
        """
        if self.PSer.ser_open:
            self.serialBaudRates = self.PSer.baudrates
        else:
            self.serialBaudRates = ()
        if len(self.serialBaudRates) > 0:
            self.logger.log(logging.INFO, "[{}]: baudrate(s) {} available.".format(int(QThread.currentThreadId()),self.serialBaudRates))
        else:
            self.logger.log(logging.WARNING, "[{}]: no baudrates available, port is closed.".format(int(QThread.currentThreadId())))
        self.newBaudListReady.emit(self.serialBaudRates)

    @pyqtSlot()
    def on_serialStatusRequest(self):
        """ 
        Request to report serial port and baudrate received
        """
        self.logger.log(logging.INFO, "[{}]: provided serial status".format(int(QThread.currentThreadId())))
        if self.PSer.ser_open:
            self.serialStatusReady.emit(self.PSer.port, self.PSer.baud, self.PSer.newline, self.PSer.encoding, self.PSer.timeout)
        else:
            self.serialStatusReady.emit("", self.PSer.baud, self.PSer.newline, self.PSer.encoding, self.PSer.timeout)

def compute_timeouts(baud: int):
    # Set timeout to the amount of time it takes to receive the shortest expected line of text
    # integer '123/n/r' 5 bytes, which is at least 45 serial bits
    # readLineTimeOut = 40 / baud [s] is very small and we should imply make it by just setting it to zero (non blocking)
    readLineTimeOut = 0 # make it non blocking        
    
    # Set the QTimer interval so that each call we get a couple of lines
    # lets assume we receive 4 integers in one line, this is approx 32 bytes, 10 serial bits per byte
    # lets request NUM_LINES_COLLATE lines per call 
    receiverInterval  = ceil(NUM_LINES_COLLATE * 32 * 10 / baud * 1000)  # in milliseconds
    # check serial should occur no more than 200 times per second no less than 10 times per second
    if receiverInterval < 5: receiverInterval = 5 # set maximum to 200 Hz
    receiverIntervalStandby  = 10 * receiverInterval # make standby 10 times slower
    if receiverIntervalStandby > 100: # 
        receiverIntervalStandby = 100 # but check at least 10 times per second
    
    return readLineTimeOut, receiverInterval, receiverIntervalStandby

################################################################################
# Serial Low Level 
################################################################################

class PSerial():
    """
    Serial Wrapper.
    Without this class the function Serial.in_waiting does not seem to work.
    """
    
    def __init__(self):
        if DEBUGPY_ENABLED:
            debugpy.debug_this_thread() # this should enable debugging of all PSerial methods
            
        self.logger = logging.getLogger("PSerial")           
        self.ser = None
        self.sio = None
        self._port = ""
        self._baud = -1
        self._newline = ""
        self._encoding = "utf-8"
        self._timeout = -1
        self.ser_open = False
        self.sio_open = False
        self.totalCharsReceived = 0
        self.totalCharsSent = 0
        self.partialLine = ""
        self.havePartialLine = False
        # check for serial ports
        _ = self.scanports()
    
    def scanports(self) -> int:
        """ 
        scans for all available ports 
        """
        self._ports = [
            [p.device, p.description]
            for p in list_ports.comports()
        ]
        return len(self._ports)

    def open(self, port: str, baud: int, lineTerminator: str, encoding, timeout: float) -> bool:
        """ open specified port """
        try:       
            self.ser = sp(
                port = port,                    # the serial device
                baudrate = baud,                # often 115200 but Teensy sends/receives as fast as possible
                bytesize = EIGHTBITS,           # most common option
                parity = PARITY_NONE,           # most common option
                stopbits = STOPBITS_ONE,        # most common option
                timeout = timeout,              # wait until requested characters are received on read request or timeout occurs
                write_timeout = timeout,        # wait until requested characters are sent
                inter_byte_timeout = None,      # disable inter character timeout
                rtscts = False,                 # do not use 'request to send' and 'clear to send' handshaking
                dsrdtr = False,                 # dont want 'data set ready' signaling
                exclusive = None,               # do not share port in POSIX
                xonxoff = False,                # dont have 'xon/xoff' hand shaking in serial data stream
                )
        except: 
            self.ser_open = False
            self.ser = None
            self.sio_open = False
            self.sio = None
            self._port=""
            self.logger.log(logging.ERROR, "[SER {}]: failed to open port {}.".format(int(QThread.currentThreadId()),port))
            return False
        else: 
            self.logger.log(logging.DEBUG, "[SER {}]: {} opened with baud {}.".format(int(QThread.currentThreadId()), port, baud))
            self.ser_open = True
            self._baud = baud
            self._port = port
            self._timeout = timeout
            # clear buffers
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            self.totalCharsReceived = 0
            self.totalCharsSent = 0
            try:
                self.bufferedRWPair = io.BufferedRWPair(self.ser, self.ser)
                self.sio = io.TextIOWrapper(self.bufferedRWPair, newline=lineTerminator, encoding=encoding)
                self.sio_open = True
                self._newline = lineTerminator
                self._encoding = encoding
                self.logger.log(logging.DEBUG, "[SIO {}]: opened with eol {} and encoding {}.".format(int(QThread.currentThreadId()), repr(lineTerminator), encoding))
            except:
                self.sio = None
                self.sio_open = False
                self._newline = ""
                self._encoding = encoding
                self.logger.log(logging.DEBUG, "[SIO {}]: could not open.".format(int(QThread.currentThreadId())))
                return False
            else:
                return True
        
    def close(self):
        """ 
        closes serial port 
        
        we have issue in closing textIOwrapped serial port
        somehow it claims to be already closed
        """
        if (self.sio is not None):
            # close the port
            try: 
                self.sio.close()
            except:
                # self.logger.log(logging.ERROR, "[SIO {}]: failed to complete closure.".format(int(QThread.currentThreadId())))
                pass
        self.logger.log(logging.INFO, "[SIO {}]: closed.".format(int(QThread.currentThreadId())))
        self.sio_open = False
        
        if (self.ser is not None):
            # close the port
            try:
            # clear buffers
                self.ser.reset_input_buffer()
                self.ser.reset_output_buffer()
                self.ser.close()
            except:
                # self.logger.log(logging.ERROR, "[SER {}]: failed to complete closure.".format(int(QThread.currentThreadId())))                      
                pass
            self._port = ""
        self.logger.log(logging.INFO, "[SER {}]: closed.".format(int(QThread.currentThreadId())))
        self.ser_open = False
            
    def changeport(self, port: str, baud: int, lineTerminator: bytes, encoding: str, timeout: float):
        """ switch to different port """
        self.close()
        self.open(port=port, baud=baud, lineTerminator=lineTerminator, encoding=encoding, timeout=timeout ) # open also clears the buffers
        self.logger.log(logging.DEBUG, "[SER {}]: changed port to {} with baud {}, eol {} and encoding {}".format(int(QThread.currentThreadId()),port,baud,repr(lineTerminator),encoding))
    
    def read(self) -> bytearray:
        """ reads serial buffer until empty """
        byte_array = bytearray()
        startTime = time.perf_counter()
        maxTime = startTime + 0.1 # dont spend more than 100ms on reading
        while True:
            bytes_to_read = self.ser.in_waiting
            if bytes_to_read:
                byte_array += self.ser.read(bytes_to_read)
                self.totalCharsReceived += bytes_to_read
            else:
                self.logger.log(logging.DEBUG, "[SER {}]: end of read, buffer empty.".format(int(QThread.currentThreadId())))
                break
            if time.perf_counter() >= maxTime: 
                self.logger.log(logging.DEBUG, "[SER {}]: end of read, timeout.".format(int(QThread.currentThreadId())))
                break
        return byte_array

    def readlines(self) -> list:
        """ 
        reads lines of text until no more lines available
        
        !!! This implementation creates up to 100% CPU load.
        !!! It will need to be rewritten to use a byte array and split it by line termination characters and deal with partial lines received.
        
        The textIOWrapper readline() function will return partial lines when serial timeout is 0 (non blocking) 
        since a complete line includes the line termination characters we can handle partial lines
        
        We still have issues with occasionally receiving empty lines and lines appear to contain numbers merged from two lines
        not sure how to fix this
        
        byte_array.split(b'\n\r')
        
        use rfind method to find last delimiter to separate partial lines
        
        
        def find_all_occurrences(byte_array, pattern):
            start = 0
            while True:
                index = byte_array.find(pattern, start)
                if index == -1:
                    return
                yield index
                start = index + len(pattern)

        def split_bytearray(data, separator=b'\r\n'):
            lines = []
            start = 0

            while True:
                # Find the separator
                index = data.find(separator, start)

                # No more separators found, break the loop
                if index == -1:
                    break

                # Extract the line and add to the list
                line = data[start:index]
                lines.append(line)

                # Move the start index past this separator
                start = index + len(separator)

            # Add the remaining data as the last line if it's not empty
            if start < len(data):
                lines.append(data[start:])

            return lines
                    
        """
        if NEW_READLINE:
            # 1. read all bytes from the serial buffer
            # 2. find the right most location of the line termination characters
            # 3. if location has been found 
            #    3.1 split the byte array into lines
            #    3.2 if there was previous a partial line add it to the first line
            #    3.3 if there is a remainder after the last delimiter make it the new partial line
            # 4. if no location has been found add the byte_array to the partial line array
            
            lines = []        
            startTime = time.perf_counter()
            byte_array = self.read()
            idx = byte_array.rfind(self.newline)
            if idx == -1:
                # no delimiter found
                self.partialLine += byte_array
                self.havePartialLine = True
            else:
                # delimiter found
                lines = byte_array.split(self.newline)
                if self.havePartialLine:
                    lines[0] = self.partialLine + lines[0] # merge previous end of line with first line
                    self.havePartialLine = False # clear partial line flag
                    self.partialLine = b'' # clear partial line
                # check remaining part of the byte_array
                e = idx + len(self.newline) 
                if e != len(byte_array):
                    # we have remainder, lets put it into the partial line
                    self.partialLine = byte_array[e:]
            endTime = time.perf_counter()
            self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}.".format(int(QThread.currentThreadId()), endTime-startTime))
            return lines
                        
        else:            
            lines = []        
            startTime = time.perf_counter()
            maxTime = startTime + 0.1 # dont spend more than 100ms on reading to keep system responsive
            while True:        
                line = self.sio.readline()
                self.totalCharsReceived += len(line)
                if line:
                    if line.endswith(self._newline):
                        if self.havePartialLine: 
                            line = self.partialLine + line # concatenate partial line from previous reading with current reading
                            self.havePartialLine = False
                        lines.append(line.rstrip('\r\n')) # add line to and remove any line termination characters
                    else:
                        self.partialLine = line # save partial line
                        self.havePartialLine = True
                else:
                    break
                if time.perf_counter() >= maxTime: 
                    break
            endTime = time.perf_counter()
            self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}.".format(int(QThread.currentThreadId()), endTime-startTime))
            return lines
    
    def readline(self) -> bytearray:
        """ 
        reads a line of text 
        
        !!! This will need to be rewritten to use a byte array and split it by line termination characters and deal with partial lines received.
        self.ser. read_until(self.newline) will include self.newline in the returned string
        """
        if NEW_READLINE:
            startTime = time.perf_counter()
            _line = self.ser.read_until(self.newline)
            self.totalCharsReceived += len(_line)
            if _line:
                if _line.endswith(self.newline):
                    # dont have partial line
                    if self.havePartialLine: 
                        line = self.partialLine + _line # concatenate partial line from previous reading with current reading
                        self.havePartialLine = False
                        self.partialLine = b''
                    else:
                        line = _line
                else:
                    # have partial line
                    self.partialLine = _line # save partial line
                    self.havePartialLine = True
                    line = b''
            endTime = time.perf_counter()
            self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}, line {}.".format(int(QThread.currentThreadId()), endTime-startTime,  repr(line)))
            return line.rstrip(self.newline)
        
        else:
            startTime = time.perf_counter()
            _line = self.sio.readline()
            self.totalCharsReceived += len(_line)
            if _line:
                if _line.endswith(self._newline):
                    if self.havePartialLine: 
                        line = self.partialLine + _line # concatenate partial line from previous reading with current reading
                        self.havePartialLine = False
                    else:
                        line = _line
                else:
                    self.partialLine = _line # save partial line
                    self.havePartialLine = True
                    line = ""
            endTime = time.perf_counter()
            self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}, line {}.".format(int(QThread.currentThreadId()), endTime-startTime,  line))
            return line.rstrip(self.newline)

    def writeline(self, text: str) -> int:
        """ sends a line of text """
        if DEBUGPY_ENABLED: debugpy.debug_this_thread()
        l = 0
        try:
            self.sio.write(text)
            l = len(text)
            self.totalCharsSent += l
        except:
            self.logger.log(logging.ERROR, "[SIO {}]: failed to write with timeout {}.".format(int(QThread.currentThreadId()), self.timeout ))
        self.logger.log(logging.DEBUG, "[SIO {}]: wrote {} chars.".format(int(QThread.currentThreadId()), l))
        return l 

    def writelines(self, lines: list) -> int:
        """ sends several lines of text """
        if DEBUGPY_ENABLED: debugpy.debug_this_thread()
        l = 0
        for line in lines:
            try:
                self.sio.write(line)
                _l = len(line)
                self.totalCharsSent += _l
                l += _l
            except:
                self.logger.log(logging.ERROR, "[SIO {}]: failed to write with timeout {}.".format(int(QThread.currentThreadId()), self.timeout ))
                return l
        self.logger.log(logging.DEBUG, "[SIO {}]: wrote {} chars.".format(int(QThread.currentThreadId()), l))
        return l
        
    def write(self, b: bytes) -> int:
        """ sends an array of bytes """
        if DEBUGPY_ENABLED: debugpy.debug_this_thread()
        l = 0
        try:
            l = self.ser.write(b)
        except:
            self.logger.log(logging.ERROR, "[SER {}]: failed to write with timeout {}.".format(int(QThread.currentThreadId()), self.timeout ))
            return l

        self.totalCharsSent += l
        self.logger.log(logging.DEBUG, "[SER {}]: wrote {} bytes.".format(int(QThread.currentThreadId()), l))
        return l

    def avail(self) -> int:
        """ is there data in the serial receiving buffer? """
        if self.ser is not None:
            return self.ser.in_waiting
        else:
            return -1
    
    def clear(self):
        """ 
        clear serial buffers
        we want to clear not flush
        """
        if self.ser is not None:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            self.totalCharsReceived = 0
            self.totalCharsSent = 0
            
    # Setting and reading internal variables
    ########################################################################################
        
    @property
    def ports(self):
        """ returns list of ports """
        return self._ports    

    @property
    def baudrates(self):
        """ returns list of baudrates """
        if self.ser_open:
            if max(self.ser.BAUDRATES) <= 115200:
                # add some higher baudrates
                return (self.ser.BAUDRATES + (230400, 250000, 460800, 500000, 921600, 1000000, 2000000))
            return self.ser.BAUDRATES
        else:
            return ()

    @property
    def connected(self):
        """ return true if connected """
        return self.ser_open

    @property
    def port(self):
        """ returns current port """
        if self.ser_open: return self._port
        else: return ""
        
    @port.setter
    def port(self, val):
        """ sets serial port """
        if (val is None) or  (val == ""):
            self.logger.log(logging.WARNING, "[SER {}]: no port given {}.".format(int(QThread.currentThreadId()), val))
            return
        else:
            # change the port, clears the buffers
            if self.changeport(self, val, self.baud):
                self.logger.log(logging.DEBUG, "[SER {}]: port:{}.".format(int(QThread.currentThreadId()), val))
                self._port = val
            else:
                self.logger.log(logging.ERROR, "[SER {}]: failed to open port {}.".format(int(QThread.currentThreadId()), val))

    @property
    def baud(self):
        """ returns current serial baudrate """
        if self.ser_open: return self._baud
        else: return -1
        
    @baud.setter
    def baud(self, val):
        """ sets serial baud rate """
        if (val is None) or (val <= 0):
            self.logger.log(logging.WARNING, "[SER {}]: baudrate not changed to {}.".format(int(QThread.currentThreadId()), val))
            return
        if self.ser_open:
            self.ser.baudrate = val        # set new baudrate
            self._baud = self.ser.baudrate # request baudrate
            if (self._baud == val) : 
                self.logger.log(logging.DEBUG, "[SER {}]: baudrate:{}.".format(int(QThread.currentThreadId()), val))
            else:
                self.logger.log(logging.ERROR, "[SER {}]: failed to set baudrate to {}.".format(int(QThread.currentThreadId()), val))
            # clear buffers
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
        else:
            self.logger.log(logging.ERROR, "[SER {}]: failed to set baudrate, serial port not open!".format(int(QThread.currentThreadId())))

    @property
    def newline(self):
        """ returns current line termination """
        return self._newline
        
    @newline.setter
    def newline(self, val):
        """ sets serial ioWrapper line termination """
        if (val is None):
            self.logger.log(logging.WARNING, "[SER {}]: newline not changed, need to provide string.".format(int(QThread.currentThreadId())))
            return
        if self.sio_open:
            if val != "": 
                self.sio.reconfigure(newline = val)
            self._newline = val
            self.logger.log(logging.DEBUG, "[SER {}]: newline:{}.".format(int(QThread.currentThreadId()), repr(val)))
        else:
            self._newline = ""
            self.logger.log(logging.ERROR, "[SER {}]: failed to set line termination, serial ioWrapper not open!".format(int(QThread.currentThreadId())))

    @property
    def encoding(self):
        """ returns current text encoding """
        if self.ser_open: return self._encoding
        else: return ""

    @encoding.setter
    def encoding(self, val):
        """ sets serial ioWrapper text encoding """
        if (val is None) or (val == ""):
            self.logger.log(logging.WARNING, "[SER {}]: encoding not changed, need to provide string.".format(int(QThread.currentThreadId())))
            return
        if self.sio_open:
            self.sio.reconfigure(encoding = val)            
            self.logger.log(logging.DEBUG, "[SER {}]: encoding:{}.".format(int(QThread.currentThreadId()), val))

        else:
            self.logger.log(logging.ERROR, "[SER {}]: failed to set encoding, serial ioWrapper not open!".format(int(QThread.currentThreadId())))

    @property
    def timeout(self):
        """ returns current serial timeout """
        if self.ser_open: return self._timeout
        else: return -1

#####################################################################################
# Testing
#####################################################################################

if __name__ == '__main__':
    # not implemented
    pass