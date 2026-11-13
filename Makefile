CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -Isrc -pthread
LDFLAGS = -lncursesw -lcurl -lm -pthread
TARGET = ueilog
TOOL_TARGET = dxcc
MANUAL_SRC = manual/manual.tex
MANUAL_PDF = manual/manual.pdf

SRCS = src/main.c src/tui.c src/logg.c src/settings.c src/callbook.c src/dxcc.c src/qrb.c src/util.c src/space_weather.c src/qrz.c src/file_dialog.c src/help.c
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard src/*.h)

# Standardmål: Bygg huvudprogrammet, hjälpbinären och manualen
all: $(TARGET) $(TOOL_TARGET) manual

# Huvudprogrammet UEI-Logg
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Hjälpprogrammet dxcc (uppdatering och statistik för dxcc.txt)
$(TOOL_TARGET): src/dxcc_tool.o src/util.o
	$(CC) $(CFLAGS) -o $(TOOL_TARGET) src/dxcc_tool.o src/util.o -lcurl -lm

# Användarmanual i PDF-format
manual: $(MANUAL_PDF)

$(MANUAL_PDF): $(MANUAL_SRC)
	pdflatex -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)
	pdflatex -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)

src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(TOOL_TARGET) src/*.o *.o
	rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf

.PHONY: all clean manual dxcc
