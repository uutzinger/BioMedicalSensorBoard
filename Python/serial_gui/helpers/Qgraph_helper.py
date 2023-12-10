############################################################################################
# QT Chart Helper
############################################################################################
# Fall 2022: initial work
# ------------------------------------------------------------------------------------------
# Urs Utzinger
# University of Arizona 2023
############################################################################################

""" 
https://www.pythonguis.com/tutorials/embed-pyqtgraph-custom-widgets-qt-app/
https://www.pyqtgraph.org/
https://www.youtube.com/watch?v=oWERFvUVDTg&ab_channel=UpskillwithGeeksforGeeks
https://www.pythonguis.com/tutorials/plotting-pyqtgraph/
https://stackoverflow.com/questions/65332619/pyqtgraph-scrolling-plots-plot-in-chunks-show-only-latest-10s-samples-in-curre
https://github.com/pbmanis/EKGMonitor
""" 

import re, logging, time

from PyQt5.QtCore import QObject, QTimer, QThread, pyqtSignal, pyqtSlot, QStandardPaths
from PyQt5.QtWidgets import QFileDialog, QLineEdit, QSlider, QTabWidget, QGraphicsView, QVBoxLayout

# QT Graphing for chart plotting
import pyqtgraph as pg

# Numerical Math
import numpy as np

# Constants
########################################################################################
MAX_ROWS = 44100 # data history length
MAX_COLUMNS = 4 # max number of signal traces
SEPARATORS = "[,;.\s]" # use most common data separator characters

########################################################################################

def clip_value(value, min_value, max_value):
    return max(min_value, min(value, max_value))

class CircularBuffer():
    '''
    This is a circular buffer to store numpy data.
    
    It initialized based on MAX_ROWS and MAX_COLUMNS.
    You add data by pushing a numpy array to it. The numpy array width needs to match MAX_COLUMNS.
    You access the data by the data property. It automatically rearranges the data with wrapping around.
    '''    
    def __init__(self):
        self._data = np.full((MAX_ROWS, MAX_COLUMNS+1), -np.inf)
        self._index = 0
        
    def push(self, data_array):
        num_new_rows, num_new_cols = data_array.shape
        if num_new_cols != MAX_COLUMNS+1:
            raise ValueError("Data array must have {} columns".format(MAX_COLUMNS+1))
        end_index = (self._index + num_new_rows) % MAX_ROWS
        if end_index < self._index:
            # wrapping is necessary
            self._data[self._index:MAX_ROWS] = data_array[:MAX_ROWS - self._index]
            self._data[:end_index] = data_array[MAX_ROWS - self._index:]
        else:
            # no wrapping necessary
            self._data[self._index:end_index] = data_array
                    
        self._index = end_index
    
    @property
    def data(self):
        if self._index == 0:
            return self._data
        else:
            return np.vstack((self._data[self._index:], self._data[:self._index]))

############################################################################################
# QChart interaction with Graphical User Interface
############################################################################################
    
class QChartUI(QObject):
    """
    QT Chart Interface for QT
    
    The chart displays up to signals in a plot.
    The data is received from the serial port and organized into columns of a numpy array.
    The plot can be zoomed in by selecting how far back in time to display it.
    The horizontal axis is the sample number.
    THe vertical axis is auto scaled to the max and minimum values of the data.

    Slots (functions available to respond to external signals)
        on_pushButton_Start
        on_pushButton_Stop
        on_pushButton_Clear
        on_pushButton_Save
        on_HorizontalSliderChanged(value)
        on_HorizontalLineEditChanged
        on_newLineReceived(text)
        on_newLinesReceived(lines)
        
    Functions
        updatePlot
        
    """
    
    # Signals
    ########################################################################################

    startRequest                 = pyqtSignal()                                            # 
    stopRequest                  = pyqtSignal()                                            # 
    changeLineTerminationRequest = pyqtSignal(bytes)                                       # request line termination to change
    saveRequest                  = pyqtSignal()                                            #
    clearRequest                 = pyqtSignal()                                            #
    finishWorkerRequest          = pyqtSignal()                                            # request worker to finish
               
    def __init__(self, parent=None, ui=None, serialUI=None, serialWorker=None):
        # super().__init__()
        super(QChartUI, self).__init__(parent)

        if ui is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to User Interface".format(int(QThread.currentThreadId())))
        self.ui = ui

        if serialUI is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to Serial User Interface".format(int(QThread.currentThreadId())))
        self.serialUI = serialUI

        if serialWorker is None:
            self.logger.log(logging.ERROR, "[{}]: need to have access to Serial Worker".format(int(QThread.currentThreadId())))
        self.serialWorker = serialWorker

        self.chartWidget = pg.PlotWidget()
        
        # Replace the GraphicsView widget in the User Interface with the pyqtgraph plot
        self.tabWidget = self.ui.findChild(QTabWidget, 'tabWidget_MainWindow')
        self.graphicsView = self.ui.findChild(QGraphicsView, 'chartView')
        self.tabLayout = QVBoxLayout(self.graphicsView)
        self.tabLayout.addWidget(self.chartWidget)
        
        # Set up the plotWidget
        self.chartWidget.setBackground('w')
        self.chartWidget.showGrid(x=True, y=True)
        self.chartWidget.setLabel('left', 'Signal', units='V')
        self.chartWidget.setLabel('bottom', 'Sample', units='')
        self.chartWidget.setTitle("Chart")
        self.chartWidget.setMouseEnabled(x=True, y=True)
                
        self.sample_number = 0  # A pointer to indicate the current x position, increases monotonically
        self.pen = [pg.mkPen(color, width=2) for color in ['green', 'red', 'blue', 'black']]
        self.data_line = [self.chartWidget.plot([], [], pen=self.pen[i % len(self.pen)], name=str(i)) for i in range(MAX_COLUMNS)]
        
        self.maxPoints = 1024 # maximum number of points to show in a plot from now to the past
        
        self.buffer = CircularBuffer()
        
        self.textDataSeparator = ','
        
        # Setup the plot
        self.chartWidget.setXRange(0, self.maxPoints)
        self.chartWidget.setYRange(-1., 1.)

        # Set up the horizontal slider
        horizontalSlider = self.ui.findChild(QSlider, "horizontalSlider_Zoom")
        horizontalSlider.setMinimum(16)
        horizontalSlider.setMaximum(MAX_ROWS)
        horizontalSlider.setValue(int(self.maxPoints))  
        lineEdit = self.ui.findChild(QLineEdit, "lineEdit_Horizontal")
        lineEdit.setText(str(self.maxPoints))
        
        self.logger = logging.getLogger("QChartUI_")           

        # Update the plot periodically
        self.ChartTimer = QTimer()
        self.ChartTimer.setInterval(100)  # milliseconds, we can not see more than 50 Hz
        self.ChartTimer.timeout.connect(self.updatePlot)
                
        self.logger.log(logging.INFO, "[{}]: initialized.".format(int(QThread.currentThreadId())))

    # Response Functions to User Interface Signals
    ########################################################################################

    def updatePlot(self):
        """
        Update the chart plot
        
        Do not plot data that is -np.inf
        Determine the number of signal traces
        Populate the data_line traces with the data
        Set the horizontal range to show from newest data to maxPoints back in time
        Set vertical range to min and max of data
        """
        
        tic = time.perf_counter()
        data = self.buffer.data # takes 0.5-1 ms

        # where is the last valid row
        row_indx = data[:,0] != -np.inf

        # we have valid data
        if np.any(row_indx): 
            x = data[row_indx,0] # get the sample numbers
            # do we have valid data in each column?
            col_indx = np.all(data[row_indx,:] != -np.inf, axis=0)
            max_y = -np.inf
            min_y = np.inf
            for i in range(MAX_COLUMNS):
                if col_indx[i+1]:
                    y = data[row_indx,i+1]
                    self.data_line[i].setData(x, y)
                    max_y = max([np.nanmax(y[-self.maxPoints:]), max_y])
                    min_y = min([np.nanmin(y[-self.maxPoints:]), min_y])
                else:
                    self.data_line[i].setData([], [])

            self.chartWidget.setXRange(x[-1] - self.maxPoints, x[-1])
            if min_y <= max_y: self.chartWidget.setYRange(min_y, max_y)
            self.chartWidget.addLegend()
            # takes 11ms

            toc = time.perf_counter()            
            self.logger.log(logging.DEBUG, "[{}]: plot updated in {} ms".format(
                int(QThread.currentThreadId()), 1000*(toc-tic)))

    @pyqtSlot()
    def on_changeDataSeparator(self):
        ''' user wants to change the data separator '''
        _tmp = self.ui.comboBoxDropDown_DataSeparator.currentText()
        if _tmp == "comma (,)":
            self.textDataSeparator = ','
        elif _tmp == "semicolon (;)":
            self.textDataSeparator = ';'
        elif _tmp == "point (.)":
            self.textDataSeparator = '.'
        elif _tmp == "space (\\s)":
            self.textDataSeparator = ' '
        else:
            self.textDataSeparator = ','            
        self.logger.log(logging.INFO, "[{}]: data separator {}".format(int(QThread.currentThreadId()), repr(self.textDataSeparator)))

    @pyqtSlot(list)
    def on_newLinesReceived(self, lines: list):
        """
        Decode a received lit of text lines and add data to the date array
        
        Decode numbers
        Shift data one row up
        Increment sample counter for the first column of the data array
        Add new data to end of data array
        """
        tic = time.perf_counter()
        # parse text into numbers
        data = [list(map(float, filter(None, line.split(self.textDataSeparator)))) for line in lines]
        try:
            data_array = np.array(data, dtype=float)
        except:
            # likely we have a list of lists with different lengths
            row_len = [len(data_rows) for data_rows in data]
            max_length = max(row_len)
            # Pad shorter lists with np.nan (or another value)
            padded_data = [data_row + [np.nan]*(max_length - len(data_row)) for data_row in data]
            data_array = np.array(padded_data, dtype=float)

        num_rows, num_cols = data_array.shape
        sample_numbers = np.arange(self.sample_number, self.sample_number + num_rows)
        sample_numbers = sample_numbers.reshape(-1, 1)
        self.sample_number += num_rows
        right_pad = MAX_COLUMNS - num_cols
        if right_pad > 0:
            new_array = np.hstack([sample_numbers, data_array, np.full((num_rows, right_pad), -np.inf)])
        else:
            new_array = np.hstack([sample_numbers, data_array[:, :MAX_COLUMNS]])

        self.buffer.push(new_array)
        toc = time.perf_counter()

        self.logger.log(logging.DEBUG, "[{}]: {} data points received to add to the plot: took {} ms".format(int(QThread.currentThreadId()), num_rows, 1000*(toc-tic)))

    @pyqtSlot()
    def on_pushButton_StartStop(self):
        """
        Start/Stop plotting
        
        Connect serial receiver new data received
        Start timer
        """
        if self.ui.pushButton_ChartStartStop.text() == "Start":
            # start plotting
            self.serialWorker.linesReceived.disconnect(self.serialUI.on_SerialReceivedLines) # turn off text display
            self.serialWorker.linesReceived.connect(self.on_newLinesReceived) # turn on plotting
            self.ChartTimer.start()            
            if self.ui.pushButton_SerialStartStop.text() == "Start":
                self.serialReceiverIsRunning = False
                self.serialUI.startReceiverRequest.emit()
                self.serialUI.startThroughputRequest.emit()
                self.ui.pushButton_SerialStartStop.setText("Stop")
            else:
                self.serialReceiverIsRunning = True
            self.ui.pushButton_ChartStartStop.setText("Stop")
            self.logger.log(logging.INFO, "[{}]: start plotting".format(int(QThread.currentThreadId())))
        else:
            self.ChartTimer.stop()
            if self.serialReceiverIsRunning == False:
                self.serialUI.stopReceiverRequest.emit()
                self.serialUI.stopThroughputRequest.emit()
                self.ui.pushButton_ChartStartStop.setText("Start")
            self.serialWorker.linesReceived.disconnect(self.on_newLinesReceived)
            self.serialWorker.linesReceived.connect(self.serialUI.on_SerialReceivedLines) # turn on text display
            self.logger.log(logging.INFO, "[{}]: stop plotting".format(int(QThread.currentThreadId())))
            self.ui.pushButton_SerialStartStop.setText("Start")

    @pyqtSlot()
    def on_pushButton_Clear(self):
        """
        Clear Plot
        
        Stop plotting
        Clear data
        Clear plot
        """
        # clear plot
        self.data = np.full((MAX_ROWS, MAX_COLUMNS+1), -np.inf)
        self.updatePlot()  
        self.logger.log(logging.INFO, "[{}]: Clear plotted data.".format(int(QThread.currentThreadId())))

    @pyqtSlot()
    def on_pushButton_Save(self):
        """ 
        Save data into Text File 
        """
        # self.ui.serialWorker.textReceived.disconnect(self.on_newLineReceived)
        self.ui.serialWorker.linesReceived.disconnect(self.on_newLinesReceived)
        stdFileName = QStandardPaths.writableLocation(QStandardPaths.DocumentsLocation) + "/data.txt"
        fname = QFileDialog.getSaveFileName(self, 'Save as', stdFileName, "Text files (*.txt)")
        np.savetxt(fname, self.data, delimiter=',')
        # self.ui.serialWorker.textReceived.connect(self.on_newLineReceived)
        self.ui.serialWorker.linesReceived.connect(self.on_newLinesReceived)
        self.logger.log(logging.INFO, "[{}]: Saved plotted data.".format(int(QThread.currentThreadId())))

    @pyqtSlot(int)
    def on_HorizontalSliderChanged(self,value):
        """ 
        Serial Plotter Horizontal Slider Handling
        This sets the maximum number of points back in history shown on the plot
        
        Update the line edit box when the slider is moved
        Update how far back in history we plot 
        """
        lineEdit = self.ui.findChild(QLineEdit, "lineEdit_Horizontal")
        lineEdit.setText(str(int(value)))
        self.maxPoints = int(value)
        self.logger.log(logging.DEBUG, "[{}]: horizontal zoom set to {}.".format(int(QThread.currentThreadId()), int(value)))

    @pyqtSlot()
    def on_HorizontalLineEditChanged(self):
        """ 
        Serial Plotter Horizontal Line Edit Handling
        Same as above but entering the number manually
        
        When text is entered manually into the horizontal edit field, 
          update the slider and update the history range 
        """
        sender = self.sender()
        value = int(sender.text())
        if value >= 16 and value <= MAX_ROWS and self.ui is not None:
            horizontalSlider = self.ui.findChild(QSlider, "horizontalSlider_Zoom")
            horizontalSlider.setValue(int(value))           
            self.maxPoints = int(value)
            self.logger.log(logging.DEBUG, "[{}]: horizontal zoom line edit set to {}.".format(int(QThread.currentThreadId()), value))

#####################################################################################
# Testing
#####################################################################################

if __name__ == '__main__':
    # not implemented
    pass