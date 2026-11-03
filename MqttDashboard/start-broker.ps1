# Starts the dev MQTT broker (1884 = MQTT, 9001 = WebSocket) in this window.
# Stop with Ctrl+C.
$conf = Join-Path $PSScriptRoot 'broker\mosquitto.conf'
& 'C:\Program Files\mosquitto\mosquitto.exe' -c $conf
