# Prompt Manager

A Qt6-based desktop application for organizing and managing text prompts/snippets in a hierarchical folder structure. Perfect for developers, writers, and anyone who needs to organize reusable text snippets.

## Features

### Core Functionality
- **Hierarchical Organization**: Organize prompts in nested folders
- **Drag & Drop**: Move prompts between folders with mouse drag-and-drop
- **Search & Filter**: Quick search across all prompts
- **Folder Search & Sort**: Search the folder tree by folder name or prompt
  title, and auto-sort folders by name (A–Z / Z–A) or recency (Newest / Oldest,
  based on the most recently modified prompt inside). Prompts keep their manual
  drag-and-drop order
- **New Project Scaffolding**: Create a project's **Refresher** + **EOD Summary**
  prompt pair from a type template (Android / Python / Rust / Node.js / C++ /
  Generic) in one step, via **File → New Project…** or the folder tree's
  **New Project Here…** right-click action
- **Import/Export**: Support for JSON and CSV formats
- **Auto-Save**: Changes are automatically persisted
- **Keyboard Shortcuts**: Efficient workflow with keyboard shortcuts
- **REST API**: Let agents and tools manage prompts with full parity to the app —
  create, edit, delete, and search over a local HTTP API. Enable it in
  **Settings → API Server…** (configurable port, generated API key, live `curl`
  example). Request bodies are validated: an unknown field, a wrong-typed value,
  or a body that isn't a JSON object all return a `400` naming the offender, so
  a malformed call can never look like a successful write. Destructive folder
  deletes require an explicit confirmation and otherwise run as a dry run that
  reports what would be lost. See [API.md](API.md) for the full reference.

### Data Management
- **Atomic Saves**: Safe file writing with automatic backups
- **Data Validation**: Built-in diagnostic tools to check data integrity
- **Cross-Platform**: Works on Linux, macOS, and Windows

## Building from Source

### Prerequisites
- Qt6 (6.2 or later)
- CMake (3.16 or later)
- C++17 compatible compiler

---

### Linux

#### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install qt6-base-dev cmake build-essential
```

**Fedora:**
```bash
sudo dnf install qt6-qtbase-devel cmake gcc-c++
```

**Arch Linux:**
```bash
sudo pacman -S qt6-base cmake base-devel
```

#### Build & Run
```bash
git clone <repository-url>
cd PromptManager_v2
mkdir build && cd build
cmake ..
make -j$(nproc)
./PromptManager
```

---

### macOS

#### Install Dependencies

**Using Homebrew (recommended):**
```bash
brew install qt@6 cmake
```

You may need to add Qt to your PATH:
```bash
echo 'export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**Using Qt Installer:**
Download and install from [qt.io](https://www.qt.io/download-qt-installer)

#### Build & Run
```bash
git clone <repository-url>
cd PromptManager_v2
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
./PromptManager
```

To create a macOS app bundle:
```bash
macdeployqt PromptManager.app
```

---

### Windows

#### Install Dependencies

**Option 1: Qt Online Installer (recommended)**
1. Download the Qt installer from [qt.io](https://www.qt.io/download-qt-installer)
2. Install Qt 6.2+ with the MSVC or MinGW toolchain
3. Install CMake from [cmake.org](https://cmake.org/download/) or via Qt

**Option 2: Using vcpkg**
```powershell
vcpkg install qt6-base:x64-windows
```

**Option 3: Using MSYS2/MinGW**
```bash
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
```

#### Build & Run

**Using Qt Creator (easiest):**
1. Open `CMakeLists.txt` in Qt Creator
2. Configure the project with your Qt kit
3. Build and run from the IDE

**Using Command Line (MSVC):**
```powershell
# Open "Developer Command Prompt for VS" or "x64 Native Tools Command Prompt"
git clone <repository-url>
cd PromptManager_v2
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
.\Release\PromptManager.exe
```

**Using Command Line (MinGW):**
```bash
git clone <repository-url>
cd PromptManager_v2
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j4
./PromptManager.exe
```

#### Deploying on Windows
To create a standalone executable with all required DLLs:
```powershell
windeployqt PromptManager.exe
```

---

## Usage

### Basic Operations

1. **Creating Prompts**: Click "New Prompt" or press Ctrl+N
2. **Creating Folders**: Click "New Folder" button in the folder panel
3. **Organizing**: Drag prompts to folders to organize them
4. **Editing**: Select a prompt to edit its title and content
5. **Saving**: Changes are saved automatically, or press Ctrl+S to save manually

### Keyboard Shortcuts

- `Ctrl+N` - New prompt
- `Ctrl+S` - Save all changes
- `Ctrl+O` - Import prompts
- `Ctrl+E` - Export prompts
- `Ctrl+C` - Copy prompt to clipboard
- `Delete` - Delete selected item
- `Ctrl+D` - Run diagnostic check

### Import/Export

#### Exporting
- **JSON Format**: Preserves all metadata including folder structure
- **CSV Format**: Compatible with spreadsheet applications

#### Importing
- **Merge Mode**: Add imported prompts to existing collection
- **Replace Mode**: Replace entire collection with imported data
- **Folder Import**: Import directly into a specific folder

## File Structure

- **Data File**: `~/.prompt_snippets_v2.json`
- **Backup File**: `~/.prompt_snippets_v2.json.bak` (created automatically)

## Architecture

### Components
- **MainWindow**: Central UI and application logic
- **FolderTreeModel**: Qt model for hierarchical folder/prompt structure
- **FolderTreeItem**: Data structure for tree nodes
- **Prompt**: Core data structure for prompt storage

### Key Improvements
- Proper drag-and-drop implementation preserving item types
- Atomic file operations preventing data corruption
- Comprehensive error handling
- Memory leak prevention
- Built-in diagnostic tools

## Troubleshooting

### Diagnostic Tool
Use `File → Diagnostic Check` (Ctrl+D) to:
- View complete folder tree structure
- Check for orphaned prompts
- Detect duplicate IDs
- Analyze selected items

### Common Issues

1. **Cannot Delete Item**: Run diagnostic check to identify if it's a corrupted item
2. **Lost Prompts**: Check the backup file (`.bak`) for recovery
3. **Drag-Drop Not Working**: Ensure you're dragging to a folder (not another prompt)

## Development

### Code Structure
```
PromptManager_v2/
├── CMakeLists.txt          # Build configuration
├── main.cpp                # Application entry point
├── mainwindow.cpp/h        # Main UI and business logic
├── foldertreemodel.cpp/h   # Tree model implementation
├── foldertreeitem.h        # Tree item data structure
└── resources/
    ├── icon.svg            # Vector source for the app icon
    ├── render_icon.py      # Rasterises icon.svg to the PNG/ICO sizes
    ├── resources.qrc       # Qt resource bundle (icons compiled into the binary)
    ├── icons/              # Generated PNGs (16-512px) and app.ico
    └── promptmanager.desktop   # Linux desktop entry
```

### Application Icon

The icon is an amber squircle carrying a prompt chevron and snippet lines, using
the same accent colour as the app's UI. `resources/icon.svg` is the source of
truth; the PNGs and the Windows `.ico` are generated from it.

To regenerate after editing the SVG (requires Pillow):
```bash
python3 resources/render_icon.py
```
This rewrites `resources/icons/`. The PNGs are compiled into the executable via
`resources.qrc`, so a rebuild is all that's needed to pick up the change. On
Linux, `make install` also installs the desktop entry and the hicolor theme
icons so the app shows up in application menus.

### Recent Fixes
- Fixed prompt-to-folder conversion bug in drag-and-drop
- Implemented automatic saving after drag-drop operations
- Added atomic file writing with backup creation
- Fixed memory leaks in model replacement
- Added comprehensive diagnostic tools
- Improved CSV parsing boundary checks

## License

This project is provided as-is for educational and personal use.

## Contributing

Feel free to submit issues and enhancement requests!