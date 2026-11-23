#!/usr/bin/env bash
# ==============================================================================
# install_dependencies.sh - Installera byggverktyg och bibliotek för UEI-Logg
# ==============================================================================
#
# Syfte:
#   Detta skript identifierar din Linux-distribution och installerar alla
#   kompilatorer, utvecklingsheaders, statiska bibliotek och (valfritt) LaTeX-
#   paket som krävs av Makefile för att bygga, testa och paketera UEI-Logg.
#
# Koppling till Makefile (vad som faktiskt kompileras och länkas):
#   1. Kompilator & byggverktyg:
#        - gcc (C11-kompilator), GNU make, binutils (strip vid "make install"),
#          samt standard C-bibliotek (glibc/musl).
#        - dpkg-deb / dpkg (för "make deb" på Debian/Ubuntu-baserade system).
#   2. Huvudprogrammet (ueilogg) och testsviten (testing) – dynamisk länkning:
#        LDFLAGS = -lncursesw -lcurl -lsqlite3 -lm -pthread -ldl
#        - ncurses med bredteckenstöd (UTF-8): -lncursesw (<ncurses.h>)
#        - libcurl (HTTP/HTTPS för QRZ/r.-väder): -lcurl (<curl/curl.h>)
#        - SQLite3-databas:                   -lsqlite3 (<sqlite3.h>)
#        - Standardmatematik, trådar, dl:     -lm -pthread -ldl
#   3. Hjälpverktyget dxcc:
#        TOOL_LDFLAGS = -lcurl -lm -pthread -ldl
#   4. Användarmanual i PDF (valfritt med --latex):
#        pdflatex på manual/manual.tex kräver stöd för svenska (babel/swedish),
#        UTF-8/T1-typsnitt, amsmath, amssymb, pifont (zapfding), geometry,
#        parskip, booktabs, tabularx, enumitem, microtype, xcolor och hyperref.
#
# Stödda plattformar / pakethanterare:
#   - Debian / Ubuntu / Linux Mint / Pop!_OS / Raspberry Pi OS / Kali  (apt-get)
#   - Red Hat / Fedora / RHEL / CentOS Stream / Rocky / AlmaLinux      (dnf/yum)
#   - Arch Linux / Manjaro / EndeavourOS / Garuda / Artix / CachyOS    (pacman)
#   - openSUSE Leap / Tumbleweed / SLES                                (zypper)
#   - Alpine Linux                                                     (apk)
#   - Void Linux                                                       (xbps-install)
#
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# Terminalfärger för tydliga och lättlästa statusmeddelanden
# ------------------------------------------------------------------------------
GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
BOLD="\033[1m"
RESET="\033[0m"

# ------------------------------------------------------------------------------
# Standardvärden för kommandoradsflaggor
# ------------------------------------------------------------------------------
INSTALL_LATEX=0
AUTO_YES=0
DRY_RUN=0

# Hitta katalogen där skriptet ligger (så vi kan kontrollera t.ex. lib/libcurl.a)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ------------------------------------------------------------------------------
# Hjälptext för --help / -h
# ------------------------------------------------------------------------------
print_usage() {
    echo -e "${BOLD}Användning:${RESET} $0 [alternativ]"
    echo ""
    echo "Installerar alla kompilatorer, headers och bibliotek som Makefile"
    echo "behöver för att kompilera, testa och installera UEI-Logg på Linux."
    echo ""
    echo "Alternativ:"
    echo "  -y, --yes          Installera automatiskt utan interaktiva frågor"
    echo "  --latex, --all     Installera även TeX Live / LaTeX för manual/manual.pdf"
    echo "  -n, --dry-run      Visa bara vilka kommandon och paket som skulle köras"
    echo "  -h, --help         Visa denna hjälpinformation"
    echo ""
}

# ------------------------------------------------------------------------------
# Tolka kommandoradsargument
# ------------------------------------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        -y|--yes)
            AUTO_YES=1
            ;;
        --latex|--all|--with-latex)
            INSTALL_LATEX=1
            ;;
        -n|--dry-run)
            DRY_RUN=1
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}[FEL]${RESET} Okänt argument: $arg"
            print_usage
            exit 1
            ;;
    esac
done

echo -e "${BLUE}================================================================${RESET}"
echo -e "${BOLD}   UEI-Logg: Installation av byggberoenden och bibliotek        ${RESET}"
echo -e "${BLUE}================================================================${RESET}"

# ------------------------------------------------------------------------------
# Identifiera Linux-distribution via /etc/os-release
# ------------------------------------------------------------------------------
# OS_ID   = exakt distributions-id (t.ex. ubuntu, fedora, arch, alpine)
# OS_LIKE = överordnade familjer (t.ex. "debian", "rhel fedora", "arch")
OS_ID=""
OS_LIKE=""
OS_NAME="Linux"
OS_VERSION_ID=""
if [ -f /etc/os-release ]; then
    # Läs in /etc/os-release i en säker delmiljö (bara kända variabler används)
    # shellcheck disable=SC1091
    . /etc/os-release
    OS_ID="${ID:-}"
    OS_LIKE="${ID_LIKE:-}"
    OS_NAME="${NAME:-Linux}"
    OS_VERSION_ID="${VERSION_ID:-}"
fi

# Normalisera ID och ID_LIKE till gemener för säker jämförelse
OS_ID="${OS_ID,,}"
OS_LIKE="${OS_LIKE,,}"

echo -e "${BLUE}[INFO]${RESET} Identifierat operativsystem: ${BOLD}${OS_NAME}${RESET} (id=${OS_ID:-okänd}, like=${OS_LIKE:-ingen})"
if [ "$DRY_RUN" -eq 1 ]; then
    echo -e "${CYAN}[DRY-RUN]${RESET} Torrkörningsläge aktivt – inga paket installeras eller ändras."
fi

# ------------------------------------------------------------------------------
# Kontrollera administratörsrättigheter (root eller sudo)
# ------------------------------------------------------------------------------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    elif [ "$DRY_RUN" -eq 1 ]; then
        # Vid --dry-run kan vi visa sudo-kommandon även om sudo saknas i sandbox
        SUDO="sudo"
    else
        echo -e "${RED}[FEL]${RESET} Detta skript kräver root-rättigheter eller installerad 'sudo'."
        exit 1
    fi
fi

# ------------------------------------------------------------------------------
# Fråga interaktivt om LaTeX om flaggan --latex inte angetts via CLI
# ------------------------------------------------------------------------------
if [ "$INSTALL_LATEX" -eq 0 ] && [ "$AUTO_YES" -eq 0 ] && [ "$DRY_RUN" -eq 0 ] && [ -t 0 ]; then
    echo ""
    read -r -p "Vill du även installera LaTeX/TeX Live för att bygga PDF-manualen (manual/manual.pdf)? [j/N]: " answer
    case "$answer" in
        [jJyY]*)
            INSTALL_LATEX=1
            ;;
        *)
            INSTALL_LATEX=0
            ;;
    esac
fi

# ------------------------------------------------------------------------------
# Hjälpfunktion för att köra eller simulera kommandon
# ------------------------------------------------------------------------------
run_cmd() {
    if [ "$DRY_RUN" -eq 1 ]; then
        echo -e "${CYAN}[DRY-RUN]${RESET} $*"
    else
        "$@"
    fi
}

# ------------------------------------------------------------------------------
# Hjälpfunktioner för att kontrollera om valfria paket finns i aktiva förråd
# ------------------------------------------------------------------------------
# På t.ex. olika versioner av Debian, Fedora/RHEL och openSUSE kan namn på vissa
# statiska paket eller TeX Live-paket skilja sig åt. Genom att fråga pakethanteraren
# först undviker vi att hela installationen avbryts på grund av ett enda paketnamn.

apt_pkg_exists() {
    local pkg="$1"
    apt-cache show "$pkg" >/dev/null 2>&1
}

rpm_pkg_available() {
    local mgr="$1"
    local pkg="$2"
    "$mgr" -q info "$pkg" >/dev/null 2>&1 || rpm -q "$pkg" >/dev/null 2>&1
}

zypper_pkg_available() {
    local pkg="$1"
    zypper --non-interactive search --match-exact "$pkg" >/dev/null 2>&1
}

# ==============================================================================
# 1. Debian / Ubuntu / Linux Mint / Pop!_OS / Raspberry Pi OS / Kali (APT)
# ==============================================================================
install_debian_ubuntu() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Debian/Ubuntu-familjen (apt-get)..."
    echo -e "${BLUE}[INFO]${RESET} Uppdaterar paketlistor..."
    run_cmd $SUDO apt-get update

    # Grundläggande byggverktyg:
    #   build-essential = gcc, g++, make, libc6-dev, binutils (inkl. strip), dpkg-dev
    local PACKAGES=(
        "build-essential"
        "dpkg-dev"
    )

    # ncursesw (-lncursesw, <ncurses.h>):
    #   Nyare Debian/Ubuntu använder "libncurses-dev"; äldre utgåvor "libncursesw5-dev".
    if [ "$DRY_RUN" -eq 1 ] || apt_pkg_exists "libncurses-dev"; then
        PACKAGES+=("libncurses-dev")
    else
        PACKAGES+=("libncursesw5-dev")
    fi

    # Utvecklingsheaders och delade bibliotek (.so) enligt Makefile:
    #   libcurl4-openssl-dev -> -lcurl (<curl/curl.h>)
    #   libsqlite3-dev       -> -lsqlite3 (<sqlite3.h>)
    PACKAGES+=(
        "libcurl4-openssl-dev"
        "libsqlite3-dev"
    )

    # Valfria LaTeX-paket för PDF-manualen (manual/manual.tex):
    #   texlive-latex-base        -> pdflatex, geometry, hyperref, standard-LaTeX
    #   texlive-latex-recommended -> parskip, booktabs, microtype, xcolor
    #   texlive-latex-extra       -> enumitem, tabularx m.fl.
    #   texlive-lang-european     -> [swedish]{babel} (svensk avstavning & rubriker)
    #   texlive-fonts-recommended -> zapfding/pifont, grundläggande PostScript-typsnitt
    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        PACKAGES+=(
            "texlive-latex-base"
            "texlive-latex-recommended"
            "texlive-latex-extra"
            "texlive-lang-european"
            "texlive-fonts-recommended"
        )
        if [ "$DRY_RUN" -eq 1 ] || apt_pkg_exists "cm-super-minimal"; then
            PACKAGES+=("cm-super-minimal")
        fi
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar paket: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        run_cmd $SUDO apt-get install -y "${PACKAGES[@]}"
    else
        run_cmd $SUDO apt-get install "${PACKAGES[@]}"
    fi
}

# ==============================================================================
# 2. Red Hat / Fedora / RHEL / CentOS Stream / Rocky / AlmaLinux (DNF / YUM)
# ==============================================================================
install_fedora_rhel() {
    # Välj dnf om tillgängligt, annars yum (för äldre RHEL/CentOS 7)
    local PKG_MGR="dnf"
    if [ "$DRY_RUN" -eq 0 ] && ! command -v dnf >/dev/null 2>&1; then
        if command -v yum >/dev/null 2>&1; then
            PKG_MGR="yum"
        else
            echo -e "${RED}[FEL]${RESET} Varken 'dnf' eller 'yum' hittades på detta Red Hat-baserade system."
            exit 1
        fi
    fi

    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Red Hat/Fedora-familjen ($PKG_MGR)..."

    # Grundläggande byggverktyg & utvecklingsheaders enligt Makefile:
    #   gcc, make, binutils (strip)
    #   glibc-devel    -> standard C-headers
    #   ncurses-devel  -> -lncursesw (<ncurses.h>)
    #   libcurl-devel  -> -lcurl (<curl/curl.h>)
    #   sqlite-devel   -> -lsqlite3 (<sqlite3.h>)
    local PACKAGES=(
        "gcc"
        "make"
        "binutils"
        "glibc-devel"
        "ncurses-devel"
        "libcurl-devel"
        "sqlite-devel"
    )

    # Valfria LaTeX-paket för manual/manual.tex
    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        local LATEX_CANDIDATES=(
            "texlive-scheme-basic"
            "texlive-latex"
            "texlive-collection-latex"
            "texlive-collection-latexrecommended"
            "texlive-collection-latexextra"
            "texlive-babel-swedish"
            "texlive-psnfss"
            "texlive-zapfding"
        )
        if [ "$DRY_RUN" -eq 1 ]; then
            PACKAGES+=("${LATEX_CANDIDATES[@]}")
        else
            for lpkg in "${LATEX_CANDIDATES[@]}"; do
                if rpm_pkg_available "$PKG_MGR" "$lpkg"; then
                    PACKAGES+=("$lpkg")
                fi
            done
        fi
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via ${PKG_MGR}: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        run_cmd $SUDO "$PKG_MGR" install -y "${PACKAGES[@]}"
    else
        run_cmd $SUDO "$PKG_MGR" install "${PACKAGES[@]}"
    fi
}

# ==============================================================================
# 3. Arch Linux / Manjaro / EndeavourOS / Garuda / Artix / CachyOS (Pacman)
# ==============================================================================
install_arch() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Arch-familjen (pacman)..."

    # På Arch Linux ingår utvecklingsheaders direkt i huvudpaketen (ingen -dev split):
    #   base-devel -> gcc, make, binutils (strip), sed m.fl.
    #   glibc      -> C-standardbiblioteket
    #   ncurses    -> -lncursesw (<ncurses.h>)
    #   curl       -> -lcurl (<curl/curl.h>)
    #   sqlite     -> -lsqlite3 (<sqlite3.h>)
    local PACKAGES=(
        "base-devel"
        "glibc"
        "ncurses"
        "curl"
        "sqlite"
    )

    # LaTeX i Arch Linux delades upp 2023 (gamla "texlive-core" ersattes av
    # "texlive-basic" + modulära paket som speglar TeX Live-samlingarna):
    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        PACKAGES+=(
            "texlive-basic"
            "texlive-latex"
            "texlive-latexrecommended"
            "texlive-latexextra"
            "texlive-langeuropean"
            "texlive-fontsrecommended"
        )
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via pacman: ${BOLD}${PACKAGES[*]}${RESET}"
    local PAC_FLAGS=("--needed")
    if [ "$AUTO_YES" -eq 1 ]; then
        PAC_FLAGS+=("--noconfirm")
    fi

    run_cmd $SUDO pacman -Syu "${PAC_FLAGS[@]}" "${PACKAGES[@]}"
}

# ==============================================================================
# 4. openSUSE (Tumbleweed / Leap) / SLES (Zypper)
# ==============================================================================
install_opensuse() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för openSUSE/SLES-familjen (zypper)..."
    echo -e "${BLUE}[INFO]${RESET} Uppdaterar paketkällor med zypper refresh..."
    run_cmd $SUDO zypper --non-interactive refresh

    # Baspaket och headers som motsvarar alla flaggor i Makefile:
    #   gcc, make, binutils (strip)
    #   glibc-devel    -> C-headers & grundläggande libc
    #   ncurses-devel  -> -lncursesw (<ncurses.h>)
    #   libcurl-devel  -> -lcurl (<curl/curl.h>)
    #   sqlite3-devel  -> -lsqlite3 (<sqlite3.h>)
    local PACKAGES=(
        "gcc"
        "make"
        "binutils"
        "glibc-devel"
        "ncurses-devel"
        "libcurl-devel"
        "sqlite3-devel"
    )

    # Valfria LaTeX-paket för manual/manual.tex
    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        local LATEX_CANDIDATES=(
            "texlive-latex"
            "texlive-latex-bin"
            "texlive-babel-swedish"
            "texlive-microtype"
            "texlive-parskip"
            "texlive-booktabs"
            "texlive-enumitem"
            "texlive-xcolor"
            "texlive-hyperref"
            "texlive-zapfding"
            "texlive-collection-latex"
            "texlive-collection-latexrecommended"
            "texlive-collection-latexextra"
        )
        if [ "$DRY_RUN" -eq 1 ]; then
            PACKAGES+=("${LATEX_CANDIDATES[@]}")
        else
            for lpkg in "${LATEX_CANDIDATES[@]}"; do
                if zypper_pkg_available "$lpkg"; then
                    PACKAGES+=("$lpkg")
                fi
            done
        fi
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via zypper: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        run_cmd $SUDO zypper --non-interactive install "${PACKAGES[@]}"
    else
        run_cmd $SUDO zypper install "${PACKAGES[@]}"
    fi
}

# ==============================================================================
# 5. Alpine Linux (APK)
# ==============================================================================
install_alpine() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Alpine Linux (apk)..."

    local PACKAGES=(
        "build-base"
        "ncurses-dev"
        "curl-dev"
        "sqlite-dev"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        PACKAGES+=(
            "texlive"
            "texmf-dist-latexextra"
            "texmf-dist-langeuropean"
            "texmf-dist-fontsrecommended"
        )
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via apk: ${BOLD}${PACKAGES[*]}${RESET}"
    run_cmd $SUDO apk update
    run_cmd $SUDO apk add "${PACKAGES[@]}"
}

# ==============================================================================
# 6. Void Linux (XBPS)
# ==============================================================================
install_void() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Void Linux (xbps-install)..."

    local PACKAGES=(
        "base-devel"
        "ncurses-devel"
        "libcurl-devel"
        "sqlite-devel"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Lägger till TeX Live / LaTeX-paket för manual/manual.pdf..."
        PACKAGES+=(
            "texlive-bin"
            "texlive-latexrecommended"
            "texlive-latexextra"
            "texlive-langeuropean"
        )
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via xbps-install: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        run_cmd $SUDO xbps-install -Sy "${PACKAGES[@]}"
    else
        run_cmd $SUDO xbps-install -S "${PACKAGES[@]}"
    fi
}

# ==============================================================================
# Dirigera till rätt installationsfunktion baserat på ID / ID_LIKE / pakethanterare
# ==============================================================================
case "$OS_ID" in
    ubuntu|debian|linuxmint|raspbian|pop|kali|elementary|zorin|neon|mx|devuan|pureos|lmde)
        install_debian_ubuntu
        ;;
    fedora|rhel|centos|rocky|almalinux|ol|nobara|bazzite)
        install_fedora_rhel
        ;;
    arch|manjaro|endeavouros|garuda|artix|cachyos|arcolinux)
        install_arch
        ;;
    opensuse*|sles|sled|tumbleweed|leap)
        install_opensuse
        ;;
    alpine)
        install_alpine
        ;;
    void)
        install_void
        ;;
    *)
        # Sekundär matchning mot ID_LIKE (fångar t.ex. mindre derivat av större distar)
        if [[ "$OS_LIKE" =~ (debian|ubuntu) ]]; then
            install_debian_ubuntu
        elif [[ "$OS_LIKE" =~ (rhel|fedora|centos) ]]; then
            install_fedora_rhel
        elif [[ "$OS_LIKE" =~ (arch) ]]; then
            install_arch
        elif [[ "$OS_LIKE" =~ (suse) ]]; then
            install_opensuse
        elif [[ "$OS_LIKE" =~ (alpine) ]]; then
            install_alpine
        elif [[ "$OS_LIKE" =~ (void) ]]; then
            install_void
        else
            # Sista utväg: identifiera utifrån installerad pakethanterare
            echo -e "${YELLOW}[VARNING]${RESET} Okänd distribution '$OS_ID'. Försöker identifiera pakethanterare..."
            if command -v apt-get >/dev/null 2>&1; then
                install_debian_ubuntu
            elif command -v dnf >/dev/null 2>&1 || command -v yum >/dev/null 2>&1; then
                install_fedora_rhel
            elif command -v pacman >/dev/null 2>&1; then
                install_arch
            elif command -v zypper >/dev/null 2>&1; then
                install_opensuse
            elif command -v apk >/dev/null 2>&1; then
                install_alpine
            elif command -v xbps-install >/dev/null 2>&1; then
                install_void
            else
                echo -e "${RED}[FEL]${RESET} Kunde inte avgöra vilken pakethanterare som används."
                echo "Installera manuellt motsvarande paket för ditt system:"
                echo "  - C-kompilator (gcc), GNU make och binutils (strip)"
                echo "  - ncursesw utveckling (libncurses-dev / ncurses-devel)"
                echo "  - libcurl utveckling (libcurl4-openssl-dev / libcurl-devel)"
                echo "  - SQLite3 utveckling (libsqlite3-dev / sqlite-devel)"
                echo "  - (Valfritt för PDF-manual) TeX Live / pdflatex med babel-swedish"
                exit 1
            fi
        fi
        ;;
esac

# ==============================================================================
# Efterkontroll: Verifiera att verktyg, C-headers och dynamisk länkning fungerar
# ==============================================================================
verify_dependencies() {
    echo ""
    echo -e "${BLUE}----------------------------------------------------------------${RESET}"
    echo -e "${BOLD}   Verifierar byggmiljö mot kraven i Makefile                   ${RESET}"
    echo -e "${BLUE}----------------------------------------------------------------${RESET}"

    local MISSING_TOOLS=0
    local MISSING_HEADERS=0
    local DYN_OK=0

    # 1. Kontrollera kommandoradsverktyg
    for tool in gcc make strip; do
        if command -v "$tool" >/dev/null 2>&1; then
            echo -e "  ${GREEN}[OK]${RESET} Verktyg: $tool ($(command -v "$tool"))"
        else
            echo -e "  ${RED}[SAKNAS]${RESET} Verktyg: $tool"
            MISSING_TOOLS=$((MISSING_TOOLS + 1))
        fi
    done

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        if command -v pdflatex >/dev/null 2>&1; then
            echo -e "  ${GREEN}[OK]${RESET} Verktyg: pdflatex ($(command -v pdflatex))"
        else
            echo -e "  ${YELLOW}[SAKNAS]${RESET} Verktyg: pdflatex (PDF-manualen kan inte byggas)"
        fi
    fi

    # 2. Kontrollera nödvändiga C-headers via kompilatorns faktiska sökvägar
    if command -v gcc >/dev/null 2>&1; then
        local HEADERS=(
            "ncurses.h"
            "curl/curl.h"
            "sqlite3.h"
        )
        for hdr in "${HEADERS[@]}"; do
            if echo "#include <${hdr}>" | gcc -E -x c - >/dev/null 2>&1; then
                echo -e "  ${GREEN}[OK]${RESET} Header:  <${hdr}>"
            else
                echo -e "  ${RED}[SAKNAS]${RESET} Header:  <${hdr}>"
                MISSING_HEADERS=$((MISSING_HEADERS + 1))
            fi
        done

        # 3. Verifiera verklig dynamisk länkning mot delade bibliotek
        local tmp_bin
        tmp_bin="$(mktemp /tmp/ueilogg_verify_XXXXXX)"
        local probe_src='void curl_global_init(long); void *initscr(void); const char *sqlite3_libversion(void); int main(void){ curl_global_init(0); initscr(); sqlite3_libversion(); return 0; }'

        if printf '%s\n' "$probe_src" | \
            gcc -x c - -lncursesw -lcurl -lsqlite3 -lm -pthread -ldl -o "$tmp_bin" >/dev/null 2>&1; then
            DYN_OK=1
            echo -e "  ${GREEN}[OK]${RESET} Länkning: Dynamisk länkning (-lncursesw -lcurl -lsqlite3) fungerar"
        else
            echo -e "  ${RED}[SAKNAS]${RESET} Länkning: Dynamisk länkning (-lncursesw -lcurl -lsqlite3) misslyckades"
        fi
        rm -f "$tmp_bin"
    fi

    echo ""
    if [ "$MISSING_TOOLS" -eq 0 ] && [ "$MISSING_HEADERS" -eq 0 ] && [ "$DYN_OK" -eq 1 ]; then
        echo -e "${GREEN}[OK]${RESET} ${BOLD}Alla byggberoenden är på plats och dynamisk länkning fungerar!${RESET}"
    else
        echo -e "${RED}[VARNING]${RESET} Vissa verktyg, headerfiler eller länkbibliotek saknas fortfarande."
        echo "          Kontrollera utskriften ovan innan du kör 'make'."
    fi
}

if [ "$DRY_RUN" -eq 0 ]; then
    verify_dependencies
else
    echo ""
    echo -e "${CYAN}[DRY-RUN]${RESET} Klar (ingen verifiering kördes eftersom inga paket installerades)."
fi

echo ""
echo "Du kan nu bygga och testa projektet med:"
echo -e "  ${BOLD}make${RESET}              (bygger ueilogg, dxcc, testing och manual)"
echo -e "  ${BOLD}make test${RESET}         (kör den automatiserade testsviten)"
echo -e "  ${BOLD}sudo make install${RESET} (installerar ueilogg, dxcc, dxcc.txt och manual)"
echo -e "  ${BOLD}./ueilogg${RESET}         (startar loggboksprogrammet lokalt)"
echo ""
