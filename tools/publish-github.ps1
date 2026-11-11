param(
    [string]$RepoName = "CursorBeacon",
    [string]$Tag = "v0.1.1",
    [switch]$Private
)

$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    $candidate = Join-Path $env:LOCALAPPDATA "toolchains\gh-2.103.3\bin\gh.exe"
    if (Test-Path $candidate) {
        $env:Path = "$(Split-Path $candidate);$env:Path"
    } else {
        Write-Error "GitHub CLI (gh) was not found in PATH. Install it or add it to PATH, then run 'gh auth login'."
        exit 1
    }
}

$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
gh auth status *> $null
$authExit = $LASTEXITCODE
$ErrorActionPreference = $previousErrorAction

if ($authExit -ne 0) {
    Write-Host "GitHub CLI is not authenticated."
    Write-Host "Run:  gh auth login"
    Write-Host "Then re-run:  powershell -ExecutionPolicy Bypass -File tools/publish-github.ps1"
    exit 1
}

$owner = (gh api user --jq .login).Trim()
if ([string]::IsNullOrWhiteSpace($owner)) {
    Write-Error "Could not determine the authenticated GitHub user."
    exit 1
}
$repoFull = "$owner/$RepoName"
$description = "Windows shake-to-find cursor: progressively enlarge the real system cursor on multi-monitor and high-DPI desktops, with per-monitor fullscreen suppression."
$topics = @(
    "windows", "cursor", "accessibility", "multi-monitor", "high-dpi",
    "raw-input", "win32", "cpp20", "shake-to-find", "find-my-mouse"
)

if ([string]::IsNullOrWhiteSpace((git config --get user.name))) {
    git config user.name $owner
}
if ([string]::IsNullOrWhiteSpace((git config --get user.email))) {
    git config user.email "$owner@users.noreply.github.com"
}

if (-not (Test-Path ".git")) {
    git init -b main | Out-Null
}

git add -A
git diff --cached --quiet
if ($LASTEXITCODE -ne 0) {
    git commit -m "CursorBeacon 0.1.1" | Out-Null
}
git branch -M main

$ErrorActionPreference = "Continue"
gh repo view $repoFull *> $null
$repoExists = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = "Stop"

if (-not $repoExists) {
    $visibility = if ($Private) { "--private" } else { "--public" }
    gh repo create $repoFull $visibility --source=. --remote=origin --push --description $description
} else {
    $remotes = git remote
    if ($remotes -notcontains "origin") {
        git remote add origin "https://github.com/$repoFull.git"
    }
    git push -u origin main
}

$topicArgs = @()
foreach ($topic in $topics) {
    $topicArgs += "--add-topic"
    $topicArgs += $topic
}
& gh repo edit $repoFull --description $description @topicArgs

$zip = Join-Path $root "dist/CursorBeacon-0.1.1-win64.zip"
$sum = "$zip.sha256"
if (-not (Test-Path $zip)) {
    Write-Error "Release asset not found: $zip"
    exit 1
}

$ErrorActionPreference = "Continue"
gh release view $Tag --repo $repoFull *> $null
$releaseExists = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = "Stop"
if ($releaseExists) {
    Write-Host "Release $Tag already exists; uploading assets is skipped."
} else {
    gh release create $Tag $zip $sum `
        --repo $repoFull `
        --title "CursorBeacon 0.1.1 - Windows shake-to-find cursor" `
        --notes-file (Join-Path $root "RELEASE_NOTES.md")
}

Write-Host ""
Write-Host "Repository: https://github.com/$repoFull"
Write-Host "Release:    https://github.com/$repoFull/releases/tag/$Tag"
