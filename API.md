# Prompt Manager REST API

Prompt Manager can expose its store over a small local REST API so that agents and
tools can read and edit your prompts with **full parity to the app** — create,
edit, delete, search, and browse folders.

The server is **off by default**. Enable it in **Settings → API Server…**, where you
also generate the API key, pick the port, and see a live `curl` example.

---

## Quick start

1. Open **Settings → API Server…** in Prompt Manager.
2. Tick **Enable API server**, choose a **port** (default `8770`), click **Generate**
   to mint a key, then **Save**.
3. Point your tool/agent at the base URL and send the key on every request.

```bash
# Health check (no auth)
curl http://127.0.0.1:8770/api/health

# List / search prompts
curl -H "Authorization: Bearer YOUR_KEY" \
     "http://127.0.0.1:8770/api/prompts?q=email"
```

---

## Connection & auth

| | |
|---|---|
| **Base URL** | `http://127.0.0.1:<port>/api` (default port `8770`) |
| **Binding** | Loopback only (`127.0.0.1`). Not reachable from the LAN. |
| **Auth** | `Authorization: Bearer <key>` **or** `X-API-Key: <key>` on every request (except `/health`). |
| **Content type** | Request and response bodies are JSON (`application/json`). |
| **CORS** | Allowed for all origins, so browser-based agents can connect. |

Missing/incorrect key → `401`. The key is stored locally via `QSettings`
(`~/.config/PromptManager/Prompt Manager.conf` on Linux). Regenerating a key in the
dialog immediately invalidates the old one once you Save.

---

## Data model

A **prompt** is:

```json
{
  "id": "1a5a9711-67f6-4a28-8f9e-baac949a0070",
  "title": "Greeting",
  "body": "Hello!",
  "folderPath": "General/Sub",
  "created": "2026-07-24T15:05:51",
  "modified": "2026-07-24T15:05:51"
}
```

- `id` — UUID, assigned by the server on create. Immutable.
- `folderPath` — `/`-separated folder names, e.g. `Work/Email`. Missing folders in
  the path are created automatically. Empty/omitted → defaults to `General`.
- `created` / `modified` — ISO-8601, managed by the server.

### Unknown fields are rejected

Request bodies are validated against the field list each endpoint accepts. An
unrecognised key returns `400` naming the offender — it is never silently
ignored:

```json
{
  "error": "Unknown field 'content' (did you mean 'body'?). Allowed fields: title, body, folderPath",
  "status": 400
}
```

This exists because a typo'd field used to look like a successful write: the
server returned `200` and echoed back the *unchanged* prompt, so the only way to
notice was to read the store again.

> **Note on folders:** folders are derived from prompt `folderPath`s. An *empty*
> folder (created via the API or the app but containing no prompts) exists only in
> the running session — it is not persisted across restarts until it holds a prompt.

---

## Endpoints

### `GET /health`
Liveness probe. **No auth required.**

```json
{ "status": "ok", "service": "prompt-manager", "version": "2.5.1" }
```

---

### `GET /prompts`
List or search prompts.

| Query param | Description |
|---|---|
| `folder` | Exact `folderPath` to filter by (optional). |
| `q` | Case-insensitive substring; matches `title`, `body`, or `folderPath` (optional). |

```bash
curl -H "Authorization: Bearer YOUR_KEY" \
     "http://127.0.0.1:8770/api/prompts?folder=Work/Email&q=invoice"
```

```json
{ "count": 1, "prompts": [ { "id": "…", "title": "…", … } ] }
```

---

### `GET /prompts/{id}`
Fetch a single prompt (full body included).

```bash
curl -H "X-API-Key: YOUR_KEY" \
     http://127.0.0.1:8770/api/prompts/1a5a9711-67f6-4a28-8f9e-baac949a0070
```

`404` if the id is unknown.

---

### `POST /prompts`
Create a prompt. Returns `201` with the created object.

| Field | Required | Notes |
|---|---|---|
| `title` | yes | Non-empty. |
| `body` | no | Defaults to empty string. |
| `folderPath` | no | Defaults to `General`; nested folders auto-created. |

Any other field is a `400` (see *Unknown fields are rejected* above).

```bash
curl -X POST -H "Authorization: Bearer YOUR_KEY" \
     -H "Content-Type: application/json" \
     -d '{"title":"Greeting","body":"Hello!","folderPath":"General"}' \
     http://127.0.0.1:8770/api/prompts
```

---

### `PUT /prompts/{id}`  (alias: `PATCH`)
Partial update — send only the fields you want to change.

| Field | Notes |
|---|---|
| `title` | If present, must be non-empty. |
| `body` | Replaces the body. |
| `folderPath` | Moves the prompt (nested folders auto-created). |
| `id`, `created`, `modified` | Accepted but **server-managed and ignored**, so you can `GET` a prompt and `PUT` the whole object back. A mismatched `id` is a `400`. |

Any other field is a `400` (see *Unknown fields are rejected* above).

If none of the supplied values actually differ from what's stored, the update is
a **no-op**: you get `200` with the prompt unchanged, `modified` is *not*
touched, and nothing is written to disk. (`modified` drives the app's
Newest/Oldest folder sort, so a redundant write would silently reorder the tree.)

```bash
curl -X PUT -H "Authorization: Bearer YOUR_KEY" \
     -H "Content-Type: application/json" \
     -d '{"body":"Updated text","folderPath":"Work/Email"}' \
     http://127.0.0.1:8770/api/prompts/1a5a9711-…
```

Returns `200` with the updated object; `404` if unknown; `400` on an empty title.

---

### `DELETE /prompts/{id}`
Delete a prompt.

```bash
curl -X DELETE -H "Authorization: Bearer YOUR_KEY" \
     http://127.0.0.1:8770/api/prompts/1a5a9711-…
```

```json
{ "deleted": "1a5a9711-…" }
```

---

### `GET /folders`
List all folders as flat `path`s.

```json
{
  "count": 2,
  "folders": [
    { "name": "Work",  "path": "Work" },
    { "name": "Email", "path": "Work/Email" }
  ]
}
```

---

### `POST /folders`
Create a folder path (nested folders auto-created).

```bash
curl -X POST -H "Authorization: Bearer YOUR_KEY" \
     -H "Content-Type: application/json" \
     -d '{"path":"Work/Email"}' \
     http://127.0.0.1:8770/api/folders
```

Returns `201`:

```json
{
  "path": "Work/Email",
  "note": "Empty folders persist across restarts only once they contain a prompt."
}
```

(See the empty-folder persistence note above.)

---

### `DELETE /folders?path=...`
Delete a folder **and every prompt inside it** (recursive). Irreversible.

```bash
curl -X DELETE -H "Authorization: Bearer YOUR_KEY" \
     "http://127.0.0.1:8770/api/folders?path=Work/Email"
```

```json
{ "deleted": "Work/Email", "promptsRemoved": 3 }
```

---

## Status codes

| Code | Meaning |
|---|---|
| `200` | OK |
| `201` | Created |
| `400` | Bad request (missing, invalid or **unknown** field, or malformed JSON) |
| `401` | Missing or invalid API key |
| `404` | Unknown endpoint or resource |
| `405` | Method not allowed on that resource |

Error bodies look like:

```json
{ "error": "Prompt not found", "status": 404 }
```

---

## Notes for agents

- **Discover, then act.** Call `GET /folders` and `GET /prompts?q=…` to orient
  yourself before creating or editing anything.
- Changes are **live** — the desktop UI updates immediately and the store is saved
  to disk on every mutation.
- Use `folderPath` to keep prompts organized; you don't need to pre-create folders,
  but you can with `POST /folders`.
- Be conservative with `DELETE /folders` — it removes all contained prompts.
- **Check the status code, not your HTTP client's exit code.** The prompt text
  field is `body`, not `content`. Since 2.5.1 a wrong field name is a loud `400`
  rather than a silent no-op, but you still have to read the code to see it —
  `curl` exits `0` on a `400`. Use `curl -f`, or parse the `error` key.
