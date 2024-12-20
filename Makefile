LDFLAGS=-L/usr/local/lib
CFLAGS=-O3 -Wall -Wuninitialized -fomit-frame-pointer -funroll-loops \
	-fstrength-reduce -DNODEBUG -I/usr/local/include/libpng `libpng-config --I_opts`
LDLIBS=-lpng -lz -lm

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
	mkdir out

out/%.o: optark/%.c out/
	gcc -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

optar: out/optar.o out/common.o out/golay_codes.o out/parity.o
	gcc $(LDFLAGS) -o $@ $^

out/golay_codes.c: out/golay
	./$< > $@

out/golay: out/golay.o out/parity.o
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

unoptar: out/unoptar.o out/common.o out/golay_codes.o out/parity.o
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean all install uninstall