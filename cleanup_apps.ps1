# cleanup_apps.ps1
# DIY Flipper Zero: Clean up external applications
# Запуск:
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\cleanup_apps.ps1

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $root

Write-Host "`n=== DIY Flipper Zero: Cleaning up external applications ===" -ForegroundColor Cyan
Write-Host "Working directory: $root`n" -ForegroundColor Gray

# -------------------------------------------------
# 1. Отключаем examples (все)
# -------------------------------------------------
$examplesPath = Join-Path $root "applications\examples"
$disabledExamples = Join-Path $root "_disabled_examples"

if (Test-Path $examplesPath) {
    if (Test-Path $disabledExamples) {
        Remove-Item $disabledExamples -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Host "Disabling applications\examples -> _disabled_examples" -ForegroundColor Yellow
    Move-Item -Path $examplesPath -Destination $disabledExamples -Force
} else {
    Write-Host "applications\examples not found, skipping" -ForegroundColor Gray
}

# -------------------------------------------------
# 2. Что оставляем в applications/external (ядро + игры + утилиты)
# -------------------------------------------------
$keepList = @(
    # Игры — 4 штуки
    "air_arkanoid",
    "air_labyrinth",
    "tetris",
    "snake_2",

    # Критически важные утилиты
    "calculator",
    "i2ctools",
    "ina_meter",
    "gpio_badge",
    "gpio_controller",
    "gpio_explorer",
    "gpio_reader_a",
    "gpio_reader_b",
    "uart_terminal",
    "hex_editor",
    "hex_viewer",
    "oscilloscope",
    "spectrum_analyzer",
    "radio_scanner",
    "subghz_remote",
    "subghz_playlist",
    "subghz_playlist_creator",
    "ir_remote",
    "ir_scope",
    "image_viewer",
    "text_viewer",
    "wav_player",
    "music_player",
    "video_player",
    "qrcode",
    "barcode_gen",
    "totp"
)

$externalPath = Join-Path $root "applications\external"
$disabledApps = Join-Path $root "_disabled_apps"

if (-Not (Test-Path $externalPath)) {
    Write-Host "ERROR: applications\external not found!" -ForegroundColor Red
    return
}

if (-Not (Test-Path $disabledApps)) {
    New-Item -ItemType Directory $disabledApps | Out-Null
}

Write-Host "`nDisabling all external apps except keepList..." -ForegroundColor Cyan

$externalFolders = Get-ChildItem $externalPath -Directory
$totalApps = $externalFolders.Count

$keepCount = 0
$removeCount = 0

Get-ChildItem $externalPath -Directory | ForEach-Object {
    $name = $_.Name
    if ($keepList -notcontains $name) {
        $dst = Join-Path $disabledApps $name
        Write-Host "Disabling: $name" -ForegroundColor Gray
        Move-Item -Path $_.FullName -Destination $dst -Force -ErrorAction SilentlyContinue
        $removeCount++
    } else {
        $keepCount++
        Write-Host "Keeping: $name" -ForegroundColor Green
    }
}

# -------------------------------------------------
# 3. Summary
# -------------------------------------------------
Write-Host "`n=== Done ===" -ForegroundColor Green
Write-Host "Total external apps: $totalApps" -ForegroundColor White
Write-Host "Kept: $keepCount" -ForegroundColor Green
Write-Host "Disabled: $removeCount" -ForegroundColor Yellow
Write-Host "" -ForegroundColor Gray
Write-Host "Disabled apps folder: _disabled_apps" -ForegroundColor Yellow
Write-Host "Disabled examples folder: _disabled_examples" -ForegroundColor Yellow
Write-Host "" -ForegroundColor Gray
Write-Host "Next: rebuild firmware" -ForegroundColor Cyan
Write-Host "  .\fbt -c" -ForegroundColor White
Write-Host "  Remove-Item .\build -Recurse -Force" -ForegroundColor White
Write-Host "  .\fbt updater_package`n" -ForegroundColor White