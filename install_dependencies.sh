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
#          samt grundläggande C-bibliotek inkl. statiska arkiv för "-static".
#        - dpkg-deb / dpkg (för "make deb" på Debian/Ubuntu-baserade system).
#   2. Huvudprogrammet (ueilogg) och testsviten (testing):
#        LDFLAGS = -static -lncursesw -ltinfo $(CURL_LIBS) -lsqlite3 -lm -pthread -ldl
#        - ncurses med bredteckenstöd (UTF-8): -lncursesw -ltinfo (<ncurses.h>)
#        - SQLite3-databas:                   -lsqlite3 (<sqlite3.h>)
#        - Standardmatematik, trådar, dl:     -lm -pthread -ldl (från glibc/musl)
#   3. CURL_LIBS (används av både ueilogg, testing och hjälpverktyget dxcc):
#        CURL_LIBS = -Llib -Wl,--start-group -lcurl -lssl -lcrypto -lz -lbrotlidec -lbrotlicommon -Wl,--end-group
#        - libcurl headers (<curl/curl.h>) + projektets medföljande lib/libcurl.a
#        - OpenSSL (TLS/HTTPS för QRZ.COM):   -lssl -lcrypto (<openssl/ssl.h>)
#        - zlib (HTTP-komprimering):          -lz (<zlib.h>)
#        - Brotli (HTTP-komprimering):        -lbrotlidec -lbrotlicommon (<brotli/decode.h>)
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
    #   build-essential = gcc, g++, make, libc6-dev (libc.a/libm.a/libpthread.a/libdl.a),
    #                     binutils (inkl. strip), dpkg-dev (för "make deb")
    local PACKAGES=(
        "build-essential"
        "dpkg-dev"
    )

    # ncursesw + tinfo (-lncursesw -ltinfo):
    #   Nyare Debian/Ubuntu använder "libncurses-dev" (som inkluderar bredtecken-
    #   stöd och statiska libncursesw.a + libtinfo.a). Äldre utgåvor använder
    #   "libncursesw5-dev". Vi väljer det paket som finns tillgängligt.
    if [ "$DRY_RUN" -eq 1 ] || apt_pkg_exists "libncurses-dev"; then
        PACKAGES+=("libncurses-dev")
    else
        PACKAGES+=("libncursesw5-dev")
    fi

    # Bibliotek för nätverk, TLS, komprimering och databas enligt Makefile:
    #   libcurl4-openssl-dev -> <curl/curl.h> för lib/libcurl.a
    #   libssl-dev           -> -lssl -lcrypto (OpenSSL headers + libssl.a/libcrypto.a)
    #   zlib1g-dev           -> -lz (zlib headers + libz.a)
    #   libbrotli-dev        -> -lbrotlidec -lbrotlicommon (Brotli headers + .a-arkiv)
    #   libsqlite3-dev       -> -lsqlite3 (SQLite3 headers + libsqlite3.a)
    PACKAGES+=(
        "libcurl4-openssl-dev"
        "libssl-dev"
        "zlib1g-dev"
        "libbrotli-dev"
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

    # På RHEL, Rocky Linux, AlmaLinux och CentOS ligger många utvecklings- och
    # -static-paket i CRB (CodeReady Linux Builder, kallat PowerTools i EL8).
    # Vi försöker aktivera CRB/PowerTools automatiskt innan vi söker efter paket.
    local RHEL_MAJOR="${OS_VERSION_ID%%.*}"
    local HOST_ARCH
    HOST_ARCH="$(uname -m)"
    if [ "$OS_ID" != "fedora" ] && [ "$OS_ID" != "nobara" ] && [ "$OS_ID" != "bazzite" ]; then
        echo -e "${BLUE}[INFO]${RESET} Kontrollerar/aktiverar CRB/PowerTools-förråd om tillgängligt..."
        if [ -x /usr/bin/crb ]; then
            run_cmd $SUDO /usr/bin/crb enable >/dev/null 2>&1 || true
        elif [ "$OS_ID" = "rhel" ] && command -v subscription-manager >/dev/null 2>&1 && [ -n "$RHEL_MAJOR" ]; then
            run_cmd $SUDO subscription-manager repos --enable "codeready-builder-for-rhel-${RHEL_MAJOR}-${HOST_ARCH}-rpms" >/dev/null 2>&1 || true
        elif [ "$DRY_RUN" -eq 0 ] && "$PKG_MGR" config-manager --help >/dev/null 2>&1; then
            run_cmd $SUDO "$PKG_MGR" config-manager --set-enabled crb >/dev/null 2>&1 || \
            run_cmd $SUDO "$PKG_MGR" config-manager --set-enabled powertools >/dev/null 2>&1 || true
        fi
    fi

    # Grundläggande byggverktyg & utvecklingsheaders enligt Makefile:
    #   gcc, make, binutils (strip)
    #   glibc-devel    -> standard C-headers
    #   ncurses-devel  -> <ncurses.h> samt dynamiska ncursesw/tinfo
    #   libcurl-devel  -> <curl/curl.h> samt delat libcurl.so
    #   openssl-devel  -> -lssl -lcrypto headers & bibliotek
    #   brotli-devel   -> -lbrotlidec -lbrotlicommon headers & bibliotek
    #   sqlite-devel   -> -lsqlite3 headers & bibliotek
    local PACKAGES=(
        "gcc"
        "make"
        "binutils"
        "glibc-devel"
        "ncurses-devel"
        "libcurl-devel"
        "openssl-devel"
        "brotli-devel"
        "sqlite-devel"
    )

    # Fedora 40+ ersatte zlib-devel med zlib-ng-compat-devel (med zlib-devel som alias,
    # men vi väljer snyggt det paket som finns).
    if [ "$DRY_RUN" -eq 1 ] || rpm_pkg_available "$PKG_MGR" "zlib-devel"; then
        PACKAGES+=("zlib-devel")
    elif rpm_pkg_available "$PKG_MGR" "zlib-ng-compat-devel"; then
        PACKAGES+=("zlib-ng-compat-devel")
    fi

    # Statiska bibliotek för "-static" i Makefile:
    #   RHEL 9+ och Fedora skeppar inte längre openssl-static (libssl.a/libcrypto.a),
    #   brotli-static (libbrotlidec.a/libbrotlicommon.a) eller sqlite-static i sina
    #   officiella förråd av säkerhets- och FIPS-skäl.
    #   Vi installerar de -static-paket som finns (t.ex. på RHEL 8 med CRB), och
    #   när några saknas växlar Makefile automatiskt till dynamisk länkning mot
    #   systemets egna .so-bibliotek (libcurl, libssl, libbrotli, sqlite, ncursesw).
    local STATIC_CANDIDATES=(
        "glibc-static"
        "ncurses-static"
        "openssl-static"
        "zlib-static"
        "zlib-ng-compat-static"
        "brotli-static"
        "sqlite-static"
    )
    local MISSING_STATIC_RPM=()

    if [ "$DRY_RUN" -eq 1 ]; then
        PACKAGES+=("glibc-static" "ncurses-static" "openssl-static" "zlib-static")
    else
        echo -e "${BLUE}[INFO]${RESET} Kontrollerar tillgängliga statiska (-static) bibliotek i aktiva förråd..."
        for spkg in "${STATIC_CANDIDATES[@]}"; do
            if rpm_pkg_available "$PKG_MGR" "$spkg"; then
                PACKAGES+=("$spkg")
            else
                # zlib-static och zlib-ng-compat-static ersätter varandra
                if [ "$spkg" != "zlib-ng-compat-static" ]; then
                    MISSING_STATIC_RPM+=("$spkg")
                fi
            fi
        done

        if [ ${#MISSING_STATIC_RPM[@]} -gt 0 ]; then
            echo -e "${YELLOW}[INFO]${RESET} Följande -static-paket finns inte i aktiva RPM-förråd: ${MISSING_STATIC_RPM[*]}"
            echo -e "       Detta är normalt på RHEL 9+/Fedora (Red Hat levererar t.ex. inte"
            echo -e "       brotli-static eller openssl-static som RPM). Makefile känner av detta"
            echo -e "       automatiskt och länkar istället dynamiskt mot systemets delade bibliotek."
        fi
    fi

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
    #   glibc      -> C-standardbiblioteket (libc.a, libm.a, libpthread.a, libdl.a)
    #   ncurses    -> -lncursesw -ltinfo (<ncurses.h>)
    #   curl       -> <curl/curl.h>
    #   openssl    -> -lssl -lcrypto
    #   zlib       -> -lz
    #   brotli     -> -lbrotlidec -lbrotlicommon
    #   sqlite     -> -lsqlite3
    local PACKAGES=(
        "base-devel"
        "glibc"
        "ncurses"
        "curl"
        "openssl"
        "zlib"
        "brotli"
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

    echo -e "${BLUE}[INFO]${RESET} Arch Linux levererar officiella bibliotekspaket utan statiska .a-filer (!staticlibs)."
    echo -e "       Makefile känner av detta automatiskt och länkar istället dynamiskt"
    echo -e "       mot systemets delade bibliotek (-lcurl, -lncursesw, -lsqlite3)."
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
    #   glibc-devel       -> C-headers & grundläggande libc
    #   ncurses-devel     -> <ncurses.h> samt ncursesw/tinfo
    #   libcurl-devel     -> <curl/curl.h> samt libcurl.so
    #   libopenssl-devel  -> -lssl -lcrypto headers & bibliotek
    #   zlib-devel        -> -lz headers & bibliotek
    #   libbrotli-devel   -> -lbrotlidec -lbrotlicommon headers & bibliotek
    #   sqlite3-devel     -> -lsqlite3 headers & bibliotek
    local PACKAGES=(
        "gcc"
        "make"
        "binutils"
        "glibc-devel"
        "ncurses-devel"
        "libcurl-devel"
        "libopenssl-devel"
        "zlib-devel"
        "libbrotli-devel"
        "sqlite3-devel"
    )

    # Kontrollera vilka statiska (-static) RPM-paket som finns i aktiva förråd
    local OPTIONAL_STATIC=(
        "glibc-devel-static"
        "zlib-devel-static"
        "ncurses-devel-static"
        "libopenssl-devel-static"
        "sqlite3-devel-static"
        "libbrotli-devel-static"
    )
    if [ "$DRY_RUN" -eq 1 ]; then
        PACKAGES+=("glibc-devel-static" "zlib-devel-static")
    else
        for spkg in "${OPTIONAL_STATIC[@]}"; do
            if zypper_pkg_available "$spkg"; then
                PACKAGES+=("$spkg")
            fi
        done
    fi

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
# 5. Alpine Linux (APK) – utmärkt stöd för kompletta statiska musl-byggen
# ==============================================================================
install_alpine() {
    echo -e "\n${BLUE}[INFO]${RESET} Förbereder installation för Alpine Linux (apk)..."

    # Alpine separerar utvecklingsheaders (-dev) och statiska arkiv (-static):
    #   build-base          -> gcc, make, musl-dev (libc.a), binutils (strip)
    #   ncurses-dev/static  -> -lncursesw -ltinfo (headers + .a)
    #   curl-dev            -> <curl/curl.h>
    #   openssl-dev         -> OpenSSL headers
    #   openssl-libs-static -> libssl.a, libcrypto.a
    #   zlib-dev/static     -> -lz (headers + libz.a)
    #   brotli-dev/static   -> -lbrotlidec -lbrotlicommon (headers + .a)
    #   sqlite-dev/static   -> -lsqlite3 (headers + libsqlite3.a)
    local PACKAGES=(
        "build-base"
        "ncurses-dev"
        "ncurses-static"
        "curl-dev"
        "openssl-dev"
        "openssl-libs-static"
        "zlib-dev"
        "zlib-static"
        "brotli-dev"
        "brotli-static"
        "sqlite-dev"
        "sqlite-static"
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
        "openssl-devel"
        "zlib-devel"
        "brotli-devel"
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
                echo "  - Statiskt C-bibliotek (libc.a / glibc-static) för -static"
                echo "  - ncursesw & tinfo utveckling + statiska arkiv (libncurses-dev / ncurses-devel)"
                echo "  - libcurl utveckling (<curl/curl.h>)"
                echo "  - OpenSSL utveckling + statiska arkiv (libssl-dev / openssl-devel)"
                echo "  - zlib utveckling + statiska arkiv (zlib1g-dev / zlib-devel)"
                echo "  - Brotli utveckling + statiska arkiv (libbrotli-dev / brotli-devel)"
                echo "  - SQLite3 utveckling + statiska arkiv (libsqlite3-dev / sqlite-devel)"
                echo "  - (Valfritt för PDF-manual) TeX Live / pdflatex med babel-swedish"
                exit 1
            fi
        fi
        ;;
esac

# ==============================================================================
# Efterkontroll: Verifiera att verktyg, C-headers och statiska bibliotek finns
# ==============================================================================
verify_dependencies() {
    echo ""
    echo -e "${BLUE}----------------------------------------------------------------${RESET}"
    echo -e "${BOLD}   Verifierar byggmiljö mot kraven i Makefile                   ${RESET}"
    echo -e "${BLUE}----------------------------------------------------------------${RESET}"

    local MISSING_TOOLS=0
    local MISSING_HEADERS=0
    local MISSING_STATIC=0

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
            "openssl/ssl.h"
            "zlib.h"
            "brotli/decode.h"
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

        # 3. Kontrollera projektets medföljande statiska libcurl samt systemets .a-arkiv
        if [ -f "${SCRIPT_DIR}/lib/libcurl.a" ]; then
            echo -e "  ${GREEN}[OK]${RESET} Statisk: lib/libcurl.a (i projektet)"
        else
            echo -e "  ${YELLOW}[VARNING]${RESET} Statisk: ${SCRIPT_DIR}/lib/libcurl.a saknas"
        fi

        local STATIC_LIBS=(
            "libc.a"
            "libm.a"
            "libncursesw.a"
            "libtinfo.a"
            "libssl.a"
            "libcrypto.a"
            "libz.a"
            "libbrotlidec.a"
            "libbrotlicommon.a"
            "libsqlite3.a"
        )
        for slib in "${STATIC_LIBS[@]}"; do
            local resolved
            resolved="$(gcc -print-file-name="$slib" 2>/dev/null || true)"
            if [ -n "$resolved" ] && [ "$resolved" != "$slib" ] && [ -e "$resolved" ]; then
                echo -e "  ${GREEN}[OK]${RESET} Statisk: ${slib}"
            else
                echo -e "  ${YELLOW}[INFO]${RESET} Statisk: ${slib} saknas (ej tillgänglig som .a på denna plattform)"
                MISSING_STATIC=$((MISSING_STATIC + 1))
            fi
        done

        # 4. Verifiera verklig länkning (både statiskt och dynamiskt) med symboler från curl/ncursesw/sqlite3
        local DYN_OK=0
        local STATIC_LINK_OK=0
        local tmp_bin
        tmp_bin="$(mktemp /tmp/ueilogg_verify_XXXXXX)"
        local probe_src='void curl_global_init(long); void *initscr(void); const char *sqlite3_libversion(void); int main(void){ curl_global_init(0); initscr(); sqlite3_libversion(); return 0; }'

        if printf '%s\n' "$probe_src" | \
            gcc -x c - -static -lncursesw -ltinfo -L"${SCRIPT_DIR}/lib" -Wl,--start-group -lcurl -lssl -lcrypto -lz -lbrotlidec -lbrotlicommon -Wl,--end-group -lsqlite3 -lm -pthread -ldl -o "$tmp_bin" >/dev/null 2>&1; then
            STATIC_LINK_OK=1
            echo -e "  ${GREEN}[OK]${RESET} Länkning: Statisk länkning (-static med lib/libcurl.a) fungerar"
        else
            echo -e "  ${YELLOW}[INFO]${RESET} Länkning: Statisk länkning ej tillgänglig här (saknade .a eller annan libc/ABI)"
        fi

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
    if [ "$MISSING_TOOLS" -eq 0 ] && [ "$MISSING_HEADERS" -eq 0 ] && [ "${STATIC_LINK_OK:-0}" -eq 1 ]; then
        echo -e "${GREEN}[OK]${RESET} ${BOLD}Alla byggberoenden och statiska bibliotek är på plats! (Statisk länkning aktiv)${RESET}"
    elif [ "$MISSING_TOOLS" -eq 0 ] && [ "$MISSING_HEADERS" -eq 0 ] && [ "${DYN_OK:-0}" -eq 1 ]; then
        echo -e "${GREEN}[OK]${RESET} ${BOLD}Alla byggberoenden är på plats!${RESET}"
        echo -e "${BLUE}[INFO]${RESET} Statisk hel-länkning är inte tillgänglig på denna plattform,"
        echo "       så Makefile växlar automatiskt till dynamisk länkning mot systemets bibliotek."
    else
        echo -e "${RED}[VARNING]${RESET} Vissa verktyg, headerfiler eller länkningsbibliotek saknas fortfarande."
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
