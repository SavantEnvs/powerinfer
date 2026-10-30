# gguf_init_from_file: n_dims from the file indexes past info->ne[4] (UBSan, ggml.c:19614)

Reproduce (the libFuzzer harness rejects inputs whose declared lengths exceed the file size, so use the harness built without that pre-check): `/mayhem/fuzz_gguf_parse-standalone-unchecked mayhem/fuzz_gguf_parse/known-findings/n-dims-ubsan/reproducer`

**Cause:** The loop at ggml.c:19614 iterates `j < info->n_dims` over a fixed `uint64_t ne[GGML_MAX_DIMS]` with a file-supplied n_dims (index 5 out of bounds for uint64_t[4]).

**Fix:** Reject n_dims > GGML_MAX_DIMS.
