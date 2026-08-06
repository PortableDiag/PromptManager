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

### Request bodies are validated

**Every field this API accepts is a JSON string**, and bodies are checked against
the field list each endpoint accepts. Three ways a body is rejected, all with
`400`, all naming the offender — none of them are silently ignored:

```json
{ "error": "Unknown field 'content' (did you mean 'body'?). Allowed fields: title, body, folderPath",
  "status": 400 }

{ "error": "Field 'body' must be a string, not a number", "status": 400 }

{ "error": "Request body must be a JSON object", "status": 400 }
```

All three used to look like success. A misnamed field was dropped and the server
returned `200` echoing the *unchanged* prompt. A wrong-typed one was worse:
`{"body": 123}` passed straight through to a string conversion that yields `""`,
so it **blanked the prompt** and reported `200`. A body that was valid JSON but
not an object (an array, say) parsed fine and then did nothing. In every case the
only way to find out was to read the store back.

> **Note on folders:** folders are derived from prompt `folderPath`s. An *empty*
> folder (created via the API or the app but containing no prompts) exists only in
> the running session — it is not persisted across restarts until it holds a prompt.

---

## Endpoints

### `GET /health`
Liveness probe. **No auth required.**

```json
{ "status": "ok", "service": "prompt-manager", "version": "2.6.0" }
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

A `folder` that matches nothing returns `{"count": 0}` rather than `404`. That is
deliberate, not an oversight: folders are derived from prompt `folderPath`s, so a
folder holding no prompts and a folder that doesn't exist are **the same state** —
there is nothing to tell apart. Use `GET /folders` if you need to know which
folder paths the running session currently knows about.

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

Any other field — or any non-string value — is a `400` (see *Request bodies are
validated* above).

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

Any other field — or any non-string value — is a `400` (see *Request bodies are
validated* above).

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

### `DELETE /folders?path=...&confirm=true`
Delete a folder **and every prompt inside it** (recursive). Irreversible.

**Without `confirm=true` this is a dry run.** It changes nothing and returns `400`
telling you exactly what the real call would destroy — so you can look before you
leap, and a mistyped path costs nothing:

```bash
curl -X DELETE -H "X-API-Key: YOUR_KEY" \
     "http://127.0.0.1:8770/api/folders?path=Work/Email"
```

```json
{
  "error": "Refusing to delete 'Work/Email' without confirmation: this would remove 3 prompt(s) and 1 nested folder(s), irreversibly. Retry with &confirm=true if that is what you want.",
  "status": 400,
  "path": "Work/Email",
  "wouldRemovePrompts": 3,
  "wouldRemoveFolders": 1,
  "dryRun": true
}
```

Add `&confirm=true` to actually delete:

```bash
curl -X DELETE -H "X-API-Key: YOUR_KEY" \
     "http://127.0.0.1:8770/api/folders?path=Work/Email&confirm=true"
```

```json
{ "deleted": "Work/Email", "promptsRemoved": 3 }
```

A path that doesn't resolve is still a `404`, checked before the guard — so a dry
run also tells you the folder exists.

---

## Status codes

| Code | Meaning |
|---|---|
| `200` | OK |
| `201` | Created |
| `400` | Bad request: missing, **unknown**, or **wrong-typed** field; a body that isn't a JSON object; or malformed JSON |
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
- **`DELETE /folders` needs `&confirm=true`.** Without it you get a `400` dry run
  reporting how many prompts and nested folders the real call would destroy. Use
  the dry run first; it is free and it is the only preview you get, because there
  is no undo.
- **Check the status code, not your HTTP client's exit code.** The prompt text
  field is `body`, not `content`, and every field is a string. Since 2.5.1 a wrong
  field name is a loud `400`, and since 2.5.2 so is a wrong *type* — but you still
  have to read the code to see it, because `curl` exits `0` on a `400`. Use
  `curl -f`, or parse the `error` key.
