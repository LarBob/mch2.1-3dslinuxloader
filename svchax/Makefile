TARGET := svchax_test

DEBUG                   = 0
BUILD_3DSX              = 1
BUILD_3DS               = 1
BUILD_CIA               = 1
LIBCTRU_NO_DEPRECATION  = 1


OBJS :=
OBJS += svchax.o
OBJS += test.o


APP_TITLE            = svchax
APP_DESCRIPTION      = svchax
APP_AUTHOR           = aliaspider
APP_PRODUCT_CODE     = SVCHAX
APP_UNIQUE_ID        = 0xBAD0F
APP_ICON             = ctr/assets/icon.png
APP_BANNER           = ctr/assets/banner.png
APP_AUDIO            = ctr/assets/audio.wav
APP_RSF              = ctr/tools/template.rsf
APP_SYSTEM_MODE      = 64MB
APP_SYSTEM_MODE_EXT  = 124MB
APP_BIG_TEXT_SECTION = 0

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

ifeq ($(strip $(CTRULIB)),)
$(error "Please set CTRULIB in your environment. export CTRULIB=<path to>ctrulib")
endif

#ifeq ($(strip $(AEMSTRO)),)
#$(error "Please set AEMSTRO in your environment. export AEMSTRO=<path to>aemstro")
#endif


APP_TITLE         := $(shell echo "$(APP_TITLE)" | cut -c1-128)
APP_DESCRIPTION   := $(shell echo "$(APP_DESCRIPTION)" | cut -c1-256)
APP_AUTHOR        := $(shell echo "$(APP_AUTHOR)" | cut -c1-128)
APP_PRODUCT_CODE  := $(shell echo $(APP_PRODUCT_CODE) | cut -c1-16)
APP_UNIQUE_ID     := $(shell echo $(APP_UNIQUE_ID) | cut -c1-7)

MAKEROM_ARGS_COMMON = -rsf $(APP_RSF) -exefslogo -elf $(TARGET).elf -icon $(TARGET).icn -banner $(TARGET).bnr -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE="$(APP_PRODUCT_CODE)" -DAPP_UNIQUE_ID=$(APP_UNIQUE_ID) -DAPP_SYSTEM_MODE=$(APP_SYSTEM_MODE) -DAPP_SYSTEM_MODE_EXT=$(APP_SYSTEM_MODE_EXT)

INCDIRS := -I$(CTRULIB)/include
LIBDIRS := -L. -L$(CTRULIB)/lib


ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -marm -mfpu=vfp -mtp=soft

CFLAGS	+=	-mword-relocations \
			-fomit-frame-pointer -ffast-math \
         -Werror=implicit-function-declaration \
			$(ARCH)

CFLAGS	+= -Wall
CFLAGS	+=	-DARM11 -D_3DS

ifeq ($(DEBUG), 1)
   CFLAGS	+= -O0 -g
else
   CFLAGS	+= -O3
endif

ifeq ($(LIBCTRU_NO_DEPRECATION), 1)
   CFLAGS	+= -DLIBCTRU_NO_DEPRECATION
endif

CFLAGS += -I.

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS   =	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

CFLAGS   += -std=gnu99 -ffast-math


LIBS	:= -lctru -lm


ifeq ($(BUILD_3DSX), 1)
TARGET_3DSX := $(TARGET).3dsx $(TARGET).smdh
endif

ifeq ($(BUILD_3DS), 1)
TARGET_3DS := $(TARGET).3ds
endif

ifeq ($(BUILD_CIA), 1)
TARGET_CIA := $(TARGET).cia
endif

.PHONY: $(BUILD) clean all

all: $(TARGET)

$(TARGET): $(TARGET_3DSX) $(TARGET_3DS) $(TARGET_CIA)
$(TARGET).3dsx: $(TARGET).elf
$(TARGET).elf: $(OBJS)

PREFIX		:=	$(DEVKITARM)/bin/arm-none-eabi-

CC      := $(PREFIX)gcc
CXX     := $(PREFIX)g++
AS      := $(PREFIX)as
AR      := $(PREFIX)ar
OBJCOPY := $(PREFIX)objcopy
STRIP   := $(PREFIX)strip
NM      := $(PREFIX)nm
LD      := $(CXX)

ifneq ($(findstring Linux,$(shell uname -a)),)
	MAKEROM    = ctr/tools/makerom-linux
	BANNERTOOL = ctr/tools/bannertool-linux
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	MAKEROM    = ctr/tools/makerom-mac
	BANNERTOOL = ctr/tools/bannertool-mac
else
	MAKEROM    = ctr/tools/makerom.exe
	BANNERTOOL = ctr/tools/bannertool.exe
endif

%.o: %.shader
	python $(AEMSTRO)/aemstro_as.py $< $(notdir $<).shbin
	$(DEVKITARM)/bin/bin2s $(notdir $<).shbin | $(PREFIX)as -o $@
	echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(notdir $<).shbin | tr . _)`.h
	echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(notdir $<).shbin | tr . _)`.h
	echo "extern const u32" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(notdir $<).shbin | tr . _)`.h
	rm $(notdir $<).shbin


%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCDIRS)

%.o: %.s
	$(CC) -c -o $@ $< $(ASFLAGS)

%.o: %.S
	$(CC) -c -o $@ $< $(ASFLAGS)

%.a:
	$(AR) -rc $@ $^


$(TARGET).smdh: $(APP_ICON)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@

$(TARGET).3dsx: $(TARGET).elf
	-3dsxtool $< $@ $(_3DSXFLAGS)

$(TARGET).elf:
	$(LD) $(LDFLAGS) $(OBJS) $(LIBDIRS) $(LIBS) -o $@
	$(NM) -CSn $@ > $(notdir $*.lst)

$(TARGET).bnr: $(APP_BANNER) $(APP_AUDIO)
	$(BANNERTOOL) makebanner -i "$(APP_BANNER)" -a "$(APP_AUDIO)" -o $@

$(TARGET).icn: $(APP_ICON)
	$(BANNERTOOL) makesmdh -s "$(APP_TITLE)" -l "$(APP_TITLE)" -p "$(APP_AUTHOR)" -i $(APP_ICON) -o $@

$(TARGET).3ds: $(TARGET).elf $(TARGET).bnr $(TARGET).icn $(APP_RSF)
	$(MAKEROM) -f cci -o $@ $(MAKEROM_ARGS_COMMON) -DAPP_ENCRYPTED=true

$(TARGET).cia: $(TARGET).elf $(TARGET).bnr $(TARGET).icn $(APP_RSF)
	$(MAKEROM) -f cia -o $@ $(MAKEROM_ARGS_COMMON) -DAPP_ENCRYPTED=false


clean:
	rm -f $(OBJS)
	rm -f $(TARGET).3dsx
	rm -f $(TARGET).elf
	rm -f $(TARGET).3ds
	rm -f $(TARGET).cia
	rm -f $(TARGET).smdh
	rm -f $(TARGET).bnr
	rm -f $(TARGET).icn
	rm -f *_shader_shbin.h

.PHONY: clean $(TARGET)

