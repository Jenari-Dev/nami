#   Nami - PS5 homebrew shell
#
# Build:   make                (produces nami.elf)
# Deploy:  make test PS5_HOST=<ps5-ip>   (sends to etaHEN elfldr on :9021)

PS5_HOST ?= ps5
PS5_PORT ?= 9021
# Elf Arsenal (etaHEN) deploy: it has no network ELF loader on 9021; it runs
# ELFs from its own filesystem registry. We upload over FTP and launch from
# the on-console Elf Arsenal menu.
PS5_FTP_PORT  ?= 2121
ARSENAL_DIR   ?= /data/elf-arsenal/payloadmgr

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined - source the ps5-payload-sdk env)
endif

ELF  := nami.elf
SRCS := src/main.c src/http_server.c

CFLAGS := -Wall -g -Isrc -lSceNotification

all: $(ELF)

$(ELF): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(ELF)

test: $(ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $(ELF)

# Preferred on this setup: upload to Elf Arsenal, then launch on the console.
deploy: $(ELF)
	curl -s --max-time 30 -T $(ELF) ftp://$(PS5_HOST):$(PS5_FTP_PORT)$(ARSENAL_DIR)/$(ELF)
	@echo "Uploaded $(ELF) to Elf Arsenal. Launch it from the arsenal menu on the PS5."

.PHONY: all clean test deploy
