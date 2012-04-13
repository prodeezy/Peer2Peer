# Macros

CC = g++
CFLAGS = -g -Wall -D_REENTRANT
#OBJ = main.o keyboard.o timer.o iniParser.o incomingConnectionsHandle.o outgoingConnectionsHandler.o signalHandler.o keepAlive.o logEntry.o beaconConnect.o indexSearch.o metaData.o
OBJ = main.o keyboard.o clientThread.o timer.o signalHandler.o  keepAliveTimer_thread.o incomingConnectionsHandle.o outgoingConnectionsHandler.o 
#LIBS = -lcrypto -lpthread
#INC = 
LIBS = -L/home/scf-22/csci551b/openssl/lib -lcrypto -lnsl -lsocket -lresolv
INC = -I/home/scf-22/csci551b/openssl/include

# Explicit rule
all: sv_node

sv_node: $(OBJ)
	$(CC) $(CFLAGS) -o sv_node $(OBJ) $(INC) $(LIBS) 

install:
	
clean:
	rm -rf *.o sv_node 

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp $(INC)
timer.o: timer.cpp
	$(CC) $(CFLAGS) -c timer.cpp $(INC)
clientThread.o: clientThread.cpp
	$(CC) $(CFLAGS) -c clientThread.cpp $(INC)
keyboard.o: keyboard.cpp
	$(CC) $(CFLAGS) -c keyboard.cpp $(INC)	
incomingConnectionsHandle.o: incomingConnectionsHandle.cpp
	$(CC) $(CFLAGS) -c incomingConnectionsHandle.cpp $(INC)
outgoingConnectionsHandler.o: outgoingConnectionsHandler.cpp
	$(CC) $(CFLAGS) -c outgoingConnectionsHandler.cpp $(INC)
signalHandler.o: signalHandler.cpp
	$(CC) $(CFLAGS) -c signalHandler.cpp $(INC)
keepAliveTimer_thread.o: keepAliveTimer_thread.cpp
	$(CC) $(CFLAGS) -c keepAliveTimer_thread.cpp $(INC)
#logEntry.o: logEntry.cpp
#	$(CC) $(CFLAGS) -c logEntry.cpp $(INC)
#beaconConnect.o: beaconConnect.cpp
#	$(CC) $(CFLAGS) -c beaconConnect.cpp $(INC)
#indexSearch.o: indexSearch.cpp
#	$(CC) $(CFLAGS) -c indexSearch.cpp $(INC)
#metaData.o: metaData.cpp
#	$(CC) $(CFLAGS) -c metaData.cpp $(INC)

