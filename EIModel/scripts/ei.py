"""Tiny Edge Impulse REST helper. Reads EI_API_KEY from the project .env (never prints it)."""
import json
import pathlib
import sys
import urllib.error
import urllib.request

ENV = pathlib.Path(__file__).resolve().parents[2] / ".env"  # project root .env
KEY = next(
    line.split("=", 1)[1].strip().strip("\"'")
    for line in ENV.read_text(encoding="utf-8").splitlines()
    if line.startswith("EI_API_KEY=")
)
BASE = "https://studio.edgeimpulse.com/v1/api"


def req(method, path, body=None, raw=False, base=BASE, headers=None, data=None):
    h = {"x-api-key": KEY, "Accept": "application/json"}
    if body is not None:
        data = json.dumps(body).encode()
        h["Content-Type"] = "application/json"
    h.update(headers or {})
    r = urllib.request.Request(base + path, data=data, method=method, headers=h)
    try:
        with urllib.request.urlopen(r, timeout=300) as resp:
            b = resp.read()
    except urllib.error.HTTPError as e:
        b = e.read()
        print("HTTP", e.code, b[:500].decode(errors="replace"), file=sys.stderr)
        raise
    return b if raw else json.loads(b)
