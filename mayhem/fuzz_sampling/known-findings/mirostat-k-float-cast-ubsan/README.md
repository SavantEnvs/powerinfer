# llama_sample_token_mirostat: top-k size cast from an out-of-range float (UBSan, llama.cpp:8552)

Reproduce: `/mayhem/fuzz_sampling-standalone mayhem/fuzz_sampling/known-findings/mirostat-k-float-cast-ubsan/reproducer`

**Cause:** `k = powf(...)` is computed from the estimated Zipf exponent `s_hat`; it overflows `int` (here 4.6e9) or is NaN (when `sum_ti_sq == 0`, e.g. a single remaining candidate), and `int(k)` is passed to `llama_sample_top_k`. The inputs are ordinary finite logits and in-range `tau`/`eta`/`m`.

**Fix:** clamp `k` to `[1, candidates->size]` as a float before casting, and treat a non-finite `k` as 1.
