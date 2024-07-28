#
CC	= gcc -g
COPTS	= -I/usr/src/linux-`uname -r`/include
KOPTS	= -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -mpreferred-stack-boundary=2 \
 -march=pentiumpro -I/usr/src/linux-`uname -r`/include/asm-i386/mach-default \
 -g -nostdinc -iwithprefix include \
 -DMODULE  -D__KERNEL__ -DKBUILD_BASENAME=$* -DKBUILD_MODNAME=$*
CFLAGS	= -Wall $(COPTS) $(KOPTS) -pipe
LDFLAGS = -m elf_i386 -r
TARGETS = rdtsc_drv.o rdtsc

all:	$(TARGETS)

rdtsc:
	edtsc.c

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

clean:
	rm -f *.o *~
