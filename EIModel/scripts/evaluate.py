"""Classify the testing set with the trained model and print a confusion count.

Run:  python EIModel/scripts/evaluate.py [int8|float32]
"""
import collections
import json
import sys
import urllib.error

from ei import req
from job import P, wait


def main():
    variant = sys.argv[1] if len(sys.argv) > 1 else "int8"
    try:
        r = req("POST", f"/{P}/jobs/classify", {"modelVariants": [variant]})
    except urllib.error.HTTPError:  # older API without modelVariants
        r = req("POST", f"/{P}/jobs/classify", {})
    print(r)
    wait(r["id"], "classify")

    res = req("GET", f"/{P}/classify/all/result?featureExplorerOnly=false&variant={variant}")
    acc = res.get("accuracy", {})
    keys = ("totalSummary", "summaryPerClass", "accuracyScore", "mseScore")
    print("summary:", json.dumps({k: acc.get(k) for k in keys}, ensure_ascii=False)[:800])

    cm = collections.Counter()
    for s in res.get("result", []):
        expected = s["sample"]["label"]
        for c in s.get("classifications", []):
            for w in c.get("result", []):
                best = max(w.items(), key=lambda kv: kv[1])
                got = best[0] if best[1] >= 0.6 else "uncertain"
                cm[(expected, got)] += 1
    print("per-window (threshold 0.6):", dict(cm))


if __name__ == "__main__":
    main()
