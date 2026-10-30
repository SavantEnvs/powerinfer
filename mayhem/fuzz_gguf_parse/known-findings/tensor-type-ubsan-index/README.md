# gguf_init_from_file: out-of-range tensor type indexes type_traits (UBSan, ggml.c:1986)

Reproduce: `/mayhem/fuzz_gguf_parse-standalone mayhem/fuzz_gguf_parse/known-findings/tensor-type-ubsan-index/reproducer`

**Cause:** A tensor-info type field read from the file is passed to ggml_blck_size() without range checking against GGML_TYPE_COUNT.

**Fix:** Validate `info->type < GGML_TYPE_COUNT` right after reading it.
