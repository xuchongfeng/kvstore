CC = gcc
JSON_C_DIR = lib/json-c/
SERVER_SRC = src/server
CLIENT_SRC = src/client
MAIN_SRC = $(SERVER_SRC)/main
CFLAGS = -std=gnu99 -g -Wall -I $(SERVER_SRC) -I$(JSON_C_DIR)/include/
MKDIR_P = mkdir -p

LINKFLAGS = -lpthread -L$(JSON_C_DIR)/lib -ljson-c
BIN = ./bin

TESTEXE = $(BIN)/kvtests

SRCS = $(wildcard src/server/*.c)
MAIN_SRCS = $(wildcard src/server/main/*.c)

OBJS = $(SRCS:.c=.o)
MAIN_OBJS = $(MAIN_SRCS:.c=.o)

TESTSRCS = $(wildcard tests/*.c)
TESTOBJS = $(TESTSRCS:.c=.o)

all: json $(BIN)/kvslave $(BIN)/kvmaster
	ln -sf ../src/client/kvclient.py bin/kvclient.py
	ln -sf ../src/client/interactive_client bin/interactive_client
	ln -sf ../src/client/kvclient.rb bin/kvclient.rb
	ln -sf ../src/server/main/tpc_server bin/tpc_server

$(BIN)/%: $(MAIN_SRC)/%.o $(OBJS)
	$(MKDIR_P) $(BIN)
	$(CC) $(OBJS) $< $(LINKFLAGS) -o $@

test: $(OBJS) $(TESTOBJS)
	$(MKDIR_P) $(BIN)
	$(CC) -o $(TESTEXE) $^ $(LINKFLAGS)

check2: test
	./$(TESTEXE) checkpoint2

check1: test
	./$(TESTEXE) checkpoint1

check: test
	./$(TESTEXE)

json: $(JSON_C_DIR)/Makefile
	make -C ./lib/json-c install

$(JSON_C_DIR)/Makefile:
	git submodule update --init
	cd ./lib/json-c; ./configure --prefix=`pwd`
skeleton:
	./create_skeleton
clean:
	rm -rf $(BIN) tests/*.o *.pyc test_tmp_dir
	$(MAKE) -C src/server clean
	$(MAKE) -C lib/json-c clean

.PHONY: all clean check json-c json-c-make
