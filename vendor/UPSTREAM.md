# Vendored sources

Files under `beads/` and `stmlib/` were copied verbatim from:

| Tree | Repository | Commit |
|---|---|---|
| `beads/dsp/` | https://github.com/pichenettes/eurorack (`beads/dsp/`) | f4cfa7697095a28d292bff758beb51deb49b5a86 |
| `stmlib/` | https://github.com/pichenettes/stmlib | d18def816c51d1da0c108236928b2bbd25c17481 |

The first commit that adds them is a pure copy. Every later change is a patch
marked with a one-line `// Beets:` comment stating what changed and why;
`git log -p vendor/` shows each one against the original text.
