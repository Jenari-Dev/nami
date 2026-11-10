#   Nami - PS5 homebrew shell
#
# Build:   make                (produces nami.elf)
# Deploy:  make test PS5_HOST=<ps5-ip>   (sends to etaHEN elfldr on :9021)

PS5_HOST ?= ps5
PS5_PORT ?= 9021

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

.PHONY: all clean test
