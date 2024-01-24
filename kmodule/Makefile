obj-m += kmod.o
CFLAGS_kmod.o := -Wall -O0
PWD := $(shell pwd)

all: 
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

install:
	make clean && make
	sudo rmmod kmod.ko || true
	sudo insmod kmod.ko

remove:
	sudo rmmod kmod.ko

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean