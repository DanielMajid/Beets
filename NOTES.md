# Beets — measurement log

Every number here is either **measured** (stated how) or explicitly marked
**PENDING (hardware)**. Nothing in the "measured" column is inferred.

## Toolchain

The SDK pins `gcc-arm-none-eabi-10.3-2021.10`. Its download host
(developer.arm.com) is blocked from the build container, so the builds logged
here use **Ubuntu `gcc-arm-none-eabi` 13.2.1 (13.2.rel1-2)** with identical
flags from the SDK Makefile. Code size from GCC 10.3 will differ by a few
percent; rebuild with the SDK toolchain (`docker/run_interactive.sh`, then
`build nts-1_mkii/beets`) for release numbers.

The `size` "text" column includes ELF headers, `.dynsym`/`.dynstr`, `.rel.*`,
`.unit_header` (0x330 bytes) and rodata — i.e. the loaded image minus data/bss.

## Size log (text / data / bss, from `arm-none-eabi-size`)

| Phase | Build | text | data | bss | total |
|---|---|---:|---:|---:|---:|
| 0 | dummy-delfx copied and renamed | 4318 | 300 | 72 | 4690 |
