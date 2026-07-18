# Запусти PowerShell ОТ ИМЕНИ АДМИНИСТРАТОРА, затем:
#   Set-ExecutionPolicy -Scope Process Bypass
#   & "...\docs\install_proteus_uart_model.ps1"

$ErrorActionPreference = 'Stop'

$srcModel = Join-Path $PSScriptRoot '..\Electronics Tree Ultrasonic Sensor Library\MODEL'
$dst = 'C:\Program Files (x86)\Labcenter Electronics\Proteus 8 Professional\MODELS'

if (-not (Test-Path -LiteralPath $dst)) {
  throw "Не найден каталог Proteus MODELS: $dst"
}

Copy-Item -LiteralPath (Join-Path $srcModel 'HCSR04_UART.MDF') -Destination (Join-Path $dst 'HCSR04_UART.MDF') -Force
Copy-Item -LiteralPath (Join-Path $srcModel 'ULTRASONICUART.dll') -Destination (Join-Path $dst 'ULTRASONICUART.dll') -Force

Write-Host 'Скопировано в MODELS:'
Get-ChildItem -LiteralPath $dst -Filter 'HCSR04_UART.MDF'
Get-ChildItem -LiteralPath $dst -Filter 'ULTRASONICUART.dll'
Write-Host 'Закрой и снова открой Proteus.'
