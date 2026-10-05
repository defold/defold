| Workload | Direct UPS | Serialized UPS | Barrier UPS | Overlap UPS | Overlap vs barrier | Overlap vs direct |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 30k bunnies | 77.55 | 60.00 | 59.82 | 59.98 | +0.3% | -22.7% |
| 50k sprites | 51.12 | 41.40 | 40.24 | 40.22 | -0.1% | -21.3% |
| Balanced | 63.89 | 54.15 | 56.10 | 56.00 | -0.2% | -12.4% |
| 200 geometry groups | 120.00 | 119.97 | 119.99 | 120.00 | +0.0% | -0.0% |
| 1,000 geometry groups | 65.46 | 60.01 | 67.10 | 83.06 | +23.8% | +26.9% |

| Workload / mode | UPS range | Mean p99 (ms) | Live allocations (MiB) | WASM capacity (MiB) | Owned frame capacity (MiB) | Complete preparations during consumption¹ |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 30k bunnies / Direct | 77.06–77.93 | 14.60 | 59.02 | 79.81 | 0.00 | 0 |
| 30k bunnies / Serialized | 60.00–60.00 | 18.13 | 64.03 | 79.81 | 0.00 | 0 |
| 30k bunnies / Preparation barrier | 59.50–60.00 | 20.62 | 75.02 | 95.81 | 11.01 | 0 |
| 30k bunnies / Full overlap | 59.93–60.00 | 18.88 | 75.02 | 95.81 | 11.01 | 0 |
| 50k sprites / Direct | 50.92–51.25 | 21.10 | 65.79 | 95.81 | 0.00 | 0 |
| 50k sprites / Serialized | 40.98–41.76 | 26.35 | 70.79 | 95.81 | 0.00 | 0 |
| 50k sprites / Preparation barrier | 40.00–40.41 | 28.48 | 82.78 | 115.00 | 11.74 | 0 |
| 50k sprites / Full overlap | 39.98–40.44 | 27.03 | 82.71 | 115.00 | 11.74 | 0 |
| Balanced / Direct | 63.72–64.00 | 19.20 | 49.50 | 66.50 | 0.00 | 0 |
| Balanced / Serialized | 53.14–55.41 | 25.63 | 54.04 | 66.50 | 0.00 | 0 |
| Balanced / Preparation barrier | 55.31–57.21 | 26.38 | 57.41 | 66.50 | 2.95 | 0 |
| Balanced / Full overlap | 55.68–56.24 | 26.22 | 56.98 | 66.50 | 2.95 | 0 |
| 200 geometry groups / Direct | 119.99–120.01 | 9.97 | 23.78 | 32.00 | 0.00 | 0 |
| 200 geometry groups / Serialized | 119.92–120.00 | 10.33 | 28.82 | 38.44 | 0.00 | 0 |
| 200 geometry groups / Preparation barrier | 119.97–120.00 | 10.13 | 30.26 | 38.44 | 1.38 | 0 |
| 200 geometry groups / Full overlap | 119.99–120.00 | 10.23 | 30.26 | 38.44 | 1.38 | 4957 |
| 1,000 geometry groups / Direct | 64.65–66.16 | 17.12 | 34.26 | 46.12 | 0.00 | 0 |
| 1,000 geometry groups / Serialized | 60.00–60.03 | 18.52 | 39.29 | 46.12 | 0.00 | 0 |
| 1,000 geometry groups / Preparation barrier | 65.90–67.83 | 18.68 | 45.93 | 55.38 | 6.44 | 0 |
| 1,000 geometry groups / Full overlap | 82.92–83.22 | 14.53 | 45.94 | 55.38 | 6.44 | 3481 |

¹ Sum of counter differences between the first and last measurement samples, across 3 runs. This excludes the warmup and undersamples the edges of measurement. Partial overlap does not count. Zero is not proof of no overlap.
