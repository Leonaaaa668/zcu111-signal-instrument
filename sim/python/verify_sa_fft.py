"""Compare REAL XSim FFT captures to independent NumPy and exact power math.

Vendor internal FFT round-off is allowed up to16 signed24 LSB/component.
This is NOT a bit-exact model of the vendor FFT's internal butterflies.
"""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
from generate_sa_decim_vectors import quantize


def signed24(value):
    value &= 0xffffff
    return value-(1<<24) if value&(1<<23) else value


def verify(directory,logn):
    n=1<<logn
    with (directory/f"fft{logn}_capture.csv").open(newline="") as stream:
        rows=list(csv.DictReader(stream))
    if len(rows)!=n or [int(r["index"]) for r in rows]!=list(range(n)):
        raise ValueError("Missing, duplicate or reordered actual FFT bins")
    raw=np.array([int(r["complex_hex"],16) for r in rows],dtype=np.int64)
    i=np.array([signed24(int(v)) for v in raw],dtype=np.int64)
    q=np.array([signed24(int(v)>>24) for v in raw],dtype=np.int64)
    power=np.array([int(r["power_hex"],16) for r in rows],dtype=np.int64)
    if not np.array_equal(power,i*i+q*q):
        raise ValueError("Power does not exactly equal the corresponding complex FFT bin")
    if [int(r["last"]) for r in rows] != [0]*(n-1)+[1]:
        raise ValueError("Invalid output frame boundary")
    h=np.array([int(line,16) for line in (directory/"hann_q17.mem").read_text().splitlines()],dtype=np.int64)
    h=h[::1<<(16-logn)]
    with np.load(directory/f"fft{logn}_reference.npz") as ref:
        wi=quantize(ref["i"]*h,17)[0]
        wq=quantize(ref["q"]*h,17)[0]
    golden=np.fft.fft(wi+1j*wq)/(2*n)
    measured=i+1j*q
    error=np.r_[i-golden.real,q-golden.imag]
    maximum=float(np.max(abs(error)))
    if maximum>16:
        raise ValueError(f"N={n} FFT error {maximum:.4f} LSB exceeds16; check scaling/packing/sign/window")
    main=int(np.argmax(power))
    if main!=7:
        raise ValueError("Positive input tone is not at bin+7")
    gain_error=float(20*np.log10(abs(measured[main])/abs(golden[main])))
    if abs(gain_error)>.01:
        raise ValueError("FFT gain differs by >0.01dB from declared DFT/(2N)")
    return dict(log_n=logn,n=n,actual_bins=len(rows),fft_total_right_shift=logn+1,
                main_peak_bin=main,max_fft_component_error_lsb=maximum,
                rms_fft_component_error_lsb=float(np.sqrt(np.mean(error**2))),
                peak_gain_error_db=gain_error,power_bit_exact=True,
                fft_internal_bit_exact_claim=False,fft_component_tolerance_lsb=16,
                status="WINDOW_REAL_FFT_POWER_REFERENCE_PASS")


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--directory",type=Path,required=True)
    p.add_argument("--logn",type=int,nargs="+",default=[7,8,9,10,15,16])
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    if a.output.exists():raise ValueError("Use a new verification result file")
    result=dict(hardware_actions="NONE",cases=[verify(a.directory,v) for v in a.logn],
                limitations="Offline functional/reference test, NOT300MHz closure or RF acceptance")
    a.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2))
