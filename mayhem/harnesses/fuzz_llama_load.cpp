// Fuzz llama_load_model_from_file in vocab_only mode: GGUF parsing + hparams + vocab loading.
#include "llama.h"
#include "scratch.h"

#include <cstring>

static void quiet_log(enum ggml_log_level, const char *, void *) {}

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
    scratch_init();
    llama_backend_init(false);
    llama_log_set(quiet_log, nullptr);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    if (size < 4 || memcmp(data, "GGUF", 4) != 0) return -1;
    if (!gguf_header_fits(data, size)) return -1;

    const char * path = scratch_write(data, size);

    llama_model_params mparams = llama_model_default_params();
    mparams.vocab_only = true;
    mparams.use_mmap = false;

    llama_model * model = llama_load_model_from_file(path, mparams);
    if (model == nullptr) return -1;
    (void) llama_n_vocab(model);
    llama_free_model(model);
    return 0;
}
