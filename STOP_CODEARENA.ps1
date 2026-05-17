$ErrorActionPreference = "SilentlyContinue"
Set-Location -Path $PSScriptRoot

if (Test-Path .\server.pid) {
    $processId = Get-Content .\server.pid
    Stop-Process -Id $processId
    Remove-Item .\server.pid
    Write-Host "CodeArena server stopped."
} else {
    Write-Host "No CodeArena server pid file found."
}
