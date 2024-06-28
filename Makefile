CC ?= cc
RM := rm -f
MKDIR := mkdir -p

BUILD_DIR := build

BUILD_BINS_DIR := ${BUILD_DIR}/bin
BUILD_OBJS_DIR := ${BUILD_DIR}/objs
BUILD_DEPS_DIR := ${BUILD_OBJS_DIR}
BUILD_LIBS_DIR := ${BUILD_DIR}/lib

TOMLC_DIR := subprojects/toml-c
TOMLC_INCLUDES := -I${TOMLC_DIR}
TOMLC_FLAGS := -L${TOMLC_DIR}
TOMLC_LIB := libtoml.so.1.0

CFLAGS := -std=c99 -fPIE ${CFLAGS}
CPPFLAGS := -Iinclude ${TOMLC_INCLUDES} -Wall -Wextra -Wpedantic -Wshadow -Wdouble-promotion -Wconversion \
						-Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes \
						-D_XOPEN_SOURCE=700 -MMD ${CPPFLAGS}
LDLIBS := -lxcb -lxcb-xkb -lxcb-util -lasound -lm -l:${TOMLC_LIB}
LDFLAGS := ${TOMLC_FLAGS} -Wl,-rpath,'$$ORIGIN'/../lib

SRC_DIR := src
SRC_SRCS := $(shell find ${SRC_DIR} -name *.c)
SRC_OBJS := $(patsubst %.c,${BUILD_OBJS_DIR}/%.o,${SRC_SRCS})
SRC_DEPS := $(patsubst %.c,${BUILD_DEPS_DIR}/%.d,${SRC_SRCS})

EXECUTABLE := ${BUILD_BINS_DIR}/status_line

.PHONY: all
all: debug

.PHONY: debug release sanitize-address sanitize-thread
debug: CFLAGS := -O0 -g ${CFLAGS}
release: CFLAGS := -O2 -DNDEBUG ${CFLAGS}
sanitize-address: CFLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ${CFLAGS}
sanitize-thread: CFLAGS := -O1 -g -fsanitize=thread -fno-omit-frame-pointer ${CFLAGS}
debug release sanitize-address sanitize-thread: ${EXECUTABLE}

${BUILD_LIBS_DIR}/${TOMLC_LIB}: ${TOMLC_DIR}/${TOMLC_LIB}
	@${MKDIR} $(dir $@)
	cp $< $@

${TOMLC_DIR}/${TOMLC_LIB}:
	${MAKE} -C ${TOMLC_DIR} CC=${CC} ${TOMLC_LIB}

.PHONY: clean
clean:
	${RM} ${SRC_OBJS} ${SRC_DEPS} ${EXECUTABLE}
	${MAKE} -C ${TOMLC_DIR} clean

${EXECUTABLE}: ${BUILD_LIBS_DIR}/${TOMLC_LIB} ${SRC_OBJS}
	@${MKDIR} $(dir $@)
	${CC} ${CFLAGS} ${CPPFLAGS} ${LDLIBS} ${LDFLAGS} -o $@ ${SRC_OBJS}

${BUILD_OBJS_DIR}/%.o: %.c
	@${MKDIR} $(dir $@)
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<

sinclude ${SRC_DEPS}
