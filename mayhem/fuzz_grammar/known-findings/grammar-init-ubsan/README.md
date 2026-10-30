# llama_grammar_init: null rule pointer dereferenced (UBSan, llama.cpp:8075)

Reproduce: `/mayhem/fuzz_grammar-standalone mayhem/fuzz_grammar/known-findings/grammar-init-ubsan/reproducer`

**Cause:** `for (pos = rules[i]; pos->type != END; ...)` dereferences a null rules[i] when the parsed grammar leaves a referenced rule undefined (empty rule vector gives a null c_rules() entry).

**Fix:** Reject undefined rule references in the parser, and validate the rules array in llama_grammar_init.
