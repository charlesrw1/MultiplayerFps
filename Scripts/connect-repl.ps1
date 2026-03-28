# connect-repl.ps1
param([int]$Port = 9999)

$host_addr = "127.0.0.1"
Write-Host "[debug] Connecting to ${host_addr}:${Port}..." -ForegroundColor Cyan

try {
    $tcp = [System.Net.Sockets.TcpClient]::new($host_addr, $Port)
    Write-Host "[debug] TCP connected." -ForegroundColor Green
}
catch {
    Write-Host "[error] Could not connect: $_" -ForegroundColor Red
    exit 1
}

$stream = $tcp.GetStream()
$reader = [System.IO.StreamReader]::new($stream)
$writer = [System.IO.StreamWriter]::new($stream)
$writer.AutoFlush = $true
Write-Host "[debug] Stream open. Waiting for banner..." -ForegroundColor Cyan

$banner = $reader.ReadLine()
Write-Host "[debug] Banner received: '$banner'" -ForegroundColor DarkGray

function Read-Response {
    while ($true) {
        $line = $reader.ReadLine()
        if ($null -eq $line) {
            Write-Host "[disconnected]" -ForegroundColor Yellow
            return $false
        }
        if ($line.StartsWith(">>ERROR")) {
            Write-Host $line -ForegroundColor Red
            return $true
        }
        elseif ($line.StartsWith(">>")) {
            Write-Host $line -ForegroundColor Green
            return $true
        }
        else {
            Write-Host $line
        }
    }
}

Write-Host "Ready. Type commands (quit to exit)." -ForegroundColor Cyan
while ($true) {
    $cmd = Read-Host "repl"
    if ($cmd -eq "quit" -or $cmd -eq "exit") { break }
    if ([string]::IsNullOrWhiteSpace($cmd)) { continue }
    $writer.WriteLine($cmd)
    $ok = Read-Response
    if (-not $ok) { break }
}

$tcp.Close()
Write-Host "Disconnected." -ForegroundColor Cyan
