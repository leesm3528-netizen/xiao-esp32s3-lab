# MqttDashboard — 로컬 MQTT 대시보드

XIAO ESP32-S3 터치값(D0)을 MQTT로 올리고, 웹에서 LED(GPIO21)를 제어한다.

```text
[XIAO] --MQTT 1884--> [Mosquitto (이 PC 192.168.0.22)] <--WebSocket 9001-- [브라우저 web/index.html]
```

## 실행

1. 브로커: `powershell -ExecutionPolicy Bypass -File MqttDashboard\start-broker.ps1` (창을 열어 둘 것)
   - Windows의 `mosquitto` 서비스는 설정 없이 localhost:1883만 열어서 보드가 붙지 못한다. 그래서 프로젝트 설정(`broker/mosquitto.conf`)으로 따로 띄운다.
2. 펌웨어: `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 MqttDashboard` → `arduino-cli upload -p COM4 ...`
   - 운영 펌웨어(`SupabaseDashboard`)를 덮어쓴다.
3. 대시보드: `web/index.html`을 더블클릭해서 연다.
   - 휴대폰 브라우저에서도 보려면 `web` 폴더에서 `python -m http.server 8080 --bind 0.0.0.0` 실행 후 휴대폰에서 <http://192.168.0.22:8080> (페이지가 접속한 주소의 9001번으로 WebSocket 연결).

**PC IP는 공유기가 바꿀 수 있다**(2026-11-02에 .27 → .22로 바뀌어 보드가 못 붙었다). 바뀌면 `secrets.h`의 `MQTT_HOST`를 고치고 다시 업로드하고, 휴대폰 앱 설정도 바꾼다. 공유기에서 PC에 DHCP 예약을 걸어 두는 것을 권장.

펌웨어는 `WiFi.setSleep(false)`로 절전을 끈다. 켜 두면 LED 명령이 2초 넘게 늦게 반영됐다.

**`Serial.setTxTimeoutMs(0)`을 지우지 말 것.** USB에 꽂혀 있는데 아무도 COM 포트를 읽지 않으면 `Serial.printf`가 몇 초씩 막혀 터치 메시지가 1초에 1개 수준으로 줄고 LED가 늦게 반응했다(2026-11-03). 시리얼 창을 열어 두면 증상이 사라져서 테스트로는 잘 안 잡힌다. 측정은 반드시 COM 포트를 닫은 상태에서 할 것.

브로커 창(`-v`)은 메시지마다 로그를 찍지 않게 했다. 창이 꺼지면 브로커도 꺼지므로 두 창은 열어 둘 것.

## 토픽

| 토픽 | 방향 | 내용 |
| --- | --- | --- |
| `xiao/status` | 보드 → | `online` / `offline` (retained, offline은 LWT) |
| `xiao/info` | 보드 → | `{"ip":"..","rssi":-51}` (retained) |
| `xiao/touch` | 보드 → | `{"touched":false,"value":16900,"baseline":16900}` 0.2초마다 + 변할 때 |
| `xiao/touch/value` | 보드 → | `17183` 숫자만 (휴대폰 앱 슬라이더·게이지용, `xiao/touch`와 같은 주기) |
| `xiao/touch/state` | 보드 → | `1` 터치 / `0` (휴대폰 앱 LED 표시용) |
| `xiao/led` | 보드 → | `1` / `0` (retained, 실제 LED 상태) |
| `xiao/led/set` | → 보드 | `on` / `off` / `toggle` |

웹 대시보드는 `#`을 구독해 모든 퍼블리셔를 "퍼블리셔" 표에 보여준다(토픽 첫 단어 = 기기 이름, `/set`으로 끝나는 명령 토픽은 제외).

## 스마트폰 퍼블리셔 (IoT MQTT Panel 앱)

- 연결: 브로커 `192.168.0.22`, 포트 `1884`, TCP, 클라이언트 ID `smartphone`, 사용자 이름·비밀번호 없음. 폰은 보드와 같은 와이파이에 있어야 한다.
- 패널 예: 버튼 `phone/hello` ← `hi`, 슬라이더 `phone/slider` 0~100, 버튼 `xiao/led/set` ← `toggle`, LED 표시 `xiao/led`(켜짐 `1`/꺼짐 `0`).

## 명령줄 테스트

```bash
cd "/c/Program Files/mosquitto"
./mosquitto_sub.exe -h 127.0.0.1 -p 1884 -t 'xiao/#' -v
./mosquitto_pub.exe -h 127.0.0.1 -p 1884 -t xiao/led/set -m toggle
```

인증 없는 브로커다(LAN 개발용). 외부에 열거나 다른 사람이 쓰는 망에서는 비밀번호·ACL을 설정할 것.

`web/mqtt.min.js`는 [MQTT.js](https://github.com/mqttjs/MQTT.js) 5.10.1 (MIT 라이선스) 배포본을 그대로 복사한 것이다. 인터넷 없이도 대시보드가 열리게 하려고 넣었다.
