CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -pthread
LDFLAGS = -lncursesw -lcurl -lm -pthread
TARGET = qsolog

SRCS = main.c tui.c logg.c settings.c callbook.c dxcc.c qrb.c util.c space_weather.c
OBJS = $(SRCS:.c=.o)
HEADERS = qsolog.h tui.h logg.h settings.h callbook.h dxcc.h qrb.h util.h space_weather.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
