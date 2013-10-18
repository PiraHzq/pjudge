all: server client judge

CC=gcc
CFLAGS=-Wall -D_REENTRANT
MYSQL_PATH=-I/opt/mysql/server-5.6/include -L/opt/mysql/server-5.6/lib
MYSQL_LIB=-lmysqlclient
THREAD=-lpthread
.c.o:
	$(CC) $(CFLAGS) $(MYSQL_PATH) -c $<
server.o: server.c pjudge.h
client.o: client.c pjudge.h
judge.o: judge.c pjudge.h oksyscall.h

server: server.o
	$(CC) $(CFLAGS) $(MYSQL_PATH) -o server server.o $(THREAD) $(MYSQL_LIB)

client: client.o
	$(CC) $(CFLAGS) $(MYSQL_PATH) -o client client.o $(MYSQL_LIB)

judge: judge.o
	$(CC) $(CFLAGS) $(MYSQL_PATH) -o judge judge.o $(MYSQL_LIB)

clean:
	rm -f server client judge *.o
