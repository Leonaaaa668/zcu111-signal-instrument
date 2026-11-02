"""Strict decoder for the diagnostic RX snapshot v1, not receiver dBm calibration."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[2]/"src/fpga"

def validate_receipt(expected):
    keys={'ticket_id','epoch','segment_id','nco_update_seq','log_n','zoom','packet_sequence','counter_epoch'}
    if not isinstance(expected,dict) or set(expected)!=keys or any(type(v) is not int or v<0 for v in expected.values()):
        raise ValueError('Incomplete/invalid host capture receipt')


def decode_packet(data,expected=None):
    if len(data)<256:raise ValueError('Short header')
    h=struct.unpack('<64I',data[:256])
    if h[:3]!=(0x31504153,1,256) or h[3]!=len(data) or h[4]!=0x3f:
        raise ValueError('Packet magic/version/length/flags mismatch')
    n,logn,zoom,shift=h[11:15]
    if not (h[5] or h[6]) or not h[10] or (h[20]|h[21]<<32)!=1800000000 or (h[22]|h[23]<<32)!=0x400000000000:
        raise ValueError('Invalid sequence/ticket or unsupported NCO profile')
    if not 7<=logn<=16 or n!=1<<logn or zoom>6 or shift!=logn+1 or len(data)!=256+8*n:
        raise ValueError('Invalid FFT geometry/scale')
    if h[15:20]!=(1,2400000000,1200000000,150000000>>zoom,1200000000):
        raise ValueError('Unsupported sign/sample-rate contract')
    if h[23]>0xffff or h[32]!=0 or h[37]!=1 or any(h[38:40]) or h[44:50]!=(17,8,24,16,1,0) or any(h[50:56]):
        raise ValueError('Invalid quality/timestamp/units/reserved metadata')
    metrics=json.loads((ROOT/'coeff/sa/hann_metrics.json').read_text())[str(logn)]
    sumw=h[40]|h[41]<<32;sumsq=h[42]|h[43]<<32
    if sumw!=metrics['sum_q17'] or sumsq!=metrics['sum_square_q34']:
        raise ValueError('Window normalization differs')
    fingerprint=hashlib.sha256((ROOT/'coeff/sa/hann_q17.mem').read_bytes()).digest()
    if data[224:256]!=fingerprint:raise ValueError('Window coefficient fingerprint differs')
    state=h[36]
    if state&31!=15 or state&0x7fe01fe0 or ((state>>13)&31)!=logn or ((state>>18)&7)!=zoom:
        raise ValueError('Snapshot DSP state does not match packet')
    powers=struct.unpack('<'+'Q'*n,data[256:])
    checksum=0
    for power in powers:
        if power>1<<47:raise ValueError('Power exceeds signed24 complex square-sum range')
        checksum^=power
    if checksum!=(h[34]|h[35]<<32):raise ValueError('Payload XOR64 mismatch')
    start=h[26]|h[27]<<32;end=h[28]|h[29]<<32
    if end<start or (end-start)%4:raise ValueError('Invalid output-observation timebase')
    peak=max(range(n),key=powers.__getitem__)
    frequency=(peak if peak<n//2 else peak-n)*h[18]/n
    normalized=powers[peak]*(2**shift/(sumw/2**17)/2**23)**2
    result=dict(status='RX_DMA_PACKET_VALID',packet_bytes=len(data),packet_sha256=hashlib.sha256(data).hexdigest(),
        packet_sequence=h[5]|h[6]<<32,epoch=h[7],segment_id=h[8],nco_update_seq=h[9],ticket_id=h[10],
        fft_n=n,log_n=logn,zoom=zoom,fft_right_shift=shift,fs_complex_hz=h[18],
        requested_nco_hz=h[20]|h[21]<<32,nco_tuning_word_raw=h[22]|h[23]<<32,
        source_completed_frames_at_first_bin=h[24]|h[25]<<32,
        first_output_observed_adc_counter=start,last_output_observed_adc_counter=end,
        counter_epoch=h[30]|h[31]<<32,quality=h[32],discarded_requests=h[33],
        timestamp_semantics='FFT output observation; not analog sample-window start',
        vbw_mode='bypass',receiver_dbm_calibrated=False,peak_bin=peak,peak_digital_offset_hz=frequency,
        peak_rf_estimate_hz=1800000000+frequency,
        peak_coherent_bin_dbfs=10*math.log10(normalized) if normalized else None)
    if expected is not None:
        validate_receipt(expected)
        for key,value in expected.items():
            if key not in result or result[key]!=value:
                raise ValueError(f'Stale/mixed capture metadata: {key}')
    return result,powers


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',type=Path);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--expected-json',type=Path,help='Host receipt with exact expected ticket/epoch/sequence/config')
    args=parser.parse_args()
    if args.output.exists():raise ValueError('Use a fresh output directory')
    expected=json.loads(args.expected_json.read_text(encoding='utf-8-sig')) if args.expected_json else None
    result,powers=decode_packet(args.input.read_bytes(),expected)
    args.output.mkdir(parents=True)
    (args.output/'packet.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    (args.output/'power.csv').write_text('bin,power_uint64\n'+''.join(f'{k},{p}\n' for k,p in enumerate(powers)))
    print(json.dumps(result,indent=2))
