import serial
import time
import sys
import socket
from threading import Thread

#default settings
portNo = 2200
debug = False
sstopFlag = False

# prints usage
def show_usage(argv):
	print "Usage: "
	print str(argv[0]) + " <port> " + " [debug]"
	print "  port - port number to listen for incoming connections"
	print "  debug - if passed then debug mode is on"


# thread for reading from JeeLink/Node over serial port
class spReader(Thread):
	def __init__ (self, serialPort, debug):
		Thread.__init__(self)
		self.debug = debug
		self.serialPort = serialPort
		self.data = ""
		self.stopFlag = False;
      
	def notifyStop(self):
		self.stopFlag = True
		print "Shutting down serial port thread, notified stop."
	
	def run(self):
		while self.stopFlag == False:
			# configure the serial connections
			sr = serial.Serial(port = self.serialPort, baudrate = 57600,
				parity = serial.PARITY_NONE, stopbits = serial.STOPBITS_ONE, bytesize = serial.EIGHTBITS,
				xonxoff = False, rtscts = False, dsrdtr = False, timeout = 2)

			if self.debug:
				print "Trying to open serial port " + self.serialPort + ", settings: " + str(sr)

			sr.open()
			time.sleep(2)

			if self.debug:
				print "Starting reading serial port."
				
			while self.stopFlag == False:
				msg = str(sr.readline())
				if len(msg) >= 13:
					currentTime = time.strftime("%Y-%m-%d %H:%M:%S ")
					if self.debug:
						print currentTime + msg
					#save into shared variable
					self.data = (currentTime + msg)
				else:
					if self.debug:
						print "Nothing read from serial port."

			#close serial port
			sr.close()
			print "Closing serial port."



#-----------------------------------------------------------------------
#read input arguments
if len(sys.argv) < 2:
	show_usage(sys.argv)
	sys.exit(1)
else:
	portNo = int(sys.argv[1])

if len(sys.argv) >= 3:
	arg = sys.argv[2]
	if arg == "debug":
		debug = True;


#start serial port thread
th = spReader('/dev/ttyUSB0', debug)
th.start()

serversocket = None;
clientsocket = None;
#start TCP listener
try:
	if debug:
		print "Creating server socket."
	# create an INET, STREAMing socket and bind
	socket.setdefaulttimeout(2.0)
	serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	serversocket.settimeout(2.0)
	serversocket.bind(('0.0.0.0', 2200))
	# become a server socket
	serversocket.listen(5)
	while sstopFlag == False:
		try:
			# try to get connected until sstopFlag (CTRL+C)
			if debug:
				print "Listening for incoming connection."
			(clientsocket, address) = serversocket.accept()
			if debug:
				print "Connected, sending current data: " + th.data
			clientsocket.send(th.data)
			clientsocket.close()
			if debug:
				print "Sent successfuly."
		except socket.timeout, msg:
			if debug:
				print "Socket accept timeout."
	serversocket.close()
	print "Shutting down socket server."
except KeyboardInterrupt:
	print "  SIGINT caught, stopping thread..."
	th.notifyStop()
	sstopFlag = True;


#wait for serial port reader thread to end
th.join()
sys.exit(0)
