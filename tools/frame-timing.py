"""Validate bounded Aurora CPU wall timestamps and summarize a settled window.

These measurements are not CPU execution time, GPU time, or physical scanout.
Percentiles use linear interpolation at (sample_count - 1) * percentile.
"""
import argparse
import json
import math
from pathlib import Path

PRODUCER_TIMES = ('begin_entry_ns', 'acquisition_complete_ns', 'record_start_ns',
                  'end_entry_ns', 'end_enqueue_complete_ns')
MISSING_METRICS = ('encoding_time', 'gpu_time', 'heap_allocations')


def integer(value, name, minimum=0, maximum=2**64 - 1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f'{name} must be an integer in [{minimum}, {maximum}]')
    return value


def timestamp(value, name):
    return integer(value, name, minimum=-(2**63), maximum=2**63 - 1)


def validate(capture):
    """Check evidence integrity independently of the chosen measurement window."""
    if not isinstance(capture, dict) or type(capture.get('schema')) is not int or capture['schema'] != 1:
        raise ValueError('expected frame timing schema 1')
    if capture.get('clock') != 'std::chrono::steady_clock' or capture.get('unit') != 'ns':
        raise ValueError('expected steady-clock timestamps in ns')
    semantics = capture.get('semantics')
    if not isinstance(semantics, dict) or any(key not in semantics or semantics[key] is not None
                                              for key in MISSING_METRICS):
        raise ValueError('unavailable encoding, GPU, and heap metrics must remain null')
    capacity = integer(capture.get('capacity_per_stream'), 'capacity_per_stream', minimum=1)
    for stream in ('producer', 'present'):
        rows = capture.get(stream)
        if not isinstance(rows, list):
            raise ValueError(f'{stream} must be an array')
        count = integer(capture.get(f'{stream}_count'), f'{stream}_count')
        dropped = integer(capture.get(f'{stream}_dropped'), f'{stream}_dropped')
        if count != len(rows) or count > capacity or (dropped and count != capacity):
            raise ValueError(f'{stream} count/capacity/drop mismatch')
    dropped = capture['producer_dropped'] + capture['present_dropped']
    if type(capture.get('truncated')) is not bool or capture['truncated'] != bool(dropped):
        raise ValueError('truncated flag does not match dropped records')
    attempts = integer(capture.get('begin_attempts'), 'begin_attempts')
    failed = integer(capture.get('failed_begins'), 'failed_begins')
    incomplete = integer(capture.get('incomplete_begins'), 'incomplete_begins')
    completed = capture['producer_count'] + capture['producer_dropped']
    if attempts != completed + failed + incomplete:
        raise ValueError('begin attempts do not match completed, failed, and incomplete begins')
    if capture['present_count'] + capture['present_dropped'] > completed:
        raise ValueError('more successful presents than completed producer frames')
    previous = None
    for row in capture['producer']:
        if not isinstance(row, dict):
            raise ValueError('producer row must be an object')
        frame_id = integer(row.get('frame_id'), 'producer frame_id', minimum=1)
        times = [timestamp(row.get(key), key) for key in PRODUCER_TIMES]
        if any(right < left for left, right in zip(times, times[1:])):
            raise ValueError('producer timestamps are reordered within a frame')
        if previous is not None:
            if frame_id <= previous['frame_id'] or times[0] <= previous['begin_entry_ns']:
                raise ValueError('producer IDs and begin timestamps must strictly increase')
            if times[0] < previous['end_enqueue_complete_ns']:
                raise ValueError('producer frames overlap on their single writer')
        previous = row
    previous = None
    for row in capture['present']:
        if not isinstance(row, dict):
            raise ValueError('present row must be an object')
        frame_id = integer(row.get('frame_id'), 'present frame_id', minimum=1)
        completion = timestamp(row.get('completion_ns'), 'completion_ns')
        if 'interval_ns' not in row:
            raise ValueError('present row is missing interval_ns')
        if previous is None:
            if row['interval_ns'] is not None:
                raise ValueError('first present interval must be null')
        else:
            interval = integer(row['interval_ns'], 'interval_ns', minimum=1)
            if frame_id <= previous['frame_id'] or completion <= previous['completion_ns']:
                raise ValueError('present IDs and completion timestamps must strictly increase')
            if interval != completion - previous['completion_ns']:
                raise ValueError('present interval differs from consecutive completion timestamps')
        previous = row


def distribution(values_ns):
    values = sorted(value / 1_000_000 for value in values_ns)
    if not values:
        return dict(count=0, min=None, max=None, p50=None, p95=None, p99=None)

    def percentile(fraction):
        index = (len(values) - 1) * fraction
        lower = int(index)
        upper = min(lower + 1, len(values) - 1)
        return values[lower] + (values[upper] - values[lower]) * (index - lower)

    return dict(count=len(values), min=values[0], max=values[-1],
                p50=percentile(.50), p95=percentile(.95), p99=percentile(.99))


def analyze(capture, start_seconds=20, duration_seconds=30):
    validate(capture)
    try:
        if (type(start_seconds) not in (int, float) or not math.isfinite(start_seconds) or start_seconds < 0 or
                type(duration_seconds) not in (int, float) or not math.isfinite(duration_seconds) or duration_seconds <= 0):
            raise ValueError('window start must be finite and nonnegative; duration must be finite and positive')
        offset_ns = round(start_seconds * 1_000_000_000)
        duration_ns = round(duration_seconds * 1_000_000_000)
    except OverflowError as error:
        raise ValueError('window arguments exceed the supported nanosecond range') from error
    if offset_ns > 2**63 - 1 or duration_ns > 2**63 - 1:
        raise ValueError('window arguments exceed the supported nanosecond range')
    if duration_ns <= 0:
        raise ValueError('window duration must cover at least one nanosecond')
    if capture['truncated']:
        raise ValueError('truncated source cannot qualify a measurement window')
    producer, present = capture['producer'], capture['present']
    if not producer or not present:
        raise ValueError('insufficient coverage: both producer and present records are required')
    origin = producer[0]['begin_entry_ns']
    start = origin + offset_ns
    end = start + duration_ns
    if producer[-1]['end_enqueue_complete_ns'] < end or present[-1]['completion_ns'] < end:
        raise ValueError('insufficient coverage for the complete requested window')
    selected = [row for row in producer if start <= row['begin_entry_ns'] and row['end_enqueue_complete_ns'] <= end]

    def intervals(rows, key):
        return [right[key] - left[key] for left, right in zip(rows, rows[1:])
                if start <= left[key] <= right[key] <= end]

    begin_intervals = intervals(producer, 'begin_entry_ns')
    present_intervals = intervals(present, 'completion_ns')
    if not selected or not begin_intervals or not present_intervals:
        raise ValueError('insufficient complete frames or consecutive intervals inside the requested window')
    spans = {
        'acquisition_wall': ('begin_entry_ns', 'acquisition_complete_ns'),
        'producer_record_wall': ('record_start_ns', 'end_entry_ns'),
        'end_enqueue_wall': ('end_entry_ns', 'end_enqueue_complete_ns'),
        'total_producer_wall': ('begin_entry_ns', 'end_enqueue_complete_ns'),
    }
    metrics = {name: distribution(row[right] - row[left] for row in selected)
               for name, (left, right) in spans.items()}
    metrics['consecutive_begin_interval'] = distribution(begin_intervals)
    metrics['successful_present_completion_interval'] = distribution(present_intervals)
    producer_ids = {row['frame_id'] for row in selected}
    selected_presents = [row for row in present if start <= row['completion_ns'] <= end]
    matched = sum(row['frame_id'] in producer_ids for row in selected_presents)
    return dict(schema=1, measurement='CPU monotonic wall timing; successful present-call completion intervals',
                unit='ms', percentile_method='linear interpolation at (count - 1) * percentile',
                window=dict(start_seconds=start_seconds, duration_seconds=duration_seconds,
                            origin_ns=origin, start_ns=start, end_ns=end),
                source=dict(producer_count=capture['producer_count'], present_count=capture['present_count'],
                            begin_attempts=capture['begin_attempts'], failed_begins=capture['failed_begins'],
                            incomplete_begins=capture['incomplete_begins'], truncated=False,
                            producer_end_ns=producer[-1]['end_enqueue_complete_ns'],
                            present_end_ns=present[-1]['completion_ns']),
                selected=dict(complete_producer_frames=len(selected), successful_presents=len(selected_presents),
                              matching_frame_ids=matched), metrics=metrics,
                encoding_time=None, gpu_time=None, heap_allocations=None,
                limitations=['Wall durations include waits and scheduling; they are not CPU execution time.',
                             'Successful present-call completion is not physical scanout or display latency.',
                             'Producer and render streams overlap; present may precede producer enqueue return.',
                             'Failed and incomplete begins are counted for the whole capture, not located within the window.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--start-seconds', type=float, default=20)
    parser.add_argument('--duration-seconds', type=float, default=30)
    args = parser.parse_args()
    try:
        capture = json.loads(args.input.read_text(encoding='utf-8-sig'))
        result = analyze(capture, args.start_seconds, args.duration_seconds)
        result['input'] = str(args.input)
        rendered = json.dumps(result, indent=2, allow_nan=False) + '\n'
        if args.output:
            args.output.write_text(rendered, encoding='utf-8')
        else:
            print(rendered, end='')
    except (ValueError, OSError, TypeError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
