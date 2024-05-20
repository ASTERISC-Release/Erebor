SHELL := /bin/bash
PWD := $(shell pwd)

CC := /usr/bin/gcc
CFLAGS := -I$(PWD)/

UL_SRC = $(wildcard $(PWD)/*.c)
UL_OBJ = $(patsubst %.c, %.o, $(UL_SRC))

obj-m += smc_bench.o
CFLAGS_smc_bench.o := -Wall -I$(PWD)/	

EXEC := syscall_bench

all: $(EXEC)

$(EXEC): syscall_bench.c
	$(CC) $(CFLAGS) -o $@ $^

module: 
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

clean:
	rm -rf *.o $(EXEC)
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean