#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

GAME_TITLE     := LttP DS
GAME_SUBTITLE1 := A Link to the Past
GAME_SUBTITLE2 := DS Phat / Lite

ifeq ($(origin LTTP_ROOT),undefined)
  LTTP_ROOT := $(CURDIR)
endif
export LTTP_ROOT

include $(DEVKITARM)/ds_rules

ARM7_ELF := $(LTTP_ROOT)/platform/nds/arm7/zelda_arm7.elf
_ARM7_ELF := -7 $(ARM7_ELF)

#---------------------------------------------------------------------------------
TARGET   := lttp-ds
BUILD    := build
SOURCES  := platform/nds/source
INCLUDES := platform/nds/include vendor/zelda3 vendor/zelda3/src vendor/zelda3/snes
DATA     :=
GRAPHICS := platform/nds/bottom-screen/images
AUDIO    :=
ICON     :=
NITRO    := platform/nds/nitrofiles
ZELDA3   := vendor/zelda3

ZELDA_SRC := $(wildcard $(ZELDA3)/src/*.c) $(wildcard $(ZELDA3)/snes/*.c)
ZELDA_SRC := $(filter-out \
	$(ZELDA3)/src/main.c \
	$(ZELDA3)/src/opengl.c \
	$(ZELDA3)/src/glsl_shader.c \
	$(ZELDA3)/src/zelda_cpu_infra.c \
	$(ZELDA3)/snes/tracing.c,\
	$(ZELDA_SRC))

#---------------------------------------------------------------------------------
ARCH := -march=armv5te -mtune=arm946e-s -marm

CFLAGS   := -g -Wall -O2 -ffunction-sections -fdata-sections\
            -fsigned-char -fno-strict-aliasing -mno-unaligned-access\
            -Wno-deprecated-declarations\
            -include $(LTTP_ROOT)/platform/nds/include/nds_force_types.h\
            $(ARCH) $(INCLUDE) -DARM9 -DLTTP_PPU_RGB15
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) \
            -Wl,--wrap=FindIndexInMemblk,--wrap=ppu_runLine,--wrap=ZeldaReadSram,--wrap=ZeldaWriteSram

LIBS := -lfilesystem -lfat -lnds9 -lm

LIBDIRS := $(LIBNDS) $(PORTLIBS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT := $(CURDIR)/$(TARGET)

export VPATH := $(CURDIR)/$(subst /,,$(dir $(ICON)))\
                $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))\
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))\
                $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))\
                $(CURDIR)/$(ZELDA3)/src \
                $(CURDIR)/$(ZELDA3)/snes

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
            $(notdir $(ZELDA_SRC))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PNGFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifneq ($(strip $(NITRO)),)
  export NITRO_FILES := $(CURDIR)/$(NITRO)
endif

ifeq ($(strip $(CPPFILES)),)
  export LD := $(CC)
else
  export LD := $(CXX)
endif

export OFILES_BIN   :=	$(addsuffix .o,$(BINFILES))
# ppu.itcm.o matches the DS linker's *.itcm.* rule, so the software
# renderer executes from instruction RAM instead of main memory.
export OFILES_SOURCES := $(filter-out ppu.o,$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)) ppu.itcm.o
export OFILES := $(PNGFILES:.png=.o) $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(PNGFILES:.png=.h) $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir))\
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include)\
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
  icons := $(wildcard *.bmp)
  ifneq (,$(findstring $(TARGET).bmp,$(icons)))
    export GAME_ICON := $(CURDIR)/$(TARGET).bmp
  else
    ifneq (,$(findstring icon.bmp,$(icons)))
      export GAME_ICON := $(CURDIR)/icon.bmp
    endif
  endif
else
  ifeq ($(suffix $(ICON)), .grf)
    export GAME_ICON := $(CURDIR)/$(ICON)
  else
    export GAME_ICON := $(CURDIR)/$(BUILD)/$(notdir $(basename $(ICON))).grf
  endif
endif

.PHONY: $(BUILD) clean

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@$(RM) -r $(BUILD) $(TARGET).elf $(TARGET).nds
	@$(MAKE) -C platform/nds/arm7 clean

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

$(OUTPUT).nds: $(OUTPUT).elf $(NITRO_FILES) $(GAME_ICON) $(ARM7_ELF)
$(OUTPUT).elf: $(OFILES)

$(ARM7_ELF):
	$(MAKE) -C $(LTTP_ROOT)/platform/nds/arm7

$(OFILES_SOURCES) : $(HFILES)

ppu.itcm.o: ppu.c
	$(SILENTMSG) $(notdir $<)
	$(ADD_COMPILE_COMMAND) add $(CC) "$(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/ppu.itcm.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

%.s %.h: %.png %.grit
	$(DEVKITPRO)/tools/bin/grit $< -fts -o$*

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
