param([string]$GamePath='C:\Program Files (x86)\Steam\steamapps\common\Order of the Sinking Star')
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
if (!(Test-Path -LiteralPath (Join-Path $target 'sinking_star.exe') -PathType Leaf)) { throw '不是预期的游戏目录。' }
if (Get-Process -Name sinking_star -ErrorAction SilentlyContinue) { throw '请先正常退出游戏。' }
$proxy=[IO.Path]::GetFullPath((Join-Path $target 'dxgi.dll'))
$folder=[IO.Path]::GetFullPath((Join-Path $target 'SinkingMap'))
if (![string]::Equals([IO.Path]::GetDirectoryName($folder),$target,[StringComparison]::OrdinalIgnoreCase)) { throw '路径校验失败。' }
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'SHA256.json') -Raw | ConvertFrom-Json
$dllHash=($manifest.files | Where-Object path -eq 'dxgi.dll').sha256
if(Test-Path -LiteralPath $proxy) {
 if((Get-FileHash -LiteralPath $proxy -Algorithm SHA256).Hash -ne $dllHash) { throw 'dxgi.dll 已改变，停止卸载以保护其他模组。' }
 Remove-Item -LiteralPath $proxy
}
# Remove only files installed by this package. Keep unknown files and never touch saves.
foreach($item in $manifest.files | Where-Object { $_.path.StartsWith('SinkingMap/') }) {
 $relative=$item.path.Substring(11)
 $file=[IO.Path]::GetFullPath((Join-Path $folder $relative))
 if (!$file.StartsWith($folder+'\',[StringComparison]::OrdinalIgnoreCase)) { throw '路径越界。' }
 if(Test-Path -LiteralPath $file) {
  if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $item.sha256) { Remove-Item -LiteralPath $file }
  else { Write-Warning ('已修改的文件保留：'+$file) }
 }
}
if(Test-Path -LiteralPath $folder) {
 Get-ChildItem -LiteralPath $folder -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object { if(!(Get-ChildItem -LiteralPath $_.FullName -Force)) { Remove-Item -LiteralPath $_.FullName } }
 if(!(Get-ChildItem -LiteralPath $folder -Force)) { Remove-Item -LiteralPath $folder }
}
Write-Host '已卸载可核实的模组文件。游戏原文件和存档未改动。'
