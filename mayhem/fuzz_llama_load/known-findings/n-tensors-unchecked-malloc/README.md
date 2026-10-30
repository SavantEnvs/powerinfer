# llama_load_model_from_file: same unchecked gguf allocation reached through the model loader

Reproduce (the libFuzzer harness rejects headers that cannot fit in the file, so use the harness built without that pre-check): `/mayhem/fuzz_llama_load-standalone-unchecked mayhem/fuzz_llama_load/known-findings/n-tensors-unchecked-malloc/reproducer`

**Cause:** llama_model_loader calls gguf_init_from_file on untrusted input (ggml.c:19602, see fuzz_gguf_parse finding).

**Fix:** Fix in gguf_init_from_file.
