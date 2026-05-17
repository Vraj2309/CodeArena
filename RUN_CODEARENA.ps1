$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

Write-Host "Starting CodeArena on http://localhost:18080 ..."
python .\local_runner.py
