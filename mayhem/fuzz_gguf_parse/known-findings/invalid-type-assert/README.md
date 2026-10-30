# gguf_init_from_file: GGML_ASSERT abort on an unknown tensor type

Reproduce: `/mayhem/fuzz_gguf_parse-standalone mayhem/fuzz_gguf_parse/known-findings/invalid-type-assert/reproducer`

**Cause:** ggml.c:19584 `GGML_ASSERT(false && "invalid type")` aborts the process for a malformed KV type instead of returning NULL.

**Fix:** Return an error (free the context, return NULL) instead of asserting on file-controlled values.
