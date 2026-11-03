param([string]$GamePath='C:\Program Files (x86)\Steam\steamapps\common\Order of the Sinking Star',[string]$TerrainPath='')
& (Join-Path $PSScriptRoot 'Manage-Install.ps1') -GamePath $GamePath -Action 'Uninstall'

