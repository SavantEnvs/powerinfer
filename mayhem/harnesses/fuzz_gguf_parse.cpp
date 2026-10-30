// Fuzz the GGUF container parser (ggml's gguf_init_from_file): header, KV table, tensor infos.
#include "ggml.h"
#include "scratch.h"

#include <cstring>

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
    scratch_init();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    if (size < 4 || memcmp(data, "GGUF", 4) != 0) return -1;
    if (!gguf_header_fits(data, size)) return -1;

    const char * path = scratch_write(data, size);

    struct ggml_context * ctx = nullptr;
    struct gguf_init_params params = { /*no_alloc =*/ true, /*ctx =*/ &ctx };
    struct gguf_context * gguf = gguf_init_from_file(path, params);
    if (gguf == nullptr) return -1;

    const int n_kv = gguf_get_n_kv(gguf);
    for (int i = 0; i < n_kv; i++) {
        (void) gguf_get_key(gguf, i);
        (void) gguf_get_kv_type(gguf, i);
    }
    const int n_tensors = gguf_get_n_tensors(gguf);
    for (int i = 0; i < n_tensors; i++) {
        (void) gguf_get_tensor_name(gguf, i);
        (void) gguf_get_tensor_offset(gguf, i);
    }

    gguf_free(gguf);
    if (ctx) ggml_free(ctx);
    return 0;
}
