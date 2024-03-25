SHELL := /bin/bash
PWD := $(shell pwd)

CC := /usr/bin/gcc
CFLAGS := -I$(PWD)/

UL_SRC = $(wildcard $(PWD)/*.c)
UL_OBJ = $(patsubst %.c, %.o, $(UL_SRC))

obj-m += kmod.o
CFLAGS_kmod.o := -Wall -O0

EXEC := encos_dev_user
EXEC2 := mprotect_test

all: $(EXEC) $(EXEC2)

$(EXEC): encos_dev_user.c
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC2): mprotect_test.c
	$(CC) $(CFLAGS) -o $@ $^

module: 
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

install:
	make clean && make
	sudo rmmod kmod.ko || true
	sudo insmod kmod.ko

remove:
	sudo rmmod kmod.ko

clean:
	rm -rf *.o $(EXEC) $(EXEC2)
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean