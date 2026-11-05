# EICollector — Edge Impulse 이미지 수집 대시보드

XIAO ESP32-S3 Sense 카메라 영상을 보면서 클래스별로 사진을 모으는 PC용 웹 대시보드다.
모은 사진은 Edge Impulse 이미지 분류(classification) 학습 데이터로 쓴다.

```text
[XIAO EICamera 펌웨어]  :81/stream, :81/capture, :80/api/*
        ▲ 영상(브라우저가 직접)      ▲ 촬영·설정(서버가 대신)
[브라우저 http://localhost:8000]  ⇄  [server.py] → dataset/<클래스>/<클래스>.<시각>.jpg
```

## 실행

1. 펌웨어(처음 한 번, 다른 펌웨어를 덮어씀):

   ```bash
   arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi EICamera
   arduino-cli upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi EICamera
   ```

   - `EICamera/secrets.h`에 와이파이 정보가 있어야 한다(`secrets.example.h` 참고).
   - 부팅하면 유저 LED가 켜진다(카메라 준비 완료). 시작 해상도는 240×240이다.
2. 수집 서버(파이썬 표준 라이브러리만 사용, 설치할 것 없음):

   ```bash
   python EICollector/server.py                  # http://localhost:8000
   python EICollector/server.py --board 192.168.0.25   # 보드 IP가 바뀌었을 때 (config.json에 저장)
   python EICollector/server.py --host 0.0.0.0   # 같은 와이파이의 휴대폰에서도 열기
   ```

   `--host 0.0.0.0`으로 열면 같은 와이파이에 있는 누구나 사진을 지울 수 있다. 필요할 때만 쓸 것.

## 기능

| 기능 | 사용법 |
| --- | --- |
| 실시간 영상 | 보드의 MJPEG를 그대로 보여 준다. "영상 끄기"를 누르면 보드 시청 자리(최대 3)가 비고 발열이 준다. 영상을 꺼도 촬영은 된다 |
| 해상도·품질 | 96×96 ~ 640×480, JPEG 품질 10~32. 바꾸면 다음 사진부터 바로 적용된다 |
| 클래스 | 이름을 입력해 추가(영문·숫자·`_`·`-`). 칩을 누르거나 숫자키 `1`~`9`로 고른다 |
| 찍기 | 버튼 또는 `Space`. 한 장에 약 0.1~0.2초 걸린다 |
| 연속 촬영 | 장수와 간격(ms)을 정하고 버튼 또는 `B`. `Esc`나 "중지"로 멈춘다 |
| 삭제 | 썸네일의 ✕, 또는 `Ctrl`+클릭·우클릭으로 여러 장 고른 뒤 "선택 삭제"(`Delete`) |
| 되돌리기 | "되돌리기" 또는 `Ctrl`+`Z`. 지운 사진은 `dataset/.trash`로 옮겨 두기 때문에 최근 50번까지 되살릴 수 있다(서버를 다시 켜면 목록은 사라지지만 파일은 trash에 남는다) |
| 클래스 삭제 | 두 번 눌러야 지워진다. 되돌리기로 복원할 수 있다 |
| 다운로드 | "이 클래스 ZIP"(사진만), "전체 ZIP"(클래스별 폴더) |
| 크게 보기 | 썸네일을 클릭 |

## 파일 이름과 Edge Impulse

- 저장 위치: `EICollector/dataset/<클래스>/<클래스>.<YYYYMMDD_HHMMSS_ms>.jpg` (원본)
- 받은 ZIP 보관: `EICollector/exports/` (원본과 같은 내용의 백업, gitignore)
- 현재 데이터(2026-11-04): `Mint` 100장, `WaterBottle` 101장, 모두 240×240
- Edge Impulse 업로더는 파일 이름에서 첫 `.` 앞부분을 라벨로 쓴다. 그래서 ZIP을 풀어 그대로 올리면 라벨이 자동으로 붙는다.
- 이미지 분류 모델의 입력은 보통 정사각형(96×96, 160×160)이다. 240×240으로 찍으면 잘리는 부분 없이 줄어든다.

## 수집 팁

- 클래스마다 비슷한 장수를 모은다(처음에는 클래스당 50~100장).
- 물체가 없는 `background`(또는 `unknown`) 클래스를 꼭 만든다.
- 각도·거리·조명·배경을 바꿔 가며 찍는다. 연속 촬영을 켠 채 물체를 천천히 돌리면 빠르다.
- 카메라를 오래 켜 두면 보드가 뜨거워진다. 오래 쓸 때는 방열판을 붙인다.

## API (server.py)

| 메서드 | 경로 | 설명 |
| --- | --- | --- |
| GET | `/api/state` | 보드 주소, 클래스별 장수, 되돌리기 가능 횟수 |
| GET | `/api/images?label=X` | 클래스 사진 목록(최신순) |
| POST | `/api/capture` `{"label"}` | 보드 `/capture`를 받아 저장 |
| POST | `/api/delete` `{"items":[{"label","file"}]}` | 휴지통으로 이동 |
| POST | `/api/undo` | 마지막 삭제 되돌리기 |
| POST / DELETE | `/api/classes` `{"name"}` / `/api/classes/X` | 클래스 추가·삭제 |
| GET | `/api/download?label=X\|all` | ZIP |
| GET / POST | `/api/board/status`, `/api/board/camera` `{"size","quality"}`, `/api/board` `{"host"}` | 보드 상태·설정 |

## 다음 단계

`.env`의 `EI_API_KEY`로 Edge Impulse 프로젝트에 업로드하고, 학습한 모델로 보드에서 추론하는 코드를 붙인다.
