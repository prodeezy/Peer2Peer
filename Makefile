# Macros

CC = g++
CFLAGS = -g -Wall -D_REENTRANT
#OBJ = main.o keyboard.o timer.o iniParser.o incomingConnectionsHandle.o outgoingConnectionsHandler.o signalHandler.o keepAlive.o logEntry.o beaconConnect.o indexSearch.o metaData.o
OBJ = main.o keyboardReader.o beaconConnect.o timer.o signalHandler.o iniParser.o  keepAlive.o incomingConnectionsHandle.o outgoingConnectionsHandler.o 
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
iniParser.o: iniParser.cpp
	$(CC) $(CFLAGS) -c iniParser.cpp $(INC)
keyboardReader.o: keyboardReader.cpp
	$(CC) $(CFLAGS) -c keyboardReader.cpp $(INC)	
incomingConnectionsHandle.o: incomingConnectionsHandle.cpp
	$(CC) $(CFLAGS) -c incomingConnectionsHandle.cpp $(INC)
outgoingConnectionsHandler.o: outgoingConnectionsHandler.cpp
	$(CC) $(CFLAGS) -c outgoingConnectionsHandler.cpp $(INC)
signalHandler.o: signalHandler.cpp
	$(CC) $(CFLAGS) -c signalHandler.cpp $(INC)
keepAlive.o: keepAlive.cpp
	$(CC) $(CFLAGS) -c keepAlive.cpp $(INC)
#logEntry.o: logEntry.cpp
#	$(CC) $(CFLAGS) -c logEntry.cpp $(INC)
beaconConnect.o: beaconConnect.cpp
	$(CC) $(CFLAGS) -c beaconConnect.cpp $(INC)
#indexSearch.o: indexSearch.cpp
#	$(CC) $(CFLAGS) -c indexSearch.cpp $(INC)
#metaData.o: metaData.cpp
#	$(CC) $(CFLAGS) -c metaData.cpp $(INC)

