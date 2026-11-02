# Publishing CursorBeacon to GitHub

Everything needed for a public release is prepared. The only missing piece on this machine is a GitHub login: there is no `gh` token, no SSH key and no stored HTTPS credential.

## Repository metadata

- **Name:** `CursorBeacon`
- **Visibility:** public
- **Description:** `Windows shake-to-find cursor: progressively enlarge the real system cursor on multi-monitor and high-DPI desktops, with per-monitor fullscreen suppression.`
- **Topics:** `windows`, `cursor`, `accessibility`, `multi-monitor`, `high-dpi`, `raw-input`, `win32`, `cpp20`, `shake-to-find`, `find-my-mouse`
- **Default branch:** `main`
- **License:** MIT

## One-time authentication

```powershell
gh auth login
```

Choose GitHub.com → HTTPS → login with a browser (or device code). Verify:

```powershell
gh auth status
```

Alternatively, set a token with repository creation and release permissions:

```powershell
$env:GH_TOKEN = "github_pat_..."
```

## Publish

```powershell
powershell -ExecutionPolicy Bypass -File tools/publish-github.ps1
```

The script:

1. reads your GitHub login from `gh api user`
2. sets repository-local `user.name` / `user.email` to your account + GitHub noreply address if unset
3. initializes `main`, commits everything, and pushes
4. creates the public repository `CursorBeacon` if it does not exist
5. updates the About description and topics
6. creates release `v0.1.0` with:
   - `dist/CursorBeacon-0.1.0-win64.zip`
   - `dist/CursorBeacon-0.1.0-win64.zip.sha256`
   - notes from `RELEASE_NOTES.md`

Optional parameters:

```powershell
powershell -ExecutionPolicy Bypass -File tools/publish-github.ps1 -RepoName CursorBeacon -Tag v0.1.0
```

Use `-Private` to create a private repository.

## Manual equivalent

```powershell
$owner = gh api user --jq .login
git init -b main
git add -A
git commit -m "CursorBeacon 0.1.0"
gh repo create "$owner/CursorBeacon" --public --source=. --remote=origin --push `
  --description "Windows shake-to-find cursor: progressively enlarge the real system cursor on multi-monitor and high-DPI desktops, with per-monitor fullscreen suppression."
gh repo edit "$owner/CursorBeacon" `
  --add-topic windows --add-topic cursor --add-topic accessibility `
  --add-topic multi-monitor --add-topic high-dpi --add-topic raw-input `
  --add-topic win32 --add-topic cpp20 --add-topic shake-to-find --add-topic find-my-mouse
gh release create v0.1.0 `
  dist/CursorBeacon-0.1.0-win64.zip `
  dist/CursorBeacon-0.1.0-win64.zip.sha256 `
  --title "CursorBeacon 0.1.0 — Windows shake-to-find cursor" `
  --notes-file RELEASE_NOTES.md
```

## Current release asset

```text
dist/CursorBeacon-0.1.0-win64.zip
SHA256 0BE3E2007DC15042B02D607A1D164E7F668ADA759E3BB8F7AA944B1CEC18C306
```

`dist/` is intentionally git-ignored; release assets are uploaded from the local build rather than committed to the repository.
