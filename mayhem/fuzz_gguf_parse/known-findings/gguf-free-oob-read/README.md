# gguf_free: reads uninitialised KV / array-string entries after a failed parse (ggml.c:19772, 19787)

Reproduce (the libFuzzer harness rejects inputs whose declared lengths exceed the file size, so use the harness built without that pre-check): `/mayhem/fuzz_gguf_parse-standalone-unchecked mayhem/fuzz_gguf_parse/known-findings/gguf-free-oob-read/reproducer`

**Cause:** gguf_init_from_file mallocs `ctx->kv` (n_kv entries) without zeroing it and, when a later entry fails to parse, calls gguf_free, which walks all `n_kv` entries and frees/derefs `key.data` and `arr.data` of the entries never filled in (ggml.c:19772 for the key, 19787/19791 for array strings).

**Fix:** `calloc` the kv array (and the array-of-string storage), or free only the entries parsed so far.
