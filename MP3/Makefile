EXTRA_CFLAGS += -w
APP_EXTRA_FLAGS:= -O2 -ansi -pedantic
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules app1 app2

obj-m:= mp3.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules

app1: work.c
	$(GCC) -o work work.c

app2: monitor.c
	$(GCC) -o monitor monitor.c

clean:
	$(RM) -f node work monitor *~ *.ko *.o *.mod.c *.o.cmd Module.symvers modules.order
