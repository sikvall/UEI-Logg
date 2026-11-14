# UEI-Logg

**UEI-Logg** (UEI-log v1.0) är ett snabbt, modernt och textbaserat loggboksprogram (TUI) skrivet i C11 för radioamatörer. Programmet är anpassat för både svenska och internationella amatörradiokontakter och fungerar i alla Linux- och Unix-terminaler med stöd för UTF-8.

---

## Innehållsförteckning

- [Funktioner](#funktioner)
- [Statistikruta och Rymdvädersruta](#statistikruta-och-rymdvädersruta)
- [Systemkrav och förutsättningar](#systemkrav-och-förutsättningar)
- [Installation av nödvändiga utvecklingspaket](#installation-av-nödvändiga-utvecklingspaket)
- [Kompilering och bygge](#kompilering-och-bygge)
- [Start och användning](#start-och-användning)
- [Hjälpprogrammet dxcc](#hjälpprogrammet-dxcc)
- [Kortkommandon](#kortkommandon)
- [Filer och konfiguration](#filer-och-konfiguration)
- [Licens](#licens)

---

## Funktioner

- **4-raders QSO-inmatning**: Optimerat och kompakt inmatningsformulär (Signal, RST T, RST R, Namn, QTH, Lokator, QRB, DXCC, Frekvens, Band, Mode, Datum, UTC, Notering).
- **Live-statistikruta**: Visar antal loggade QSO, unika anropssignaler, unika DXCC, unika lokatorrutor, körda bandland samt QSO-takt per timme (Rate). Kan stängas av via inställningsmenyn (F12).
- **Rymdväder och solförhållanden**: Integrerad rymdvädersruta med realtidsdata från Avitec HF (SFI, SSN, A-index, K-index, solflares, geomagnetiska stormar och HF-konditioner per band).
- **Automatiska uppslag**:
  - Svenska anropssignaler slås automatiskt upp mot **SM Callbook** (SSA).
  - Utländska anropssignaler slås automatiskt upp mot **QRZ.COM XML API**.
  - Automatisk detektering av Silent Keys (visas med `-SK-` och `SILENT KEY`).
- **Geografiska beräkningar**: Automatisk beräkning av avstånd (QRB i kilometer) och bäring via Maidenhead-lokatorer.
- **ADIF 3.1.8-stöd**: Fullt stöd för import och export med inbyggd fildialog. Export kan ske av hela loggen, markerade rader eller endast nya/ohanterade kontakter.
- **Flera färgscheman**: Borland Turbo Pascal, Midnight Commander, Monokrom, Amber Retro, Grön CRT, Matrix m.fl.

---

## Statistikruta och Rymdvädersruta

UEI-Logg har två specialkomponenter i användargränssnittet som presenterar realtidsinformation och automatiskt anpassar sig efter terminalfönstrets höjd och användarens inställningar:

### 1. Live-statistikrutan (övre delen)

Statistikrutan är placerad högst upp i programmet, direkt ovanför QSO-inmatningsfältet, och ger operatören en samlad översikt över loggbokens innehåll och aktivitet i realtid.

- **Visningsvillkor**: Visas när terminalfönstret har minst 30 rader (`LINES >= 30`) och reglaget **Statistikruta** är aktiverat i inställningsdialogen (F12). Om terminalen är mindre än 30 rader eller om reglaget stängs av, döljs rutan automatiskt så att logghistoriken får maximalt utrymme (4 extra rader).
- **Innehåll och fält**:
  - **QSO**: Totalt antal loggade kontakter i den aktuella loggboken.
  - **Stationer**: Antal unika anropssignaler (stationer) som körts.
  - **DXCC**: Antal unika DXCC-entiteter och länder som bekräftats i loggen.
  - **Lokatorer**: Antal unika 4-teckens Maidenhead-rutor (t.ex. `JO65`, `JP80`).
  - **Bandland**: Antal unika kombinationer av DXCC-land och amatörband (t.ex. Sverige på 80 m räknas separat från Sverige på 20 m).
  - **Rate**: Genomsnittlig QSO-takt, beräknad som antalet loggade QSO under de senaste 60 minuterna (uppdateras dynamiskt varje minut i bakgrunden).

### 2. Rymdvädersrutan (nedre delen)

Rymdvädersrutan är placerad längst ner i terminalfönstret (ovanför funktionstangentraden) och presenterar aktuella jonosfäriska, sol- och geomagnetiska förhållanden för HF-amatörbanden.

- **Visningsvillkor**: Visas när terminalfönstret har minst 25 rader (`LINES >= 25`). Vid lägre fönsterhöjder döljs den automatiskt så att formulär och logghistorik prioriteras.
- **Asynkron datahämtning**: Data hämtas i bakgrunden via en separat POSIX-tråd från `hf.avitec.se` och uppdateras automatiskt utan att störa eller fördröja pågående QSO-inmatning.
- **Presentation och nyckelvärden**:
  - **SFI (Solar Flux Index)**: Solens radiostrålning vid 2,8 GHz (10,7 cm). Högre värden (>100–150) indikerar god jonisering och förbättrade dagsförhållanden på de högre HF-banden (20 m till 10 m).
  - **SSN (Sunspot Number)**: Aktuellt solfläckstal. Fler solfläckar ger generellt starkare jonisering i jonosfärens F-skikt och högre maximal användbar frekvens (MUF).
  - **A-index**: Globalt geomagnetiskt dygnsvärde (linjär skala). Låga värden (0–7) innebär lugna magnetiska förhållanden.
  - **K-index**: Planetärt och lokalt geomagnetiskt index för de senaste 3 timmarna (skala 0–9, mätt bl.a. i Kiruna):
    - *0–2 (Grönt)*: Lugna och stabila geomagnetiska förhållanden. Mycket goda HF-signaler och lågt atmosfäriskt brus.
    - *3–4 (Gult)*: Oroligt/aktiverat magnetfält. Risk för fading och dämpning, särskilt på polar- och nordliga signalvägar.
    - *5–9 (Rött)*: Geomagnetisk storm (G1–G5). Kraftig absorption, störd polarkalott och radioutslag (men ger möjlighet till polarskensreflektion/aurorastuds på 6 m och 2 m).
  - **Solflares (Röntgenstrålning)**: Senaste solutbrott i klasserna A, B, C, M och X. Kraftiga M- och X-flares kan orsaka omedelbar D-skiktsabsorption (Radio Blackout) på dagssidan.
  - **HF-konditioner per band**: Färgkodad sammanställning av aktuella signalförhållanden (*Bra* / *Måttlig* / *Dålig*) uppdelat på dag och natt för:
    - 80 m – 40 m
    - 30 m – 20 m
    - 17 m – 15 m
    - 12 m – 10 m

---

## Systemkrav och förutsättningar

För att kompilera och köra UEI-Logg krävs följande:

1. **Operativsystem**: Linux eller POSIX-kompatibelt operativsystem med UTF-8 locale.
2. **Kompilator**: GCC eller Clang med stöd för C11 (`-std=c11`).
3. **Byggverktyg**: GNU Make (`make`).
4. **Externa bibliotek**:
   - **ncursesw** (ncurses med bredteckenstöd / UTF-8): Terminalgränssnitt och fönsterhantering (`libncursesw5-dev` eller `libncurses-dev`).
   - **libcurl**: HTTP- och HTTPS-förfrågningar för rymdväder, QRZ.COM och DXCC-uppdateringar (`libcurl4-openssl-dev`).
   - **sqlite3**: Databasmotorn för lokal lagring i `qsolog.sq3` (`libsqlite3-dev`).
   - **libm**: Standard C matematikbibliotek för QRB- och storcirkelberäkningar.
   - **pthread**: POSIX-trådar för bakgrundshämtning utan att frysa terminalgränssnittet.
   - **pdfLaTeX (valfritt)**: TeX Live för att kompilera användarmanualen till PDF (`make manual`).

---

## Installation av nödvändiga utvecklingspaket

### Automatisk installation via hjälpskript (rekommenderas)

Projektet inkluderar ett installationsskript som automatiskt känner av din Linux-distribution och installerar samtliga nödvändiga utvecklingspaket:

```bash
# Kör installationsskriptet
./install_dependencies.sh

# För att installera allt automatiskt inkl. LaTeX för PDF-manualen:
./install_dependencies.sh --latex -y
```

### Manuell installation per distribution

Installera kompilator och utvecklingsbibliotek via din distributions pakethanterare:

### Ubuntu / Debian / Linux Mint / Raspberry Pi OS

Kör följande exakta kommandon för att installera allt som krävs:

```bash
# 1. Uppdatera paketlistorna
sudo apt update

# 2. Installera nödvändiga utvecklingspaket för att bygga ueilog, dxcc och testing
sudo apt install -y build-essential libncursesw5-dev libcurl4-openssl-dev libsqlite3-dev

# 3. (Valfritt) Installera LaTeX-paket för att kunna bygga användarmanualen (manual/manual.pdf)
sudo apt install -y texlive-latex-base texlive-latex-extra texlive-lang-european
```

### Fedora / RHEL / CentOS / AlmaLinux / Rocky Linux

```bash
sudo dnf install -y gcc make ncurses-devel libcurl-devel sqlite-devel
```

### Arch Linux / Manjaro

```bash
sudo pacman -Syu --needed base-devel ncurses curl sqlite
```

### openSUSE

```bash
sudo zypper refresh
sudo zypper install -y gcc make ncurses-devel libcurl-devel sqlite3-devel
```

---

## Kompilering och bygge

Projektet byggs med GNU Make.

### Bygg alla mål (huvudprogram, dxcc, testbinär och manual)

```bash
make
# eller
make all
```

Detta skapar följande i projektkatalogen:
- **`ueilog`**: Det huvudsakliga TUI-loggboksprogrammet.
- **`dxcc`**: Hjälpverktyg för uppdatering och analys av DXCC-databasen (`dxcc.txt`).
- **`testing`**: Fristående automatiserad testsvit för enhets- och regressionstester.
- **`manual/manual.pdf`**: Komplett användarmanual i PDF-format.

### Bygg enskilda mål

```bash
# Bygg endast UEI-Logg
make ueilog

# Bygg endast DXCC-verktyget
make dxcc

# Bygg och kör testsviten
make test

# Bygg användarmanualen
make manual
```

### Rensa byggfiler

För att ta bort kompilerade objektfiler (`*.o`), binärer och temporära LaTeX-filer:

```bash
make clean
```

---

## Start och användning

### Starta loggboken

Starta programmet i din terminal:

```bash
./ueilog
```

### Kommandoradsflaggor

UEI-Logg stöder valfri loggfil via kommandoraden:

```bash
# Använd en specifik ADIF-fil (skapas automatiskt med giltig header om den saknas)
./ueilog -l contest2026.adi
./ueilog --logfile=/path/to/my_log.adi

# Visa hjälpinformation
./ueilog --help

# Visa programversion
./ueilog --version
```

---

## Hjälpprogrammet dxcc

Projektet inkluderar det fristående verktyget `dxcc` för hantering av [dxcc.txt](file:///home/anders/Documents/UEI-Logg/dxcc.txt):

```bash
# Visa hjälp och tillgängliga kommandon
./dxcc help

# Visa version
./dxcc version

# Uppdatera dxcc.txt från Jim Reisert AD1C:s Country Files online
./dxcc update

# Presentera detaljerad statistik och formaterade tabeller över databasen
./dxcc stat

# Slå upp DXCC-entitet för en anropssignal direkt i terminalen
./dxcc lookup SM5UEI
./dxcc lookup OH0
```

---

## Kortkommandon

När `ueilog` körs är följande tangenter tillgängliga:

| Tangent | Beskrivning |
|---|---|
| **F2** / **Enter** | Spara och logga aktuellt QSO till ADIF-filen |
| **F3** / **Ctrl+F** | Sök och filtrera i logghistoriken på signal eller fritext |
| **F4** / **Ctrl+O** | Växla fokus mellan inmatningsformulär och loggrader |
| **F5** | I inmatningsläge: Manuellt uppslag mot SM Callbook / QRZ.COM<br>I loggläge: Sätt status till Hanterad (grönt bockmärke ✓) |
| **F6** | I loggläge: Sätt status till Ny (rött kryss ✗) |
| **F8** | Radera vald eller markerade rader ur loggen (med bekräftelse) |
| **F9** | Öppna den interaktiva hjälpdialogen med fullständig handbok |
| **F12** | Öppna inställningsdialogen (Min Station, QRZ-inloggning, färgteman, ADIF import/export, reglage för statistikruta m.m.) |
| **Ctrl+Q** / **Ctrl+C** | Avsluta programmet |
| **Tab** / **Skift+Tab** | Stega framåt / bakåt mellan inmatningsfälten |
| **Pilar Upp/Ned/Vänster/Höger** | Navigera i formulär, fält och menyer |
| **Insert (INS)** | Markera / avmarkera enskild rad i loggen för gruppåtgärder |
| **Delete (DEL)** | Tömmer hela det aktiva inmatningsfältet |
| **Page Up** / **Page Down** | Bläddra en sida i taget i logghistoriken |
| **Home** / **End** | Gå till början (Signal) eller slutet (Notering) i formuläret |
| **Ctrl+Home** / **Ctrl+End** | Gå till första eller sista posten i hela loggen |

---

## Filer och konfiguration

- **`station.conf`**: Konfigurationsfil för stationen (anropssignal, operatörsnamn, QTH, lokator, tidszon, QRZ-inloggning, färgtema, reglage för statistikruta m.m.).
- **`qsolog.adi`**: Standardloggfil i ADIF-format där alla genomförda kontakter lagras.
- **`dxcc.txt`**: DXCC-prefix- och undantagsdatabas med över 3 900 regler och svenska översättningar.
- **`dxcc.txt.bak`**: Automatisk säkerhetskopia som skapas vid körning av `./dxcc update`.

---

## Licens

UEI-Logg är utvecklat för radioamatörer och distribueras fritt för amatörradioändamål.
