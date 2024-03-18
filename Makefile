COMMIT_HASH=$(shell git rev-parse --short=8 HEAD 2>/dev/null)
BUILD_TIME=$(shell date +%FT%T%z)

COMMIT=$(shell git rev-parse HEAD 2>/dev/null)

VERSION_TAG = $(shell git describe --tags --exact-match 2>/dev/null | cut -f 1 -d '-' 2>/dev/null)

# If no git tag is set, fallback to 'DEVELOPMENT'
ifeq ($(strip ${VERSION_TAG}),)
VERSION_TAG := Development
endif

CC ?= cc
CFLAGS ?= -O -Wall -Wextra -Wformat-security -D_FORTIFY_SOURCE=2 -fstack-protector-strong
CFLAGS += -std=c99 -D_POSIX_C_SOURCE=200809L -fPIC 
LDFLAGS += -shared

CFLAGS += -DBUILD_TIME=$(BUILD_TIME) -DVERSION=$(VERSION_TAG) -DCOMMIT_HASH=$(COMMIT_HASH) -I. -I/usr/include/openvpn

# Output Files
SRC 	= $(wildcard *.c)
OUT	= $(SRC:%.c=%.so)

%.so: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	tar -czvf ovpn-plugin-auth-script.linux-amd64.tar.gz ovpn-plugin-auth-script.so

all: plugin

plugin: $(OUT)

clean:
	rm -f *.so
