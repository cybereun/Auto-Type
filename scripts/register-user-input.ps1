param([switch]$Remove)

$ErrorActionPreference = 'Stop'

$tip = '0412:{103733A2-F0D5-4DF0-B88A-D6281528AC32}{45F819E1-0A0C-47B2-BF9C-0A94A5A4A4AB}'
$languages = Get-WinUserLanguageList
$korean = $languages | Where-Object { $_.LanguageTag -eq 'ko' } | Select-Object -First 1

if ($Remove) {
    if ($korean -and $korean.InputMethodTips.Contains($tip)) {
        [void]$korean.InputMethodTips.Remove($tip)
        Set-WinUserLanguageList -LanguageList $languages -Force | Out-Null
    }
    return
}

if (-not $korean) {
    $korean = New-WinUserLanguageList 'ko'
    $languages.Add($korean)
}

if (-not $korean.InputMethodTips.Contains($tip)) {
    $korean.InputMethodTips.Add($tip)
    Set-WinUserLanguageList -LanguageList $languages -Force | Out-Null
}
