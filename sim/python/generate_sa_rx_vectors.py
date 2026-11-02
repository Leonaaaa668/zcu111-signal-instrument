"""Generate coefficient artifacts and independent integer/NumPy RX references."""
import argparse
import json
from pathlib import Path
import numpy as np
from sa_filter_design import halfband
from generate_sa_decim_vectors import pack_mem, quantize


def coefficients(output):
    output.mkdir(parents=True,exist_ok=True)
    for name,n in [("d2_1200_to600",15),("d2_600_to300",23),("d2_300_to150",63),("zoom_d2_normalized",63)]:
        path=output/(name+".mem")
        content="".join(f"{int(v)&0x3ffff:05x}\n" for v in halfband(n))
        if path.exists() and path.read_text()!=content:
            raise ValueError("Existing coefficients differ; preserve and version them")
        path.write_text(content,encoding="ascii")
    hann=np.clip(np.rint((.5-.5*np.cos(2*np.pi*np.arange(65536)/65536))*(1<<17)),0,(1<<17)-1).astype(np.int64)
    path=output/"hann_q17.mem"
    content="".join(f"{int(v):05x}\n" for v in hann)
    if path.exists() and path.read_text()!=content:
        raise ValueError("Existing window coefficients differ")
    path.write_text(content,encoding="ascii")
    metrics={str(logn):dict(sum_q17=int(np.sum(hann[::1<<(16-logn)])),
                               sum_square_q34=int(hann[::1<<(16-logn)]@hann[::1<<(16-logn)])) for logn in range(7,17)}
    (output/"hann_metrics.json").write_text(json.dumps(metrics,indent=2)+"\n",encoding="utf-8")
    return hann


def fft_vectors(output,logn):
    n=1<<logn
    t=np.arange(n)
    rng=np.random.default_rng(832+logn)
    x=(.37*np.exp(2j*np.pi*7*t/n)+.11*np.exp(-2j*np.pi*11*t/n)+
       .015*(rng.normal(size=n)+1j*rng.normal(size=n)))
    i=np.rint(x.real*(1<<23)).astype(np.int64)
    q=np.rint(x.imag*(1<<23)).astype(np.int64)
    pack_mem(output/f"fft{logn}_i.mem",i,24,1)
    pack_mem(output/f"fft{logn}_q.mem",q,24,1)
    np.savez_compressed(output/f"fft{logn}_reference.npz",i=i,q=q,logn=logn)


def d8_vectors(output,zoom):
    samples=8192 if zoom==0 else (16384 if zoom==2 else 65536)
    rng=np.random.default_rng(224+zoom)
    x=rng.integers(-8192,8192,(samples,2),dtype=np.int64)
    y=x.copy()
    for stage,taps in enumerate([15,23,63]+[63]*zoom):
        h=halfband(taps)
        y=np.column_stack([quantize(np.convolve(y[:,axis],h)[:len(y):2],9 if stage==0 else 17)[0] for axis in range(2)])
    for axis,letter in enumerate(("i","q")):
        pack_mem(output/f"d8_z{zoom}_{letter}.mem",x[:,axis],16,4)
        pack_mem(output/f"d8_z{zoom}_e{letter}.mem",y[:,axis],24,1)


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--coeff-only",action="store_true")
    a=p.parse_args()
    coefficients(a.output)
    if not a.coeff_only:
        for logn in (7,8,9,10,15,16):fft_vectors(a.output,logn)
        for zoom in (0,2,6):d8_vectors(a.output,zoom)
    print("SA_RX_REFERENCES_GENERATED; not a hardware test")
