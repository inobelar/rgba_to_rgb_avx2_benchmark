# Benchmark: RGBA to RGB copying using AVX2

Usage:

```shell
$ g++ -v -std=c++11 -O3 -march=native -mtune=native -mavx2 -DNDEBUG -I./third_party/nanobench/include -o bench main.cpp
$ ./bench
```

For benchmarking used: [nanobench](https://github.com/martinus/nanobench)

--------------------------------------------------------------------------------

## Stats (`Intel(R) Core(TM) i3-6300 CPU @ 3.80GHz`)

<details> 
  <summary><code>$ lscpu</code></summary>

  ```
  Architecture:                         x86_64
  CPU op-mode(s):                       32-bit, 64-bit
  Address sizes:                        39 bits physical, 48 bits virtual
  Byte Order:                           Little Endian
  CPU(s):                               4
  On-line CPU(s) list:                  0-3
  Vendor ID:                            GenuineIntel
  Model name:                           Intel(R) Core(TM) i3-6300 CPU @ 3.80GHz
  CPU family:                           6
  Model:                                94
  Thread(s) per core:                   2
  Core(s) per socket:                   2
  Socket(s):                            1
  Stepping:                             3
  CPU max MHz:                          3800,0000
  CPU min MHz:                          800,0000
  BogoMIPS:                             7599.80
  Flags:                                fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc cpuid aperfmperf pni pclmulqdq dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase tsc_adjust bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm arat pln pts hwp hwp_notify hwp_act_window hwp_epp md_clear flush_l1d arch_capabilities
  Virtualization:                       VT-x
  L1d cache:                            64 KiB (2 instances)
  L1i cache:                            64 KiB (2 instances)
  L2 cache:                             512 KiB (2 instances)
  L3 cache:                             4 MiB (1 instance)
  NUMA node(s):                         1
  NUMA node0 CPU(s):                    0-3
  Vulnerability Gather data sampling:   Vulnerable: No microcode
  Vulnerability Itlb multihit:          KVM: Mitigation: VMX disabled
  Vulnerability L1tf:                   Mitigation; PTE Inversion; VMX conditional cache flushes, SMT vulnerable
  Vulnerability Mds:                    Mitigation; Clear CPU buffers; SMT vulnerable
  Vulnerability Meltdown:               Mitigation; PTI
  Vulnerability Mmio stale data:        Mitigation; Clear CPU buffers; SMT vulnerable
  Vulnerability Reg file data sampling: Not affected
  Vulnerability Retbleed:               Mitigation; IBRS
  Vulnerability Spec rstack overflow:   Not affected
  Vulnerability Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl and seccomp
  Vulnerability Spectre v1:             Mitigation; usercopy/swapgs barriers and __user pointer sanitization
  Vulnerability Spectre v2:             Mitigation; IBRS; IBPB conditional; STIBP conditional; RSB filling; PBRSB-eIBRS Not affected; BHI Not affected
  Vulnerability Srbds:                  Mitigation; Microcode
  Vulnerability Tsx async abort:        Not affected
  ```
</details>

Iterations count: 1000

| relative |               ns/op |                op/s |    err% |     total | RGBA to RGB
|---------:|--------------------:|--------------------:|--------:|----------:|:------------
|   100.0% |        1,864,929.20 |              536.21 |    0.9% |     22.65 | `memcpy (1 pixel)`
|   199.7% |          933,888.46 |            1,070.79 |    1.8% |     11.47 | `raw_pointers (1 pixel)`
|   199.3% |          935,789.45 |            1,068.62 |    3.6% |     11.49 | `raw_pointers (4 pixels)`
|   200.8% |          928,520.33 |            1,076.98 |    3.1% |     11.30 | `avx2 (8 pixels)`
|   200.0% |          932,569.12 |            1,072.31 |    1.5% |     11.06 | `avx2 (16 pixels)`
|   208.9% |          892,919.83 |            1,119.92 |    3.2% |     11.03 | `avx2 (32 pixels)`
|   202.5% |          921,093.37 |            1,085.67 |    2.9% |     11.11 | `avx2 (64 pixels)`

--------------------------------------------------------------------------------
