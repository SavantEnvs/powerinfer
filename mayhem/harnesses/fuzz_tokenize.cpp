// Fuzz llama_tokenize / llama_token_to_piece against a real vocabulary. The vocabulary
// (VOCAB_SYMBOL, linked in as an object by build.sh) is spilled to a scratch file once at start-up.
#include "llama.h"
#include "scratch.h"

#include <cstring>
#include <vector>

extern "C" const unsigned char VOCAB_START[];
extern "C" const unsigned char VOCAB_END[];

static llama_model * g_model = nullptr;

static void quiet_log(enum ggml_log_level, const char *, void *) {}

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
    scratch_init();
    llama_backend_init(false);
    llama_log_set(quiet_log, nullptr);
    const char * path = scratch_write(VOCAB_START, (size_t) (VOCAB_END - VOCAB_START));
    llama_model_params mparams = llama_model_default_params();
    mparams.vocab_only = true;
    g_model = llama_load_model_from_file(path, mparams);
    if (g_model == nullptr) abort();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    if (size < 1 || size > (1 << 16)) return -1;

    const bool add_bos = data[0] & 1;
    const bool special = data[0] & 2;
    const char * text = (const char *) data + 1;
    const int text_len = (int) size - 1;

    std::vector<llama_token> tokens(text_len + 8);
    int n = llama_tokenize(g_model, text, text_len, tokens.data(), (int) tokens.size(), add_bos, special);
    if (n < 0) {
        tokens.resize(-n);
        n = llama_tokenize(g_model, text, text_len, tokens.data(), (int) tokens.size(), add_bos, special);
    }
    if (n < 0) return -1;

    char piece[256];
    for (int i = 0; i < n; i++) {
        int len = llama_token_to_piece(g_model, tokens[i], piece, (int) sizeof(piece));
        if (len < 0) {
            std::vector<char> big(-len);
            (void) llama_token_to_piece(g_model, tokens[i], big.data(), (int) big.size());
        }
    }
    return 0;
}
