CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -Isrc -pthread
LDFLAGS = -lncursesw -lcurl -lsqlite3 -lm -pthread
TARGET = ueilog
TOOL_TARGET = dxcc
TEST_TARGET = testing
MANUAL_SRC = manual/manual.tex
MANUAL_PDF = manual/manual.pdf

SRCS = src/main.c src/db.c src/tui.c src/logg.c src/settings.c src/callbook.c src/dxcc.c src/qrb.c src/util.c src/space_weather.c src/qrz.c src/file_dialog.c src/help.c
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard src/*.h)

TEST_SRCS = tests/test_main.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_DEPS = src/db.o src/tui.o src/logg.o src/settings.o src/callbook.o src/dxcc.o src/qrb.o src/util.o src/space_weather.o src/qrz.o src/file_dialog.o src/help.o

# Standardmål: Bygg huvudprogrammet, hjälpbinären, testbinären och manualen
all: $(TARGET) $(TOOL_TARGET) $(TEST_TARGET) manual

# Huvudprogrammet UEI-Logg
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Hjälpprogrammet dxcc (uppdatering och statistik för dxcc.txt)
$(TOOL_TARGET): src/dxcc_tool.o src/util.o
	$(CC) $(CFLAGS) -o $(TOOL_TARGET) src/dxcc_tool.o src/util.o -lcurl -lm

# Testbinären testing (automatiserad testsvit)
$(TEST_TARGET): $(TEST_OBJS) $(TEST_DEPS)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_OBJS) $(TEST_DEPS) $(LDFLAGS)

# Kör testerna
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Användarmanual i PDF-format
manual: $(MANUAL_PDF)

$(MANUAL_PDF): $(MANUAL_SRC)
	pdflatex -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)
	pdflatex -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)

src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(TOOL_TARGET) $(TEST_TARGET) src/*.o tests/*.o *.o
	rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf

.PHONY: all clean manual dxcc test
