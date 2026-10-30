# gguf_init_from_file: heap-buffer-overflow writing a gguf_str into the KV array (ggml.c:19428)

Reproduce (the libFuzzer harness rejects headers that cannot fit in the file, so use the harness built without that pre-check): `/mayhem/fuzz_gguf_parse-standalone-unchecked mayhem/fuzz_gguf_parse/known-findings/fread-str-heap-overflow/reproducer`

**Cause:** The kv array is allocated from the file-supplied n_kv with an unchecked multiplication; gguf_fread_str writes `p->n` past the (overflow-shrunk) allocation.

**Fix:** Check n_kv for overflow and against the file size before allocating.
