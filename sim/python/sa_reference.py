"""v0.4 standalone receiver numerical reference, NOT a hardware receiver.

Implements candidate scan planning, periodic-Hann normalization, window-time
VBW and complete-scan linear averaging. No register writes, DMA, device access
or GUI. Candidate decimation/FFT combinations are NOT deployed capabilities.
"""
from __future__ import annotations

import argparse
import json
import math
from dataclasses import asdict, dataclass
from functools import lru_cache
from pathlib import Path
import numpy as np

LOW_HZ, HIGH_HZ = 1_300_000_000, 2_300_000_000
MAX_POINTS = 32*1024*1024
MAX_SEGMENTS = 4096
NCO_STEP_HZ = 2_400_000_000 / 2**48
HALF_POWER_3DB = 10**(-3/10)  # exactly 3.000 dB, not 10*log10(2)


def finite_positive(value, name):
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} must be finite and positive")


def periodic_hann(n):
    if not isinstance(n, int) or n < 4:
        raise ValueError("Hann N must be an integer >=4")
    return .5 - .5*np.cos(2*np.pi*np.arange(n)/n)


@lru_cache(maxsize=32)
def hann_width_bins(n):
    w = periodic_hann(n)
    indices = np.arange(n)/n
    lo, hi = 0., 1.
    for _ in range(45):
        mid = (lo+hi)/2
        ratio = abs(np.sum(w*np.exp(-2j*np.pi*mid*indices))/np.sum(w))**2
        if ratio > HALF_POWER_3DB:
            lo = mid
        else:
            hi = mid
    return lo+hi  # 2 * half-width


def hann_metrics(fs_hz, n):
    finite_positive(fs_hz, "Fs")
    w = periodic_hann(n)
    return dict(bin_hz=fs_hz/n, rbw_3db_hz=hann_width_bins(n)*fs_hz/n,
                enbw_hz=fs_hz*float(w@w)/float(np.sum(w)**2), coherent_gain=float(np.mean(w)))


def bin_power(iq, fs_hz, full_scale_counts=32768., restored_voltage_gain=1.):
    """Tone-normalized analysis-bin power, NOT PSD and NOT absolute input dBm.

    Full scale and voltage-gain restoration must match the hardware contract;
    the defaults do NOT prove that RFDC integer output equals ADC input scale.
    """
    finite_positive(fs_hz, "Fs")
    finite_positive(full_scale_counts, "full_scale_counts")
    finite_positive(restored_voltage_gain, "restored_voltage_gain")
    z = np.asarray(iq, dtype=complex)
    if z.ndim != 1 or len(z) < 4 or not np.all(np.isfinite(z)):
        raise ValueError("Need a finite 1D complete complex frame")
    w = periodic_hann(len(z))
    x = z*restored_voltage_gain/full_scale_counts
    spectrum = np.fft.fftshift(np.fft.fft(x*w))
    power = abs(spectrum)**2 / float(np.sum(w)**2)
    frequencies = np.fft.fftshift(np.fft.fftfreq(len(z), 1/fs_hz))
    return frequencies, power


def vbw_parameters(frame_rate_hz, vbw_hz, step_residual=.01):
    finite_positive(frame_rate_hz, "FFT window rate")
    if not 0 < step_residual < 1:
        raise ValueError("Establishment residual must be between 0 and 1")
    if vbw_hz is None:
        return dict(mode="off", coefficient_a=0., establishment_windows=1, actual_3db_hz=None)
    finite_positive(vbw_hz, "VBW")
    if vbw_hz >= frame_rate_hz/2:
        raise ValueError("VBW must be below half FFT WINDOW rate; choose Off to bypass")
    omega = 2*np.pi*vbw_hz/frame_rate_hz
    u = HALF_POWER_3DB*(2*np.sin(omega/2)**2)/(1-HALF_POWER_3DB)
    a = float(1/(1+u+np.sqrt(u*(2+u))))
    if not 0 < a < 1:
        raise ValueError("VBW too small for floating-point coefficient precision")
    count = max(1, math.ceil(math.log(step_residual)/math.log(a)))
    return dict(mode="manual", coefficient_a=a, establishment_windows=count,
                actual_3db_hz=vbw_hz, step_residual=step_residual)


class VideoFilter:
    """One state per bin over successive windows, with mandatory gap rejection."""
    def __init__(self, bins, a):
        if bins <= 0 or not 0 <= a < 1:
            raise ValueError("Invalid filter shape/coefficient")
        self.state = np.zeros(bins)
        self.a = a
        self.next_frame_id = None
        self.invalid = False

    def update(self, power, frame_id):
        p = np.asarray(power, dtype=float)
        if (self.invalid or p.shape != self.state.shape or not np.all(np.isfinite(p)) or
            np.any(p < 0) or (self.next_frame_id is not None and frame_id != self.next_frame_id)):
            self.invalid = True
            raise ValueError("Video sequence invalid; reset at the next valid tune/config transaction")
        self.next_frame_id = frame_id+1
        self.state = self.a*self.state+(1-self.a)*p
        return self.state.copy()


class ScanAverage:
    """Only complete, valid, same-epoch SAME-GRID sweeps may enter an average."""
    def __init__(self, epoch, grid_id, points, count):
        if not isinstance(count, int) or count < 1 or points < 1:
            raise ValueError("Invalid average shape/count")
        self.epoch, self.grid_id = epoch, grid_id
        self.target, self.completed = count, 0
        self.mean = np.zeros(points)
        self.last_sweep_id = None

    def add(self, power, *, epoch, grid_id, sweep_id, complete, quality_ok):
        p = np.asarray(power, dtype=float)
        if (not complete or not quality_ok or epoch != self.epoch or grid_id != self.grid_id or
            p.shape != self.mean.shape or not np.all(np.isfinite(p)) or np.any(p < 0) or
            self.completed >= self.target or
            (self.last_sweep_id is not None and sweep_id <= self.last_sweep_id)):
            raise ValueError("Reject incomplete/invalid/old/duplicate/incompatible sweep or held average")
        self.completed += 1
        self.last_sweep_id = sweep_id
        self.mean += (p-self.mean)/self.completed
        return self.mean.copy()


@dataclass(frozen=True)
class Request:
    center_hz: float = 1.8e9
    span_hz: float = 100e6
    rbw_hz: float | None = None  # None = Span/1000 initial AUTO strategy
    vbw_hz: float | None = None  # None = Off; AUTO policy not silently invented
    average_count: int = 1
    sweep_time_s: float | None = None


def plan(request: Request):
    """Candidate mathematical budget only. Tune/settle/DMA costs remain UNKNOWN."""
    r = request
    finite_positive(r.span_hz, "Span")
    if (not math.isfinite(r.center_hz) or r.center_hz-r.span_hz/2 < LOW_HZ or
        r.center_hz+r.span_hz/2 > HIGH_HZ):
        raise ValueError("Start/Stop must be within TARGET 1.3-2.3 GHz")
    if not isinstance(r.average_count, int) or r.average_count < 1 or r.average_count > 1_000_000:
        raise ValueError("Invalid average count")
    rbw = r.span_hz/1000 if r.rbw_hz is None else r.rbw_hz
    finite_positive(rbw, "RBW")
    if r.vbw_hz is not None:
        finite_positive(r.vbw_hz, "VBW")
    if r.sweep_time_s is not None:
        finite_positive(r.sweep_time_s, "Manual sweep time")
        raise ValueError("Manual sweep scheduling needs measured tune/settle/DMA overheads; unavailable")
    candidates = []
    for extra_d2 in range(7):
        fs = 150e6/2**extra_d2
        for log_n in range(7, 17):
            n = 2**log_n
            actual_rbw = hann_width_bins(n)*fs/n
            if actual_rbw > rbw:
                continue
            df = fs/n
            half_bins = n//3  # reliable +/-Fs/3 TARGET, not a measured passband claim
            window_bins = 2*half_bins+1
            grid_half = math.floor(r.span_hz/(2*df))
            points = 2*grid_half+1
            if points > MAX_POINTS or points < 3:
                continue
            step = max(1, math.floor(.8*(window_bins-1)))
            last_left = 2*grid_half-(window_bins-1)
            if last_left > 0 and 1+math.ceil(last_left/step) > MAX_SEGMENTS:
                continue
            if r.vbw_hz is not None and r.vbw_hz >= fs/n/2:
                continue
            if last_left <= 0:
                centers = [0]
            else:
                left = list(range(0, last_left+1, step))
                if left[-1] != last_left:
                    left.append(last_left)
                centers = [x-grid_half+half_bins for x in left]
            if len(centers) > MAX_SEGMENTS:
                continue
            video = vbw_parameters(fs/n, r.vbw_hz)
            frames = video["establishment_windows"]
            candidates.append((len(centers), n, frames*len(centers)*n/fs,
                               dict(fs_hz=fs, fft_n=n, additional_decimation=8*2**extra_d2,
                                    bin_hz=df, rbw_3db_hz=actual_rbw, enbw_hz=1.5*df,
                                    useful_window_hz=(window_bins-1)*df,
                                    retained_bins=window_bins, video=video, centers=centers,
                                    grid_half=grid_half, output_points=points)))
    if not candidates:
        raise ValueError("No candidate satisfies RBW/VBW/grid/segment budgets; no silent relaxation")
    # User document: fewest segments, then smallest FFT working storage.
    _, _, lower_bound, chosen = min(candidates, key=lambda x: x[:3])
    segments = []
    df, half = chosen["bin_hz"], chosen.pop("grid_half")
    for index, center_bin in enumerate(chosen.pop("centers")):
        desired = r.center_hz+center_bin*df
        rounded = round(desired/NCO_STEP_HZ)*NCO_STEP_HZ
        segments.append(dict(segment_id=index, desired_nco_hz=desired,
                             quantized_frequency_candidate_hz=rounded,
                             nco_grid_error_hz=rounded-desired,
                             center_grid_bin=center_bin))
    return dict(status="OFFLINE_MATH_CANDIDATE_NOT_DEPLOYED", request=asdict(r),
                config_epoch_semantics="One user configuration epoch spans all internal segments",
                rbw_requested_hz=rbw, actual=chosen, segments=segments,
                segment_count=len(segments), mode="dwell" if len(segments) == 1 else "sequential_scan",
                grid=dict(origin_hz=r.center_hz, step_hz=df, first_bin=-half, last_bin=half,
                          first_hz=r.center_hz-half*df, last_hz=r.center_hz+half*df,
                          endpoint_gap_hz=r.span_hz/2-half*df),
                observation_lower_bound_s=lower_bound,
                average_observation_lower_bound_s=lower_bound*r.average_count,
                actual_sweep_time_s=None, verified_rf_range_hz=None,
                limitations=["No PL filters, FFT, DMA or Linux driver deployed by this script",
                             "No measured tune/settle/DMA costs: lower bound is NOT actual sweep time",
                             "ADC spectral_sign and scale require independent RF validation",
                             "Quantized frequency is not a RFDC driver command/sign conversion",
                             "Filter transition/stopband and coefficient quantization not yet validated",
                             "Spectrum stitch/interpolation and fixed-point FFT not implemented here"],
                hardware_actions="NONE")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--center", type=float, default=1.8e9)
    parser.add_argument("--span", type=float, default=100e6)
    parser.add_argument("--rbw", type=float)
    parser.add_argument("--vbw", type=float)
    parser.add_argument("--average", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    a = parser.parse_args()
    result = plan(Request(a.center, a.span, a.rbw, a.vbw, a.average))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    if a.output.exists():
        raise ValueError("Use a new report path; old reports are preserved")
    a.output.write_text(json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False)+"\n",
                        encoding="utf-8")
    print(f"SA_MATH_CANDIDATE segments={result['segment_count']} "
          f"rbw={result['actual']['rbw_3db_hz']:.6f}Hz; NOT hardware acceptance")


if __name__ == "__main__":
    main()
