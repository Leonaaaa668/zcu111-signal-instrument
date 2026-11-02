"""Independent stimulus/header/payload checks of Verilog snapshot simulation."""
import argparse
import csv
import json
from pathlib import Path
import struct
from decode_rx_dma import decode_packet


def main(directory):
    packets=[];current=[];packet_index=0
    with (directory/'packets.csv').open(newline='') as stream:
        for row in csv.DictReader(stream):
            if int(row['packet'])!=packet_index or int(row['beat'])!=len(current):raise ValueError('Output order/gap')
            current.append(struct.pack('<Q',int(row['data_hex'],16)))
            if int(row['last']):packets.append(b''.join(current));current=[];packet_index+=1
    if current or len(packets)!=6:raise ValueError('Truncated/extra packet')
    results=[]
    for raw,(ticket,n,sequence) in zip(packets,[(1,128,1),(2,1024,2),(3,65536,3),(4,128,4),(5,128,5),(20,128,1)]):
        info,powers=decode_packet(raw)
        if (info['ticket_id'],info['fft_n'],info['packet_sequence'],info['epoch'],info['segment_id'],info['nco_update_seq'])!=(ticket,n,sequence,23,2,1):
            raise ValueError('Ticket/order/metadata mixed between buffers')
        if info['requested_nco_hz']!=1800000000 or info['nco_tuning_word_raw']!=0x400000000000:
            raise ValueError('NCO metadata changed')
        if info['source_completed_frames_at_first_bin']!=1000+2*ticket:raise ValueError('Source frame metadata mixed')
        expected_values=tuple((1<<47) if k==0 else 0 if k==1 else (1<<46)+((k&16383)<<32)+33*k+ticket for k in range(n))
        if tuple(powers)!=expected_values:
            raise ValueError('Power RAM/AXIS payload differs from stimulus')
        results.append(dict(ticket=ticket,n=n,sequence=sequence,sha256=info['packet_sha256']))
    # Actual packet corruption must be rejected, not merely framing in TB.
    changes=[(0,1),(4,1),(8,1),(12,1),(16,1),(44,1),(48,1),(60,1),(128,1),(160,1),(176,1),(224,1),(256,1)]
    for offset,mask in changes:
        raw=bytearray(packets[0]);raw[offset]^=mask
        try:decode_packet(raw)
        except ValueError:pass
        else:raise ValueError(f'Corrupt packet accepted at byte{offset}')
    try:decode_packet(packets[0][:-8])
    except ValueError:pass
    else:raise ValueError('DMA short packet accepted')
    result=dict(status='SA_SNAPSHOT_NUMERIC_PACKET_CHECK_PASS',packets=results,corruption_cases=len(changes)+1,
                scope='RTL simulation only; not a physical DMA/board test')
    (directory/'packet_verification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);main(p.parse_args().directory)
