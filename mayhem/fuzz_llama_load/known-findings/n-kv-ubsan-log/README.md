# llama_model_loader: invalid enum value for llama_fver (UBSan, llama.cpp:1931)

Reproduce: `/mayhem/fuzz_llama_load-standalone mayhem/fuzz_llama_load/known-findings/n-kv-ubsan-log/reproducer`

**Cause:** The gguf version read from the file is converted to the `llama_fver` enum without a range check and then loaded as that enum value (10, not a valid enumerator).

**Fix:** Reject unsupported gguf versions before storing them in an enum.
