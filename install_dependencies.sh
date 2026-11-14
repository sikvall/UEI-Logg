#!/usr/bin/env bash
# ==============================================================================
# install_dependencies.sh - Installera nödvändiga utvecklingspaket för UEI-Logg
# ==============================================================================

set -e

# Färger för terminalutskrift
GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
BLUE="\033[1;34m"
BOLD="\033[1m"
RESET="\033[0m"

INSTALL_LATEX=0
AUTO_YES=0

print_usage() {
    echo -e "${BOLD}Användning:${RESET} $0 [alternativ]"
    echo ""
    echo "Hjälper dig att installera alla förutsättningar och utvecklingspaket"
    echo "som krävs för att kompilera och bygga UEI-Logg på Linux."
    echo ""
    echo "Alternativ:"
    echo "  -y, --yes          Installera automatiskt utan att fråga (non-interactive)"
    echo "  --latex, --all     Installera även TeX Live / LaTeX för att bygga PDF-manualen"
    echo "  -h, --help         Visa denna hjälpinformation"
    echo ""
}

# Behandla argument
for arg in "$@"; do
    case "$arg" in
        -y|--yes)
            AUTO_YES=1
            ;;
        --latex|--all|--with-latex)
            INSTALL_LATEX=1
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

echo -e "${BLUE}====================================================${RESET}"
echo -e "${BOLD}   UEI-Logg: Installation av utvecklingspaket       ${RESET}"
echo -e "${BLUE}====================================================${RESET}"

# Identifiera distribution via /etc/os-release
OS_ID=""
OS_LIKE=""
OS_NAME="Linux"
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS_ID="${ID:-}"
    OS_LIKE="${ID_LIKE:-}"
    OS_NAME="${NAME:-Linux}"
fi

echo -e "${BLUE}[INFO]${RESET} Identifierat operativsystem: ${BOLD}${OS_NAME}${RESET} (${OS_ID})"

# Kontrollera sudo / root
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo -e "${RED}[FEL]${RESET} Detta script kräver administratörsrättigheter (root eller sudo)."
        exit 1
    fi
fi

# Fråga interaktivt om LaTeX om flagga inte angetts och stdin är en terminal
if [ "$INSTALL_LATEX" -eq 0 ] && [ "$AUTO_YES" -eq 0 ] && [ -t 0 ]; then
    echo ""
    read -r -p "Vill du även installera paket för att bygga PDF-manualen (LaTeX/TeX Live)? [j/N]: " answer
    case "$answer" in
        [jJyY]*)
            INSTALL_LATEX=1
            ;;
        *)
            INSTALL_LATEX=0
            ;;
    esac
fi

install_debian_ubuntu() {
    echo -e "\n${BLUE}[INFO]${RESET} Uppdaterar paketlistor med apt..."
    $SUDO apt-get update

    PACKAGES=(
        "build-essential"
        "libncursesw5-dev"
        "libcurl4-openssl-dev"
        "libsqlite3-dev"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        echo -e "${BLUE}[INFO]${RESET} Inkluderar LaTeX-paket för användarmanualen..."
        PACKAGES+=(
            "texlive-latex-base"
            "texlive-latex-extra"
            "texlive-lang-european"
        )
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar paket: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        $SUDO apt-get install -y "${PACKAGES[@]}"
    else
        $SUDO apt-get install "${PACKAGES[@]}"
    fi
}

install_fedora_rhel() {
    PACKAGES=(
        "gcc"
        "make"
        "ncurses-devel"
        "libcurl-devel"
        "sqlite-devel"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        PACKAGES+=("texlive-scheme-basic" "texlive-collection-latex")
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via dnf: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        $SUDO dnf install -y "${PACKAGES[@]}"
    else
        $SUDO dnf install "${PACKAGES[@]}"
    fi
}

install_arch() {
    PACKAGES=(
        "base-devel"
        "ncurses"
        "curl"
        "sqlite"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        PACKAGES+=("texlive-core")
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via pacman: ${BOLD}${PACKAGES[*]}${RESET}"
    local PAC_ARGS="--needed"
    if [ "$AUTO_YES" -eq 1 ]; then
        PAC_ARGS="$PAC_ARGS --noconfirm"
    fi
    $SUDO pacman -Syu $PAC_ARGS "${PACKAGES[@]}"
}

install_opensuse() {
    PACKAGES=(
        "gcc"
        "make"
        "ncurses-devel"
        "libcurl-devel"
        "sqlite3-devel"
    )

    if [ "$INSTALL_LATEX" -eq 1 ]; then
        PACKAGES+=("texlive-latex")
    fi

    echo -e "${BLUE}[INFO]${RESET} Installerar via zypper: ${BOLD}${PACKAGES[*]}${RESET}"
    if [ "$AUTO_YES" -eq 1 ]; then
        $SUDO zypper install -y "${PACKAGES[@]}"
    else
        $SUDO zypper install "${PACKAGES[@]}"
    fi
}

case "$OS_ID" in
    ubuntu|debian|linuxmint|raspbian|pop)
        install_debian_ubuntu
        ;;
    fedora|rhel|centos|rocky|almalinux)
        install_fedora_rhel
        ;;
    arch|manjaro|endeavouros)
        install_arch
        ;;
    opensuse*|sles)
        install_opensuse
        ;;
    *)
        if [[ "$OS_LIKE" =~ (debian|ubuntu) ]]; then
            install_debian_ubuntu
        elif [[ "$OS_LIKE" =~ (rhel|fedora|centos) ]]; then
            install_fedora_rhel
        elif [[ "$OS_LIKE" =~ (arch) ]]; then
            install_arch
        elif [[ "$OS_LIKE" =~ (suse) ]]; then
            install_opensuse
        else
            echo -e "${YELLOW}[VARNING]${RESET} Okänd distribution '$OS_ID'. Försöker med apt..."
            if command -v apt-get >/dev/null 2>&1; then
                install_debian_ubuntu
            elif command -v dnf >/dev/null 2>&1; then
                install_fedora_rhel
            elif command -v pacman >/dev/null 2>&1; then
                install_arch
            else
                echo -e "${RED}[FEL]${RESET} Kunde inte avgöra vilken pakethanterare som används."
                echo "Installera manuellt följande paket:"
                echo "  - C-kompilator (gcc) och make"
                echo "  - libncursesw5-dev"
                echo "  - libcurl4-openssl-dev"
                echo "  - libsqlite3-dev"
                exit 1
            fi
        fi
        ;;
esac

echo ""
echo -e "${GREEN}[OK]${RESET} ${BOLD}Alla utvecklingspaket är installerade och klara!${RESET}"
echo ""
echo "Du kan nu bygga projektet med:"
echo -e "  ${BOLD}make${RESET}         (bygger ueilog, dxcc, testing och manual)"
echo -e "  ${BOLD}make test${RESET}    (kör automatiserade enhetstester)"
echo -e "  ${BOLD}./ueilog${RESET}     (startar loggboksprogrammet)"
echo ""
