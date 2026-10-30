param([string]$GamePath='C:\Program Files (x86)\Steam\steamapps\common\Order of the Sinking Star',[string]$TerrainPath='')
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$exe=Join-Path $target 'sinking_star.exe'
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw '请选择含 sinking_star.exe 的正式版游戏目录。' }
$gameHashes=@{
 '0AE10A513DAE25286FE93D0DB4FA8A2DF9A473BC07E1734E12BC1AD30166DB1C'='408ABA3676925FAEA0C823F8C231E30844FEFFE9C32AE42644D273EF31863589'
 'D802098B7CB85EADFC733B88D1554042FAD950C572E916FEBEA3F669A474A549'='D747E5D9B8F540C9C6B90E8E987BD74A8C7F8F2F0672FCE189403128F1F3117C'
}
$exeHash=(Get-FileHash -LiteralPath $exe).Hash
$levels=Join-Path $target 'data\levels.package'
if(!$gameHashes.ContainsKey($exeHash) -or !(Test-Path -LiteralPath $levels) -or (Get-FileHash -LiteralPath $levels).Hash -ne $gameHashes[$exeHash]) { throw '此游戏版本或地图资源尚未验证，停止安装。' }
if (Get-Process -Name sinking_star -ErrorAction SilentlyContinue) { throw '请先正常退出游戏。' }
$proxy=Join-Path $target 'dxgi.dll'
$folder=Join-Path $target 'SinkingMap'
if ((Test-Path -LiteralPath $proxy) -or (Test-Path -LiteralPath $folder)) { throw '发现已有 dxgi.dll 或 SinkingMap；为避免覆盖其他模组，请先核实并卸载。' }
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'SHA256.json') -Raw | ConvertFrom-Json
foreach($item in $manifest.files) {
 $file=Join-Path $PSScriptRoot $item.path
 if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $item.sha256) { throw ('安装包校验失败：'+$item.path) }
}
if(!$TerrainPath -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.terrain-cache\provenance.json'))) { $TerrainPath=Join-Path $PSScriptRoot '.terrain-cache' }
if($TerrainPath) {
 $terrain=Get-Content -LiteralPath (Join-Path $TerrainPath 'provenance.json') -Raw | ConvertFrom-Json
 if($terrain.revision -ne '818260c8b5fe704362722a9369350807ba635e68') { throw '地形版本未经验证。' }
 foreach($asset in $terrain.files) {
  if($asset.name -notin @('map-0.smt','map-1.smt','map-2.smt')) { throw '素材文件名无效。' }
  if((Get-FileHash -LiteralPath (Join-Path $TerrainPath $asset.name)).Hash -ne $asset.sha256) { throw '地形缓存损坏。' }
 }
}
try {
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SinkingMap') -Destination $folder -Recurse
 # Terrain containers are already in the verified SinkingMap payload.
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dxgi.dll') -Destination $proxy
} catch {
 Write-Error '复制未完成；请检查权限，并按卸载说明清理本次创建的 SinkingMap 文件。原存档未改动。'
 throw
}
Write-Host '已安装。用 Steam 启动游戏，按键可在安装器或地图的“按键设置”中修改。建议无边框或窗口模式。'
