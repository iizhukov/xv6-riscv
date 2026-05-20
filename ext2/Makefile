CC     = gcc
CFLAGS = -Wall -Wextra -g
PROGS  = ext2stat ext2cat ext2ls

.PHONY: all test clean

all: $(PROGS)

$(PROGS): %: %.c ext2.h
	$(CC) $(CFLAGS) -o $@ $<

test:
	podman run --privileged --rm \
	    -v "$(CURDIR):/work" \
	    ubuntu:latest \
	    bash /work/test.sh

clean:
	rm -f $(PROGS)
