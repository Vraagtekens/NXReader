#---------------------------------------------------------------------------------
# Minimal Nintendo Switch homebrew Makefile for devkitPro/libnx.
#---------------------------------------------------------------------------------

TARGET      := NXReader
BUILD       := build
DIST        := dist
SOURCES     := source
DATA        := data
INCLUDES    := include
ROMFS       := romfs

DEVKITPRO   ?= /opt/devkitpro
DEVKITA64   ?= $(DEVKITPRO)/devkitA64
LIBNX       ?= $(DEVKITPRO)/libnx
PORTLIBS    ?= $(DEVKITPRO)/portlibs/switch

export DEVKITPRO
export DEVKITA64
export LIBNX
export PORTLIBS
export COPYFILE_DISABLE := 1

APP_TITLE   := NXReader
APP_AUTHOR  := Vraagtekens
APP_VERSION := 0.1.0
SWITCH_IP   ?= 
UPLOAD_PATH ?= /switch/$(TARGET).nro

ifeq ($(filter clean clean-macos,$(MAKECMDGOALS)),)
  ifeq ($(wildcard $(DEVKITA64)/bin/aarch64-none-elf-g++),)
    $(error devkitPro Switch toolchain not found. Install switch-dev and set DEVKITPRO/DEVKITA64)
  endif
  ifeq ($(wildcard $(PORTLIBS)/include/zlib.h),)
    $(error switch-zlib not found. Run: sudo dkp-pacman -S switch-zlib)
  endif
  ifeq ($(wildcard $(PORTLIBS)/include/SDL2/SDL.h),)
    $(error switch-sdl2 not found. Run: sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf)
  endif
  ifeq ($(wildcard $(PORTLIBS)/include/SDL2/SDL_ttf.h),)
    $(error switch-sdl2_ttf not found. Run: sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf)
  endif
  ifeq ($(wildcard $(PORTLIBS)/include/SDL2/SDL_image.h),)
    $(error switch-sdl2_image not found. Run: sudo dkp-pacman -S switch-sdl2_image)
  endif
  ifeq ($(wildcard $(PORTLIBS)/include/curl/curl.h),)
    $(error switch-curl not found. Run: sudo dkp-pacman -S switch-curl)
  endif
endif

PREFIX      := $(DEVKITA64)/bin/aarch64-none-elf-
CC          := $(PREFIX)gcc
CXX         := $(PREFIX)g++
AS          := $(PREFIX)as
NACPTOOL    := $(DEVKITPRO)/tools/bin/nacptool
ELF2NRO     := $(DEVKITPRO)/tools/bin/elf2nro
NXLINK      := $(DEVKITPRO)/tools/bin/nxlink
PKG_CONFIG  := $(PORTLIBS)/bin/aarch64-none-elf-pkg-config
CURL_CONFIG := $(PORTLIBS)/bin/curl-config
SDL_CFLAGS  := $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf SDL2_image 2>/dev/null)
SDL_LIBS    := $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf SDL2_image 2>/dev/null)
CURL_CFLAGS := $(shell $(CURL_CONFIG) --cflags 2>/dev/null)
CURL_LIBS   := $(shell $(CURL_CONFIG) --libs 2>/dev/null)

ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS      := -g -Wall -O2 -ffunction-sections $(SDL_CFLAGS) $(CURL_CFLAGS)
CFLAGS      += $(ARCH) $(DEFINES)
CFLAGS      += -D__SWITCH__

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++20

ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        := $(SDL_LIBS) $(CURL_LIBS)

LIBDIRS     := $(PORTLIBS) $(LIBNX)

ifneq ($(IN_BUILD),1)
#---------------------------------------------------------------------------------
export OUTPUT := ../$(DIST)/$(TARGET)
export TOPDIR := ..

export VPATH := $(foreach dir,$(SOURCES),../$(dir)) \
                $(foreach dir,$(DATA),../$(dir))

export DEPSDIR := .

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)

export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I../$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I.

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean clean-macos run $(BUILD)

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ -d $(DIST) ] || mkdir -p $(DIST)
	@$(MAKE) --no-print-directory -C $(BUILD) -f ../Makefile IN_BUILD=1

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(DIST) $(TARGET).elf $(TARGET).nro $(TARGET).nacp

clean-macos:
	@echo cleaning macOS AppleDouble files ...
	@find . -name .git -prune -o -name '._*' -type f -exec rm -f {} +

run: all
	@if [ -z "$(SWITCH_IP)" ]; then \
		echo "Usage: make run SWITCH_IP=192.168.1.225"; \
		exit 1; \
	fi
	$(NXLINK) -a $(SWITCH_IP) -p $(UPLOAD_PATH) $(DIST)/$(TARGET).nro

else
#---------------------------------------------------------------------------------

DEPENDS := $(OFILES:.o=.d)

.PHONY: all

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

%.elf:
	@echo linking $(notdir $@)
	@$(CXX) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

%.nro: %.elf
	@echo creating $(notdir $@)
	@$(NACPTOOL) --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" "$(OUTPUT).nacp"
	@$(ELF2NRO) $< $@ --nacp="$(OUTPUT).nacp" --romfsdir="$(TOPDIR)/$(ROMFS)"

%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.s
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(ASFLAGS) -c $< -o $@

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
