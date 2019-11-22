# General makefile for generic unix & linux platforms

TARGET = sdlpal-dingoo

INTER=.unix

HOST ?= mipsel-linux-

GENERATED = -DPAL_HAS_GIT_REVISION $(shell ../scripts/gengitrev)

DEPFLAGS = -MT $@ -MMD -MP -MF $*$(INTER).Td

USE_ALSA ?= $(shell echo "int main(){}" | gcc -o /dev/null -x c - -lasound 2>/dev/null && echo 1)
ifeq (${USE_ALSA},1)
	MIDI_CFILES = $(wildcard ../native_midi/*.c) native_midi_alsa.c
	MIDI_CPPFILES =
	MIDI_LDFLAGS = -lasound
else
	MIDI_CFILES =
	MIDI_CPPFILES = native_midi.cpp
	MIDI_LDFLAGS =
endif

CFILES = $(wildcard ../adplug/*.c) $(wildcard ../libmad/*.c) $(wildcard ../liboggvorbis/src/*.c) $(wildcard ../*.c) $(MIDI_CFILES)
CPPFILES = $(wildcard ../adplug/*.cpp) $(wildcard ../*.cpp) unix.cpp $(MIDI_CPPFILES)
OBJFILES = $(CFILES:.c=$(INTER).o) $(CPPFILES:.cpp=$(INTER).o)
DEPFILES = $(OBJFILES:.o=.d)
SYSROOT    = $(shell $(CC) --print-sysroot)
SDL_CONFIG = $(SYSROOT)/usr/bin/sdl-config


CC = $(HOST)gcc
CXX = $(HOST)g++
CCFLAGS = `$(SDL_CONFIG) --cflags` -mips32r2 -mno-mips16 -mno-abicalls -fomit-frame-pointer -fno-builtin \
	-fno-common -Wno-write-strings -Wno-sign-compare -ffast-math -ftree-vectorize -funswitch-loops \
	-D_GNU_SOURCE -g -Wall -O2 -fno-strict-aliasing \
	-I. -I../ -I../liboggvorbis/include \
	-I../liboggvorbis/src -DPAL_HAS_PLATFORM_SPECIFIC_UTILS $(GENERATED)
CXXFLAGS = $(CCFLAGS) -std=gnu++11
CFLAGS = $(CCFLAGS) -std=gnu99
LDFLAGS = `$(SDL_CONFIG) --libs` $(MIDI_LDFLAGS) -lstdc++ -lm -pthread -s

POSTCOMPILE = @mv -f $*$(INTER).Td $*$(INTER).d && touch $@

ifeq (${USE_TW},1)
OPK_EXTRA_DATA = external/zh_TW/*
else
OPK_EXTRA_DATA = external/zh_CN/*
endif
ifeq (${USE_VIDEO},1)
OPK_EXTRA_DATA += external/video/*
endif

.PHONY : all clean check

all: $(TARGET) opk

opk: $(TARGET).opk

$(TARGET).opk: $(TARGET)
	echo "  OPK     $@"
	$(CMD)mksquashfs external/common/* $(OPK_EXTRA_DATA) $(TARGET) $@ -all-root -noappend -no-exports -no-xattrs -no-progress >/dev/null

$(TARGET): $(OBJFILES)
	@echo [LD] $@
	@$(CXX) $^ -o $@ $(LDFLAGS)

%$(INTER).o: %.c %$(INTER).d
	@echo [CC] $<
	@$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@
	$(POSTCOMPILE)

%$(INTER).o: %.cpp %$(INTER).d
	@echo [CC] $<
	@$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@
	$(POSTCOMPILE)

clean:
	-@rm -f $(TARGET) $(OBJFILES) $(DEPFILES)

%.d: ;
.PRECIOUS: %.d

-include $(DEPFILES)
