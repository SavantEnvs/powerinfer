# ggml_quantize_q5_1: shift exponent too large (UBSan, ggml.c:19196)

Reproduce: `/mayhem/fuzz_ggml_quants-standalone mayhem/fuzz_ggml_quants/known-findings/quantize-q5-1-shift-ubsan/reproducer`

**Cause:** same pattern as `quantize-q5-0-shift-ubsan`: `ggml_quantize_q5_1` computes `1u << (j + 16)` with `j` up to `QK5_1 - 1`, so the exponent reaches 32 or more.

**Fix:** iterate `j` over `QK5_1/2` so the shift exponents stay below 32.
