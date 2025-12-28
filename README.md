# Prompt Snippet Manager

A Qt6-based desktop application for organizing and managing text prompts/snippets in a hierarchical folder structure. Perfect for developers, writers, and anyone who needs to organize reusable text snippets.

## Features

### Core Functionality
- **Hierarchical Organization**: Organize prompts in nested folders
- **Drag & Drop**: Move prompts between folders with mouse drag-and-drop
- **Search & Filter**: Quick search across all prompts
- **Import/Export**: Support for JSON and CSV formats
- **Auto-Save**: Changes are automatically persisted
- **Keyboard Shortcuts**: Efficient workflow with keyboard shortcuts

### Data Management
- **Atomic Saves**: Safe file writing with automatic backups
- **Data Validation**: Built-in diagnostic tools to check data integrity
- **Cross-Platform**: Works on Linux, macOS, and Windows

## Building from Source

### Prerequisites
- Qt6 (6.2 or later)
- CMake (3.16 or later)
- C++17 compatible compiler

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd PromptManager_v2

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make -j4

# Run the application
./PromptSnippetManager
```

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
└── foldertreeitem.h        # Tree item data structure
```

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