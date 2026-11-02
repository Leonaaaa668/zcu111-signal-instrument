"""Replay included measured packets and recompute published comparisons; read-only."""
from pathlib import Path
import hashlib
import json
import math
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'sim/python'))
from decode_rx_dma import decode_packet

def read(name):
    return json.loads((ROOT/name).read_text(encoding='utf-8-sig'))

def close(a, b, tolerance=1e-9):
    if not math.isclose(a, b, rel_tol=0, abs_tol=tolerance):
        raise ValueError(f'Evidence mismatch: {a} != {b}')

def main():
    checked = []
    for p in sorted((ROOT/'src/linux/apps/instrument_cpp/resources/fixtures').glob('*/spectrum.bin')):
        raw = p.read_bytes()
        receipt = read(p.relative_to(ROOT).as_posix()+'.receipt.json')
        # Receipts contain exact ticket/epoch/sequence, not inferred timestamps.
        packet, powers = decode_packet(raw, receipt)
        checked.append({'file': p.relative_to(ROOT).as_posix(), 'bins': len(powers),
                        'sha256': packet['packet_sha256'], 'receipt': 'PASS'})
    measurement = read('data/rx_loop65536/measurement/measurement.json')
    raw = (ROOT/'data/rx_loop65536/frame_0000.bin').read_bytes()
    packet, powers = decode_packet(raw, read('data/rx_loop65536/frame_0000.bin.receipt.json'))
    if packet['packet_sha256'] != measurement['packet_sha256'][0]:
        raise ValueError('Loopback raw packet differs from measured record')
    if packet['peak_bin'] != measurement['peak_bin']:
        raise ValueError('Loopback peak bin differs')
    close(packet['peak_rf_estimate_hz'], measurement['peak_bin_rf_hz'], .001)
    close(packet['peak_coherent_bin_dbfs'], measurement['peak_coherent_bin_dbfs'], 1e-7)
    close(measurement['bin_spacing_hz'], 150e6/65536)
    # Recompute affine train + independent holdout from recorded raw dBFS/reference readings.
    import csv
    profile = read('data/calibration/rx_live/calibration.json')
    with (ROOT/'data/calibration/points.csv').open(newline='', encoding='utf-8') as stream:
        points = {p['point_id']: p for p in csv.DictReader(stream)}
    residuals = {}
    for point_id in profile['training_point_ids']+profile['holdout_point_ids']:
        point = points[point_id]
        readings = [float(point['ref_dbm_'+str(i)]) for i in (1,2,3)]
        # Reference records average power in linear mW, then convert to dBm.
        reference = 10*math.log10(sum(10**(x/10) for x in readings)/len(readings))
        estimate = profile['gain_db_per_db']*float(point['dut_cw_signal_dbfs'])+profile['offset_db']
        residuals[point_id] = estimate-reference
    for point_id, expected in zip(profile['training_point_ids'], profile['training_errors_db']):
        close(residuals[point_id], expected, 1e-7)
    for point_id, expected in zip(profile['holdout_point_ids'], profile['holdout_errors_db']):
        close(residuals[point_id], expected, 1e-7)
    # Manual TX reference replicates: preserve the recorded arithmetic-dB statistic.
    tx = read('data/calibration/tx_current/tx_cw_4025g_verification.json')
    for point in tx['baseline_points']+[tx['independent_corrected_check']]:
        mean = sum(point['readings_dbm'])/len(point['readings_dbm'])
        close(mean, point['mean_dbm'])
        close(mean-point['target_dbm'], point['mean_difference_from_target_db'])
    live = read('data/calibration/rx_live/live_acceptance.json')
    close(live['native_saved_input_dbm']-live['reference_dbm'],live['native_minus_reference_db'])
    fft_cases = []
    for name in ('narrow09_1024_numeric.json','narrow09_8192_65536_numeric.json'):
        for case in read('sim/results/fft/'+name)['cases']:
            if not case['power_bit_exact'] or case['max_fft_component_error_lsb'] > case['fft_component_tolerance_lsb']:
                raise ValueError('Recorded FFT reference verification failed')
            fft_cases.append(case['n'])
    metrics = read('data/comparison/metrics.json')
    log = (ROOT/'report/validation/v16_20261101/board_test.log').read_text()
    match = re.search(r'displayed=(\d+) painted=(\d+) seconds=(\d+(?:\.\d+)?)',log)
    if not match:
        raise ValueError('No V16 stream result')
    close(int(match[1])/float(match[3]),metrics['v16_display_updates_per_second'])
    result = {'status':'V16_INCLUDED_EVIDENCE_REPLAY_PASS','date':'2026-11-02',
              'packet_receipts':checked,'loopback_bin_count':len(powers),
              'fft_recorded_cases':fft_cases,'rx_affine_residual_db':residuals,
              'rx_live_minus_reference_db':live['native_minus_reference_db'],
              'tx_corrected_holdout_mean_error_db':tx['independent_corrected_check']['mean_difference_from_target_db'],
              'v16_display_updates_per_second':metrics['v16_display_updates_per_second'],
              'scope':'Offline replay and arithmetic; no new RTL simulation, compilation, RF or hardware actions'}
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__ == '__main__':
    main()
