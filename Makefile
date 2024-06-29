## Global variables
CC ?= cc
RM := rm -f
MKDIR := mkdir -p

# Build directories
BUILD_DIR := build
BUILD_BINS_DIR := ${BUILD_DIR}/bin
BUILD_OBJS_DIR := ${BUILD_DIR}/objs
BUILD_DEPS_DIR := ${BUILD_OBJS_DIR}

# C, C++, ld flags and libs
CFLAGS := -std=c99 -fPIE ${CFLAGS}
CPPFLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wdouble-promotion -Wconversion -Wsign-conversion ${CPPFLAGS} \
						-D_XOPEN_SOURCE=700 -Iinclude
LDLIBS := -lxcb -lxcb-xkb -lxcb-util -lasound -lm
LDFLAGS :=

## Main
EXECUTABLE := ${BUILD_BINS_DIR}/status_line

# TOMLC
TOMLC_DIR := subprojects/toml-c
TOMLC_STATIC_LIB := ${TOMLC_DIR}/libtoml.a

CPPFLAGS += -I${TOMLC_DIR}

${TOMLC_STATIC_LIB}:
	@${MAKE} -C ${TOMLC_DIR} CC=${CC}

# Object and dependency files
${BUILD_OBJS_DIR}/%.o: %.c
	@${MKDIR} $(dir $@)
	${CC} ${CFLAGS} ${CPPFLAGS} -MMD -MF $(patsubst ${BUILD_OBJS_DIR}/%.o, ${BUILD_DEPS_DIR}/%.d, $@) -o $@ -c $<

# Executable
SRC_DIR := src
SRC_SRCS := $(shell find ${SRC_DIR} -name *.c)
SRC_OBJS := $(patsubst %.c, ${BUILD_OBJS_DIR}/%.o, ${SRC_SRCS})
SRC_DEPS := $(patsubst %.c, ${BUILD_DEPS_DIR}/%.d, ${SRC_SRCS})

-include ${SRC_DEPS}

${EXECUTABLE}: ${TOMLC_STATIC_LIB} ${SRC_OBJS}
	@${MKDIR} $(dir $@)
	${CC} ${CFLAGS} ${CPPFLAGS} ${LDLIBS} ${LDFLAGS} -o $@ ${SRC_OBJS} ${TOMLC_STATIC_LIB}

# Build types
.PHONY: all
all: debug

.PHONY: debug release sanitize-address sanitize-thread
debug: CFLAGS := -O0 -g ${CFLAGS}
release: CFLAGS := -O2 -DNDEBUG ${CFLAGS}
sanitize-address: CFLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ${CFLAGS}
sanitize-thread: CFLAGS := -O1 -g -fsanitize=thread -fno-omit-frame-pointer ${CFLAGS}
debug release sanitize-address sanitize-thread: ${EXECUTABLE}

# Cleanup
.PHONY: clean
clean:
	${RM} ${SRC_OBJS} ${SRC_DEPS} ${EXECUTABLE}
	@${MAKE} -C ${TOMLC_DIR} clean
