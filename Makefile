CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -Isrc -pthread
LDFLAGS = -lncursesw -lcurl -lsqlite3 -lm -pthread -ldl
TOOL_LDFLAGS = -lcurl -lm -pthread -ldl
TARGET = ueilogg
TOOL_TARGET = dxcc
TEST_TARGET = testing
MANUAL_SRC = manual/manual.tex
MANUAL_PDF = manual/manual.pdf

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR = $(DESTDIR)$(PREFIX)/bin
DATADIR = $(DESTDIR)$(PREFIX)/share/ueilogg
DOCDIR = $(DESTDIR)$(PREFIX)/share/doc/ueilogg
APPDIR = $(DESTDIR)$(PREFIX)/share/applications

APP_VERSION := $(shell sed -n 's/^#define[[:space:]]\+APP_VERSION[[:space:]]\+"\([^"]*\)"/\1/p' src/version.h)
DEB_ARCH := $(shell dpkg --print-architecture 2>/dev/null || uname -m)
DEB_STAGE = build/deb
DEB_PKG = dist/ueilogg_$(APP_VERSION)_$(DEB_ARCH).deb

SRCS = src/main.c src/db.c src/tui.c src/logg.c src/settings.c src/callbook.c src/dxcc.c src/qrb.c src/util.c src/space_weather.c src/qrz.c src/file_dialog.c src/help.c src/theme_editor.c
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard src/*.h)

TEST_SRCS = tests/test_main.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_DEPS = src/db.o src/tui.o src/logg.o src/settings.o src/callbook.o src/dxcc.o src/qrb.o src/util.o src/space_weather.o src/qrz.o src/file_dialog.o src/help.o src/theme_editor.o

# Standardmål: Bygg huvudprogrammet, hjälpbinären, testbinären och manualen
all: $(TARGET) $(TOOL_TARGET) $(TEST_TARGET) manual

# Huvudprogrammet UEI-Logg
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Hjälpprogrammet dxcc (uppdatering och statistik för dxcc.txt)
$(TOOL_TARGET): src/dxcc_tool.o src/util.o
	$(CC) $(CFLAGS) -o $(TOOL_TARGET) src/dxcc_tool.o src/util.o $(TOOL_LDFLAGS)

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

install: $(TARGET) $(TOOL_TARGET) $(MANUAL_PDF)
	install -d "$(BINDIR)" "$(DATADIR)" "$(DOCDIR)" "$(APPDIR)"
	install -m 755 $(TARGET) "$(BINDIR)/$(TARGET)"
	strip "$(BINDIR)/$(TARGET)"
	install -m 755 $(TOOL_TARGET) "$(BINDIR)/$(TOOL_TARGET)"
	strip "$(BINDIR)/$(TOOL_TARGET)"
	install -m 644 dxcc.txt "$(DATADIR)/dxcc.txt"
	install -m 644 packaging/ueilogg.desktop "$(APPDIR)/ueilogg.desktop"
	install -m 644 README.md "$(DOCDIR)/README.md"
	install -m 644 $(MANUAL_PDF) "$(DOCDIR)/manual.pdf"
	install -m 644 LICENSE "$(DOCDIR)/copyright"

uninstall:
	rm -f "$(BINDIR)/$(TARGET)" "$(BINDIR)/$(TOOL_TARGET)"
	rm -f "$(APPDIR)/ueilogg.desktop"
	rm -rf "$(DATADIR)" "$(DOCDIR)"

deb: $(TARGET) $(TOOL_TARGET) $(MANUAL_PDF)
	@if [ -z "$(APP_VERSION)" ]; then echo "Fel: Kunde inte läsa APP_VERSION från src/version.h"; exit 1; fi
	rm -rf $(DEB_STAGE)
	mkdir -p $(DEB_STAGE)/DEBIAN dist
	$(MAKE) install DESTDIR=$(DEB_STAGE) PREFIX=/usr
	INSTALLED_SIZE=$$(du -sk $(DEB_STAGE)/usr | cut -f1); \
	sed -e "s/@VERSION@/$(APP_VERSION)/g" \
	    -e "s/@ARCH@/$(DEB_ARCH)/g" \
	    -e "s/@INSTALLED_SIZE@/$$INSTALLED_SIZE/g" \
	    packaging/deb/control.in > $(DEB_STAGE)/DEBIAN/control
	chmod 644 $(DEB_STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(DEB_STAGE) $(DEB_PKG)
	@echo "Skapade paket: $(DEB_PKG)"

clean:
	rm -f $(TARGET) $(TOOL_TARGET) $(TEST_TARGET) src/*.o tests/*.o *.o
	rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf
	rm -rf build dist

.PHONY: all clean manual test install uninstall deb
