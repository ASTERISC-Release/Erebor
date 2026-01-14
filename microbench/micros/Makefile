SHELL := /bin/bash
PWD := $(shell pwd)

CC := /usr/bin/gcc
CFLAGS := -I$(PWD)/

UL_SRC = $(wildcard $(PWD)/*.c)
UL_OBJ = $(patsubst %.c, %.o, $(UL_SRC))

obj-m += smc_bench.o
CFLAGS_smc_bench.o := -Wall -I$(PWD)/	

obj-m += cr_msr_bench.o
CFLAGS_cr_msr_bench.o := -Wall -I$(PWD)/

EXEC := syscall_bench
EXEC2 := tdreport

all: $(EXEC) $(EXEC2)

$(EXEC): syscall_bench.c
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC2): tdreport.c
	$(CC) $(CFLAGS) -o $@ $^

module: 
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

clean:
	rm -rf *.o $(EXEC) $(EXEC2)
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean