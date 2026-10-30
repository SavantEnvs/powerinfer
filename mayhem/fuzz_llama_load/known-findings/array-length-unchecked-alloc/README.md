# gguf_init_from_file: KV array length drives an unchecked malloc (ggml.c:19570, 19575)

Reproduce (the libFuzzer harness rejects inputs whose declared lengths exceed the file size, so use the harness built without that pre-check): `/mayhem/fuzz_llama_load-standalone-unchecked mayhem/fuzz_llama_load/known-findings/array-length-unchecked-alloc/reproducer`  (reached through `llama_load_model_from_file`; same parser root cause as the `fuzz_gguf_parse` finding of the same name)

**Cause:** `kv->value.arr.n` is read from the file and used as `malloc(n * element_size)` (19575 for scalar arrays, 19570 for string arrays via `n * sizeof(gguf_str)`) with no bound against the file size.

**Fix:** Reject an array length that cannot fit in the remaining file size before allocating.
