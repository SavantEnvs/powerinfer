# llama_tokenize (BPE): uncaught std::exception terminates the process

Reproduce: `/mayhem/fuzz_tokenize_bpe-standalone mayhem/fuzz_tokenize_bpe/known-findings/uncaught-invalid-argument/reproducer`

**Cause:** The BPE tokenizer throws (byte not found in vocab / stoi invalid_argument) out of the C API `llama_tokenize`, which is not noexcept-safe; the exception reaches std::terminate.

**Fix:** Catch exceptions at the C API boundary and return a negative error, or make the BPE byte fallback total.
