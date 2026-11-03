# Opens two windows: MQTT broker (1884/9001) and the dashboard web server (8080).
# Close a window to stop that server.
$conf = Join-Path $PSScriptRoot 'broker\mosquitto.conf'
$web  = Join-Path $PSScriptRoot 'web'
Start-Process 'C:\Program Files\mosquitto\mosquitto.exe' -ArgumentList '-c', "`"$conf`""
# "python" can resolve to the Microsoft Store stub, which does nothing here
$py = (Get-Command python -All | Where-Object { $_.Source -notlike '*WindowsApps*' } | Select-Object -First 1).Source
if (-not $py) { $py = 'py' }
Start-Process $py -ArgumentList '-m', 'http.server', '8080', '--bind', '0.0.0.0' -WorkingDirectory $web
Write-Host 'PC:    http://localhost:8080'
Write-Host 'Phone: http://<this PC IP>:8080  (ipconfig -> Wi-Fi IPv4)'
