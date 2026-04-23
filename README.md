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

## Requirements

- GCC (or any C compiler)
- libcurl development headers

### Install dependencies

```bash
# Ubuntu / Debian
sudo apt-get install build-essential libcurl4-openssl-dev

# macOS (Homebrew)
brew install curl

# Arch Linux
sudo pacman -S curl

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-curl
```

## Build

```bash
make
```

## Usage

### Direct URL (recommended)

```bash
./ccdes https://example.com
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
