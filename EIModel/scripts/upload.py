"""Upload EICollector/dataset to Edge Impulse: 80% training / 20% testing per class (seed 42).

Run:  python EIModel/scripts/upload.py
Delete the old samples in Edge Impulse first, or the same photos get uploaded twice.
"""
import pathlib
import random
import uuid

from ei import req

DATA = pathlib.Path(__file__).resolve().parents[2] / "EICollector" / "dataset"
ING = "https://ingestion.edgeimpulse.com"
BATCH = 20


def upload(category, label, files):
    for i in range(0, len(files), BATCH):
        b = uuid.uuid4().hex
        body = b""
        for f in files[i:i + BATCH]:
            body += (f"--{b}\r\nContent-Disposition: form-data; name=\"data\"; filename=\"{f.name}\"\r\n"
                     f"Content-Type: image/jpeg\r\n\r\n").encode() + f.read_bytes() + b"\r\n"
        body += f"--{b}--\r\n".encode()
        r = req("POST", f"/api/{category}/files", base=ING, data=body,
                headers={"Content-Type": f"multipart/form-data; boundary={b}", "x-label": label})
        bad = [x for x in r.get("files", []) if not x.get("success")]
        print(category, label, i, "ok" if r.get("success") and not bad else r)


def main():
    random.seed(42)
    for d in sorted(p for p in DATA.iterdir() if p.is_dir() and not p.name.startswith(".")):
        files = sorted(d.glob("*.jpg"))
        random.shuffle(files)
        n_test = round(len(files) * 0.2)
        upload("testing", d.name, files[:n_test])
        upload("training", d.name, files[n_test:])
        print(d.name, "train", len(files) - n_test, "test", n_test)


if __name__ == "__main__":
    main()
