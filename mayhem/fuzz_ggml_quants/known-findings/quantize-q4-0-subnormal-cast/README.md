# quantize_row_q4_0_reference: float-to-int8 cast of inf/NaN on subnormal weights (UBSan, ggml-quants.c:456)

Reproduce: `/mayhem/fuzz_ggml_quants-standalone mayhem/fuzz_ggml_quants/known-findings/quantize-q4-0-subnormal-cast/reproducer`

**Cause:** the block scale `d` is derived from the block's largest magnitude; when that magnitude is subnormal (or zero-adjacent) `id = 1.0f/d` overflows to `inf`, so `x*id` is `inf` or `NaN` and is cast to `int8_t`, which is out of range. The input is finite, bounded float32 weights.

**Fix:** treat `d` below `FLT_MIN` (or a non-finite `id`) as zero, as the `d ? ... : 0` guard already does for exactly zero.
