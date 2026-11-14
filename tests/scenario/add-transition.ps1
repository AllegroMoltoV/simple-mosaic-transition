# テスト専用のシーンコレクション smt-scenario-test に、このトランジションを 1 件足す。
# obs-websocket にはトランジションを追加する要求が無く、GUI の + ボタンの代わりにシーンコレクションの JSON を編集する。
# OBS Studio は終了時に現在のシーンコレクションを保存するため、OBS Studio を終了してから実行する。
# JSON のファイル名は名前から OBS Studio が決めるため、決め打ちせず、最上位の name で探す。
#
# 使い方: pwsh tests/scenario/add-transition.ps1 -ScenesDir <config\obs-studio\basic\scenes> -Jq <jq.exe のパス>

param(
	[Parameter(Mandatory)] [string] $ScenesDir,
	[Parameter(Mandatory)] [string] $Jq,
	[string] $CollectionName = 'smt-scenario-test',
	[string] $TransitionName = 'シンプルモザイクトランジション'
)

$ErrorActionPreference = 'Stop'

if (Get-Process obs64 -ErrorAction SilentlyContinue) {
	throw 'quit OBS Studio first; it overwrites the current scene collection when it exits'
}

$target = $null
foreach ($file in Get-ChildItem -LiteralPath $ScenesDir -Filter '*.json') {
	$name = & $Jq -r '.name // empty' $file.FullName
	if ($name -eq $CollectionName) {
		$target = $file
		break
	}
}
if (-not $target) {
	throw "no scene collection named $CollectionName in $ScenesDir; run scenario.py run once first"
}

$has = & $Jq '[.transitions[]? | select(.id == "simple_mosaic_transition")] | length' $target.FullName
if ([int]$has -gt 0) {
	Write-Output "$($target.Name) already has the transition"
	exit 0
}

Copy-Item -LiteralPath $target.FullName -Destination "$($target.FullName).before-add-transition"
$filter = '.transitions = ((.transitions // []) + [{"name": $name, "id": "simple_mosaic_transition", "settings": {}}])'
$json = & $Jq --arg name $TransitionName $filter $target.FullName
if ($LASTEXITCODE -ne 0) {
	throw 'jq failed'
}
[System.IO.File]::WriteAllText($target.FullName, ($json -join "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Output "added the transition to $($target.Name)"
