#!/usr/bin/env python3
import argparse, csv, requests, time

def measure(R, mode, n):
    url = "http://localhost:9000/api/search?prompt=cute+cats+in+a+house"
    latencies = []
    for _ in range(n):
        t0 = time.perf_counter()
        r  = requests.get(url)
        if r.status_code != 200:
            print("ERROR", r.status_code, r.text)
        latencies.append((time.perf_counter() - t0) * 1000)
    # Write raw data
    fname = f"../results/latencies_R{R}_{mode}.csv"
    with open(fname, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["R","mode","lat_ms"])
        for l in latencies:
            w.writerow([R, mode, l])
    print(f"Wrote {len(latencies)} rows to {fname}")

if __name__=="__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--R",   type=int,              required=True)
    p.add_argument("--mode", choices=["random","det"], required=True)
    p.add_argument("--n",   type=int,   default=500)
    args = p.parse_args()
    measure(args.R, args.mode, args.n)
