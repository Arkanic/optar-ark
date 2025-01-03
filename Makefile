CC=gcc
AR=ar
PACKAGE_NAME=out.zip
LDFLAGS=-lpng
CFLAGS=-O3 -Wall -Wuninitialized -fomit-frame-pointer -funroll-loops -fstrength-reduce -DNODEBUG -lpng
LDLIBS=-lpng -lz -lm

SUBDIRS=lib

all: optar unoptar

install:
	install optar /usr/local/bin/
	install unoptar /usr/local/bin
	install pgm2ps /usr/local/bin

uninstall:
	rm /usr/local/bin/optar
	rm /usr/local/bin/unoptar
	rm /usr/local/bin/pgm2ps

clean:
	rm -rf out optar unoptar

out/:
	mkdir -p out

$(SUBDIRS):
	mkdir -p out/$(SUBDIRS)

out/$(SUBDIRS)/%.o: optark/$(SUBDIRS)/%.c out/ $(SUBDIRS)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

out/%.o: optark/%.c out/
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

out/golay: out/lib/golay.o out/lib/parity.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

out/golay_codes.c: out/golay
	./$< > $@

unoptar: out/unoptar.o out/liboptark.a out/arg.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

optar: out/optar.o out/liboptark.a out/arg.o
	$(CC) $(LDFLAGS) -o $@ $^

out/liboptark.a: out/lib/liboptar.o out/lib/libunoptar.o out/lib/common.o out/lib/dimensions.o out/lib/parity.o out/golay_codes.o
	$(AR) -rcs $@ $^

package: all
	zip -r $(PACKAGE_NAME) optar unoptar README.md doc

.PHONY: clean all install uninstall package