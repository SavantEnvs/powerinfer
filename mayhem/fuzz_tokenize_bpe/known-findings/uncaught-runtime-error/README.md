# llama_tokenize (BPE): uncaught std::runtime_error terminates the process

Reproduce: `/mayhem/fuzz_tokenize_bpe-standalone mayhem/fuzz_tokenize_bpe/known-findings/uncaught-runtime-error/reproducer`

**Cause:** Same as above, runtime_error variant.

**Fix:** Same as above.
