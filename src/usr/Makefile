CC = gcc
TARGET = app

CFLAGS += -std=c11 -Wall -Wextra
LDFLAGS += -pthread

BUILD_DIR = ./bin
SRC_DIRS = ./

LDIR = ../lib
LIB = ums

SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:./%.c=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -L $(LDIR) -l$(LIB) -o $@

# c source
$(BUILD_DIR)/%.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

MKDIR_P = mkdir -p
