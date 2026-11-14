# シナリオテスト用に、OBS Studio のポータブル版を人の手を借りずに起動できる状態にする。
# 依頼者が普段使う OBS Studio の設定・シーン・%PROGRAMDATA% のプラグインには触れない。
#
# 使い方: pwsh tests/scenario/setup-portable.ps1 -PortableRoot <展開先> -ProductDir <製品フォルダー>
#   PortableRoot: OBS-Studio-<版>-Windows-x64.zip を展開したフォルダー (bin・core・data がある)
#   ProductDir:   cmake --install で書き出した simple-mosaic-transition フォルダー
#
# 行うこと:
# - portable_mode.txt を置き、設定を PortableRoot\config に閉じ込める
# - user.ini の FirstRun を true にし、初回起動時の自動構成ウィザードを開かせない (OBS 33.0.1 frontend/widgets/OBSBasic.cpp:1158-1167)
# - obs-websocket を有効にする。パスワードはこのテスト用の OBS Studio だけのもので、乱数で作る
# - 製品フォルダーを PortableRoot\plugins へコピーする (frontend/plugin-manager/PluginModuleLoader_Windows.cpp:125-136)

param(
	[Parameter(Mandatory)] [string] $PortableRoot,
	[Parameter(Mandatory)] [string] $ProductDir,
	[int] $Port = 4466
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path (Join-Path $PortableRoot 'bin\64bit\obs64.exe'))) {
	throw "obs64.exe was not found under $PortableRoot"
}

New-Item -ItemType File -Force (Join-Path $PortableRoot 'portable_mode.txt') | Out-Null

$config = Join-Path $PortableRoot 'config\obs-studio'
New-Item -ItemType Directory -Force $config | Out-Null
$userIni = Join-Path $config 'user.ini'
if (-not (Test-Path $userIni)) {
	Set-Content -LiteralPath $userIni -Value "[General]`nFirstRun=true`n" -NoNewline -Encoding utf8NoBOM
}

$websocketDir = Join-Path $config 'plugin_config\obs-websocket'
New-Item -ItemType Directory -Force $websocketDir | Out-Null
$websocketConfig = Join-Path $websocketDir 'config.json'
if (-not (Test-Path $websocketConfig)) {
	$bytes = [byte[]]::new(24)
	[System.Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
	$settings = [ordered]@{
		alerts_enabled  = $false
		auth_required   = $true
		first_load      = $false
		server_enabled  = $true
		server_password = [Convert]::ToBase64String($bytes)
		server_port     = $Port
	}
	$settings | ConvertTo-Json | Set-Content -LiteralPath $websocketConfig -Encoding utf8NoBOM
}

$plugins = Join-Path $PortableRoot 'plugins'
New-Item -ItemType Directory -Force $plugins | Out-Null
$name = Split-Path $ProductDir -Leaf
$target = Join-Path $plugins $name
if (Test-Path $target) {
	Remove-Item -LiteralPath $target -Recurse -Force
}
Copy-Item -LiteralPath $ProductDir -Destination $plugins -Recurse

Write-Output "portable OBS Studio is ready: $PortableRoot (obs-websocket port $Port)"
