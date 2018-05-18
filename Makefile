# Makefile for netapp hw3
CC := gcc
CFLAGS := -g -Wall -o3 -std=gnu99

TGT := server
SRC := server.c \
       user.c \
       common.c
OBJ := $(SRC:.c=.o)

TGT2 := client
SRC2 := client.c \
        common.c
OBJ2 := $(SRC2:.c=.o)

TGT3 := ipc_server
SRC3 := ipc_server.c \
        ipc_user.c \
        common.c
OBJ3 := $(SRC3:.c=.o)

all: $(TGT) $(TGT2) $(TGT3)

$(TGT): $(OBJ)

$(TGT2): LDLIBS = -pthread
$(TGT2): $(OBJ2)

$(TGT3): LDLIBS = -pthread
$(TGT3): $(OBJ3)

dep:
	$(CC) -M *.c > depend

pack:
	rm -f B023040025_NETAPP_HW3.tar
	tar cvf B023040025_NETAPP_HW3.tar *.c *.h Makefile README.md

clean:
	@rm -f $(TGT) $(TGT2) $(TGT3) $(OBJ) $(OBJ2) $(OBJ3) depend
