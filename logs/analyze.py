#!/usr/bin/env python3
"""Analisi log sensori microdrone: IMU + optical flow."""

import csv
import sys
import os

LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log.csv")


def load(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            rows.append({k: float(v) for k, v in r.items() if v.strip() != ''})
    return rows


def has_comp(rows):
    """Controlla se il log ha le colonne di compensazione gyro."""
    return "comp_px" in rows[0]


def overview(rows):
    dur = (rows[-1]["timestamp_ms"] - rows[0]["timestamp_ms"]) / 1000
    dt_avg = dur / (len(rows) - 1) * 1000
    print(f"Campioni: {len(rows)}  |  Durata: {dur:.1f}s  |  dt medio: {dt_avg:.1f}ms")


def range_stats(rows):
    vals = sorted(set(r["range_mm"] for r in rows))
    print(f"Range ToF: {vals}  ({'costante' if len(vals) == 1 else 'variabile'})")


def flow_stats(rows):
    fpx = [r["flow_px"] for r in rows]
    fpy = [r["flow_py"] for r in rows]
    print(f"\n--- FLOW POSITION (raw) ---")
    print(f"  X: escursione={max(fpx)-min(fpx):.4f} m  ({(max(fpx)-min(fpx))*100:.2f} cm)  finale={fpx[-1]*100:.2f} cm")
    print(f"  Y: escursione={max(fpy)-min(fpy):.4f} m  ({(max(fpy)-min(fpy))*100:.2f} cm)  finale={fpy[-1]*100:.2f} cm")

    raw_x = sum(r.get("flow_raw_x", 0) for r in rows)
    raw_y = sum(r.get("flow_raw_y", 0) for r in rows)
    print(f"  Raw counts totali: X={raw_x:.0f}  Y={raw_y:.0f}")

    if has_comp(rows):
        cpx = [r["comp_px"] for r in rows]
        cpy = [r["comp_py"] for r in rows]
        print(f"\n--- FLOW POSITION (gyro-compensato) ---")
        print(f"  X: escursione={max(cpx)-min(cpx):.4f} m  ({(max(cpx)-min(cpx))*100:.2f} cm)  finale={cpx[-1]*100:.2f} cm")
        print(f"  Y: escursione={max(cpy)-min(cpy):.4f} m  ({(max(cpy)-min(cpy))*100:.2f} cm)  finale={cpy[-1]*100:.2f} cm")


def imu_double_integrate(rows):
    """Doppia integrazione ax/ay (con drift — solo indicativo)."""
    vx, vy = 0.0, 0.0
    sx, sy = 0.0, 0.0
    max_sx, max_sy = 0.0, 0.0
    for i in range(1, len(rows)):
        dt = (rows[i]["timestamp_ms"] - rows[i - 1]["timestamp_ms"]) / 1000
        vx += rows[i]["ax"] * 9.81 * dt
        vy += rows[i]["ay"] * 9.81 * dt
        sx += vx * dt
        sy += vy * dt
        max_sx = max(max_sx, abs(sx))
        max_sy = max(max_sy, abs(sy))

    print(f"\n--- IMU DOPPIA INTEGRAZIONE (con drift) ---")
    print(f"  Posizione finale: ({sx*100:.1f}, {sy*100:.1f}) cm")
    print(f"  Escursione max:   |X|={max_sx*100:.1f}  |Y|={max_sy*100:.1f} cm")
    print(f"  Velocita' finale: ({vx*100:.1f}, {vy*100:.1f}) cm/s  (!=0 = drift)")


def detect_phases(rows, axis="ax", thresh=0.008, min_samples=2):
    """Rileva fasi di movimento dall'accelerazione."""
    phases = []
    moving = False
    start = 0
    for i, r in enumerate(rows):
        above = abs(r[axis]) > thresh
        if above and not moving:
            moving = True
            start = i
        elif not above and moving:
            if i - start >= min_samples:
                phases.append((start, i - 1))
            moving = False
    if moving and len(rows) - start >= min_samples:
        phases.append((start, len(rows) - 1))
    return phases


def print_phases(rows):
    phases = detect_phases(rows)
    if not phases:
        print("\n--- FASI MOVIMENTO: nessuna rilevata ---")
        return

    print(f"\n--- FASI MOVIMENTO (|ax| > 0.008g, min 2 campioni) ---")
    for j, (s, e) in enumerate(phases):
        t0 = rows[s]["timestamp_ms"]
        t1 = rows[e]["timestamp_ms"]
        ax_avg = sum(rows[i]["ax"] for i in range(s, e + 1)) / (e - s + 1)
        fpx_d = (rows[e]["flow_px"] - rows[s]["flow_px"]) * 100
        line = f"  [{j}] t={t0:.0f}-{t1:.0f}ms ({(t1-t0)/1000:.2f}s)  ax_avg={ax_avg:+.4f}g  flow_dx={fpx_d:+.2f}cm"
        if has_comp(rows):
            cpx_d = (rows[e]["comp_px"] - rows[s]["comp_px"]) * 100
            line += f"  comp_dx={cpx_d:+.2f}cm"
        print(line)


def timeline(rows):
    """Stampa i campioni con attivita' significativa."""
    comp = has_comp(rows)
    hdr = f"  {'t_ms':>8}  {'ax':>7}  {'ay':>7}  {'gz':>5}  {'flow_vx':>8}  {'flow_px':>8}  {'raw_x':>5}  {'raw_y':>5}"
    if comp:
        hdr += f"  {'comp_px':>8}  {'comp_py':>8}"
    print(f"\n--- TIMELINE (solo campioni con movimento) ---")
    print(hdr)
    count = 0
    for r in rows:
        if abs(r["ax"]) > 0.008 or abs(r.get("flow_vx", 0)) > 0.01 or abs(r.get("flow_vy", 0)) > 0.01:
            line = (f"  {r['timestamp_ms']:8.0f}  {r['ax']:+7.3f}  {r['ay']:+7.3f}  {r['gz']:+5.1f}"
                    f"  {r.get('flow_vx',0):+8.4f}  {r['flow_px']:8.4f}"
                    f"  {r.get('flow_raw_x',0):5.0f}  {r.get('flow_raw_y',0):5.0f}")
            if comp:
                line += f"  {r['comp_px']:8.4f}  {r['comp_py']:8.4f}"
            print(line)
            count += 1
    print(f"  ({count} campioni attivi su {len(rows)} totali)")


def correlazione(rows):
    """Confronta segno ax vs segno flow_vx nei campioni attivi."""
    agree, disagree, solo_imu, solo_flow = 0, 0, 0, 0
    for r in rows:
        imu_mov = abs(r["ax"]) > 0.008
        flow_mov = abs(r.get("flow_vx", 0)) > 0.01
        if imu_mov and flow_mov:
            if (r["ax"] > 0) == (r.get("flow_vx", 0) > 0):
                agree += 1
            else:
                disagree += 1
        elif imu_mov:
            solo_imu += 1
        elif flow_mov:
            solo_flow += 1

    print(f"\n--- CORRELAZIONE IMU vs FLOW ---")
    print(f"  Concordi (stesso segno): {agree}")
    print(f"  Discordi (segno opposto): {disagree}")
    print(f"  Solo IMU (flow fermo): {solo_imu}")
    print(f"  Solo flow (IMU sotto soglia): {solo_flow}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else LOG_FILE
    print(f"=== ANALISI LOG: {path} ===\n")

    rows = load(path)
    overview(rows)
    range_stats(rows)
    flow_stats(rows)
    imu_double_integrate(rows)
    print_phases(rows)
    correlazione(rows)
    timeline(rows)


if __name__ == "__main__":
    main()
