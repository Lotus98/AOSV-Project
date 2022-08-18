CC 		= gcc
CFLAGS       	= -std=c99 -fPIC -Wall -Wextra -Wconversion
LDFLAGS      	= -shared

# Comment to disable/enable debugging
DEBUG = y
ifeq ($(DEBUG), y)
	CFLAGS += -Og -g -D DEBUG
else
	CFLAGS += -O2
endif

TARGET  = libums.so
SOURCES = $(shell echo *.c)
HEADERS = $(shell echo *.h)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: clean tidy
clean:
	rm -rf *.o *.so

tidy:
	rm -rf *.o
