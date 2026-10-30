// Fuzz llama's logit-sampling chain (penalties, top-k/p, min-p, tail-free, typical, temperature,
// softmax, greedy, random and mirostat selection) over a fuzzer-built candidate array.
// Input = 16 parameter bytes, then little-endian float32 logits (one candidate per float).
#include "llama.h"
#include "scratch.h"

#include <cmath>
#include <cstring>
#include <vector>

extern "C" const unsigned char VOCAB_START[];
extern "C" const unsigned char VOCAB_END[];

static llama_model * g_model = nullptr;
static llama_context * g_ctx = nullptr;

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
    g_ctx = llama_new_context_with_model(g_model, llama_context_default_params());
    if (g_ctx == nullptr) abort();
    return 0;
}

static float unit(uint8_t b) { return b / 255.0f; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    const size_t header = 16;
    if (size < header + sizeof(float) || size > (1 << 14)) return -1;

    const int n_vocab = llama_n_vocab(g_model);
    const size_t n = (size - header) / sizeof(float);

    std::vector<llama_token_data> cand(n);
    std::vector<llama_token> last;
    for (size_t i = 0; i < n; i++) {
        float logit;
        memcpy(&logit, data + header + i * sizeof(float), sizeof(float));
        if (!std::isfinite(logit)) return -1; // real logits are finite
        cand[i] = { (llama_token) ((i * 7919 + data[0]) % n_vocab), logit, 0.0f };
        if (i < 32 && (data[1] >> (i & 7)) & 1) last.push_back(cand[i].id);
    }
    llama_token_data_array arr = { cand.data(), cand.size(), false };

    const int    top_k    = 1 + data[2] % (n + 1);   // callers (common/sampling.cpp) pass k >= 1, min_keep >= 1
    const size_t min_keep = 1 + data[3] % 4;
    const float  top_p    = unit(data[4]);
    const float  min_p    = unit(data[5]) * 0.5f;
    const float  tfs_z    = unit(data[6]);
    const float  typ_p    = unit(data[7]);
    const float  temp     = unit(data[8]) * 3.0f;
    const float  rep      = 0.5f + unit(data[9]) * 1.5f;
    const float  freq     = unit(data[10]) - 0.5f;
    const float  pres     = unit(data[11]) - 0.5f;
    const int    mode     = data[12] % 5;
    const float  tau      = unit(data[13]) * 10.0f;
    const float  eta      = unit(data[14]);
    const int    m        = 1 + data[15] % 100;

    llama_sample_repetition_penalties(g_ctx, &arr, last.data(), last.size(), rep, freq, pres);
    llama_sample_top_k(g_ctx, &arr, top_k, min_keep);
    llama_sample_tail_free(g_ctx, &arr, tfs_z, min_keep);
    llama_sample_typical(g_ctx, &arr, typ_p, min_keep);
    llama_sample_top_p(g_ctx, &arr, top_p, min_keep);
    llama_sample_min_p(g_ctx, &arr, min_p, min_keep);
    llama_sample_temp(g_ctx, &arr, temp);
    llama_sample_softmax(g_ctx, &arr);
    if (arr.size == 0) return 0;

    float mu = 2.0f * tau;
    switch (mode) {
        case 0: (void) llama_sample_token_greedy(g_ctx, &arr); break;
        case 1: (void) llama_sample_token(g_ctx, &arr); break;
        case 2: (void) llama_sample_token_mirostat(g_ctx, &arr, tau, eta, m, &mu); break;
        case 3: (void) llama_sample_token_mirostat_v2(g_ctx, &arr, tau, eta, &mu); break;
        default: break;
    }
    return 0;
}
