# Ändringslogg (CHANGELOG)

Alla betydande ändringar i **UEI-Logg** dokumenteras i denna fil.
Formatet baseras på [Keep a Changelog](https://keepachangelog.com/sv/1.1.0/) och projektet följer [Semantic Versioning](https://semver.org/lang/sv/).

---

## [1.0.1] - 2026-11-23

### Tillagt
- **GNU-kompatibel konfigurering (`./configure`)**:
  - Stöder standardflaggor enligt GNU Coding Standards (`--prefix`, `--exec-prefix`, `--bindir`, `--datarootdir`, `--datadir`, `--docdir`, `--mandir`, `--enable-debug`, `--enable-werror`, `--enable-static`/`--disable-static`, `--enable-manual`/`--disable-manual` samt miljövariabler som `CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `PKG_CONFIG`, `STRIP`, `PDFLATEX`).
  - Kontrollerar C11-kompilator, `pkg-config`-flaggor (`ncursesw`, `libcurl`, `sqlite3`), headers, länkning och `pdflatex`.
  - Genererar `config.mk`, `config.status` (`--recheck`) och `config.log`.
- **Paketering för flera Linux-distributioner (`dist/`)**:
  - `make package` / `make pkg`: Identifierar automatiskt plattformens paketeringsverktyg (`dpkg-deb`, `rpmbuild`, `makepkg`, `abuild` eller portabel tarball).
  - `make deb`: Debian / Ubuntu / Linux Mint (`.deb`, inkl. dynamiska runtime-beroenden i `packaging/deb/control.in`).
  - `make rpm`: RHEL / Fedora / Rocky Linux / AlmaLinux / CentOS / openSUSE (`.rpm` och `.src.rpm` via `packaging/rpm/ueilogg.spec.in`).
  - `make archpkg`: Arch Linux / Manjaro / EndeavourOS (`.pkg.tar.*` via `packaging/arch/PKGBUILD.in`).
  - `make apk`: Alpine Linux (`.apk` via `packaging/alpine/APKBUILD.in`).
  - `make dist` och `make tarball`: Ren källkodstarboll respektive portabel binärtarboll.
- **Installation & skrivbordsgenväg**:
  - Lagt till `make install`, `make install-strip`, `make uninstall` och skrivbordsfilen `packaging/ueilogg.desktop`.
- **Lagring och databasval i `~/.ueilogg/`**:
  - Samlar användarspecifika filer (`station.conf`, `dxcc.txt`, `qsolog.sq3`) i `~/.ueilogg/`.
  - Ny inställning för att välja eller skapa ny SQLite3-databas direkt i inställningsmenyn (`F12` och `F3`), med automatisk migrering och isolering per databas.
- **Centraliserad versionshantering (`src/version.h`)**:
  - Applikationsnamn, versionsnummer (`APP_VERSION`) och byggdatum (`APP_BUILD_DATE`) visas enhetligt i titelrad, CLI (`--version`) och byggskript.
- **Ändringslogg**: Lagt till denna `CHANGELOG.md` och inkluderat den i dokumentationsinstallation och källkodstarboll.
- **Manualsidor (`man`)**: Lagt till riktiga manualsidor i sektion 1 ([man/ueilogg.1](man/ueilogg.1) och [man/dxcc.1](man/dxcc.1)) med installation till `$(MANDIR)/man1` samt stöd i paketmålen (`.deb`, `.rpm`, Arch, Alpine, tarball och `make dist`).

### Ändrat
- **Utdata till `bin/`**: Flyttade kompilerade binärer till `bin/ueilogg`, `bin/dxcc` och `bin/testing` (med bibehållna bekvämlighetsmål `make ueilogg`, `make dxcc`, `make testing` samt `make check` och `make distclean`).
- **Dynamisk länkning som standard i `Makefile`**: Växlat från `-static` till dynamisk systemlänkning (`-lncursesw -lcurl -lsqlite3 -lm -pthread -ldl`) för bred kompatibilitet på bl.a. RHEL, Fedora, Arch och openSUSE.
- **Utökat beroendeskript (`install_dependencies.sh`)**: Stöd för Debian/Ubuntu, Red Hat/Fedora/RHEL/Rocky/AlmaLinux, Arch/Manjaro, openSUSE/SLES, Alpine och Void Linux (inkl. `pkg-config`/`pkgconf`, `--dry-run`, `--latex` och verifiering av byggmiljö).
- **Tomma RST-fält sparas blankt**: När `Auto RST 59/59` är avstängt (eller fält lämnas tomma) sparas och importeras blankt RST utan tvångsifyllning.
- **Refaktorisering & dokumentation**: Dedup-städning av hjälpfunktioner i `src/` samt uppdaterade `README.md` och `manual/manual.tex` (totalt 113 automatiserade enhetstester i `bin/testing`).

### Rättat
- **UTF-8/QTH vid QRZ-import**: Rättade trunkering av QTH och andra UTF-8-fält (t.ex. Å, Ä, Ö) vid import av ADIF från QRZ.COM.

---

## [1.0.0] - 2026-11-17

### Tillagt
- **Terminalgränssnitt (TUI) i C11 & `ncursesw`**:
  - Fullt UTF-8-stöd (inkl. svenska Å, Ä, Ö med teckenbaserad markörpositionering och klippning).
  - Kompakt 4-raders QSO-inmatningsformulär med snabbnavigering (`Tab`, `Shift+Tab`, pilar, `Home`/`End`, `Del` för rensning och automatisk rensning av förifyllt fält vid skrivning).
  - Responsiv loggradstabell med dynamisk kolumnprioritering och QTH-expansion beroende på terminalbredd.
  - Live-statistikruta (antal QSO, unika signaler, DXCC, 4-teckens lokatorer, bandland, QSO/h) samt rymdvädersbox från `hf.avitec.se` med K/Kp, SFI, SSN, solvind, röntgenflöde och DX/NVIS-rekommenderade band.
- **SQLite3 & ADIF 3.1.8 & CSV-export**:
  - Persistent SQLite3-lagring med Write-Ahead Logging (WAL), indexering, automatisk ADIF-migrering och automatisk Latin-1-till-UTF-8-sanering.
  - ADIF-import och -export (`F5`/`F6` i inställningar) med interaktiv fildialog (`src/file_dialog.c`), dubblettkontroll och exportlägen för alla, endast nya eller markerade rader (`Ins`).
  - **CSV-export**: Exporterar fälten i exakt samma ordning som loggraderna (`#`, `Datum`, `Tid`, `Signal`, `Frekvens`, `Band`, `Mode`, `RST`, `Namn`, `QTH`, `QRB`, `QUJ`, `Lokator`, `DXCC`, `Notering`) med UTF-8 BOM och RFC 4180-citering för Excel, Google Docs och LibreOffice.
- **QRZ.COM-integration**:
  - Asynkront signaluppslag via XML Data Service med lägen `Av`, `Auto` och `Manuell` (`F5`).
  - Direktloggning mot QRZ.COM Logbook API (`Av`, `Automatisk` vid Enter, `Manuell` via `Ctrl+L`).
  - Tvåvägssynkronisering i inställningsmenyn: **Synka till QRZ.COM** (laddar upp ohanterade QSO) och **Synka från QRZ (`F8`)** (hämtar, XML/HTML-avkodar och sammanfogar utan dubbletter).
- **DXCC, QRB & QUJ**:
  - Realtidsuppslag av DXCC-entitet från `dxcc.txt` samt fristående CLI-verktyg `dxcc` (`update`, `stat`, `lookup`, `version`, `help`).
  - Avståndsberäkning (`QRB` i km) och kompassbäring (`QUJ` i grader) från Maidenhead-lokator eller QTH, med omräkning via `F7`.
- **Färgteman & profilereditor**:
  - 16 färgteman (med **UEI-tema** som default, inklusive Ambra, Solarized Dark, Dracula, Militärradio och Monokrom gråskala) samt interaktiv färgprofilseditor (`F7` i inställningar).
- **Logghantering & dokumentation**:
  - Ändra ordning på loggrader (`w`/`s`), cykla radbakgrundsfärg (`c`), markera alla som hanterade (`F7`), flervalsmarkering (`Ins`) och fritextfiltrering (`F3`/`Ctrl+F`, `Esc`).
  - Inbyggd hjälpdialog (`F9`), komplett användarmanual i LaTeX/PDF (`manual/manual.tex`) och fristående automatiserad testsvit (`tests/test_main.c`).

### Ändrat & Rättat
- Högerställd frekvenskolumn i loggtabellen med låst decimalpunkt på fast kolumnposition, samt stöd för både `HH:MM` och `HHMM` vid tidsinmatning och omstart av UTC-klockan med `F2`.
- Maskering av QRZ-lösenord och QRZ API-nyckel som `**********` i inställningsvyn när de inte redigeras.
- Rättade att `auto_rst_59`-inställningen respekteras när man lämnar `Mode`-fältet.
- Rättade rymdvädershämtningen från `hf.avitec.se` (tvingad IPv4-uppslagning, `Accept-Encoding` samt snabbare 30-sekunders omförsök vid initialt fel).
- Tog bort kvarvarande referenser till SM Callbook från gränssnitt och dokumentation.
