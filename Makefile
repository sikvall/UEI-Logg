CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -pthread
LDFLAGS = -lncursesw -lcurl -lm -pthread
TARGET = qsolog

all: $(TARGET)

$(TARGET): qsolog.c
	$(CC) $(CFLAGS) -o $(TARGET) qsolog.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
