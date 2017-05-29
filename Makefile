
APP_NAME=bst
CC=g++
INC_DIR=.
CFLAGS=-c -std=c++11 -Wall -I$(INC_DIR)

all: $(APP_NAME)

$(APP_NAME): main.o
	$(CC) -pthread -o $(APP_NAME) main.o

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

clean:
	rm -rf *.o $(APP_NAME)
