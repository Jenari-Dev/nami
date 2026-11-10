#   Nami - PS5 homebrew shell
#
# Build:   make                     (nami_bootstrap.elf + nami_ui.elf)
# Deploy:  make deploy PS5_HOST=<ip> (FTP upload + Elf Arsenal register)
#
# Nami needs the pacbrew SDK (ships SDL2 et al). Override PS5_PAYLOAD_SDK to
# point elsewhere.

# Force the pacbrew SDK even if the environment already points PS5_PAYLOAD_SDK
# at the base SDK (which has no SDL2). Override with NAMI_SDK=... on the CLI.
NAMI_SDK ?= /home/jenari/dev/emu/ps5-tools/ps5-payload-sdk-pacbrew
override PS5_PAYLOAD_SDK := $(NAMI_SDK)

PS5_HOST      ?= ps5
PS5_FTP_PORT  ?= 2121
ARSENAL_DIR   ?= /data/elf-arsenal/payloadmgr
NAMI_DIR      ?= /data/nami

ifeq ($(wildcard $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk),)
    $(error pacbrew SDK not found at $(PS5_PAYLOAD_SDK))
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

HB := $(PS5_PAYLOAD_SDK)/target/user/homebrew

BOOT_ELF  := nami_bootstrap.elf
UI_ELF    := nami_ui.elf
PROBE_ELF := nami_probe.elf

GPL_SRCS  := src/gpl/pt.c src/gpl/elfldr.c src/gpl/hbldr.c src/gpl/fs_readfile.c
BOOT_SRCS := src/bootstrap.c $(GPL_SRCS)
UI_SRCS   := src/nami_ui.c
PROBE_SRCS:= src/main.c src/http_server.c

COMMON_CFLAGS := -g -O1 -Wall -Wextra -Wno-unused-parameter -D__PS5__ -Isrc

BOOT_CFLAGS := $(COMMON_CFLAGS) -DLAUNCHER_UI_PATH=\"$(NAMI_DIR)/$(UI_ELF)\"
BOOT_LDADD  := -L$(HB)/lib -lSceSystemService -lSceUserService -lkernel_sys -lm -lz -lbz2 -liconv

UI_CFLAGS := $(COMMON_CFLAGS) -I$(HB)/include -I$(HB)/include/SDL2
UI_LDADD  := \
	$(HB)/lib/libSDL2main.a $(HB)/lib/libSDL2.a $(HB)/lib/libSDL2_test.a \
	$(HB)/lib/libSDL2_image.a \
	$(HB)/lib/libpng.a $(HB)/lib/libjpeg.a \
	$(HB)/lib/libwebp.a $(HB)/lib/libwebpdemux.a $(HB)/lib/libsharpyuv.a \
	$(HB)/lib/libsqlite3.a \
	-Wl,--start-group $(HB)/lib/libSDL2_ttf.a $(HB)/lib/libfreetype.a $(HB)/lib/libharfbuzz.a -Wl,--end-group \
	-lSceSystemService -lSceUserService \
	-lSceVideoOut -lSceAudioOut -lScePad -lSceKeyboard -lSceImeDialog \
	-lkernel_sys -lm -lz -lbz2 -liconv

all: $(BOOT_ELF) $(UI_ELF)

$(BOOT_ELF): $(BOOT_SRCS)
	$(CC) $(BOOT_CFLAGS) -o $@ $(BOOT_SRCS) $(BOOT_LDADD)

$(UI_ELF): $(UI_SRCS)
	$(CC) $(UI_CFLAGS) -o $@ $(UI_SRCS) $(UI_LDADD)

# M0 pipeline probe (toast + web status page).
$(PROBE_ELF): $(PROBE_SRCS)
	$(CC) $(COMMON_CFLAGS) -lSceNotification -o $@ $(PROBE_SRCS)
probe: $(PROBE_ELF)

clean:
	rm -f $(BOOT_ELF) $(UI_ELF) $(PROBE_ELF)

# Upload both ELFs into /data/nami and put the bootstrap in Elf Arsenal.
deploy: all
	curl -s --max-time 30 --ftp-create-dirs -T assets/nami.ttf ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(NAMI_DIR)/nami.ttf
	curl -s --max-time 30 --ftp-create-dirs -T $(UI_ELF)   ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(NAMI_DIR)/$(UI_ELF)
	curl -s --max-time 30 --ftp-create-dirs -T $(BOOT_ELF) ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(NAMI_DIR)/$(BOOT_ELF)
	curl -s --max-time 30 -T $(BOOT_ELF) ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(ARSENAL_DIR)/$(BOOT_ELF)
	@echo "Uploaded. Launch '$(BOOT_ELF)' from the Elf Arsenal menu (it loads the UI)."

.PHONY: all clean probe deploy
