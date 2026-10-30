# llama_grammar_advance_stack: heap-buffer-overflow on an out-of-range rule reference (llama.cpp:7917)

Reproduce: `/mayhem/fuzz_grammar-standalone mayhem/fuzz_grammar/known-findings/grammar-rule-id-heap-overflow/reproducer`

**Cause:** rules[rule_id] is indexed with pos->value without checking it against the rule count; a grammar with a reference to an undefined rule parses but is not rejected.

**Fix:** Reject undefined rule references in grammar_parser::parse / llama_grammar_init.
