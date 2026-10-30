# gguf_init_from_file: signed overflow multiplying the tensor dimensions (ggml.c:19655)

Reproduce: `/mayhem/fuzz_gguf_parse-standalone mayhem/fuzz_gguf_parse/known-findings/tensor-ne-signed-overflow/reproducer`

**Cause:** `ne[0]*ne[1]*ne[2]*ne[3]` is computed in int64_t from four file-supplied uint64 dimensions, which overflows.

**Fix:** Validate each dimension (and the product) with overflow-checked arithmetic.
