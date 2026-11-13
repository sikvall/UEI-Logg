# UEI-Logg

**UEI-Logg** (UEI-log v1.0) är ett snabbt, modernt och textbaserat loggboksprogram (TUI) skrivet i C11 för radioamatörer. Programmet är anpassat för både svenska och internationella amatörradiokontakter och fungerar i alla Linux- och Unix-terminaler med stöd för UTF-8.

---

## Innehållsförteckning

- [Funktioner](#funktioner)
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

## Systemkrav och förutsättningar

För att kompilera och köra UEI-Logg krävs följande:

1. **Operativsystem**: Linux eller POSIX-kompatibelt operativsystem med UTF-8 locale.
2. **Kompilator**: GCC eller Clang med stöd för C11 (`-std=c11`).
3. **Byggverktyg**: GNU Make (`make`).
4. **Externa bibliotek**:
   - **ncursesw** (ncurses med bredteckenstöd / UTF-8): Terminalgränssnitt och fönsterhantering.
   - **libcurl**: HTTP- och HTTPS-förfrågningar för rymdväder, QRZ.COM och DXCC-uppdateringar.
   - **libm**: Standard C matematikbibliotek för QRB- och storcirkelberäkningar.
   - **pthread**: POSIX-trådar för bakgrundshämtning utan att frysa terminalgränssnittet.

---

## Installation av nödvändiga utvecklingspaket

Installera kompilator och utvecklingsbibliotek via din distributions pakethanterare:

### Debian / Ubuntu / Linux Mint / Raspberry Pi OS

```bash
sudo apt update
sudo apt install -y build-essential libncursesw5-dev libcurl4-openssl-dev
```

### Fedora / RHEL / CentOS / AlmaLinux / Rocky Linux

```bash
sudo dnf install -y gcc make ncurses-devel libcurl-devel
```

### Arch Linux / Manjaro

```bash
sudo pacman -Syu --needed base-devel ncurses curl
```

### openSUSE

```bash
sudo zypper refresh
sudo zypper install -y gcc make ncurses-devel libcurl-devel
```

---

## Kompilering och bygge

Projektet byggs med GNU Make.

### Bygg alla mål (huvudprogram och hjälpverktyg)

```bash
make
# eller
make all
```

Detta skapar två körbara binärer i projektets rotkatalog:
- **`ueilog`**: Det huvudsakliga loggboksprogrammet.
- **`dxcc`**: Hjälpverktyg för uppdatering och analys av DXCC-databasen (`dxcc.txt`).

### Bygg enskilda mål

```bash
# Bygg endast UEI-Logg
make ueilog

# Bygg endast DXCC-verktyget
make dxcc
```

### Rensa byggfiler

För att ta bort kompilerade objektfiler (`*.o`) och binärer:

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
