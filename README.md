# CCDES - Next.js Site Decompiler

A command-line tool written in C that downloads and reconstructs Next.js site builds for security analysis.

## Features

- **Full asset discovery**: Parses HTML to find all `_next/static/` assets (JS, CSS, fonts, media)
- **App Router & Pages Router**: Supports both Next.js routing modes
- **CDN prefix detection**: Handles custom asset prefixes and CDN paths
- **Build ID extraction**: Detects build IDs from multiple sources
- **Route reconstruction**: Extracts route structure from chunk filenames
- **JS beautification**: De-minifies downloaded JavaScript files
- **Source map detection**: Checks for `.map` files to recover original source
- **Build manifest**: Downloads `_buildManifest.js` for full route mapping
- **Project skeleton**: Generates `package.json`, `next.config.js`, and analysis report
- **Cross-platform**: Works on Linux, macOS, and Windows

## Requirements

- GCC (or any C compiler)
- libcurl development headers

---

## Build Instructions

### Linux (Ubuntu / Debian)

```bash
sudo apt-get install build-essential libcurl4-openssl-dev
make
```

### macOS (Homebrew)

```bash
brew install curl
make
```

### Arch Linux

```bash
sudo pacman -S base-devel curl
make
```

### Windows (MSYS2 — recommended)

**Step 1: Install MSYS2**

Download and install from [https://www.msys2.org/](https://www.msys2.org/)

**Step 2: Open the MSYS2 MinGW64 terminal** (NOT the MSYS2 MSYS terminal)

From the Start menu, open **"MSYS2 MinGW x64"**.

**Step 3: Install the toolchain and libcurl**

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl make
```

**Step 4: Build**

```bash
cd /c/path/to/ccdes
make
```

Or use the provided build script:

```bash
./build-msys2.sh
```

Or from CMD / PowerShell (after adding `C:\msys64\mingw64\bin` to your PATH):

```cmd
build-windows.bat
```

The result is a **ccdes.exe** that you can run from any terminal.

### Windows (vcpkg + MSVC)

If you prefer Visual Studio:

```powershell
vcpkg install curl:x64-windows
cl /O2 /Fe:ccdes.exe main.c download.c parser.c reconstruct.c /I<vcpkg-include> /link /LIBPATH:<vcpkg-lib> libcurl.lib ws2_32.lib
```

---

## Usage

### Direct URL (recommended)

```bash
# Linux / macOS
./ccdes https://example.com

# Windows
ccdes.exe https://example.com
```

### Interactive mode

```bash
./ccdes
```

### Analyse a local file

```bash
./ccdes --file bundle.js
```

### Offline test mode

```bash
./ccdes --test
```

## Output Structure

After decompiling a site, CCDES creates the following directory structure:

```
output/<domain>/
├── raw/                     # Original downloaded files
│   ├── index.html           # Main HTML page
│   ├── __NEXT_DATA__.json   # Page data (Pages Router only)
│   └── _next/static/
│       ├── chunks/          # JavaScript chunks (original)
│       ├── css/             # Stylesheets
│       └── media/           # Fonts, images
├── reconstructed/           # Rebuilt project skeleton
│   ├── app/ or pages/       # Route components (beautified JS)
│   ├── chunks/              # Shared chunks (beautified)
│   ├── styles/              # CSS files
│   ├── package.json         # Generated
│   └── next.config.js       # Generated
├── sourcemaps/              # Source maps (if found)
└── report.md                # Full analysis report
```

## How It Works

1. **Download** the target site's HTML
2. **Parse** the HTML to extract all `_next/static/` asset URLs
3. **Detect** the build configuration (build ID, asset prefix, router type)
4. **Extract** route information from chunk filenames
5. **Download** all JS, CSS, font, and media assets
6. **Search** for source maps (`.map` files)
7. **Beautify** minified JavaScript files
8. **Reconstruct** a project directory with organized files
9. **Generate** analysis report with findings

## Disclaimer

This tool is intended for **security testing and educational purposes only**. Always ensure you have proper authorization before analysing any website. Respect the terms of service of the websites you analyse.

## License

MIT
