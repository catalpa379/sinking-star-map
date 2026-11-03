$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$test=Join-Path ([IO.Path]::GetTempPath()) ('sinking-map-install-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $test|Out-Null
function WriteFile($p,$text){New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($p))|Out-Null;[IO.File]::WriteAllText($p,$text)}
function Package($name){$p=Join-Path $test $name;$items=@();foreach($relative in @('dxgi.dll','SinkingMap/worker/main.mjs','SinkingMap/retired.txt')){WriteFile (Join-Path $p $relative) ($name+':'+$relative);$items+=@{path=$relative;sha256=(Get-FileHash -LiteralPath (Join-Path $p $relative)).Hash}};@{files=$items}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $p 'SHA256.json') -Encoding UTF8;Copy-Item -LiteralPath (Join-Path $repo 'Manage-Install.ps1') -Destination (Join-Path $p 'Manage-Install.ps1') -Force;return $p}
function Assert($condition,$message){if(!$condition){throw $message}}
$a=Package 'A';$b=Package 'B';$g=Join-Path $test 'game';WriteFile (Join-Path $g 'sinking_star.exe') 'fixture';WriteFile (Join-Path $g 'dxgi.dll') 'foreign-mod';WriteFile (Join-Path $g 'save.txt') 'keep'
& (Join-Path $a 'Manage-Install.ps1') -GamePath $g
Assert ((Get-Content (Join-Path $g 'dxgi.sinkingmap-original.dll') -Raw) -eq 'foreign-mod') 'Foreign mod backup missing'
& (Join-Path $b 'Manage-Install.ps1') -GamePath $g
Assert ((Get-Content (Join-Path $g 'dxgi.dll') -Raw) -eq 'B:dxgi.dll') 'Upgrade failed'
& (Join-Path $a 'Manage-Install.ps1') -GamePath $g
Assert ((Get-Content (Join-Path $g 'dxgi.dll') -Raw) -eq 'A:dxgi.dll') 'Downgrade failed'
Remove-Item -LiteralPath (Join-Path $g 'SinkingMap/worker/main.mjs')
& (Join-Path $a 'Manage-Install.ps1') -GamePath $g
Assert (Test-Path (Join-Path $g 'SinkingMap/worker/main.mjs')) 'Repair failed'
& (Join-Path $b 'Manage-Install.ps1') -GamePath $g -Action Uninstall
Assert ((Get-Content (Join-Path $g 'dxgi.dll') -Raw) -eq 'foreign-mod') 'Uninstall did not restore foreign mod'
Assert ((Get-Content (Join-Path $g 'save.txt') -Raw) -eq 'keep') 'Unrelated file changed'
Write-Host 'PASS foreign DXGI, upgrade, downgrade, repair, cross-version uninstall and foreign restoration'
