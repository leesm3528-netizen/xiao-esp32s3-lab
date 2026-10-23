# XIAO ESP32-S3 Sense IoT 프로젝트 — 에이전트 작업 지침

이 문서는 이 저장소에서 작업하는 모든 에이전트가 먼저 읽어야 할 규칙이다.
아래 "하지 말 것"은 실제로 문제가 났던 항목이다. 근거 없이 되돌리지 말 것.
(다른 PC에서 검증된 구조를 이 PC 환경에 맞춘 버전이다.)

## 1. 구성 한눈에 보기

| 구성 요소 | 위치 | 설명 |
| --- | --- | --- |
| 보드 | COM4, IP 192.168.0.25 (집 와이파이, `secrets.h`), mDNS `xiao.local` | Seeed XIAO ESP32-S3 **Sense** |
| 펌웨어 | `SupabaseDashboard/` | 실제 운영 펌웨어. `Blink/`, `TouchLED/`, `WebDashboard/`는 이전 예제라 건드릴 필요 없음 |
| BLE 예제 | `BLEDashboard/` | BLE 이름 `XIAO-ESP32S3`. 터치 특성(read/notify, `"touched,value,baseline"`, 200ms), LED 특성(read/write/notify, `"1"`/`"0"`, `on`/`off`/`toggle` 쓰기 가능). UUID는 `.ino` 상단. 휴대폰 페이지 `ble.html` → https://leesm3528-netizen.github.io/xiao-dashboard/ble.html (Web Bluetooth는 HTTPS 필수, 아이폰 Safari 미지원). PC 테스트는 scratchpad venv의 `bleak`로 했다 |
| AP 모드 예제 | `APDashboard/` | 인터넷 없이 보드가 Wi-Fi(`secrets.h`의 `AP_SSID`)를 직접 띄우고 http://192.168.4.1/ 에서 터치값·LED 제어. 카메라·클라우드 없음, PSRAM 옵션 불필요. 올리면 운영 펌웨어를 덮어쓰므로 다시 쓰려면 `SupabaseDashboard`를 재업로드 |
| 로컬 대시보드 | 보드 80번 포트 `/`, `/api/status`, `/api/led?state=on\|off\|toggle`, `/api/camera?size=5..11` | Arduino `WebServer`, HTML은 `dashboard_html.h` |
| 카메라 | 보드 81번 포트 `/stream`(MJPEG), `/capture`(JPEG) | `camera_stream.h`의 자체 HTTP 서버 |
| 클라우드 동기화 | Supabase `arduino-shop` 프로젝트(`dqwrwqkbvelhqytqyfhi`)의 `public.xiao_state` 테이블 + `public.xiao_report` RPC | 외부망 LED 제어, 터치 상태, 보드 내부 IP |
| 외부 대시보드 | `SupabaseDashboard/dashboard.html` → https://leesm3528-netizen.github.io/xiao-dashboard/ (GitHub 저장소 `leesm3528-netizen/xiao-dashboard`) | Supabase Realtime 사용, 카메라는 내부 IP 링크만 |
| 외부망 카메라 | 미구성 (이 PC에 Tailscale 없음) | 7절 참고 |
| 보드 레퍼런스 | `Xiao.md` | Seeed 공식 위키 사본 |

## 2. 빌드·업로드

```bash
# 반드시 PSRAM=opi 포함. 빠지면 카메라가 "카메라 없음"으로 뜬다.
FQBN=esp32:esp32:XIAO_ESP32S3:PSRAM=opi
arduino-cli compile --fqbn $FQBN SupabaseDashboard
arduino-cli upload -p COM4 --fqbn $FQBN SupabaseDashboard
```

- 이 PC의 esp32 코어는 **3.3.12** (ESP-IDF 5.5). 사용자 요청 없이 올리거나 내리지 말 것.
- 업로드 전에 COM4를 열고 있는 시리얼 리더를 모두 닫을 것. 포트가 잡혀 있으면 업로드가 실패한다.
- 업로드에 계속 실패하면 사용자에게 BootLoader 모드 진입을 요청한다(BOOT 버튼을 누른 채 USB 연결).
- 펌웨어를 바꿨으면 직접 업로드까지 할 것. 예전 펌웨어가 남아 있어 "제어가 안 된다"는 문제가 실제로 있었다.

### 시리얼 로그 읽기

이 환경(Git Bash)에서 `arduino-cli monitor`에 `timeout`을 걸면 출력이 비어 나온다. PowerShell을 쓸 것.

```powershell
$p = New-Object System.IO.Ports.SerialPort COM4,115200; $p.Open(); Start-Sleep -Seconds 8; $p.ReadExisting(); $p.Close()
```

보드는 5초마다 `IP / touch / led / cloud / cam / viewers` 상태를 출력한다.
아무도 영상을 보지 않을 때 나오는 `cam_hal: FB-OVF`는 프레임이 버려진다는 뜻이고 오류가 아니다.

## 3. 하드웨어 규칙

- **유저 LED는 GPIO21(`LED_BUILTIN`)이고 active-low다.** `LOW`가 켜짐, `HIGH`가 꺼짐.
- **터치 센서는 D0(GPIO1, TOUCH1).** ESP32-S3는 터치하면 `touchRead()` 값이 **올라간다**. 손을 대지 않으면 약 17,000이다. 부팅 때 기준값을 재고 그 1.3배를 넘으면 터치로 판단한다. 부팅 중에는 D0를 만지면 안 된다.
- **GPIO21은 Sense 확장보드의 SD 카드 CS 핀과 겹친다.** SD 카드를 쓰려면 LED 제어와의 충돌부터 해결할 것.
- 카메라 핀은 `Xiao.md`의 Camera 표대로 `camera_stream.h`에 정의되어 있다(XCLK 10, SIOD 40, SIOC 39, Y2~Y9 15/17/18/16/14/12/11/48, VSYNC 38, HREF 47, PCLK 13).
- 카메라 영상을 계속 보내면 보드가 뜨거워진다. 장시간 운용 시 방열판을 붙이라고 안내할 것.

## 4. 카메라 서버 — 하지 말 것

1. **`esp_http_server`(httpd)로 되돌리지 말 것.** 요청 헤더 제한(1024바이트)이 미리 컴파일된 코어에 들어 있어 스케치에서 바꿀 수 없다. 프록시가 긴 헤더를 붙이면 모바일에서 `431`이 난다. 지금의 `WiFiServer` 기반 서버는 요청 첫 줄만 읽고 나머지 헤더는 건너뛴다.
2. **스트림을 서버 작업 하나에서 무한 루프로 돌리지 말 것.** 한 명이 보는 동안 다른 시청자와 `/capture`가 전부 막힌다(이 PC에서도 "한 기기만 볼 수 있음" 문제가 있었다). 클라이언트마다 FreeRTOS 작업을 만드는 현재 구조를 유지할 것.
3. 동시 시청은 `MAX_STREAMS` = 3, 동시 연결은 `MAX_CLIENTS` = 6, 프레임 버퍼는 `FB_COUNT` = 3이다. 시청자 수를 늘리면 프레임 버퍼와 메모리도 함께 검토할 것.
4. **HTML/JS를 `.ino` 안의 raw string에 넣지 말 것.** Arduino 전처리기가 JS의 `function foo()`를 C++ 함수로 오인해 `#line`과 프로토타입을 HTML 안에 끼워 넣는다. 스크립트가 통째로 죽어 영상이 까맣게 나왔다. HTML은 `.h` 파일(`dashboard_html.h`)에 둘 것.
5. 테스트할 때는 반드시 **긴 헤더**를 붙여 볼 것. 짧은 헤더의 curl은 통과해도 휴대폰에서는 실패할 수 있다.

## 5. Supabase — 하지 말 것

- 이 프로젝트(`dqwrwqkbvelhqytqyfhi`, `arduino-shop`)는 **쇼핑몰 서비스와 공유**한다. `profiles`, `categories`, `products`, `orders`, `order_items`, `reviews` 테이블과 `is_admin`, `create_order` 함수는 쇼핑몰 것이다. **`xiao_` 접두사가 붙은 객체 외에는 읽지도 수정하지도 말 것.** 보안 점검에서 그쪽 경고가 나와도 사용자 확인 없이 고치지 말 것.
- XIAO 객체:
  - `public.xiao_state` — 행은 `device_id = 'xiao'` 하나. 열: `led_on, touched, touch_value, baseline, local_ip, device_seen_at, updated_at`.
  - anon/authenticated는 SELECT와 **`led_on` 열의 UPDATE만** 가능하다. INSERT·DELETE는 일부러 막았다.
  - `public.xiao_report(p_device, p_secret, p_touched, p_value, p_baseline, p_ip, p_led)` — 보드 전용. `private.xiao_device_secret`의 시크릿이 맞아야 실행된다. 반환값은 LED 목표 상태.
  - 보안 점검의 "anon이 SECURITY DEFINER 함수 실행 가능" 경고 중 `xiao_report`는 시크릿으로 막은 의도된 상태다.
- 접근 제한 없음: 공개 키를 가진 누구나 LED를 바꿀 수 있다. 바꾸려면 사용자에게 먼저 물을 것.
- 공개 키는 `sb_publishable_...` 형식이다. REST 호출 시 `apikey` 헤더만 보낸다(`Authorization: Bearer`에 넣지 말 것).
- **`device_seen_at`은 `xiao_report`만 쓴다**(서버 시각, 2초 하트비트). 대시보드는 이 값으로 온라인 여부(6초)를 판단한다. 다른 곳에서 건드리면 꺼진 보드도 온라인으로 보인다.
- 펌웨어 동기화 규칙: 0.2초마다 `xiao_report`를 호출한다(응답 = LED). 로컬 대시보드에서 바꾼 LED는 `ledPending` 플래그로 `p_led`에 실어 먼저 올린 뒤 응답을 받는다. 이 순서를 바꾸면 LED가 되돌아가는 경쟁 상태가 생긴다.
- 보드는 TLS 인증서를 검증하지 않는다(`setInsecure`). 시연용 설정이다.
- 스키마를 바꾸면 `get_advisors`(security)로 점검할 것.

## 6. 외부 대시보드(GitHub Pages) — 하지 말 것

- 배포 저장소에는 `index.html`(= `SupabaseDashboard/dashboard.html` 복사본) **하나만** 올릴 것. `WIFI.md`와 `SupabaseDashboard/secrets.h`(와이파이 비밀번호, 장치 시크릿)가 공개 저장소에 올라가면 안 된다.
- 배포: 임시 폴더에 저장소를 클론하고 `dashboard.html`을 `index.html`로 복사해 커밋·푸시. 반영까지 1분쯤 걸린다.
- 외부 대시보드는 HTTPS 공개 페이지다. 보드의 `http://` 영상을 페이지 안에 넣으면 mixed content로 막힌다. 그래서 내부 IP로 가는 **링크**만 둔다. tailnet 주소를 넣으면 공개 페이지에 내부 주소가 드러나므로 넣기 전에 사용자에게 물을 것.

## 7. 외부망 카메라 (Tailscale) — 아직 미구성

- 이 PC에는 Tailscale이 설치되어 있지 않다. ESP32에는 Tailscale을 설치할 수 없으므로 같은 망의 PC를 거쳐야 한다.
- 구성하려면 사용자가 Tailscale 설치·로그인 후, 이 PC에서 Serve 설정(tailnet 전용, Funnel 아님):
  - `tailscale serve --bg --https=443 http://192.168.0.25:80`
  - `tailscale serve --bg --https=443 --set-path=/cam http://192.168.0.25:81`
- 로컬 대시보드 JS는 이미 대비되어 있다: `location.protocol === 'https:'`이면 영상 주소를 `origin + '/cam'`으로, 아니면 `http://<host>:81`로 잡는다. 둘 중 하나만 남기지 말 것.
- **보드 IP가 바뀌면 Serve 설정도 바꿔야 한다.** 공유기 DHCP 예약을 권장한다. 현재 IP는 `xiao_state.local_ip`에서 확인할 수 있다.

## 8. 비밀 정보

- 와이파이 정보는 `WIFI.md`가 원본이고, 펌웨어는 `SupabaseDashboard/secrets.h`(와이파이, Supabase URL·키, 장치 ID·시크릿)에서 읽는다. 둘 다 `.gitignore`에 있다. 다른 파일에 비밀번호를 하드코딩하지 말 것.
- 이 폴더는 git 저장소가 아니다. 커밋·푸시는 사용자가 요청할 때만 한다.

## 9. 코드 수정 시 주의

- C/C++ 문자열 리터럴(`"...\n"`, `\"`)을 Python·sed 같은 스크립트로 치환하면 이스케이프가 풀려 컴파일이 깨진다. 문자열이 들어간 수정은 Edit 도구를 쓰거나, 수정 뒤 해당 줄을 직접 확인할 것.
- `dashboard_html.h`는 `R"rawliteral( ... )rawliteral"` 안에 HTML 전체가 들어 있다. 내용에 `)rawliteral"`가 들어가지 않게 할 것.
- 펌웨어 `loop()` 안에서 블로킹 작업(긴 `delay`, 무한 루프)을 하지 말 것. 터치 읽기(50ms), 클라우드 동기화(200ms), 로컬 웹서버가 모두 `loop()`에서 돈다.

## 10. 변경 후 검증 체크리스트

변경 범위에 맞는 항목을 실제로 돌려 보고, 결과를 사용자에게 그대로 보고할 것.

```bash
B=http://192.168.0.25
K=<secrets.h의 SUPABASE_KEY>; U="https://dqwrwqkbvelhqytqyfhi.supabase.co/rest/v1/xiao_state?device_id=eq.xiao"
LONG="Tailscale-User-Profile-Pic: https://x/$(head -c 3000 /dev/zero | tr '\0' 'A')"

curl -s $B/api/status                                    # camera:true, cloud:true 여야 함
curl -s -o /dev/null -w "%{http_code}\n" -H "$LONG" $B:81/capture   # 200 (431이면 회귀)
curl -s -m 5 -o s.bin -H "$LONG" $B:81/stream; grep -ac -- '--xiaoframe' s.bin   # 5초에 100프레임 이상
curl -s -X PATCH "$U" -H "apikey: $K" -H "Content-Type: application/json" -d '{"led_on":true}'; sleep 1; curl -s $B/api/status   # led:true
curl -s -X POST "$B/api/led?state=off"; sleep 1; curl -s "$U&select=led_on" -H "apikey: $K"   # led_on:false (로컬 → 클라우드)
```

- 동시 시청: 영상 3개를 동시에 열면 모두 프레임이 오고, 4번째는 503, 그 사이 `/capture`는 200이어야 한다.
- 페이지 JS가 살아 있는지 브라우저로 한 번 열어 볼 것(4절 4번 문제는 curl로 안 보인다). 확인 뒤 탭은 닫을 것(시청 자리를 차지한다).
- 터치·실제 LED 점등·휴대폰 화면처럼 에이전트가 직접 볼 수 없는 것은 "확인하지 못했다"고 밝히고 사용자에게 확인을 요청할 것.
