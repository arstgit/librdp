CC=gcc

DEPS = rdp.h libdict/dict.h libdict/crc.h
OBJS = $(patsubst %.h,%.o,$(DEPS))

BUILD = release
CFLAGS_release = 
CFLAGS_debug = -g -O0 -DRDP_DEBUG
CFLAGS = ${CFLAGS_${BUILD}} 
CFLAGS += -fPIC 

# EXAMPLE: 
#   $ make clean && make BUILD=debug

all: librdp.so librdp.a rdptest rdptest-static

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

librdp.so: $(OBJS)
	$(CC) $(CFLAGS) -o librdp.so -shared $^

librdp.a: $(OBJS)
	ar rvs librdp.a $^

rdptest: test.o librdp.so
	$(CC) -o $@ -o $@ $< -L. -lrdp

rdptest-static: test.o librdp.a
	$(CC) -o $@ -o $@ $^

.PHONY: test
test: clean install rdptest
	./rdptest

.PHONY: install
install: librdp.so
	sudo cp -f rdp.h /usr/local/include/
	sudo cp -f librdp.so /usr/lib/

.PHONY: pretty
pretty:
	clang-format -i *.c *.h

anyway: clean all
.PHONY: clean
clean:
	rm -f **/*.o *.o *.so *.a rdptest*
