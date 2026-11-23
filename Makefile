CC = gcc
STRIP = strip
PDFLATEX = pdflatex
CFLAGS = -Wall -Wextra -std=c11 -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE_EXTENDED -Isrc -pthread
LDFLAGS = -lncursesw -lcurl -lsqlite3 -lm -pthread -ldl
TOOL_LDFLAGS = -lcurl -lm -pthread -ldl

BIN_DIR = bin
TARGET = $(BIN_DIR)/ueilogg
TOOL_TARGET = $(BIN_DIR)/dxcc
TEST_TARGET = $(BIN_DIR)/testing
MANUAL_SRC = manual/manual.tex
MANUAL_PDF = manual/manual.pdf
BUILD_MANUAL = 1

PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
DESTDIR ?=
BINDIR = $(DESTDIR)$(EXEC_PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
DATADIR = $(DESTDIR)$(DATAROOTDIR)/ueilogg
DOCDIR = $(DESTDIR)$(DATAROOTDIR)/doc/ueilogg
MANDIR = $(DESTDIR)$(DATAROOTDIR)/man
APPDIR = $(DESTDIR)$(DATAROOTDIR)/applications

# Inkludera genererad GNU configure-konfiguration om ./configure har körts
-include config.mk

ifeq ($(BUILD_MANUAL),1)
ALL_MANUAL = manual
INSTALL_MANUAL_DEP = $(MANUAL_PDF)
else
ALL_MANUAL =
INSTALL_MANUAL_DEP =
endif

APP_VERSION := $(shell sed -n 's/^#define[[:space:]]\+APP_VERSION[[:space:]]\+"\([^"]*\)"/\1/p' src/version.h)
HOST_ARCH := $(shell uname -m)
DEB_ARCH := $(shell dpkg --print-architecture 2>/dev/null || uname -m)
DEB_STAGE = build/deb
DEB_PKG = dist/ueilogg_$(APP_VERSION)_$(DEB_ARCH).deb
DIST_STAGE = build/dist
DIST_TARBALL = dist/ueilogg-$(APP_VERSION).tar.gz
RPM_TOPDIR = $(CURDIR)/build/rpm
ARCH_STAGE = build/arch
APK_STAGE = build/apk
TAR_STAGE = build/tarball
BIN_TARBALL = dist/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH).tar.gz

SRCS = src/main.c src/db.c src/tui.c src/logg.c src/settings.c src/callbook.c src/dxcc.c src/qrb.c src/util.c src/space_weather.c src/qrz.c src/file_dialog.c src/help.c src/theme_editor.c
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard src/*.h)

TEST_SRCS = tests/test_main.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_DEPS = src/db.o src/tui.o src/logg.o src/settings.o src/callbook.o src/dxcc.o src/qrb.o src/util.o src/space_weather.o src/qrz.o src/file_dialog.o src/help.o src/theme_editor.o

# Standardmål: Bygg huvudprogrammet, hjälpbinären, testbinären (och manualen om aktiverad)
all: $(TARGET) $(TOOL_TARGET) $(TEST_TARGET) $(ALL_MANUAL)

# Bekväma kortnamn för enskilda mål (bygger alltid i bin/)
ueilogg: $(TARGET)
dxcc: $(TOOL_TARGET)
testing: $(TEST_TARGET)

# Huvudprogrammet UEI-Logg
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Hjälpprogrammet dxcc (uppdatering och statistik för dxcc.txt)
$(TOOL_TARGET): src/dxcc_tool.o src/util.o
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TOOL_TARGET) src/dxcc_tool.o src/util.o $(TOOL_LDFLAGS)

# Testbinären testing (automatiserad testsvit)
$(TEST_TARGET): $(TEST_OBJS) $(TEST_DEPS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_OBJS) $(TEST_DEPS) $(LDFLAGS)

# Kör testerna (stöder både "make test" och GNU-standard "make check")
test check: $(TEST_TARGET)
	./$(TEST_TARGET)

# Användarmanual i PDF-format
manual: $(MANUAL_PDF)

$(MANUAL_PDF): $(MANUAL_SRC)
	$(PDFLATEX) -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)
	$(PDFLATEX) -interaction=nonstopmode -output-directory manual $(MANUAL_SRC)

src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET) $(TOOL_TARGET) $(INSTALL_MANUAL_DEP)
	install -d "$(BINDIR)" "$(DATADIR)" "$(DOCDIR)" "$(MANDIR)/man1" "$(APPDIR)"
	install -m 755 $(TARGET) "$(BINDIR)/ueilogg"
	$(STRIP) "$(BINDIR)/ueilogg"
	install -m 755 $(TOOL_TARGET) "$(BINDIR)/dxcc"
	$(STRIP) "$(BINDIR)/dxcc"
	install -m 644 dxcc.txt "$(DATADIR)/dxcc.txt"
	install -m 644 man/ueilogg.1 "$(MANDIR)/man1/ueilogg.1"
	install -m 644 man/dxcc.1 "$(MANDIR)/man1/dxcc.1"
	install -m 644 packaging/ueilogg.desktop "$(APPDIR)/ueilogg.desktop"
	install -m 644 README.md "$(DOCDIR)/README.md"
	install -m 644 CHANGELOG.md "$(DOCDIR)/CHANGELOG.md"
	@if [ -f "$(MANUAL_PDF)" ]; then install -m 644 "$(MANUAL_PDF)" "$(DOCDIR)/manual.pdf"; fi
	install -m 644 LICENSE "$(DOCDIR)/copyright"

install-strip: install

uninstall:
	rm -f "$(BINDIR)/ueilogg" "$(BINDIR)/dxcc"
	rm -f "$(MANDIR)/man1/ueilogg.1" "$(MANDIR)/man1/dxcc.1"
	rm -f "$(APPDIR)/ueilogg.desktop"
	rm -rf "$(DATADIR)" "$(DOCDIR)"

deb: $(TARGET) $(TOOL_TARGET) $(INSTALL_MANUAL_DEP)
	@if [ -z "$(APP_VERSION)" ]; then echo "Fel: Kunde inte läsa APP_VERSION från src/version.h"; exit 1; fi
	@command -v dpkg-deb >/dev/null 2>&1 || { echo "Fel: dpkg-deb saknas (installera paketet dpkg)."; exit 1; }
	rm -rf $(DEB_STAGE)
	mkdir -p $(DEB_STAGE)/DEBIAN dist
	$(MAKE) install DESTDIR=$(DEB_STAGE) PREFIX=/usr BINDIR=$(DEB_STAGE)/usr/bin DATADIR=$(DEB_STAGE)/usr/share/ueilogg DOCDIR=$(DEB_STAGE)/usr/share/doc/ueilogg MANDIR=$(DEB_STAGE)/usr/share/man APPDIR=$(DEB_STAGE)/usr/share/applications
	INSTALLED_SIZE=$$(du -sk $(DEB_STAGE)/usr | cut -f1); \
	sed -e "s/@VERSION@/$(APP_VERSION)/g" \
	    -e "s/@ARCH@/$(DEB_ARCH)/g" \
	    -e "s/@INSTALLED_SIZE@/$$INSTALLED_SIZE/g" \
	    packaging/deb/control.in > $(DEB_STAGE)/DEBIAN/control
	chmod 644 $(DEB_STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(DEB_STAGE) $(DEB_PKG)
	@echo "Skapade paket: $(DEB_PKG)"

# Källkodstarboll (används som bas för RPM, Arch PKGBUILD, Alpine APKBUILD m.fl.)
dist:
	@if [ -z "$(APP_VERSION)" ]; then echo "Fel: Kunde inte läsa APP_VERSION från src/version.h"; exit 1; fi
	rm -rf $(DIST_STAGE)
	mkdir -p $(DIST_STAGE)/ueilogg-$(APP_VERSION) dist
	cp -a configure Makefile install_dependencies.sh dxcc.txt README.md CHANGELOG.md LICENSE $(DIST_STAGE)/ueilogg-$(APP_VERSION)/
	cp -a src tests packaging man $(DIST_STAGE)/ueilogg-$(APP_VERSION)/
	rm -f $(DIST_STAGE)/ueilogg-$(APP_VERSION)/src/*.o $(DIST_STAGE)/ueilogg-$(APP_VERSION)/tests/*.o
	mkdir -p $(DIST_STAGE)/ueilogg-$(APP_VERSION)/manual
	cp -a manual/manual.tex $(DIST_STAGE)/ueilogg-$(APP_VERSION)/manual/
	@if [ -f manual/manual.pdf ]; then cp -a manual/manual.pdf $(DIST_STAGE)/ueilogg-$(APP_VERSION)/manual/; fi
	tar -C $(DIST_STAGE) -czf $(DIST_TARBALL) ueilogg-$(APP_VERSION)
	@echo "Skapade källkodstarboll: $(DIST_TARBALL)"

# RPM-paket för RHEL / Fedora / Rocky Linux / AlmaLinux / CentOS / openSUSE
rpm: dist
	@command -v rpmbuild >/dev/null 2>&1 || { echo "Fel: rpmbuild saknas (installera 'rpm-build' på RHEL/Fedora/openSUSE)."; exit 1; }
	rm -rf $(RPM_TOPDIR)
	mkdir -p $(RPM_TOPDIR)/BUILD $(RPM_TOPDIR)/BUILDROOT $(RPM_TOPDIR)/RPMS $(RPM_TOPDIR)/SOURCES $(RPM_TOPDIR)/SPECS $(RPM_TOPDIR)/SRPMS dist
	cp -a $(DIST_TARBALL) $(RPM_TOPDIR)/SOURCES/
	sed -e "s/@VERSION@/$(APP_VERSION)/g" packaging/rpm/ueilogg.spec.in > $(RPM_TOPDIR)/SPECS/ueilogg.spec
	rpmbuild -ba --define "_topdir $(RPM_TOPDIR)" $(RPM_TOPDIR)/SPECS/ueilogg.spec
	find $(RPM_TOPDIR)/RPMS $(RPM_TOPDIR)/SRPMS -type f -name "*.rpm" -exec cp -f {} dist/ \;
	@echo "Skapade RPM-paket i dist/"

# Arch Linux / Manjaro / EndeavourOS-paket (.pkg.tar.*)
archpkg: dist
	@command -v makepkg >/dev/null 2>&1 || { echo "Fel: makepkg saknas (kör på Arch/Manjaro/EndeavourOS med base-devel)."; exit 1; }
	@if [ "$$(id -u)" -eq 0 ]; then echo "Fel: makepkg får inte köras som root på Arch Linux."; exit 1; fi
	rm -rf $(ARCH_STAGE)
	mkdir -p $(ARCH_STAGE) dist
	cp -a $(DIST_TARBALL) $(ARCH_STAGE)/
	SHA=$$(sha256sum $(DIST_TARBALL) | awk '{print $$1}'); \
	sed -e "s/@VERSION@/$(APP_VERSION)/g" -e "s/@SHA256@/$$SHA/g" packaging/arch/PKGBUILD.in > $(ARCH_STAGE)/PKGBUILD
	(cd $(ARCH_STAGE) && makepkg -f)
	find $(ARCH_STAGE) -maxdepth 1 -type f -name "*.pkg.tar.*" -exec cp -f {} dist/ \;
	@echo "Skapade Arch-paket i dist/"

# Alpine Linux-paket (.apk)
apk: dist
	@command -v abuild >/dev/null 2>&1 || { echo "Fel: abuild saknas (installera 'alpine-sdk' på Alpine Linux)."; exit 1; }
	rm -rf $(APK_STAGE)
	mkdir -p $(APK_STAGE)/repo dist
	cp -a $(DIST_TARBALL) $(APK_STAGE)/
	sed -e "s/@VERSION@/$(APP_VERSION)/g" packaging/alpine/APKBUILD.in > $(APK_STAGE)/APKBUILD
	(cd $(APK_STAGE) && REPODEST="$(CURDIR)/$(APK_STAGE)/repo" abuild checksum && REPODEST="$(CURDIR)/$(APK_STAGE)/repo" abuild -r)
	find $(APK_STAGE)/repo -type f -name "*.apk" -exec cp -f {} dist/ \;
	@echo "Skapade Alpine APK-paket i dist/"

# Portabel binärtarboll för valfri Linux-distribution (Void, Gentoo, Slackware m.fl.)
tarball: $(TARGET) $(TOOL_TARGET) $(INSTALL_MANUAL_DEP)
	@if [ -z "$(APP_VERSION)" ]; then echo "Fel: Kunde inte läsa APP_VERSION från src/version.h"; exit 1; fi
	rm -rf $(TAR_STAGE)
	mkdir -p $(TAR_STAGE) dist
	$(MAKE) install DESTDIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH) PREFIX= BINDIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)/bin DATADIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)/share/ueilogg DOCDIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)/share/doc/ueilogg MANDIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)/share/man APPDIR=$(TAR_STAGE)/ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)/share/applications
	tar -C $(TAR_STAGE) -czf $(BIN_TARBALL) ueilogg-$(APP_VERSION)-linux-$(HOST_ARCH)
	@echo "Skapade portabel binärtarboll: $(BIN_TARBALL)"

# Bygg automatiskt rätt paketformat för aktuell Linux-distribution
package pkg:
	@if command -v dpkg-deb >/dev/null 2>&1; then \
		$(MAKE) deb; \
	elif command -v rpmbuild >/dev/null 2>&1; then \
		$(MAKE) rpm; \
	elif command -v makepkg >/dev/null 2>&1; then \
		$(MAKE) archpkg; \
	elif command -v abuild >/dev/null 2>&1; then \
		$(MAKE) apk; \
	else \
		echo "Ingen inbyggd paketbyggare (dpkg-deb/rpmbuild/makepkg/abuild) hittades – bygger portabel tarball..."; \
		$(MAKE) tarball; \
	fi

clean:
	rm -rf $(BIN_DIR)
	rm -f ueilogg dxcc testing src/*.o tests/*.o *.o
	rm -f manual/*.aux manual/*.log manual/*.out manual/*.toc manual/*.pdf
	rm -rf build dist

distclean: clean
	rm -f config.mk config.log config.status

.PHONY: all ueilogg dxcc testing clean distclean manual test check install install-strip uninstall deb dist rpm archpkg apk tarball package pkg
