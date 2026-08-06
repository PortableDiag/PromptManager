# Changelog

All notable changes to PromptManager will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.5.2] - 2026-08-05

### Fixed
- **A wrong-typed field silently blanked a prompt.** v2.5.1 caught misnamed
  fields but not misvalued ones: `{"body": 123}`, `{"body": null}` or
  `{"body": ["x"]}` passed validation, reached a string conversion that yields
  an empty string, and **erased the prompt's text while returning `200`**. That
  was worse than the bug v2.5.1 fixed — the earlier one lost a write, this one
  destroyed content. Every field the API accepts is a JSON string, and a
  non-string value is now a `400` naming the field and the type it got
  (`Field 'body' must be a string, not a number`)
- **A body that was valid JSON but not an object was silently ignored.** Sending
  an array (`[{"body":"x"}]`) parsed successfully, produced an empty field set,
  and returned `200` having changed nothing — the same silent-success shape.
  Now a `400` (`Request body must be a JSON object`)

### Documented
- `GET /prompts?folder=` naming a folder with no prompts returns `{"count": 0}`
  rather than `404`, and that is deliberate: folders are derived from prompt
  `folderPath`s, so an empty folder and a nonexistent one are indistinguishable
  by construction. API.md now says so instead of leaving it ambiguous

## [2.5.1] - 2026-08-05

### Fixed
- **REST API silently ignored unknown fields.** Sending a misnamed field —
  most commonly `{"content": …}` instead of `{"body": …}` — returned `200`
  and echoed back the *unchanged* prompt, so a write that never happened was
  indistinguishable from a successful one. `POST /prompts`, `PUT`/`PATCH
  /prompts/{id}` and `POST /folders` now reject any unrecognised field with
  `400`, naming it and suggesting the intended one
  (`Unknown field 'content' (did you mean 'body'?). Allowed fields: …`)
- **A no-op update no longer bumps `modified` or rewrites the store.** An update
  whose values all match what's already stored returns `200` with the prompt
  untouched. Previously every request stamped `modified` and saved to disk —
  and since `modified` drives the Newest/Oldest folder sort, a redundant or
  typo'd write silently reordered the folder tree
- `PUT`/`PATCH` now accept the server-managed `id`, `created` and `modified`
  fields so a caller can `GET` a prompt and `PUT` the whole object back. They
  are ignored rather than written; an `id` that doesn't match the URL is a `400`

## [2.5.0] - 2026-08-02

### Added
- **New Project** — scaffold a project's **Refresher** and **EOD Summary** prompt
  pair from a type template in one step, instead of hand-writing them each time.
  Available from **File → New Project…** (creates under `Prompts`) and from the
  folder-tree right-click menu as **New Project Here…** (creates under the
  clicked folder). A small dialog takes the project **name**, a **type**
  (Android / Python / Rust / Node.js / C++ / Generic), and the **parent folder**,
  and shows a live preview of what will be created plus the report slug. The
  generated prompts point session reports at `/media/veracrypt1/AICodeLogs/`
  using a `<slug>-SESSION-*.md` name and enforce the no-attribution rule. Adding
  to a project that already has these prompts asks for confirmation first

## [2.4.1] - 2026-07-27

### Changed
- Renamed the folder-tree context-menu action on a **prompt** from
  "Copy Folder Path" to **"Copy Prompt Path"** (it copies the full path
  including the prompt's own name). On a **folder** it remains
  "Copy Folder Path"

## [2.4.0] - 2026-07-27

### Added
- **Folder search** — a search box above the Folders tree that matches both
  folder names and prompt titles, keeping the path to each match visible (and a
  matched folder's contents). Independent of the existing prompt-list search
- **Folder auto-sort** — sort the Folders tree by **Name A–Z / Z–A** or
  **Newest / Oldest** (a folder's recency is the most recently modified prompt
  inside it). Sorting applies to folders only; prompts keep their manual
  drag-and-drop order. Choice persists across restarts. A **Manual** option
  restores drag order

## [2.3.1] - 2026-07-27

### Fixed
- **Copy Folder Path** on a prompt now includes the prompt's own title
  (e.g. `Work/Email/Invoice reminder`), pointing at the prompt itself instead of
  just its containing folder

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
