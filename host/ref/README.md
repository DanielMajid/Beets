Reference material for host tests.

- `units_upstream.cc`, `units_tables_only.inc`: the original stmlib pitch
  tables (MIT, Emilie Gillet), used by `test_units.cc`.
- `test_fidelity.cc` is compiled twice: once against the untouched upstream
  trees (`make fidelity EURORACK=... STMLIB=...`, `-DUPSTREAM`) and once
  against `vendor/`. Both runs drive `beads::Processor` with the same inputs,
  parameters, seed / clock / freeze streams and 12-frame blocks, and the
  outputs are compared bit for bit.
