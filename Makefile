TARGET = debug
GAWK = gawk

CFLAGS = -Wall -Wextra -std=gnu11
ifeq ($(TARGET), debug)
    CFLAGS += -fsanitize=address -fsanitize=undefined -Og -g3
else
    CFLAGS += -O3 -g
endif

MODULE_NAME = amdgpu_metrics
obj-m += $(MODULE_NAME).o

VENDOR_H = vendor/kgd_pp_interface.h
MAINLINE_REMOTE = https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain

GEN_DUMP_AWK = dumper/gen_dump_gpu_metrics_h.awk
DUMP_H = dumper/dump_gpu_metrics.h
DUMP_GENERATED_C := $(DUMP_H:.h=.generated.c)

PROG = utilities
SRCS = $(PROG).c $(DUMP_GENERATED_C) $(MODULE_NAME).h $(VENDOR_H)

.PHONY: all modules insmod test _test clean distclean sync

all: $(PROG) modules _test

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insmod: modules
	sudo rmmod $(MODULE_NAME) || true
	sudo insmod $(MODULE_NAME).ko

$(DUMP_GENERATED_C): $(GEN_DUMP_AWK) $(VENDOR_H) $(DUMP_H)
	$(GAWK) -f $(word 1,$^) $(word 2,$^) > $@

$(VENDOR_H):
	curl -L -o $@ $(MAINLINE_REMOTE)/drivers/gpu/drm/amd/include/kgd_pp_interface.h

$(PROG): $(SRCS) 
	$(CC) $(CFLAGS) -o $@ $(wordlist 1,2,$^)

test: $(PROG)
	./$(PROG)

_test:
	make test || true

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f $(PROG)

distclean: clean
	rm -f $(DUMP_GENERATED_C)

sync:
	rm -f $(VENDOR_H) $(DUMP_GENERATED_C)
	make $(VENDOR_H) $(DUMP_GENERATED_C)
