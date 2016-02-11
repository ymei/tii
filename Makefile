OSTYPE = $(shell uname)
ARCH   = $(shell uname -m)
##################################### Defaults ################################
CC             := gcc
INCLUDE        := -I.
CFLAGS         := -Wall -std=gnu99 -pedantic -O2
CFLAGS_32      := -m32
SHLIB_CFLAGS   := -fPIC -shared
SHLIB_EXT      := .so
LIBS           := -lutil
LDFLAGS        :=
############################# Library add-ons #################################
INCLUDE += -I/opt/local/include -I/usr/local/include
#LIBS    += -L/opt/local/lib -L/usr/local/lib -lpthread -lhdf5
#GSLLIBS  = $(shell gsl-config --libs)
GLLIBS   =
############################# OS & ARCH specifics #############################
ifneq ($(OSTYPE), Linux)
  ifeq ($(OSTYPE), Darwin)
    CC            = clang
    GLLIBS       += -framework GLUT -framework OpenGL -framework Cocoa
    SHLIB_CFLAGS := -dynamiclib
    SHLIB_EXT    := .dylib
    ifeq ($(shell sysctl -n hw.optional.x86_64), 1)
      ARCH       := x86_64
    endif
  else ifeq ($(OSTYPE), FreeBSD)
    CC      = clang
    GLLIBS += -lGL -lGLU -lglut
  else ifeq ($(OSTYPE), SunOS)
    CFLAGS     := -Wall
  else
    # Let's assume this is win32
    SHLIB_EXT  := .dll
  endif
else
  GLLIBS += -lGL -lGLU -lglut
endif

ifneq ($(ARCH), x86_64)
  CFLAGS_32 += -m32
endif

# Are all G5s ppc970s?
ifeq ($(ARCH), ppc970)
  CFLAGS += -m64
endif
############################ Define targets ###################################
SHLIBS = libmreadarray$(SHLIB_EXT)
ifeq ($(ARCH), x86_64) # compile a 32bit version on 64bit platforms
  SHLIBS += libmreadarray_m32$(SHLIB_EXT)
endif

EXE_TARGETS = tiic tiis
DEBUG_EXE_TARGETS = tii

.PHONY: exe_targets shlib_targets debug_exe_targets clean
exe_targets: $(EXE_TARGETS)
shlib_targets: $(SHLIB_TARGETS)
debug_exe_targets: $(DEBUG_EXE_TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
tiic: tiic.c tii.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
tiis: tiis.c tii.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
tii: tii.c tii.h
	$(CC) $(CFLAGS) -DTII_DEBUG_ENABLE_MAIN -o $@ $<
clean:
	rm -f *.o *.so *.dylib *.dll *.bundle

