BEFORE

Performance
PerformancePut: 65535 ops, 223 us, 3.40 ns/op, checksum=21473853450
PerformanceGet: 65535 ops, 61 us, 0.93 ns/op, checksum=21473853450
PerformanceEraseHalf: 32768 ops, 73 us, 2.23 ns/op, checksum=21473853450
PerformanceReinsertHalf: 32768 ops, 141 us, 4.30 ns/op, checksum=21473853450
Performance PASS (526 µs)
Performance64
Performance64Put: 32768 ops, 472 us, 14.40 ns/op, checksum=5368545280
Performance64Get: 32768 ops, 359 us, 10.96 ns/op, checksum=5368545280
Performance64 PASS (837 µs)
PerformanceEntryLayout
PerformanceEntryLayout32To64: entry_size=24, capacity=4096, 65536 misses, 89 us, 1.36 ns/miss, checksum=402620416
PerformanceEntryLayout64To32: entry_size=16, capacity=4096, 65536 misses, 75 us, 1.14 ns/miss, checksum=402620416
PerformanceEntryLayout32ToStructPOD: entry_size=32, capacity=4096, 65536 misses, 79 us, 1.21 ns/miss, checksum=402620416
PerformanceEntryLayout PASS (78.348 ms)


AFTER

Performance
PerformancePut: 65535 ops, 242 us, 3.69 ns/op, checksum=21473853450
PerformanceGet: 65535 ops, 60 us, 0.92 ns/op, checksum=21473853450
PerformanceEraseHalf: 32768 ops, 71 us, 2.17 ns/op, checksum=21473853450
PerformanceReinsertHalf: 32768 ops, 136 us, 4.15 ns/op, checksum=21473853450
Performance PASS (560 µs)
Performance64
Performance64Put: 32768 ops, 455 us, 13.89 ns/op, checksum=5368545280
Performance64Get: 32768 ops, 360 us, 10.99 ns/op, checksum=5368545280
Performance64 PASS (898 µs)
PerformanceEntryLayout
PerformanceEntryLayout32To64: entry_size=16, capacity=4096, 65536 misses, 73 us, 1.11 ns/miss, checksum=402620416
PerformanceEntryLayout64To32: entry_size=16, capacity=4096, 65536 misses, 73 us, 1.11 ns/miss, checksum=402620416
PerformanceEntryLayout32ToStructPOD: entry_size=24, capacity=4096, 65536 misses, 88 us, 1.34 ns/miss, checksum=402620416
PerformanceEntryLayout PASS (377 µs)


ANDROID

Before

Performance
PerformancePut: 65535 ops, 220 us, 3.36 ns/op, checksum=21473853450
PerformanceGet: 65535 ops, 66 us, 1.01 ns/op, checksum=21473853450
PerformanceEraseHalf: 32768 ops, 83 us, 2.53 ns/op, checksum=21473853450
PerformanceReinsertHalf: 32768 ops, 145 us, 4.43 ns/op, checksum=21473853450
Performance PASS (577 µs)
Performance64
Performance64Put: 32768 ops, 431 us, 13.15 ns/op, checksum=5368545280
Performance64Get: 32768 ops, 347 us, 10.59 ns/op, checksum=5368545280
Performance64 PASS (804 µs)

After

Performance
PerformancePut: 65535 ops, 293 us, 4.47 ns/op, checksum=21473853450
PerformanceGet: 65535 ops, 77 us, 1.17 ns/op, checksum=21473853450
PerformanceEraseHalf: 32768 ops, 84 us, 2.56 ns/op, checksum=21473853450
PerformanceReinsertHalf: 32768 ops, 143 us, 4.36 ns/op, checksum=21473853450
Performance PASS (664 µs)
Performance64
Performance64Put: 32768 ops, 432 us, 13.18 ns/op, checksum=5368545280
Performance64Get: 32768 ops, 343 us, 10.47 ns/op, checksum=5368545280
Performance64 PASS (800 µs)

