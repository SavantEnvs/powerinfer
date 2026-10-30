# gguf_fread_str: string length from the file drives an unchecked calloc (ggml.c:19433)

Reproduce (the libFuzzer harness rejects inputs whose declared lengths exceed the file size, so use the harness built without that pre-check): `/mayhem/fuzz_gguf_parse-standalone-unchecked mayhem/fuzz_gguf_parse/known-findings/string-length-unchecked-alloc/reproducer`

**Cause:** `p->n` (uint64) is read from the file and passed to `calloc(p->n + 1, 1)` with no bound against the remaining file size, so any key/name/string value with a huge length aborts with allocation-size-too-big.

**Fix:** Reject a string length larger than the remaining file size before allocating.
