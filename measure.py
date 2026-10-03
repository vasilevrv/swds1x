#!/usr/bin/env python3
"""Capture HLW8032 UART (4800 8E1) or decode a binary capture.

Diagnostic interpretation follows this project's hlw8032.c. PF deltas are
raw modulo differences, NOT validated energy or proof of a counter wrap.
"""
import argparse
import json
from pathlib import Path
import sys
import time


def header_valid(frame):
    return frame[1] == 0x5A and (frame[0] in (0x55, 0xAA) or frame[0] & 0xF0 == 0xF0)


class Decoder:
    def __init__(self, voltage_coefficient, current_coefficient):
        self.kv = voltage_coefficient
        self.ki = current_coefficient
        self.buffer = bytearray()
        self.bytes = self.frames = self.bad_checksum = self.skipped = 0
        self.previous_pf = None
        self.preview = bytearray()

    def feed(self, data):
        self.bytes += len(data)
        self.preview.extend(data[:max(0, 96 - len(self.preview))])
        self.buffer.extend(data)
        while len(self.buffer) >= 24:
            frame = bytes(self.buffer[:24])
            if not header_valid(frame):
                del self.buffer[0]
                self.skipped += 1
                continue
            expected = sum(frame[2:23]) & 0xFF
            if expected != frame[23]:
                self.bad_checksum += 1
                print(json.dumps({
                    'event': 'checksum_error', 'raw': frame.hex(' '),
                    'expected': expected, 'received': frame[23],
                }), flush=True)
                del self.buffer[0]
                self.skipped += 1
                continue
            del self.buffer[:24]
            self.frames += 1
            self.decode(frame)

    def decode(self, frame):
        state, update = frame[0], frame[20]
        words = [int.from_bytes(frame[i:i + 3], 'big') for i in range(2, 20, 3)]
        vp, vr, ip, ir, pp, pr = words
        overflow = 0 if state == 0x55 else state
        chip_error = state == 0xAA or (state != 0x55 and bool(state & 1))
        parameters_ok = bool(vp and ip and pp)
        notes = []
        if chip_error:
            notes.append('Firmware rejects chip status')
        if not parameters_ok:
            notes.append('Firmware rejects zero calibration parameter')

        # Show values updated in THIS frame. Firmware also caches previous
        # register updates for up to 2 seconds, unlike this diagnostic output.
        def measurement(name, parameter, register, update_bit, overflow_bit, coefficient, maximum):
            if chip_error or not parameters_ok:
                return None
            if overflow & (1 << overflow_bit):
                notes.append(f'{name}: overflow flag; firmware uses zero')
                return 0.0
            if not update & (1 << update_bit):
                notes.append(f'{name}: not updated in this frame')
                return None
            if not register:
                notes.append(f'{name}: zero measurement register')
                return None
            value = parameter * coefficient / register
            if value > maximum:
                notes.append(f'{name}: exceeds firmware limit')
            return value

        voltage = measurement('voltage', vp, vr, 6, 3, self.kv, 1000)
        current = measurement('current', ip, ir, 5, 2, self.ki, 1000)
        power = measurement('power', pp, pr, 4, 1, self.kv * self.ki, 25000)
        apparent = voltage * current if voltage is not None and current is not None else None
        factor = min(power / apparent, 1.0) if apparent and power is not None else None
        pf16 = int.from_bytes(frame[21:23], 'big')
        pf17 = pf16 | (0x10000 if update & 0x80 else 0)
        delta = None if self.previous_pf is None else (pf17 - self.previous_pf) & 0x1FFFF
        self.previous_pf = None if chip_error or not parameters_ok else pf17
        print(json.dumps({
            'event': 'frame', 'index': self.frames, 'checksum_ok': True,
            'raw': frame.hex(' '), 'status': f'0x{state:02X}',
            'update': f'0x{update:02X}', 'parameters_ok': parameters_ok,
            'registers': {
                'voltage_parameter': vp, 'voltage_register': vr,
                'current_parameter': ip, 'current_register': ir,
                'power_parameter': pp, 'power_register': pr,
            },
            'updated': {'voltage': bool(update & 0x40), 'current': bool(update & 0x20), 'power': bool(update & 0x10)},
            'voltage_V': voltage, 'current_A': current, 'power_W': power,
            'apparent_power_VA': apparent, 'power_factor': factor,
            'pf_reg_16': pf16, 'pf_carry': bool(update & 0x80),
            'pf_counter_17': pf17, 'pf_delta_modulo_unvalidated': delta,
            'notes': notes,
        }, allow_nan=False), flush=True)

    def summary(self):
        print(json.dumps({
            'event': 'summary', 'bytes': self.bytes, 'valid_frames': self.frames,
            'checksum_errors': self.bad_checksum, 'skipped_bytes': self.skipped,
            'unparsed_tail_bytes': len(self.buffer), 'first_bytes_hex': self.preview.hex(' '),
        }), flush=True)


def positive_coefficient(value):
    import math
    number = float(value)
    if not math.isfinite(number) or not 0 < number <= 1000:
        raise argparse.ArgumentTypeError('coefficient must be finite and in (0, 1000]')
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--port', help='USB-UART port, e.g. /dev/cu.wchusbserial10')
    source.add_argument('--input', type=Path, help='Decode an existing binary capture; no UART needed')
    parser.add_argument('--seconds', type=float, default=10, help='Capture duration; 0 = until Ctrl+C (default: 10)')
    parser.add_argument('--raw', type=Path, help='Save raw bytes, default for UART: hlw8032_capture.bin')
    parser.add_argument('--voltage-coefficient', type=positive_coefficient, default=1.995)
    parser.add_argument('--current-coefficient', type=positive_coefficient, default=1.0)
    args = parser.parse_args()
    import math
    if not math.isfinite(args.seconds) or args.seconds < 0:
        parser.error('--seconds must be finite and >= 0')
    if args.input and args.raw and args.input.resolve() == args.raw.resolve():
        parser.error('--input and --raw must be different files')
    decoder = Decoder(args.voltage_coefficient, args.current_coefficient)
    try:
        if args.input:
            with args.input.open('rb') as stream:
                while data := stream.read(4096):
                    decoder.feed(data)
        else:
            try:
                import serial
            except ImportError:
                parser.error('pyserial is required: python3 -m pip install pyserial')
            raw_path = args.raw or Path('hlw8032_capture.bin')
            # Exclusive creation prevents accidental loss of a previous capture.
            with raw_path.open('xb') as raw:
                with serial.Serial(args.port, baudrate=4800, bytesize=serial.EIGHTBITS,
                                   parity=serial.PARITY_EVEN, stopbits=serial.STOPBITS_ONE,
                                   timeout=0.1, rtscts=False, dsrdtr=False) as port:
                    started = time.monotonic()
                    while not args.seconds or time.monotonic() - started < args.seconds:
                        data = port.read(max(1, min(port.in_waiting, 4096)))
                        if data:
                            raw.write(data)
                            decoder.feed(data)
    except KeyboardInterrupt:
        pass
    except Exception as error:
        print(f'Capture failed: {error}', file=sys.stderr)
        return 1
    finally:
        decoder.summary()
    return 0 if decoder.frames else 2


if __name__ == '__main__':
    sys.exit(main())

