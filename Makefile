OSTYPE = $(shell uname)
ARCH   = $(shell uname -m)

CC             := gcc
INCLUDE        := -I.
CFLAGS         := -Wall -std=gnu99 -pedantic -O2
SHLIB_CFLAGS   := -shared
SHLIB_EXT      := .so
LIBS           := -lutil

ifneq ($(if $(filter Linux %BSD,$(OSTYPE)),OK), OK)
  ifeq ($(OSTYPE), Darwin)
    SHLIB_CFLAGS   := -dynamiclib
    SHLIB_EXT      := .dylib
    ifeq ($(shell sysctl -n hw.optional.x86_64), 1)
      ARCH           := x86_64
      CFLAGS_64      := -m64
    endif
  else
    ifeq ($(OSTYPE), SunOS)
      CFLAGS         := -c -Wall -std=c99 -pedantic
    else
      # Let's assume this is win32
      SHLIB_EXT      := .dll
    endif
  endif
endif

ifneq ($(ARCH), x86_64)
  CFLAGS += -fPIC
endif

ifeq ($(ARCH), x86_64)
  CFLAGS_64 += -fPIC
endif

# Are all G5s ppc970s?
ifeq ($(ARCH), ppc970)
  CFLAGS_64 += -m64
endif

SHLIBS = libmreadarray$(SHLIB_EXT)
ifeq ($(ARCH), x86_64)
  SHLIBS += libmreadarray_m32$(SHLIB_EXT)
endif

# shlibs: $(SHLIBS)
.PHONY: all clean
all: tiic tiis
tiic: tiic.c tii.o
	$(CC) $(CFLAGS) $(CFLAGS_64) $(LIBS) -o $@ $^
tiis: tiis.c tii.o
	$(CC) $(CFLAGS) $(CFLAGS_64) $(LIBS) -o $@ $^
tii.o: tii.c tii.h
	$(CC) $(CFLAGS) $(CFLAGS_64) -c -o $@ $<
tii: tii.c tii.h
	$(CC) $(CFLAGS) $(CFLAGS_64) -DTII_DEBUG_ENABLE_MAIN -o $@ $<
clean:
	rm -f *.o *.so *.dylib *.dll *.bundle
