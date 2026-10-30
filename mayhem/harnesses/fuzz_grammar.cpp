// Fuzz the GBNF grammar parser and the grammar-constrained sampling path.
// Input = grammar text, a NUL byte, then bytes that select candidate tokens for sampling steps.
#include "llama.h"
#include "grammar-parser.h"
#include "scratch.h"

#include <cmath>
#include <cstring>
#include <string>
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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    if (size < 1 || size > (1 << 14)) return -1;

    const uint8_t * nul = (const uint8_t *) memchr(data, 0, size);
    const size_t glen = nul ? (size_t) (nul - data) : size;
    const std::string text((const char *) data, glen);

    grammar_parser::parse_state parsed = grammar_parser::parse(text.c_str());
    if (parsed.rules.empty()) return -1;
    auto it = parsed.symbol_ids.find("root");
    if (it == parsed.symbol_ids.end()) return -1;

    std::vector<const llama_grammar_element *> rules = parsed.c_rules();
    llama_grammar * grammar = llama_grammar_init(rules.data(), rules.size(), it->second);
    if (grammar == nullptr) return -1;

    const int n_vocab = llama_n_vocab(g_model);
    const uint8_t * sel = nul ? nul + 1 : data + size;
    const size_t n_sel = (size_t) (data + size - sel);

    // candidate set of 8 tokens per step, chosen by the trailing bytes
    for (size_t pos = 0; pos + 2 <= n_sel && pos < 64; pos += 16) {
        std::vector<llama_token_data> cand;
        for (size_t j = pos; j + 2 <= n_sel && j < pos + 16; j += 2) {
            llama_token id = (llama_token) (((sel[j] << 8) | sel[j + 1]) % n_vocab);
            cand.push_back({ id, 0.0f, 0.0f });
        }
        llama_token_data_array arr = { cand.data(), cand.size(), false };
        llama_sample_grammar(g_ctx, &arr, grammar);
        bool accepted = false;
        for (const auto & c : cand) {
            if (c.logit != -INFINITY && c.id != llama_token_eos(g_model)) {
                llama_grammar_accept_token(g_ctx, grammar, c.id);
                accepted = true;
                break;
            }
        }
        if (!accepted) break;
    }

    llama_grammar * copy = llama_grammar_copy(grammar);
    llama_grammar_free(copy);
    llama_grammar_free(grammar);
    return 0;
}
