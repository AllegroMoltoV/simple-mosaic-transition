# シナリオテスト用に起動した OBS Studio を、強制終了せずに終わらせる。
# プロジェクターは親を持たない独立したウィンドウで、Process.MainWindowHandle がプロジェクターを指すことがある。
# そのため CloseMainWindow() を、プロセスが終わるまで回数の上限つきで繰り返す。
#
# 使い方: pwsh tests/scenario/stop-obs.ps1 -ProcessId <PID>

param(
	[Parameter(Mandatory)] [int] $ProcessId,
	[int] $MaxAttempts = 6
)

$process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
if (-not $process) {
	Write-Output "OBS Studio (PID $ProcessId) is not running"
	exit 0
}
for ($i = 1; $i -le $MaxAttempts; $i++) {
	$process.Refresh()
	[void]$process.CloseMainWindow()
	if ($process.WaitForExit(10000)) {
		Write-Output "OBS Studio (PID $ProcessId) exited after $i attempt(s)"
		exit 0
	}
}
Write-Output "OBS Studio (PID $ProcessId) is still running after $MaxAttempts attempts"
exit 1
