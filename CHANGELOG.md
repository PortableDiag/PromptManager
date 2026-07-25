# Changelog

All notable changes to PromptManager will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.3.0] - 2026-07-25

### Added
- Right-click context menu on the folder tree for copying the exact identifiers
  the REST API uses: **Copy Folder Path** on a folder (e.g. `Work/Email`), and
  **Copy Prompt ID** (the UUID for `/api/prompts/{id}`) plus **Copy Folder Path**
  on a prompt. Copies go to both the system clipboard and the X11 primary
  selection, so you can hand agents precise paths and ids
- **Help → About** dialog showing the application name and version

## [2.2.0] - 2026-07-24

### Added
- **REST API** so agents and tools can manage prompts with full parity to the app
  (create, edit, delete, search, browse folders). Configure it in the new
  **Settings → API Server…** dialog: enable the server, choose a configurable port
  (default `8770`), generate/copy an API key, and view a live `curl` example
- Server binds to `127.0.0.1` only and requires the API key on every request
  (`Authorization: Bearer` or `X-API-Key`); a `/health` probe is unauthenticated
- Server settings (enabled/port/key) persist via `QSettings` and the API auto-starts
  on launch when enabled
- `API.md` — full REST API reference for humans and agents to integrate against
- New `Settings` menu in the menu bar

## [2.1.1] - 2026-07-19

### Changed
- Renamed the application and its executable from "Prompt Snippet Manager"
  (`PromptSnippetManager`) to "Prompt Manager" (`PromptManager`) for a
  consistent name across the binary, window title, and desktop entry
- Renamed the Linux desktop entry to `promptmanager.desktop`

### Fixed
- Stopped tracking the `build/` directory in git and added a `.gitignore`

## [2.1.0] - 2026-07-11

### Added
- Application icon: a modern amber squircle mark matching the app's accent colour,
  bundled into the binary as a Qt resource and shown in the window title bar,
  taskbar and app switcher
- Icon shipped at 16-512px plus a Windows `.ico`; the SVG source and the
  Pillow rasteriser (`resources/render_icon.py`) live in `resources/`
- Linux desktop entry (`promptmanager.desktop`) and hicolor icon theme
  install rules, so the app appears properly in application menus
- Collapse/expand all button for the Folders tree
- Toggle button to collapse/expand the prompt list panel
- Prompts are copied to both the system clipboard and the X11 primary selection

### Fixed
- Fixed drag-and-drop crash and made reordering persist across restarts
- Fixed critical bug where prompts would incorrectly become folders when their
  folder path included their own name
- Added automatic data migration to clean up existing prompts with incorrect
  folder paths
- Prevented creation of redundant folders during prompt loading when the last
  path segment matches the prompt title
- Fixed label text colours for dark theme readability

### Changed
- Prompt list is hidden on startup for a cleaner initial view
- Modified prompt loading logic to strip prompt names from folder paths
  automatically
- Enhanced data integrity by adding validation during the loading process

## [2.0.0] - Previous Release

### Added
- Initial implementation of folder tree structure
- Support for organizing prompts in hierarchical folders
- Drag and drop functionality for moving prompts between folders
- Import/Export capabilities for prompts and folders
