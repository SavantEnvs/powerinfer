# gguf_init_from_file: modulo by a zero block size (ggml.c:19660)

Reproduce: `/mayhem/fuzz_gguf_parse-standalone mayhem/fuzz_gguf_parse/known-findings/tensor-blck-size-div-zero/reproducer`

**Cause:** `ne % ggml_blck_size(info->type)` with a file-supplied tensor type for which `ggml_blck_size` returns 0, so the modulo divides by zero.

**Fix:** Reject tensor types whose block size is 0 before the modulo.
