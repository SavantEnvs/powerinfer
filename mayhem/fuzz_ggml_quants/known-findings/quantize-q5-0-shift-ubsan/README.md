# ggml_quantize_q5_0: shift exponent too large (UBSan, ggml.c:19166)

Reproduce: `/mayhem/fuzz_ggml_quants-standalone mayhem/fuzz_ggml_quants/known-findings/quantize-q5-0-shift-ubsan/reproducer`

**Cause:** the histogram loop in `ggml_quantize_q5_0` runs `j` over `0..QK5_0` and computes `1u << (j + 16)`; for `j >= 16` the exponent reaches 32 or more, which is undefined for a 32-bit `unsigned`. It is reached from `ggml_quantize_chunk(GGML_TYPE_Q5_0, ...)` with ordinary finite weights.

**Fix:** iterate `j` over `QK5_0/2` (the nibble index) so the shift exponents stay below 32.
