#!/usr/bin/env python3
import os
import pandas as pd
import numpy as np
import plotly.express as px
import plotly.graph_objects as go

# — Paths —
SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "results")
PLOTS_DIR   = os.path.join(RESULTS_DIR, "plots")
os.makedirs(PLOTS_DIR, exist_ok=True)

R_VALUES = [1, 2, 4, 6]
MODES    = ["random", "det"]
BINS     = np.arange(0, 310, 10)

# 1) Median vs. Replicas
med_records = []
for R in R_VALUES:
    for mode in MODES:
        df = pd.read_csv(os.path.join(RESULTS_DIR, f"latencies_R{R}_{mode}.csv"))
        med_records.append({
            "Number of Replicas (R)": R,
            "Mode": mode,
            "Median": df["lat_ms"].median()
        })
med_df = pd.DataFrame(med_records)

fig = px.line(
    med_df,
    x="Number of Replicas (R)",
    y="Median",
    color="Mode",
    markers=True,
    title="Median Latency vs Number of Replicas"
)
fig.update_layout(xaxis=dict(dtick=1))
fig.write_image(os.path.join(PLOTS_DIR, "median_latency_vs_replicas.png"))
# fig.show()

# 2) Histograms per R
for R in R_VALUES:
    df_r = pd.read_csv(os.path.join(RESULTS_DIR, f"latencies_R{R}_random.csv"))
    df_d = pd.read_csv(os.path.join(RESULTS_DIR, f"latencies_R{R}_det.csv"))
    fig2 = go.Figure()
    fig2.add_trace(go.Histogram(
        x=df_r["lat_ms"],
        xbins=dict(start=0, end=300, size=10),
        name="Random",
        opacity=0.75
    ))
    fig2.add_trace(go.Histogram(
        x=df_d["lat_ms"],
        xbins=dict(start=0, end=300, size=10),
        name="Deterministic",
        opacity=0.5
    ))
    fig2.update_layout(
        barmode="overlay",
        title=f"Latency Distribution (R={R})",
        xaxis_title="Latency (ms)",
        yaxis_title="Number of Queries"
    )
    fig2.write_image(os.path.join(PLOTS_DIR, f"hist_latency_R{R}.png"))
    # fig2.show()
