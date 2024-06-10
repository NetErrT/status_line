MAKEFLAGS += --no-print-directory

CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Iinclude -D_XOPEN_SOURCE=700 $(CFLAGS)
LDLIBS := -lxcb -lxcb-xkb -lxcb-util -lasound -lm
LDFLAGS := -fpie

OUT_DIR := build

EXECUTABLE := $(OUT_DIR)/status_line
SUBMODULES := tomlc99

ifneq (,$(filter tomlc99,$(SUBMODULES)))
LDLIBS += -l:libtoml.so.1.0
LDFLAGS += -Ltomlc99 -Wl,-rpath=./tomlc99
CFLAGS += -Itomlc99
endif

SRCS := $(shell find src -name *.c)
OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(SRCS))
DEPS := $(patsubst %.o,%.d,$(OBJS))

.PHONY: all
all: release

.PHONY: release
release:: CFLAGS := -O2 -DNDEBUG $(CFLAGS)
release:: $(EXECUTABLE)

.PHONY: debug
debug:: CFLAGS := -O0 -g $(CFLAGS)
debug:: $(EXECUTABLE)

.PHONY: clean
clean:
	$(MAKE) -C $(SUBMODULES) clean
	rm -f $(OBJS) $(DEPS) $(EXECUTABLE)

.PHONY: tomlc99
tomlc99:
	$(MAKE) -C tomlc99

$(EXECUTABLE): $(SUBMODULES) $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDLIBS) $(LDFLAGS) $(OBJS) -o $@

$(OUT_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)
