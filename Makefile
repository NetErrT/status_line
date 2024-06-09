CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Iinclude -D_XOPEN_SOURCE=700 $(CFLAGS)
LDLIBS := $(shell pkg-config --libs xcb xcb-xkb xcb-aux alsa) -lm

OUT_DIR := build

PROGRAM := $(OUT_DIR)/status_line

SRCS := $(shell find src -name *.c)
OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(SRCS))
DEPS := $(patsubst %.o,%.d,$(OBJS))

.PHONY: all
all: release

.PHONY: release
release:: CFLAGS := -O2 -DNDEBUG $(CFLAGS)
release:: $(PROGRAM)

.PHONY: debug
debug:: CFLAGS := -O0 -g -DVERBOSE $(CFLAGS)
debug:: $(PROGRAM)

.PHONY: sanitize
sanitize:: CFLAGS := -fsanitize-trap=all -fsanitize="address,undefined,null" -fsanitize-recover=all -ftrapv -O0 $(CFLAGS)
sanitize:: $(PROGRAM)

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(PROGRAM)

$(PROGRAM): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDLIBS) $(OBJS) -o $@

$(OUT_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)
