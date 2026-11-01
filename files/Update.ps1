param([string]$GamePath='C:\Program Files (x86)\Steam\steamapps\common\Order of the Sinking Star')
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
if(!(Test-Path -LiteralPath (Join-Path $target 'sinking_star.exe'))) { throw '请选择正确游戏目录。' }
if(Get-Process -Name sinking_star -ErrorAction SilentlyContinue) { throw '请先正常退出游戏，再运行 Update.ps1。' }
$supported=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Supported-Previous.json') -Raw | ConvertFrom-Json
$new=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'SHA256.json') -Raw | ConvertFrom-Json
function SafePath([string]$base,[string]$relative) {
 $file=[IO.Path]::GetFullPath((Join-Path $base $relative))
 if(!$file.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw '清单路径越界。' }
 return $file
}
foreach($item in $new.files) {
 $file=SafePath $PSScriptRoot $item.path
 if((Get-FileHash -LiteralPath $file).Hash -ne $item.sha256) { throw ('新安装包校验失败：'+$item.path) }
}
$newProxy=($new.files|Where-Object path -eq 'dxgi.dll').sha256
$proxy=Join-Path $target 'dxgi.dll'
$current=if(Test-Path -LiteralPath $proxy){(Get-FileHash -LiteralPath $proxy).Hash}else{''}
# The DLL may remain identical in a worker-only update. Verify the entire old payload below.
$old=$null
foreach($candidate in $supported.versions) {
 if($candidate.files.Count -lt 10) { continue }
 if(($candidate.files | Where-Object path -eq 'dxgi.dll').sha256 -ne $current) { continue }
 $valid=$true
 foreach($item in $candidate.files) {
  $file=SafePath $target $item.path
  if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ne $item.sha256) { $valid=$false; break }
 }
 if($valid) { $old=$candidate; break }
}
if(!$old) { throw '不是可核实的0.1.1至0.1.15完整模组，停止更新，避免覆盖被修改文件。' }
$changes=@($new.files | Where-Object { $path=SafePath $target $_.path; !(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $_.sha256 })
# Retire only paths from the fully verified prior payload. Unknown files are never enumerated for deletion.
$newPaths=@{}
foreach($item in $new.files) { $newPaths[$item.path]=$true }
$retired=@($old.files | Where-Object { !$newPaths.ContainsKey($_.path) })
$backup=Join-Path $target ('SinkingMap\rollback-before-0.1.18-'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
$applied=@()
$retiredApplied=@()
try {
 foreach($item in $changes) {
  $destination=SafePath $target $item.path
  $saved=SafePath $backup $item.path
  if(Test-Path -LiteralPath $destination) { New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($saved)) | Out-Null; Copy-Item -LiteralPath $destination -Destination $saved }
  $applied+=$item
  New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($destination)) | Out-Null
  Copy-Item -LiteralPath (SafePath $PSScriptRoot $item.path) -Destination $destination -Force
 }
 foreach($item in $retired) {
  $destination=SafePath $target $item.path
  $saved=SafePath $backup $item.path
  New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($saved)) | Out-Null
  Copy-Item -LiteralPath $destination -Destination $saved
  $retiredApplied+=$item
  Remove-Item -LiteralPath $destination
 }
 foreach($item in $new.files) { if((Get-FileHash -LiteralPath (SafePath $target $item.path)).Hash -ne $item.sha256) { throw '安装后校验失败。' } }
} catch {
 foreach($item in $retiredApplied) { $saved=SafePath $backup $item.path; if(Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination (SafePath $target $item.path) -Force } }
 foreach($item in $applied) { $saved=SafePath $backup $item.path; if(Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination (SafePath $target $item.path) -Force } else { $created=SafePath $target $item.path; if(Test-Path -LiteralPath $created -PathType Leaf) { Remove-Item -LiteralPath $created } } }
 throw
}
# Only remove now-empty parents of retired paths, always staying inside the mod directory.
$modRoot=Join-Path $target 'SinkingMap'
foreach($item in $retired) {
 $parent=[IO.Path]::GetDirectoryName((SafePath $target $item.path))
 while($parent.StartsWith($modRoot+'\',[StringComparison]::OrdinalIgnoreCase)) {
  if(!(Test-Path -LiteralPath $parent) -or (Get-ChildItem -LiteralPath $parent -Force)) { break }
  Remove-Item -LiteralPath $parent
  $parent=[IO.Path]::GetDirectoryName($parent)
 }
}
Write-Host '已更新为沉星之序地图0.1.18。已修复完成统计与局部迷雾的高度判断。'
Write-Host ('旧版本备份：'+$backup)


