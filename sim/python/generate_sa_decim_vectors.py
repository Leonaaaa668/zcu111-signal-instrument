"""Independent integer convolution reference vectors for FIR RTL simulation."""
import argparse
from pathlib import Path
import numpy as np
from sa_filter_design import halfband


def pack_mem(path, values, width, lanes):
    assert len(values) % lanes == 0
    mask = (1 << width)-1
    words = [sum((int(v)&mask) << (width*k) for k, v in enumerate(values[first:first+lanes]))
             for first in range(0, len(values), lanes)]
    path.write_text("".join(f"{v:0{(lanes*width+3)//4}x}\n" for v in words), encoding="ascii")


def quantize(accumulator, shift):
    floor = accumulator >> shift
    rem = accumulator-(floor << shift)
    half = 1 << (shift-1)
    rounded = floor+((rem > half) | ((rem == half) & ((floor & 1) != 0)))
    clipped = np.clip(rounded, -(1 << 23), (1 << 23)-1)
    return clipped, rounded != clipped


def generate(output):
    if output.exists():
        raise ValueError("Use a new simulation vector directory")
    output.mkdir(parents=True)
    rng = np.random.default_rng(20261026)
    for case, lanes, width, taps, shift, name in [
        (0,4,16,15,9,"d2_1200_to600"), (1,2,24,23,17,"d2_600_to300"), (2,1,24,63,17,"d2_300_to150")]:
        q = halfband(taps)
        x = rng.integers(-(1 << (width-2)), 1 << (width-2), size=(1024,2), dtype=np.int64)
        # Constant blocks near both rails intentionally exercise overshoot/saturation.
        x[128:256] = (1 << (width-1))-1
        x[256:384] = -(1 << (width-1))
        y, sat = [], []
        for axis in range(2):
            full = np.convolve(x[:,axis], q)[:1024:2]
            value, clipped = quantize(full, shift)
            y.append(value); sat.append(clipped)
        out_lanes = max(1, lanes//2)
        pack_mem(output/f"case{case}_i.mem",x[:,0],width,lanes)
        pack_mem(output/f"case{case}_q.mem",x[:,1],width,lanes)
        pack_mem(output/f"case{case}_ei.mem",y[0],24,out_lanes)
        pack_mem(output/f"case{case}_eq.mem",y[1],24,out_lanes)
        groups = (sat[0] | sat[1]).reshape(-1,out_lanes).any(axis=1).astype(np.int64)
        pack_mem(output/f"case{case}_s.mem",groups,1,1)
        (output/(name+".mem")).write_text("".join(f"{int(v)&0x3ffff:05x}\n" for v in q),encoding="ascii")
    print("SA_DECIM_INTEGER_VECTORS_GENERATED; synthetic only")


if __name__ == "__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output",type=Path,required=True)
    generate(p.parse_args().output.resolve())
