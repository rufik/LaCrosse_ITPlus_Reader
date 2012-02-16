This is simple Python 2.7 application for reading data from LaCrosse (weather station) wireless sensor (IT+ technology, like TX29-IT, TX29DTH-IT) using JeeLink/JeeNode USB device. Application uses serial port to read data. Data need to be length of 13 characters, because reading from port blocks until 13 characters arrived. So you should use my JeeLink code or modify app to your needs. You can find it here:
https://github.com/rufik/LaCrosse_ITPlus_Reader/tree/master/jeelink_stuff/


Requirements & installation (Ubuntu 11.10)
You need Python (2.7.x) installed with serial library:
    $ sudo apt-get install python  python-serial
JeeLink/JeeNode USB device with my code is required also, from obvious reasons :)


Usage
Just put this application into some directory (where you like best, ex. /opt/lcr/) and 
run it with debug=ON for a first time to see if it works well:
    $ python /opt/lcr/lcr-server.py 2200 debug
It should start listening on port 2200 (bound to all network interfaces) and start reading data from serial port at /dev/ttyUSB0. You can see some useful debug messages.
If you need to stop application, just press CTRL+C or send SIGINT signal using kill:
    $ kill -SIGING <pid>


Getting data from my application
There is simple client app written in C (for simplicity), which you can use to grab data from my "server" application. Just run it loke this:
    $ ./lcr-client localhost 2200
Or you can you just telnet:
    $ telnet localhost 2200
