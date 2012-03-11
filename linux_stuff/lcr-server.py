import serial
import time
import sys
import socket
from threading import Thread
import signal
import os

#default settings
portNo = 2200
debug = False
sstopFlag = False

#signal  handler for dumping internal state
def sig_handler(signum, frame):
	if debug:
		print 'Signal handler called with signal ', signum
	f = open('dump.log', 'w+')
	f.write( "--- DUMPING STATE ---\n")
	for key, val in th.data.iteritems():
		f.write("["+key+"] " + val)
	f.write("---------------------\n")
	f.close()


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
		self.data = {}
		self.stopFlag = False;
      
	def notifyStop(self):
		self.stopFlag = True
		print "Shutting down serial port thread, notified stop."
	
	def run(self):
		while self.stopFlag == False:
			try:
				# configure the serial connections
				sr = serial.Serial(port = self.serialPort, baudrate = 57600,
					parity = serial.PARITY_NONE, stopbits = serial.STOPBITS_ONE, bytesize = serial.EIGHTBITS,
					xonxoff = False, rtscts = False, dsrdtr = False, timeout = 2)
				if self.debug:
					print "Trying to open serial port " + self.serialPort + ", settings: " + str(sr)
				sr.open()
			except:
				print "Fatal error opening serial port, aborting..."
				sys.exit(3)
				
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
					key = msg[2:4]
					if key is not '\x00\x00':
						self.data[key] = (currentTime + msg)
				else:
					if self.debug:
						print "Nothing read from serial port"

			#close serial port
			sr.close()
			print "Closing serial port."



#----- MAIN --------------
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

signal.signal(signal.SIGUSR1, sig_handler)
if debug:
	print "Handler registred for SIGUSR1"

#start serial port thread
th = spReader('/dev/ttyUSB0', debug)
th.start()

serversocket = None;
clientsocket = None;
#start TCP listener
try:
	# check for serial port thread erros first
	if not th.isAlive():
		sstopFlag = True;
	
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
		if not th.isAlive():
			sstopFlag = True;
		try:
			# try to get connected until sstopFlag (CTRL+C)
			if debug:
				print "Listening for incoming connection."
			(clientsocket, address) = serversocket.accept()
			if debug:
				print "Connected, sending current data: " + str(th.data)
			for val in th.data.values():
				clientsocket.send(val)
			clientsocket.close()
			if debug:
				print "Sent successfuly."
		except socket.timeout:
			if debug:
				print "Socket accept timeout."
		except IOError as ioe:
			if debug:
				print "IOError catched and ignored: ", type(ioe), ioe
		
	serversocket.close()
	print "Shutting down socket server."
except KeyboardInterrupt:
	print "  SIGINT caught, stopping thread..."
	th.notifyStop()
	sstopFlag = True;


#wait for serial port reader thread to end
th.join()
sys.exit(0)
