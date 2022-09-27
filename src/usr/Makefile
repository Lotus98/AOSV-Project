CC = gcc

# Comment to disable/enable debugging
DEBUG = y
ifeq ($(DEBUG), y)
	CFLAGS += -Og -g
else
	CFLAGS += -O2
endif

CFLAGS += -std=c11 -Wall -Wextra
LDFLAGS += -pthread

BUILD_DIR = ./bin
SRC_DIRS = ./

LDIR = ../lib
LIB = ums

SRCS := $(shell find $(SRC_DIRS) -name "*.c")
OBJS := $(SRCS:./%.c=$(BUILD_DIR)/%.o)
TARGETS := $(OBJS:./%.o=%)

all: $(TARGETS)

%: %.o
	$(CC) $(LDFLAGS) $< -L $(LDIR) -l$(LIB) -o $@

# c source
$(BUILD_DIR)/%.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

MKDIR_P = mkdir -p
