LDFLAGS=-L/usr/local/lib
CFLAGS=-O3 -Wall -Wuninitialized -fomit-frame-pointer -funroll-loops \
	-fstrength-reduce -DNODEBUG -I/usr/local/include/libpng `libpng-config --I_opts`
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
	gcc -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

out/%.o: optark/%.c out/
	gcc -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

out/golay: out/golay.o out/lib/parity.o
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

out/golay_codes.c: out/golay
	./$< > $@

unoptar: out/unoptar.o out/liboptark.a
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

optar: out/optar.o out/liboptark.a
	gcc $(LDFLAGS) -o $@ $^

out/liboptark.a: out/lib/liboptar.o out/lib/libunoptar.o out/lib/common.o out/lib/parity.o out/golay_codes.o
	ar -rcs $@ $^

.PHONY: clean all install uninstall