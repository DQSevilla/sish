PROG := sish
OBJS := builtins.o
CFLAGS := -g -Wall -Werror -Wextra -Wformat=2 -Wjump-misses-init \
	-Wlogical-op -Wpedantic -Wshadow

# For portability between Linux and NetBSD:
UNAME := $$(uname -s)
EXTRA_LDFLAGS := $$(if [ x"${UNAME}" = x"Linux" ]; then echo "-lbsd"; fi)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(EXTRA_LDFLAGS) $(OBJS) $@.c -o $@

clean:
	rm -f $(PROG) $(OBJS)

re: clean all

.PHONY: all clean re

