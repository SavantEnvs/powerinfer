# llama_model_load vocab: invalid llama_token_type value (UBSan, llama.cpp:2630)

Reproduce: `/mayhem/fuzz_llama_load-standalone mayhem/fuzz_llama_load/known-findings/token-type-ubsan/reproducer`

**Cause:** The `tokenizer.ggml.token_type` array from the file is reinterpreted as the `llama_token_type` enum with arbitrary int32 values.

**Fix:** Validate token_type values against the enum range (or store them as int32).
