# Macros

CC = g++
CFLAGS = -g -Wall -D_REENTRANT
#OBJ = main.o keyboard.o timer.o iniParser.o incomingConnectionsHandle.o outgoingConnectionsHandler.o signalHandler.o keepAliveTimer.o logEntry.o connectBeacon.o indexSearch.o metaData.o
OBJ = main.o incomingConnectionsHandle.o outgoingConnectionsHandler.o 
LIBS = -lcrypto -lpthread
INC = 
#LIBS = -L/home.scf-22/csci551b/openssl/lib -lcrypto -lnsl -lsocket -lresolv
#INC = -I/home/scf-22/csci551b/openssl/include

# Explicit rule
all: sv_node

sv_node: $(OBJ)
	$(CC) $(CFLAGS) -o sv_node $(OBJ) $(INC) $(LIBS) 

install:

clean:
	rm -rf *.o sv_node 

main.o: main.cc
	$(CC) $(CFLAGS) -c main.cc $(INC)
#keyboard.o: keyboard.cc
#	$(CC) $(CFLAGS) -c keyboard.cc $(INC)
#timer.o: timer.cc
#	$(CC) $(CFLAGS) -c timer.cc $(INC)
#iniParser.o: iniParser.cc
#	$(CC) $(CFLAGS) -c iniParser.cc $(INC)
incomingConnectionsHandle.o: incomingConnectionsHandle.cc
	$(CC) $(CFLAGS) -c incomingConnectionsHandle.cc $(INC)
outgoingConnectionsHandler.o: outgoingConnectionsHandler.cc
	$(CC) $(CFLAGS) -c outgoingConnectionsHandler.cc $(INC)
#signalHandler.o: signalHandler.cc
#	$(CC) $(CFLAGS) -c signalHandler.cc $(INC)
#keepAliveTimer.o: keepAliveTimer.cc
#	$(CC) $(CFLAGS) -c keepAliveTimer.cc $(INC)
#logEntry.o: logEntry.cc
#	$(CC) $(CFLAGS) -c logEntry.cc $(INC)
#connectBeacon.o: connectBeacon.cc
#	$(CC) $(CFLAGS) -c connectBeacon.cc $(INC)
#indexSearch.o: indexSearch.cc
#	$(CC) $(CFLAGS) -c indexSearch.cc $(INC)
#metaData.o: metaData.cc
#	$(CC) $(CFLAGS) -c metaData.cc $(INC)

