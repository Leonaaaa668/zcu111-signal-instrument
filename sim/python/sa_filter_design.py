"""Independent quantized halfband design candidates for the SAME project's RX.

NumPy mathematical reference/export only. Coefficient files are NOT integrated
into the XPR by this program and do not prove FPGA resource or timing closure.
"""
import argparse
import json
from pathlib import Path
import numpy as np

FRAC = 17
SCALE = 1 << FRAC


def halfband(length):
    if length < 7 or length % 4 != 3:
        raise ValueError("Symmetric type-I halfband length must be 4k+3")
    middle = (length-1)//2
    offsets = np.arange(length)-middle
    h = .5*np.sinc(offsets/2)*np.kaiser(length, 8.6)
    zero = (offsets % 2 == 0) & (offsets != 0)
    h[zero] = 0
    h[middle] = 0
    h *= .5/np.sum(h)
    h[middle] = .5
    q = np.rint(h*SCALE).astype(np.int64)
    correction = SCALE-int(np.sum(q))
    if correction % 2:
        raise ValueError("Symmetric unity-gain correction must be even")
    largest = int(np.argmax(abs(q[:middle])))
    q[largest] += correction//2
    q[-largest-1] += correction//2
    if np.max(q) >= 1 << 17 or np.min(q) < -(1 << 17):
        raise ValueError("18-bit coefficient overflow")
    assert np.sum(q) == SCALE and q[middle] == SCALE//2
    np.testing.assert_array_equal(q, q[::-1])
    assert not np.any(q[zero])
    return q


def characterize(q, fs, pass_hz, stop_hz):
    size = 262144
    response = abs(np.fft.rfft(q.astype(float)/SCALE, n=size))
    freq = np.fft.rfftfreq(size, 1/fs)
    db = 20*np.log10(np.maximum(response, 1e-18))
    return dict(fs_hz=fs, pass_edge_hz=pass_hz, stop_edge_hz=stop_hz,
                taps=len(q), coefficient_width=18, coefficient_fraction_bits=FRAC,
                dc_gain=float(np.sum(q)/SCALE), center_coefficient=float(q[len(q)//2]/SCALE),
                nonzero_taps=int(np.count_nonzero(q)),
                pass_max_abs_db=float(max(abs(db[freq <= pass_hz]))),
                stop_max_db=float(max(db[freq >= stop_hz])),
                model_grid_points=size, hardware_timing="NOT_TESTED")


def scalar_decimate(x, q):
    return np.convolve(x, q.astype(float)/SCALE)[:len(x):2]


def parallel4_to2_reference(x, q):
    if len(x) % 4:
        raise ValueError("Need complete groups of four sequential IQ samples")
    padded = np.pad(x, (len(q)-1, 0))
    h = q.astype(float)/SCALE
    output = []
    for first in range(0, len(x), 4):
        for phase in (0, 2):
            newest = first+phase
            output.append(padded[newest:newest+len(q)][::-1]@h)
    return np.array(output)


def cascade_alias_check(coefficients):
    offsets = np.linspace(-50e6, 50e6, 501)
    worst = -1000.
    for image in range(-4, 5):
        if image == 0:
            continue
        original = offsets+image*150e6
        original = original[(original >= -600e6) & (original < 600e6)]
        current = original.copy()
        gain = np.ones(len(original))
        for q, fs in zip(coefficients, (1.2e9, 600e6, 300e6)):
            response = np.exp(-2j*np.pi*current[:, None]/fs*np.arange(len(q)))@(q/SCALE)
            gain *= abs(response)
            rate_out = fs/2
            current = (current+rate_out/2) % rate_out-rate_out/2
        if len(gain):
            worst = max(worst, float(20*np.log10(max(np.max(gain), 1e-18))))
    return worst


def export(output):
    if output.exists():
        raise ValueError("Use a NEW coefficient evidence directory")
    definitions = [("d2_1200_to600", 15, 1.2e9, 50e6, 550e6),
                   ("d2_600_to300", 23, 600e6, 50e6, 250e6),
                   ("d2_300_to150", 63, 300e6, 50e6, 100e6),
                   ("zoom_d2_normalized", 63, 150e6, 25e6, 50e6)]
    candidates = []
    coefficients = []
    for name, length, fs, pass_hz, stop_hz in definitions:
        q = halfband(length)
        metrics = characterize(q, fs, pass_hz, stop_hz)
        if metrics["pass_max_abs_db"] > .01 or metrics["stop_max_db"] > -70:
            raise ValueError(f"Filter target failed: {name} {metrics}")
        coefficients.append(q)
        candidates.append(dict(name=name, **metrics))
    alias_db = cascade_alias_check(coefficients[:3])
    if alias_db > -70:
        raise ValueError(f"D8 image rejection target failed: {alias_db} dB")
    output.mkdir(parents=True)
    for (name, *_), q in zip(definitions, coefficients):
        (output/(name+".coe")).write_text("radix=10;\ncoefdata=\n"+",\n".join(map(str, q))+";\n", encoding="ascii")
        (output/(name+".mem")).write_text("".join(f"{int(v)&0x3ffff:05x}\n" for v in q), encoding="ascii")
    report = dict(status="QUANTIZED_FILTER_MATH_PASS_NOT_INTEGRATED", filters=candidates,
                  d8_worst_single_image_model_db=alias_db,
                  target_passband_hz=[-50e6, 50e6], output_complex_fs_hz=150e6,
                  coefficient_gain_scale=SCALE,
                  limitations=["Double-precision convolution of quantized coefficients, not RTL arithmetic",
                               "Quantization/rounding of sample datapaths and noise remain to model",
                               "Sample-domain halfband latency is not FPGA pipeline latency",
                               "All ADC out-of-band Nyquist images still require analog verification",
                               "No Vivado integration, synthesis, implementation or new XSA performed"],
                  hardware_actions="NONE")
    (output/"filter_design.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    export(args.output.resolve())
