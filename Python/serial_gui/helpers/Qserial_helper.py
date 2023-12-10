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

import io, locale
import time, logging
from math import ceil
from enum import Enum
import debugpy # for debugging code in Qthread

from PyQt5.QtCore import QObject, QTimer, QThread, pyqtSignal, pyqtSlot, QStandardPaths
from PyQt5.QtCore import Qt

from PyQt5.QtWidgets import QFileDialog

# Constants
########################################################################################
DEFAULT_BAUDRATE          = 115200
MAX_TEXTBROWSER_LENGTH    = 128*1024 # display window character length is trimmed to this length
MAX_LINE_LENGTH           = 1024 # number of characters after which an end of line characters is expected
RECEIVER_FINISHCOUNT      = 10   # [times] If we encountered a timeout 10 times we slow down serial polling

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
        on_SerialReceivedText(str)           pickup text from serial port
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
           
    def __init__(self, parent=None, ui=None):

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
        
        self.logger = logging.getLogger("QSerUI_")           
                   
        if ui is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to User Interface".format(int(QThread.currentThreadId())))

        self.ui = ui

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
        self.sendLineRequest.emit(text)
        self.ui.lineEdit_SerialText.clear()

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
            self.startReceiverRequest.emit()
            self.startThroughputRequest.emit()
        else:
            self.ui.pushButton_SerialStartStop.setText("Start")
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
        elif _tmp == "return newline (\\r\\n)":
            self.textLineTerminator = '\r\n'
        else:
            self.textLineTerminator = '\n'
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
            # item_count = self.ui.comboBoxDropDown_LineTermination.count()
            # items_text = []
            # for index in range(item_count):
            #     text = self.ui.comboBoxDropDown_LineTermination.itemText(index)
            #     items_text.append(text)
            # # Now items_text contains the text of all items in the comboBox
            # print(items_text)
            index = self.ui.comboBoxDropDown_LineTermination.findText(_tmp)
            self.ui.comboBoxDropDown_LineTermination.setCurrentIndex(index)
            self.logger.log(logging.DEBUG, "[{}]: line termination {}.".format(int(QThread.currentThreadId()),_tmp))
        except:
            self.logger.log(logging.ERROR, "[{}]: line termination not available.".format(int(QThread.currentThreadId())))
        
        # handle timeout and encoding (not implemented as its adjusted or fixed in the code)
        # not implemented as currently not selectable in the UI
        # encoding is fixed to utf-8
        # timeout is computed from baud rate and longest expected line of text

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

    @pyqtSlot(str)
    def on_SerialReceivedText(self, text: str):
        """ 
        Received line of text on serial port 
        Display the line in the text display window
        """
        self.logger.log(logging.DEBUG, "[{}]: text received.".format(int(QThread.currentThreadId())))
        self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),text))
        self.ui.plainTextEdit_SerialTextDisplay.appendPlainText("{}".format(text))

    @pyqtSlot(list)
    def on_SerialReceivedLines(self, lines: list):
        """ 
        Received lines of text on serial port 
        Display the lines in the text display window
        """
        self.logger.log(logging.DEBUG, "[{}]: text received.".format(int(QThread.currentThreadId())))
        for line in lines:
            self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),line))
            self.ui.plainTextEdit_SerialTextDisplay.appendPlainText("{}".format(line))

    def on_throughputReceived(self, numReceived, numSent):
        """
        Report throughput
        """
        rx = numReceived - self.lastNumReceived
        tx = numSent - self.lastNumSent
        self.rx = 0.8*self.rx + 0.2*rx # poor mans low pass
        self.tx = 0.8*self.tx + 0.2*tx # poor mans low pass
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
        textReceived                     received one line on serial RX
        linesReceived                    received multiple lines on serial RX
        newPortListReady                 completed a port scan
        newBaudListReady                 completed a baud scan
        throughputReady                  throughput data is available
        serialStatusReady                report on port and baudrate available

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
    textReceived      = pyqtSignal(str)                                                    # text received on serial port
    linesReceived     = pyqtSignal(list)                                                   # lines of text received on serial port
    newPortListReady  = pyqtSignal(list, list)                                             # updated list of serial ports is available
    newBaudListReady  = pyqtSignal(tuple)                                                  # updated list of baudrates is available
    serialStatusReady = pyqtSignal(str, int, str, str, float)                              # serial status is available
    throughputReady   = pyqtSignal(int,int)                                                # number of characters received/sent on serial port
    finished          = pyqtSignal() 
        
    def __init__(self, parent=None):

        super(QSerial, self).__init__(parent)

        # debugpy.debug_this_thread()

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
        # Transmitting one byte e.g. with 8N1 (8 data bits, no stop bit, one stop bit) might take up to 10 bits
        # Transmitting two int16 like "-8192, -8191\r\n" takes 14 bytes (3 times more than the actual numbers)
        # This would result in receiving 1k lines/second with 115200 and 40k lines/second with 5,000,000        
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
        # debugpy.debug_this_thread()
        
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
        # debugpy.debug_this_thread()
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
        # debugpy.debug_this_thread()
        # clear serial buffers
        self.PSer.clear()
        # start the receiver timer
        self.receiverTimer.setInterval(self.receiverIntervalStandby) 
        self.receiverTimer.start()        
        self.serialReceiverState = SerialReceiverState.awaitingData
        self.logger.log(logging.INFO, "[{}]: started receiver.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopReceiverRequest(self):
        """ 
        Stop the receiver timer
        """
        # debugpy.debug_this_thread()
        self.receiverTimer.stop()
        self.serialReceiverState = SerialReceiverState.stopped
        self.logger.log(logging.INFO, "[{}]: stopped receiver.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_startThroughputRequest(self):
        """ 
        Stop QTimer for reading through put from PSer)
        """
        # debugpy.debug_this_thread()
        self.throughputTimer.start()
        self.logger.log(logging.INFO, "[{}]: started throughput timer.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopThroughputRequest(self):
        """ 
        Stop QTimer for reading through put from PSer)
        """
        # debugpy.debug_this_thread()
        self.throughputTimer.stop()
        self.logger.log(logging.INFO, "[{}]: stopped throughput timer.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def updateReceiver(self):
        """ 
        Reading lines of text from serial RX 
        """
        debugpy.debug_this_thread()
        if (self.serialReceiverState != SerialReceiverState.stopped):
            startTime = time.perf_counter()
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

        # if self.ser.ser_open and (self.serialReceiverState != SerialReceiverState.stopped):

        #     # update states
        #     avail = self.ser.avail()
        #     if avail > 0:
        #         # switch to receiving data
        #         self.logger.log(logging.DEBUG, "[{}]: checking input, {} chars available.".format(int(QThread.currentThreadId()),avail))
        #         self.serialReceiverState = SerialReceiverState.receivingData
        #         self.receiverTimer.setInterval(self.receiverInterval)
        #     else:
        #         # not receiving data
        #         if self.serialReceiverState == SerialReceiverState.receivingData:
        #             # we  try a few more times then switch to slower awaiting mode
        #             self.serialReceiverCountDown += 1
        #             if self.serialReceiverCountDown >= RECEIVER_FINISHCOUNT:    
        #                 # switch to awaiting data
        #                 self.serialReceiverState = SerialReceiverState.awaitingData
        #                 self.receiverTimer.setInterval(self.receiverIntervalStandby)
        #                 self.serialReceiverCountDown = 0

        #     if self.serialReceiverState == SerialReceiverState.receivingData:
        #         # receiving data
        #         while avail > 0:
        #             # startTime = time.perf_counter()
        #             line = self.ser.readline(eol_chars=self.textLineTerminator, timeout=self.readLineTimeOut, encoding='utf-8')
        #             # endTime = time.perf_counter()
        #             # up to here takes 0.0034 to 0.0054 secs
        #             self.logger.log(logging.DEBUG, "[{}]: {}".format(int(QThread.currentThreadId()),line))
        #             if line != None: self.textReceived.emit(line)
        #             # endTime = time.perf_counter()
        #             # up to here takes 0.0035 to 0.0053 secs
        #             avail = self.ser.avail()
        #             # self.logger.log(logging.INFO, "[{}]: serial data available {}, toc-tic: {}.".format(int(QThread.currentThreadId()),avail,(endTime-startTime)))

        else:
            self.logger.log(logging.ERROR, "[{}]: checking input, receiver is stopped or port is not open.".format(int(QThread.currentThreadId())))

        self.logger.log(logging.DEBUG, "[{}]: completed receiving lines.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_stopWorkerRequest(self): 
        """ 
        Worker received request to stop
        We want to stop QTimer and close serial port and then let subscribers know that serial worker is no longer available
        """
        # debugpy.debug_this_thread()
        self.receiverTimer.stop()
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
        # debugpy.debug_this_thread()
        if self.PSer.ser_open:
            res = self.PSer.write(text, encoding='utf-8')
            self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),text, res))
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(str)
    def on_sendLineRequest(self, text: str):
        """ 
        Request to transmit a line of text to serial TX line 
        Terminate the text with eol characters.
        """
        # debugpy.debug_this_thread()
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
        # debugpy.debug_this_thread()
        if self.PSer.ser_open:
            for text in lines:
                res = self.PSer.writeline(text)
                self.logger.log(logging.DEBUG, "[{}]: transmitted \"{}\" [{}].".format(int(QThread.currentThreadId()),text, res))
        else:
            self.logger.log(logging.ERROR, "[{}]: tx, port not opened.".format(int(QThread.currentThreadId())))

    @pyqtSlot(str, int)
    def on_changePortRequest(self, port: str, baud: int):
        """ 
        Request to change port received 
        """
        #debugpy.debug_this_thread()
        self.PSer.close()
        if port != "":
            readLineTimeOut, receiverInterval, receiverIntervalStandby = compute_timeouts(baud)
            if self.PSer.open(port=port, baud=baud, lineTerminator=self.textLineTerminator, encoding='utf-8', timeout=readLineTimeOut):
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

        stop timer
        close port
        """
        # debugpy.debug_this_thread()
        self.PSer.close()

    @pyqtSlot(int)
    def on_changeBaudRateRequest(self, baud: int):
        """ 
        New baudrate received 
        """
        #debugpy.debug_this_thread()
        if (baud is None) or (baud <= 0):
            self.logger.log(logging.WARNING, "[{}]: range error, baudrate not changed to {},".format(int(QThread.currentThreadId()), baud))
        else:                
            readLineTimeOut, receiverInterval, receiverIntervalStandby = compute_timeouts(baud)    
            if self.PSer.ser_open:
                if self.serialBaudRates.index(baud) >= 0: # check if baud rate is available by searching for its index in the baud rate list
                    self.PSer.changeport(self.PSer.port, baud, lineTerminator=self.textLineTerminator, encoding='utf-8', timeout=readLineTimeOut)
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
    def on_changeLineTerminationRequest(self, lineTermination='\r\n'):
        """ 
        New LineTermination received
        """
        # debugpy.debug_this_thread()
        if (lineTermination is None):
            self.logger.log(logging.WARNING, "[{}]: line termination not changed, line termination string not provided.".format(int(QThread.currentThreadId())))
            return
        else:            
            self.PSer.newline = lineTermination
            self.logger.log(logging.INFO, "[{}]: changed line termination to {}.".format(int(QThread.currentThreadId()), repr(self.textLineTerminator)))

    @pyqtSlot()
    def on_scanPortsRequest(self):
        """ 
        Request to scan for serial ports received 
        """            
        # debugpy.debug_this_thread()
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
        # debugpy.debug_this_thread()
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
        # debugpy.debug_this_thread()
        self.logger.log(logging.INFO, "[{}]: provided serial status".format(int(QThread.currentThreadId())))
        if self.PSer.ser_open:
            self.serialStatusReady.emit(self.PSer.port, self.PSer.baud, self.PSer.newline, self.PSer.encoding, self.PSer.timeout)
        else:
            self.serialStatusReady.emit("", self.PSer.baud, self.PSer.newline, self.PSer.encoding, self.PSer.timeout)

def compute_timeouts(baud: int):
    # Set timeout to the amount of time it takes to receive the shortest expected line of text
    # integer '123/n/r' 5 bytes, which is at least 45 serial bits
    # readLineTimeOut = 40 / baud [s] is very small and we should imply make it non blocking (zero)
    readLineTimeOut = 0 # make it non blocking        
    
    # Set the QTimer interval so that each call we get a couple of lines
    # lets assume we receive 4 integers in one line, this is approx 32 bytes, 10 serial bits per byte
    # lets request 4 lines per call
    receiverInterval  = ceil(4 * 32 * 10 / baud * 1000)  # in milliseconds
    # check serial should occur no more than 200 times per second no less than 10 times per second
    if receiverInterval < 5: receiverInterval = 5 # set maximum to 200 Hz
    receiverIntervalStandby  = 10 * receiverInterval
    if receiverIntervalStandby > 100: 
        receiverIntervalStandby = 100 # set minim to 10Hz
    
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
        # debugpy.debug_this_thread()
        self.logger = logging.getLogger("PSerial")           
        self.ser = None
        self.sio = None
        self._port = ""
        self._baud = -1
        self._newline = ""
        self._encoding = ""
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
        # debugpy.debug_this_thread()
        self._ports = [
            [p.device, p.description]
            for p in list_ports.comports()
        ]
        return len(self._ports)

    def open(self, port: str, baud: int, lineTerminator: str, encoding='utf-8', timeout: float= 0.05) -> bool:
        """ open specified port """
        #debugpy.debug_this_thread()
        try:       
            self.ser = sp(
                port = port,                    # the serial device
                baudrate = baud,                # often 115200 but Teensy sends/receives as fast as possible
                bytesize = EIGHTBITS,           # most common option
                parity = PARITY_NONE,           # most common option
                stopbits = STOPBITS_ONE,        # most common option
                timeout = timeout,              # wait until requested characters are received on read request or timeout occurs
                write_timeout = None,           # wait until requested characters are sent
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
                self._encoding = ""
                self.logger.log(logging.DEBUG, "[SIO {}]: could not open.".format(int(QThread.currentThreadId())))
                return False
            else:
                return True
        
    def close(self):
        """ closes serial port """
        # debugpy.debug_this_thread()
        if (self.sio is not None):
            # close the port
            try: 
                self.sio.close()
            except:
                self.logger.log(logging.ERROR, "[SIO {}]: failed to complete closure.".format(int(QThread.currentThreadId())))
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
                self.logger.log(logging.ERROR, "[SER {}]: failed to complete closure.".format(int(QThread.currentThreadId())))                      
            self._port = ""
        self.logger.log(logging.INFO, "[SER {}]: closed.".format(int(QThread.currentThreadId())))
        self.ser_open = False
            
    def changeport(self, port: str, baud: int, lineTerminator: bytes, encoding: str = 'utf-8', timeout: float= 0.05):
        """ switch to different port """
        # debugpy.debug_this_thread()
        self.close()
        self.open(port=port, baud=baud, lineTerminator=lineTerminator, encoding=encoding, timeout=timeout ) # open also clears the buffers
        self.logger.log(logging.DEBUG, "[SER {}]: changed port to {} with baud {}, eol {} and encoding {}".format(int(QThread.currentThreadId()),port,baud,repr(lineTerminator),encoding))
    
    def read(self) -> bytes:
        """ reads serial buffer until empty """
        # debugpy.debug_this_thread()
        binaryArray = bytearray()
        startTime = time.perf_counter()
        maxTime = startTime + 0.1 # dont spend more than 100ms on reading
        while True:
            bytes_to_read = self.ser.in_waiting
            if bytes_to_read:
                binaryArray += self.ser.read(bytes_to_read)
                self.totalCharsReceived += bytes_to_read
            else:
                break
            if time.perf_counter() >= maxTime: 
                break
        return binaryArray

    def readlines(self) -> list:
        """ 
        reads lines of text until no more lines available
        
        the textIOWrapper readline() function will return partial lines when serial timeout is 0 (non blocking) 
        a line includes the line termination characters
        """
        # debugpy.debug_this_thread()
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
    
    def readline(self) -> str:
        """ reads a line of text """
        # debugpy.debug_this_thread()
        startTime = time.perf_counter()
        _line = self.sio.readline()
        self.totalCharsReceived += len(line)
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
        return line.rstrip('\r\n')


        # line = bytearray()
        # end_time = time.time() + timeout
        # eol_len = len(eol_chars)

        # self.logger.log(logging.DEBUG, "[SER {}]: eol {} timeout {} [s].".format(int(QThread.currentThreadId()), repr(eol_chars), timeout))

        # while time.time() < end_time:
        #     c = self.ser.read(1)
        #     if c:
        #         line += c
        #         if line[-eol_len:] == eol_chars:
        #             try:
        #                 decoded_text = line[:-eol_len].decode(encoding) # Strip EOL characters from end of line 
        #             except UnicodeDecodeError:
        #                 decoded_text = None  # or handle it in a way that makes sense for your application
        
        #             endTime = time.perf_counter()
        #             self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}, line {}.".format(int(QThread.currentThreadId()), endTime-startTime,  repr(line)))
        #             # 0.00013 to 0.00016 secs        
        #             return decoded_text
        #     else:
        #         break  # Timeout or other read issue

        # try:
        #     decoded_text = line.decode(encoding) # Return all characters read so far as there was no EOL found
        # except UnicodeDecodeError:
        #     decoded_text = None  # or handle it in a way that makes sense for your application

        # endTime = time.perf_counter()
        # self.logger.log(logging.DEBUG, "[SER {}]: tic toc {}, line {}.".format(int(QThread.currentThreadId()), endTime-startTime,  repr(line)))
        # return decoded_text

    # def writeline(self, text, eol_chars=b'\r\n', encoding='utf-8') -> int:
    def writeline(self, text) -> int:
        """ sends a line of text """
        # debugpy.debug_this_thread()
        len = self.sio.write(text)
        self.sio.flush()
        return len 
        
    def write(self, binaryArray: bytes) -> int:
        """ sends byte array """
        # debugpy.debug_this_thread()
        return self.ser.write(binaryArray)

    def avail(self) -> int:
        """ is there data in the serial receiving buffer? """
        # debugpy.debug_this_thread()
        if self.ser is not None:
            return self.ser.in_waiting
        else:
            return -1
    
    def clear(self):
        """ 
        clear serial buffers
        we want to clear not flush
        """
        # debugpy.debug_this_thread()
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
        if self.ser_open: return self._newline
        else: return ""

    @newline.setter
    def newline(self, val):
        """ sets serial ioWrapper line termination """
        if (val is None) or (val == ""):
            self.logger.log(logging.WARNING, "[SER {}]: newline not changed, need to provide string.".format(int(QThread.currentThreadId())))
            return
        if self.sio_open:
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