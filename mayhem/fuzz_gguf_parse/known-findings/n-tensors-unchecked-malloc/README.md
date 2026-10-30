# gguf_init_from_file: header n_tensors drives an unchecked malloc (allocation-size-too-big)

Reproduce (the libFuzzer harness rejects headers that cannot fit in the file, so use the harness built without that pre-check): `/mayhem/fuzz_gguf_parse-standalone-unchecked mayhem/fuzz_gguf_parse/known-findings/n-tensors-unchecked-malloc/reproducer`

**Cause:** ggml.c:19602 `malloc(n_tensors * sizeof(gguf_tensor_info))` uses the attacker-controlled uint64 count with no bound against the file size (nor overflow check).

**Fix:** Reject n_tensors/n_kv larger than the remaining file size / a sane maximum before allocating.
