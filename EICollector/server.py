"""Edge Impulse image collector for XIAO ESP32-S3 Sense (EICamera firmware).

Standard library only. Run:
    python EICollector/server.py                     # http://localhost:8000
    python EICollector/server.py --board 192.168.0.25 --host 0.0.0.0   # also from a phone

Images go to EICollector/dataset/<label>/<label>.<timestamp>.jpg
(the "label." prefix lets Edge Impulse infer the label on upload).
Deleted images move to dataset/.trash, so they can be restored with undo.
"""

import argparse
import io
import json
import re
import shutil
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent
WEB = ROOT / "web"
DATASET = ROOT / "dataset"
TRASH = DATASET / ".trash"
CONFIG = ROOT / "config.json"

LABEL_RE = re.compile(r"^[A-Za-z0-9_-]{1,40}$")
FILE_RE = re.compile(r"^[A-Za-z0-9_.-]{1,120}\.jpg$")

capture_lock = threading.Lock()  # one /capture at a time keeps burst frames in order
state_lock = threading.Lock()
undo_stack = []  # list of batches: [(trash_path, original_path), ...]
config = {"board": "192.168.0.25"}


def load_config():
    if CONFIG.exists():
        try:
            config.update(json.loads(CONFIG.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            pass


def save_config():
    CONFIG.write_text(json.dumps(config, indent=2), encoding="utf-8")


def board_url(port, path):
    return f"http://{config['board']}:{port}{path}"


def jpeg_size(data):
    """Return (width, height) from a JPEG SOF marker, or (0, 0)."""
    i = 2
    while i + 9 < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        marker = data[i + 1]
        if marker in (0xC0, 0xC1, 0xC2):
            return int.from_bytes(data[i + 7:i + 9], "big"), int.from_bytes(data[i + 5:i + 7], "big")
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            i += 2
            continue
        i += 2 + int.from_bytes(data[i + 2:i + 4], "big")
    return 0, 0


def class_dirs():
    DATASET.mkdir(exist_ok=True)
    return sorted(p for p in DATASET.iterdir() if p.is_dir() and LABEL_RE.match(p.name))


def image_info(path):
    st = path.stat()
    return {
        "label": path.parent.name,
        "file": path.name,
        "url": f"/dataset/{path.parent.name}/{urllib.parse.quote(path.name)}",
        "bytes": st.st_size,
        "mtime": st.st_mtime,
    }


def list_images(label):
    d = DATASET / label
    if not d.is_dir():
        return []
    files = [p for p in d.iterdir() if p.is_file() and FILE_RE.match(p.name)]
    files.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return [image_info(p) for p in files]


def summary():
    classes = [{"name": d.name, "count": sum(1 for p in d.glob("*.jpg"))} for d in class_dirs()]
    return {
        "board": config["board"],
        "classes": classes,
        "total": sum(c["count"] for c in classes),
        "undo": len(undo_stack),
    }


def rename_retry(src, dst, tries=20):
    """os.replace with retries: on Windows a file the browser (or antivirus) is still
    reading cannot be renamed for a moment. Never falls back to copy, so a failure
    leaves no duplicate behind."""
    for i in range(tries):
        try:
            src.replace(dst)
            return True
        except PermissionError:
            time.sleep(0.05 * (i + 1))
        except FileNotFoundError:
            return False
    return False


def move_to_trash(paths):
    """Move files to the trash as one undo batch. Returns (moved, failed)."""
    TRASH.mkdir(parents=True, exist_ok=True)
    batch, failed = [], 0
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    for src in paths:
        dst = TRASH / f"{stamp}__{src.parent.name}__{src.name}"
        if rename_retry(src, dst):
            batch.append((dst, src))
        else:
            failed += 1
    if batch:
        undo_stack.append(batch)
        del undo_stack[:-50]
    return len(batch), failed


def make_zip(labels):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_STORED) as z:  # JPEG is already compressed
        for label in labels:
            for p in sorted((DATASET / label).glob("*.jpg")):
                arc = p.name if len(labels) == 1 else f"{label}/{p.name}"
                z.write(p, arc)
    return buf.getvalue()


class ApiError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code
        self.message = message


def check_label(label):
    if not label or not LABEL_RE.match(label):
        raise ApiError(400, "클래스 이름은 영문·숫자·_·- 만 (1~40자)")
    return label


class Handler(BaseHTTPRequestHandler):
    server_version = "EICollector/1.0"

    def log_message(self, fmt, *args):
        if "/api/board/status" not in self.path:  # polled every 2s
            super().log_message(fmt, *args)

    # ---------- helpers
    def send_bytes(self, code, body, ctype, extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache")
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def send_json(self, obj, code=200):
        self.send_bytes(code, json.dumps(obj, ensure_ascii=False).encode(), "application/json; charset=utf-8")

    def read_json(self):
        n = int(self.headers.get("Content-Length") or 0)
        if not n:
            return {}
        try:
            return json.loads(self.rfile.read(n))
        except ValueError:
            raise ApiError(400, "잘못된 JSON")

    def route(self, method):
        url = urllib.parse.urlsplit(self.path)
        path = urllib.parse.unquote(url.path)
        query = urllib.parse.parse_qs(url.query)
        try:
            handler = getattr(self, f"{method}_{self.route_name(path)}", None)
            if handler is None:
                raise ApiError(404, "not found")
            handler(path, query)
        except ApiError as e:
            self.send_json({"error": e.message}, e.code)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass
        except Exception as e:  # noqa: BLE001 -- last resort: reply with JSON instead of dropping the connection
            self.log_error("%s %s failed: %r", method, path, e)
            try:
                self.send_json({"error": f"서버 오류: {e}"}, 500)
            except OSError:
                pass

    @staticmethod
    def route_name(path):
        if path in ("/", "/index.html"):
            return "index"
        if path.startswith("/dataset/"):
            return "image"
        if path.startswith("/api/classes/"):
            return "class"
        return path.strip("/").replace("/", "_")

    def do_GET(self):
        self.route("GET")

    def do_POST(self):
        self.route("POST")

    def do_DELETE(self):
        self.route("DELETE")

    # ---------- pages and files
    def GET_index(self, path, query):
        self.send_bytes(200, (WEB / "index.html").read_bytes(), "text/html; charset=utf-8")

    def GET_image(self, path, query):
        parts = path.split("/")  # ['', 'dataset', label, file]
        if len(parts) != 4 or not LABEL_RE.match(parts[2]) or not FILE_RE.match(parts[3]):
            raise ApiError(404, "not found")
        try:
            data = (DATASET / parts[2] / parts[3]).read_bytes()
        except OSError:  # missing, or being moved to the trash right now (Windows lock)
            raise ApiError(404, "not found")
        self.send_bytes(200, data, "image/jpeg", {"Cache-Control": "max-age=86400"})

    # ---------- state
    def GET_api_state(self, path, query):
        with state_lock:
            self.send_json(summary())

    def GET_api_images(self, path, query):
        label = check_label((query.get("label") or [""])[0])
        self.send_json({"label": label, "images": list_images(label)})

    # ---------- board
    def POST_api_board(self, path, query):
        host = str(self.read_json().get("host", "")).strip()
        if not re.match(r"^[A-Za-z0-9.-]{1,64}$", host):
            raise ApiError(400, "보드 주소 형식이 잘못됐습니다")
        config["board"] = host
        save_config()
        self.send_json(summary())

    def proxy_board(self, path):
        try:
            with urllib.request.urlopen(board_url(80, path), timeout=3) as r:
                self.send_bytes(r.status, r.read(), "application/json")
        except urllib.error.HTTPError as e:
            self.send_bytes(e.code, e.read(), "application/json")
        except (urllib.error.URLError, OSError) as e:
            raise ApiError(502, f"보드 응답 없음 ({config['board']}): {e}")

    def GET_api_board_status(self, path, query):
        self.proxy_board("/api/status")

    def POST_api_board_camera(self, path, query):
        body = self.read_json()
        params = {k: int(body[k]) for k in ("size", "quality") if k in body}
        self.proxy_board("/api/camera?" + urllib.parse.urlencode(params))

    # ---------- classes
    def POST_api_classes(self, path, query):
        label = check_label(str(self.read_json().get("name", "")).strip())
        with state_lock:
            (DATASET / label).mkdir(parents=True, exist_ok=True)
            self.send_json(summary())

    def DELETE_class(self, path, query):
        label = check_label(path.rsplit("/", 1)[-1])
        d = DATASET / label
        if not d.is_dir():
            raise ApiError(404, "없는 클래스")
        with state_lock:
            files = list(d.glob("*.jpg"))
            moved, failed = move_to_trash(files)
            if moved:
                undo_stack[-1].append((None, d))  # recreate the folder on undo
            if failed:
                raise ApiError(409, f"{failed}장을 옮기지 못했습니다(다른 프로그램이 사용 중). 다시 시도하세요")
            shutil.rmtree(d, ignore_errors=True)
            self.send_json(summary())

    # ---------- capture / delete / undo
    def POST_api_capture(self, path, query):
        label = check_label(str(self.read_json().get("label", "")).strip())
        t0 = time.perf_counter()
        with capture_lock:
            try:
                with urllib.request.urlopen(board_url(81, "/capture"), timeout=5) as r:
                    data = r.read()
            except (urllib.error.URLError, OSError) as e:
                raise ApiError(502, f"촬영 실패 ({config['board']}:81): {e}")
            if len(data) < 100 or data[:2] != b"\xff\xd8":
                raise ApiError(502, "보드가 JPEG가 아닌 응답을 보냈습니다")
            d = DATASET / label
            d.mkdir(parents=True, exist_ok=True)
            name = f"{label}.{datetime.now().strftime('%Y%m%d_%H%M%S_%f')[:-3]}.jpg"
            p = d / name
            p.write_bytes(data)
        w, h = jpeg_size(data)
        info = image_info(p)
        info.update(width=w, height=h, ms=round((time.perf_counter() - t0) * 1000))
        with state_lock:
            info["summary"] = summary()
        self.send_json(info)

    def POST_api_delete(self, path, query):
        items = self.read_json().get("items") or []
        paths = []
        for it in items:
            label, file = str(it.get("label", "")), str(it.get("file", ""))
            if LABEL_RE.match(label) and FILE_RE.match(file) and (DATASET / label / file).is_file():
                paths.append(DATASET / label / file)
        with state_lock:
            n, failed = move_to_trash(paths)
            s = summary()
        self.send_json({"deleted": n, "failed": failed, "summary": s})

    def POST_api_undo(self, path, query):
        with state_lock:
            if not undo_stack:
                raise ApiError(400, "되돌릴 삭제가 없습니다")
            batch = undo_stack.pop()
            restored = 0
            for trash_path, original in batch:
                if trash_path is None:
                    original.mkdir(parents=True, exist_ok=True)
                    continue
                original.parent.mkdir(parents=True, exist_ok=True)
                if rename_retry(trash_path, original):
                    restored += 1
            self.send_json({"restored": restored, "summary": summary()})

    # ---------- download
    def GET_api_download(self, path, query):
        label = (query.get("label") or ["all"])[0]
        labels = [d.name for d in class_dirs()] if label == "all" else [check_label(label)]
        if label != "all" and not (DATASET / label).is_dir():
            raise ApiError(404, "없는 클래스")
        data = make_zip(labels)
        fname = f"xiao_{label}_{datetime.now():%Y%m%d_%H%M}.zip"
        self.send_bytes(200, data, "application/zip",
                        {"Content-Disposition": f'attachment; filename="{fname}"'})


def main():
    ap = argparse.ArgumentParser(description="XIAO ESP32-S3 Edge Impulse image collector")
    ap.add_argument("--board", help="board IP or host (saved to config.json)")
    ap.add_argument("--host", default="127.0.0.1", help="bind address; 0.0.0.0 to allow phones on the LAN")
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()

    load_config()
    if args.board:
        config["board"] = args.board
        save_config()
    DATASET.mkdir(exist_ok=True)

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    shown = "localhost" if args.host in ("127.0.0.1", "0.0.0.0") else args.host
    print(f"EICollector: http://{shown}:{args.port}  (board {config['board']}, dataset {DATASET})")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
