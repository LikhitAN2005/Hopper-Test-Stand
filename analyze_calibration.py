"""
Diagnostics for a hopper-calibration JSON export (see dashboard.html's own
`fit()` for the reference model this mirrors: raw = A*mass + B).

Usage:
    python analyze_calibration.py [path-to-calibration.json]
"""
import json
import math
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---- palette (categorical slots 1-3 + status critical, from the dataviz skill) ----
RUN_COLORS = ["#2a78d6", "#eb6834", "#1baf7a"]
INK = "#0b0b0b"
SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
SURFACE = "#fcfcfb"
CRITICAL = "#d03b3b"

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "axes.edgecolor": GRID,
    "axes.labelcolor": SECONDARY,
    "text.color": INK,
    "xtick.color": MUTED,
    "ytick.color": MUTED,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "font.size": 9,
    "font.family": "sans-serif",
})

PATH = sys.argv[1] if len(sys.argv) > 1 else "hopper-calibration-2026-10-01-20261005-1724.json"

with open(PATH) as f:
    data = json.load(f)

unit = data.get("unit", "g")
runs = data["runs"]

# ---- flatten all included points ----
pts = []
for ri, run in enumerate(runs):
    if not run.get("include", True):
        continue
    for phase in ("loading", "unloading"):
        for p in run[phase]["points"]:
            if p["mass"] is None or p["raw"] is None:
                continue
            pts.append(dict(
                run=ri, phase=phase, mass=float(p["mass"]), raw=float(p["raw"]),
                t=p["t"], tempC=p.get("tempC"),
            ))

mass = np.array([p["mass"] for p in pts])
raw = np.array([p["raw"] for p in pts])
run_idx = np.array([p["run"] for p in pts])
phase = np.array([p["phase"] for p in pts])
t = np.array([p["t"] for p in pts], dtype=float)
tempC = np.array([np.nan if p["tempC"] is None else p["tempC"] for p in pts])
n = len(mass)

# ---- overall OLS fit: raw = A*mass + B  (identical model to dashboard.html fit()) ----
X = np.vstack([mass, np.ones(n)]).T
A, B = np.linalg.lstsq(X, raw, rcond=None)[0]
pred = A * mass + B
ssr = float(np.sum((raw - pred) ** 2))
sst = float(np.sum((raw - raw.mean()) ** 2))
r2 = 1 - ssr / sst
rms_counts = math.sqrt(ssr / n)
rms_mass = rms_counts / abs(A)
resid = (raw - pred) / A  # residual expressed in mass units (g)

print("=" * 70)
print(f"Loaded {PATH}")
print(f"{n} points across {len(runs)} run(s)")
print("-" * 70)
print(f"Fresh OLS fit   : countsPerUnit={A:.5f}  zeroOffset={B:.4f}")
print(f"                  R2={r2:.7f}  RMS={rms_mass:.3f} {unit}")
stored = data.get("fit")
if stored:
    print(f"Stored fit in file: countsPerUnit={stored['countsPerUnit']:.5f} "
          f"zeroOffset={stored['zeroOffset']:.4f}  R2={stored['r2']:.7f} "
          f"RMS={stored['rmsError']:.3f} (n={stored['points']})")
print("-" * 70)

# ---- per-run fits (repeatability / drift across sessions) ----
print("Per-run fits (atMinutes, own slope/intercept, own R2/RMS):")
run_stats = []
for ri, run in enumerate(runs):
    sel = run_idx == ri
    if sel.sum() < 2:
        continue
    Xr = np.vstack([mass[sel], np.ones(sel.sum())]).T
    Ar, Br = np.linalg.lstsq(Xr, raw[sel], rcond=None)[0]
    predr = Ar * mass[sel] + Br
    ssrr = float(np.sum((raw[sel] - predr) ** 2))
    sstr = float(np.sum((raw[sel] - raw[sel].mean()) ** 2))
    r2r = 1 - ssrr / sstr
    rmsr = math.sqrt(ssrr / sel.sum()) / abs(Ar)
    run_stats.append(dict(ri=ri, A=Ar, B=Br, r2=r2r, rms=rmsr, n=int(sel.sum())))
    print(f"  Run {ri + 1} (t={run['atMinutes']:>8} min, n={sel.sum():3d}): "
          f"A={Ar:10.4f}  B={Br:11.3f}  R2={r2r:.6f}  RMS={rmsr:6.3f} {unit}")
print("-" * 70)

# ---- duplicate-mass points per run/phase (recaptures left in the data) ----
print("Repeated mass values within a run+phase (possible recaptures):")
dup_found = False
for ri, run in enumerate(runs):
    for ph in ("loading", "unloading"):
        masses = [p["mass"] for p in run[ph]["points"] if p["mass"] is not None]
        seen = {}
        for mv in masses:
            seen[mv] = seen.get(mv, 0) + 1
        dups = {mv: c for mv, c in seen.items() if c > 1}
        if dups:
            dup_found = True
            print(f"  Run {ri + 1} {ph}: {dups}")
if not dup_found:
    print("  none")
print("-" * 70)

# ---- outliers (|residual| > 3*std) ----
std = resid.std()
outlier_mask = np.abs(resid) > 3 * std
print(f"Residual std={std:.3f} {unit}  |  outliers beyond 3*std: {outlier_mask.sum()}")
for i in np.where(outlier_mask)[0]:
    print(f"  run {run_idx[i] + 1} {phase[i]:>9}  mass={mass[i]:6.0f}{unit}  "
          f"raw={raw[i]:9.0f}  resid={resid[i]:+7.2f} {unit}")
print("=" * 70)

# ---- hysteresis per run: loading vs unloading raw at matching mass ----
def mean_raw_by_mass(points):
    out = {}
    for p in points:
        if p["mass"] is None or p["raw"] is None:
            continue
        out.setdefault(p["mass"], []).append(p["raw"])
    return {m: float(np.mean(v)) for m, v in out.items()}


hyst_rows = []  # (run_idx, mass, gap_mass_units)
for ri, run in enumerate(runs):
    load_m = mean_raw_by_mass(run["loading"]["points"])
    unload_m = mean_raw_by_mass(run["unloading"]["points"])
    for mv in sorted(set(load_m) & set(unload_m)):
        gap = (unload_m[mv] - load_m[mv]) / A  # signed, in mass units
        hyst_rows.append((ri, mv, gap))

# ===================================================================
# Plots
# ===================================================================
fig, axes = plt.subplots(2, 3, figsize=(17, 9.5))
fig.suptitle(f"Hopper load-cell calibration diagnostics — {data.get('name', PATH)}",
             color=INK, fontsize=12, fontweight="bold")

marker_for_phase = {"loading": "o", "unloading": "s"}
label_for_phase = {"loading": "loading", "unloading": "unloading"}


def run_color(ri):
    return RUN_COLORS[ri % len(RUN_COLORS)]


# 1. raw vs mass, with overall fit line
ax = axes[0, 0]
for ri in range(len(runs)):
    for ph in ("loading", "unloading"):
        sel = (run_idx == ri) & (phase == ph)
        if not sel.any():
            continue
        ax.scatter(mass[sel], raw[sel], s=22,
                   facecolors=run_color(ri) if ph == "loading" else "none",
                   edgecolors=run_color(ri), linewidths=1.2,
                   marker=marker_for_phase[ph], label=f"run {ri + 1} {ph}", zorder=3)
mx = np.array([mass.min(), mass.max()])
ax.plot(mx, A * mx + B, "--", color=MUTED, linewidth=1.2, zorder=2, label="overall fit")
ax.set_xlabel(f"mass ({unit})")
ax.set_ylabel("raw counts")
ax.set_title("Raw counts vs mass")
ax.legend(fontsize=6.5, ncol=2, framealpha=0.9)

# 2. residual vs mass
ax = axes[0, 1]
for ri in range(len(runs)):
    for ph in ("loading", "unloading"):
        sel = (run_idx == ri) & (phase == ph)
        if not sel.any():
            continue
        ax.scatter(mass[sel], resid[sel], s=22,
                   facecolors=run_color(ri) if ph == "loading" else "none",
                   edgecolors=run_color(ri), linewidths=1.2,
                   marker=marker_for_phase[ph], zorder=3)
if outlier_mask.any():
    ax.scatter(mass[outlier_mask], resid[outlier_mask], s=90, facecolors="none",
               edgecolors=CRITICAL, linewidths=1.6, zorder=4, label="|resid|>3σ")
    ax.legend(fontsize=7)
ax.axhline(0, color=MUTED, linewidth=1)
ax.axhspan(-rms_mass, rms_mass, color=MUTED, alpha=0.12, zorder=1)
ax.set_xlabel(f"mass ({unit})")
ax.set_ylabel(f"residual ({unit})")
ax.set_title(f"Residual vs mass  (RMS={rms_mass:.2f} {unit}, R²={r2:.5f})")

# 3. residual vs time (chronological, across the whole session)
ax = axes[0, 2]
t0 = t.min()
t_hours = (t - t0) / 3_600_000.0
for ri in range(len(runs)):
    for ph in ("loading", "unloading"):
        sel = (run_idx == ri) & (phase == ph)
        if not sel.any():
            continue
        ax.scatter(t_hours[sel], resid[sel], s=22,
                   facecolors=run_color(ri) if ph == "loading" else "none",
                   edgecolors=run_color(ri), linewidths=1.2,
                   marker=marker_for_phase[ph], zorder=3)
ax.axhline(0, color=MUTED, linewidth=1)
ax.set_xlabel("time since first point (hours)")
ax.set_ylabel(f"residual ({unit})")
ax.set_title("Residual vs time (drift check)")

# 4. residual vs temperature
ax = axes[1, 0]
has_temp = ~np.isnan(tempC)
if has_temp.any():
    for ri in range(len(runs)):
        for ph in ("loading", "unloading"):
            sel = (run_idx == ri) & (phase == ph) & has_temp
            if not sel.any():
                continue
            ax.scatter(tempC[sel], resid[sel], s=22,
                       facecolors=run_color(ri) if ph == "loading" else "none",
                       edgecolors=run_color(ri), linewidths=1.2,
                       marker=marker_for_phase[ph], zorder=3)
    ax.axhline(0, color=MUTED, linewidth=1)
    ax.set_xlabel("temperature (°C)")
    ax.set_ylabel(f"residual ({unit})")
    ax.set_title("Residual vs temperature")
else:
    ax.text(0.5, 0.5, "no temperature data", ha="center", va="center", color=MUTED)
    ax.set_title("Residual vs temperature")

# 5. hysteresis: loading->unloading gap at matching mass, per run
ax = axes[1, 1]
if hyst_rows:
    for ri in range(len(runs)):
        rows = [(mv, g) for (r, mv, g) in hyst_rows if r == ri]
        if not rows:
            continue
        rows.sort()
        xs = [mv for mv, _ in rows]
        ys = [g for _, g in rows]
        ax.plot(xs, ys, "-o", color=run_color(ri), markersize=4, linewidth=1.3,
                 label=f"run {ri + 1}")
    ax.axhline(0, color=MUTED, linewidth=1)
    ax.set_xlabel(f"mass ({unit})")
    ax.set_ylabel(f"unload - load ({unit})")
    ax.set_title("Hysteresis gap vs mass")
    ax.legend(fontsize=7)
else:
    ax.text(0.5, 0.5, "no matching mass points", ha="center", va="center", color=MUTED)
    ax.set_title("Hysteresis gap vs mass")

# 6. histogram of residuals
ax = axes[1, 2]
ax.hist(resid, bins=24, color=RUN_COLORS[0], edgecolor=SURFACE, linewidth=0.5, alpha=0.9)
ax.axvline(0, color=MUTED, linewidth=1)
ax.set_xlabel(f"residual ({unit})")
ax.set_ylabel("count")
ax.set_title(f"Residual distribution (σ={std:.2f} {unit})")

for a in axes.flat:
    for spine in ("top", "right"):
        a.spines[spine].set_visible(False)

fig.tight_layout(rect=[0, 0, 1, 0.96])
out_path = "calibration-diagnostics.png"
fig.savefig(out_path, dpi=150, facecolor=SURFACE)
print(f"Saved {out_path}")
