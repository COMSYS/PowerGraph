obj-m += pgboard.o
pgboard-objs := main.o device_registration.o debug.o bcm2835_interface.o

export KCFLAGS = -Wno-declaration-after-statement

all:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
