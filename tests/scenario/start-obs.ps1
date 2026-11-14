# シナリオテスト用のポータブル版の OBS Studio を起動し、obs-websocket に接続できるまで待つ。
# 前回の異常終了の目印 (config\obs-studio\.sentinel) が残っていると、起動時に Safe Mode を選ぶモーダル画面が開き、
# 人の操作なしには先へ進めない。そのため目印が残っているときは起動しない。
#
# 使い方: pwsh tests/scenario/start-obs.ps1 -PortableRoot <展開先> [-PidFile <PID を書くファイル>]

param(
	[Parameter(Mandatory)] [string] $PortableRoot,
	[string] $PidFile,
	[int] $Port = 4466
)

$ErrorActionPreference = 'Stop'

if (Get-Process obs64 -ErrorAction SilentlyContinue) {
	throw 'another OBS Studio is running'
}
$sentinel = Join-Path $PortableRoot 'config\obs-studio\.sentinel'
if ((Test-Path $sentinel) -and (Get-ChildItem $sentinel)) {
	throw "the unclean shutdown marker $sentinel is not empty; OBS Studio would open the Safe Mode dialog"
}

$bin = Join-Path $PortableRoot 'bin\64bit'
$process = Start-Process -FilePath (Join-Path $bin 'obs64.exe') -WorkingDirectory $bin `
	-ArgumentList '--portable', '--disable-updater' -PassThru
if ($PidFile) {
	$process.Id | Set-Content -LiteralPath $PidFile
}

$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline) {
	if (Test-NetConnection -ComputerName localhost -Port $Port -InformationLevel Quiet -WarningAction SilentlyContinue) {
		Write-Output "OBS Studio started (PID $($process.Id)); obs-websocket is listening on port $Port"
		exit 0
	}
	Start-Sleep -Seconds 2
}
Write-Output "OBS Studio (PID $($process.Id)) started but obs-websocket did not open port $Port within 60 seconds"
exit 1
