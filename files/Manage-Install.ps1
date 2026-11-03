param([Parameter(Mandatory=$true)][string]$GamePath,[ValidateSet('Install','Uninstall')][string]$Action='Install',[string]$PackagePath=$PSScriptRoot,[string]$TerrainPath='')
[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$package=[IO.Path]::GetFullPath($PackagePath).TrimEnd('\')
if(!(Test-Path -LiteralPath (Join-Path $target 'sinking_star.exe'))){throw '请选择游戏目录。'}
if(Get-Process -Name sinking_star -ErrorAction SilentlyContinue){throw '请先退出游戏。'}
function Inside([string]$base,[string]$relative){
 $p=[IO.Path]::GetFullPath((Join-Path $base $relative))
 if(!$p.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw '文件路径超出安装目录。'}
 return $p
}
function Hash([string]$p){if(Test-Path -LiteralPath $p -PathType Leaf){return (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash};return ''}
function SaveJson($value,[string]$p){$value|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $p -Encoding UTF8}
$folder=Join-Path $target 'SinkingMap';$proxy=Join-Path $target 'dxgi.dll';$receiptPath=Join-Path $folder 'installed.json'
$new=Get-Content -LiteralPath (Join-Path $package 'SHA256.json') -Raw|ConvertFrom-Json
$sourceFiles=@{};foreach($item in $new.files){$sourceFiles[$item.path]=Inside $package $item.path}
$receipt=$null;if(Test-Path -LiteralPath $receiptPath){$receipt=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json}
$known=@($new)
if($receipt){$known+=@($receipt)}
$ownHashes=@($known|ForEach-Object {$_.files}|Where-Object path -eq 'dxgi.dll'|ForEach-Object {$_.sha256})
$legacy=Join-Path $PSScriptRoot 'Legacy-Dxgi.json'
if(Test-Path -LiteralPath $legacy){$ownHashes+=@((Get-Content -LiteralPath $legacy -Raw|ConvertFrom-Json).hashes)}
$current=Hash $proxy;$ours=$current -and $current -in $ownHashes
$chain=$null;if($receipt){$chain=$receipt.chain}
if($Action -eq 'Uninstall'){
 if($ours){Remove-Item -LiteralPath $proxy -Force}
 $core=Join-Path $target 'SinkingMap-map.dll';if($receipt -and (Test-Path -LiteralPath $core)){Remove-Item -LiteralPath $core -Force}
 if($chain -and !(Test-Path -LiteralPath $proxy)){
  $original=Inside $target $chain.path
  if(Test-Path -LiteralPath $original){Move-Item -LiteralPath $original -Destination $proxy -Force;Write-Host '已恢复原有 DXGI 模组。'}
 }
 $paths=@($known|ForEach-Object {$_.files}|Where-Object {$_.path.StartsWith('SinkingMap/')}|ForEach-Object {$_.path}|Select-Object -Unique)
 foreach($relative in $paths){$p=Inside $target $relative;if(Test-Path -LiteralPath $p -PathType Leaf){Remove-Item -LiteralPath $p -Force}}
 foreach($name in @('installed.json','chain-dxgi.txt')){$p=Join-Path $folder $name;if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Force}}
 if(Test-Path -LiteralPath $folder){Get-ChildItem -LiteralPath $folder -Directory -Recurse|Sort-Object {$_.FullName.Length} -Descending|ForEach-Object {if(!(Get-ChildItem -LiteralPath $_.FullName -Force)){Remove-Item -LiteralPath $_.FullName -Force}};if(!(Get-ChildItem -LiteralPath $folder -Force)){Remove-Item -LiteralPath $folder -Force}}
 Write-Host '地图模组已卸载。';exit
}
# Each package installs its own payload. The installed receipt describes any newer or older version.
foreach($item in $new.files){if((Hash $sourceFiles[$item.path]) -ne $item.sha256){throw ('安装包文件损坏：'+$item.path)}}
$backup=Join-Path $target ('SinkingMap-install-backup-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup|Out-Null
$changed=New-Object System.Collections.Generic.List[string]
$createdChain=$null
try{
 if($current -and !$ours){
  $name='dxgi.sinkingmap-original.dll';$index=0
  while(Test-Path -LiteralPath (Join-Path $target $name)){$index++;$name='dxgi.sinkingmap-original-'+$index+'.dll'}
  Copy-Item -LiteralPath $proxy -Destination (Join-Path $target $name)
  $createdChain=$name;$chain=[pscustomobject]@{path=$name;sha256=$current}
  Write-Host '已保留原有 DXGI 模组，将与地图一起加载。'
 }
 $owned=@($new.files|ForEach-Object {$_.path})
 $retired=@($known|ForEach-Object {$_.files}|Where-Object {$_.path.StartsWith('SinkingMap/') -and $_.path -notin $owned}|ForEach-Object {$_.path}|Select-Object -Unique)
 foreach($relative in @($owned)+$retired+@('SinkingMap/installed.json','SinkingMap/chain-dxgi.txt')){
  $p=Inside $target $relative;$saved=Inside $backup $relative
  if(Test-Path -LiteralPath $p -PathType Leaf){New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($saved))|Out-Null;Copy-Item -LiteralPath $p -Destination $saved}
  $changed.Add($relative)
 }
 foreach($item in $new.files){$p=Inside $target $item.path;New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($p))|Out-Null;Copy-Item -LiteralPath $sourceFiles[$item.path] -Destination $p -Force}
 foreach($relative in $retired){$p=Inside $target $relative;if(Test-Path -LiteralPath $p -PathType Leaf){Remove-Item -LiteralPath $p -Force}}
 if($chain){[IO.File]::WriteAllText((Join-Path $folder 'chain-dxgi.txt'),$chain.path,(New-Object Text.UTF8Encoding($false)))}else{$p=Join-Path $folder 'chain-dxgi.txt';if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Force}}
 SaveJson ([pscustomobject]@{format=1;files=$new.files;chain=$chain}) $receiptPath
 foreach($item in $new.files){if((Hash (Inside $target $item.path)) -ne $item.sha256){throw '安装后的文件不完整。'}}
}catch{
 foreach($relative in $changed){$p=Inside $target $relative;$saved=Inside $backup $relative;if(Test-Path -LiteralPath $saved){Copy-Item -LiteralPath $saved -Destination $p -Force}else{if(Test-Path -LiteralPath $p -PathType Leaf){Remove-Item -LiteralPath $p -Force}}}
 if($createdChain){Remove-Item -LiteralPath (Inside $target $createdChain) -Force}
 throw
}finally{if(Test-Path -LiteralPath $backup){Remove-Item -LiteralPath $backup -Recurse -Force}}
Write-Host '安装完成，已有地图版本已自动替换。'
