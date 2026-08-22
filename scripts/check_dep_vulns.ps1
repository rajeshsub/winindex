# Checks winindex's pinned FetchContent dependencies against the OSV.dev vulnerability
# database (rule 18). No off-the-shelf scanner covers CMake FetchContent deps pinned by git
# tag, so this resolves each tag to a commit and queries OSV's public commit-lookup API
# directly (https://osv.dev/docs/#tag/api/operation/OSV_QueryAffected), rather than guessing
# at a scanner's CMake support.
#
# Usage: pwsh scripts/check_dep_vulns.ps1 [-CMakeListsPath <path>]

param(
    [string]$CMakeListsPath = "$PSScriptRoot\..\CMakeLists.txt"
)

$ErrorActionPreference = "Stop"

$content = Get-Content -Raw $CMakeListsPath

# Matches FetchContent_Declare(<name> ... GIT_REPOSITORY <url> ... GIT_TAG <tag> ...)
$pattern = '(?ms)FetchContent_Declare\(\s*(\S+)\s+.*?GIT_REPOSITORY\s+(\S+)\s+.*?GIT_TAG\s+(\S+)'
$matches = [regex]::Matches($content, $pattern)

if ($matches.Count -eq 0) {
    Write-Host "No FetchContent_Declare blocks found in $CMakeListsPath"
    exit 0
}

$failed = $false

foreach ($m in $matches) {
    $name = $m.Groups[1].Value
    $repoUrl = $m.Groups[2].Value.TrimEnd('.git')
    $tag = $m.Groups[3].Value

    Write-Host "=== $name ($repoUrl @ $tag) ==="

    $refs = git ls-remote --tags --refs $repoUrl $tag
    if (-not $refs) {
        # Some tags (e.g. re2's date-based tags) may need the raw ref form.
        $refs = git ls-remote $repoUrl $tag
    }
    if (-not $refs) {
        Write-Warning "Could not resolve $tag for $name - skipping (verify manually)."
        continue
    }
    $commit = ($refs -split "\s+")[0]
    Write-Host "  resolved commit: $commit"

    $body = @{ commit = $commit } | ConvertTo-Json
    try {
        $response = Invoke-RestMethod -Uri "https://api.osv.dev/v1/query" -Method Post `
            -Body $body -ContentType "application/json"
    } catch {
        Write-Warning "OSV query failed for $name ($commit): $_"
        continue
    }

    if ($response.vulns -and $response.vulns.Count -gt 0) {
        $failed = $true
        Write-Host "::error::$name @ $commit has $($response.vulns.Count) known advisory(ies):"
        foreach ($v in $response.vulns) {
            Write-Host "  - $($v.id): $($v.summary)"
        }
    } else {
        Write-Host "  no known advisories."
    }
}

if ($failed) {
    Write-Host "`nOne or more pinned dependencies have known vulnerabilities. Bump the pin or record a waiver."
    exit 1
}

Write-Host "`nAll pinned dependencies clean."
