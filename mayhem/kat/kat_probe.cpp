// Known-answer probe for mayhem/test.sh: prints values computed by the real library so the
// shell side can compare them against exact expected strings.
#include "ggml.h"
#include "llama.h"
#include "grammar-parser.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static void quiet_log(enum ggml_log_level, const char *, void *) {}

static llama_model * load_vocab(const char * path) {
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true;
    return llama_load_model_from_file(path, mp);
}

static void print_tokens(llama_model * model, const char * label, const std::string & text) {
    std::vector<llama_token> toks(text.size() + 8);
    int n = llama_tokenize(model, text.data(), (int) text.size(), toks.data(), (int) toks.size(), false, false);
    printf("tokens[%s]=", label);
    for (int i = 0; i < n; i++) printf("%s%d", i ? "," : "", toks[i]);
    printf("\n");
}

int main(int argc, char ** argv) {
    if (argc < 3) { fprintf(stderr, "usage: kat_probe <llama-vocab> <falcon-vocab>\n"); return 2; }
    llama_backend_init(false);
    llama_log_set(quiet_log, nullptr);

    {
        grammar_parser::parse_state st = grammar_parser::parse(
            "root  ::= (expr \"=\" term \"\\n\")+\nexpr  ::= term ([-+*/] term)*\nterm  ::= [0-9]+");
        printf("grammar symbols=%zu rules=%zu root=%u term=%u\n",
               st.symbol_ids.size(), st.rules.size(), st.symbol_ids["root"], st.symbol_ids["term"]);
    }

    {
        struct ggml_context * ctx = nullptr;
        struct gguf_init_params gp = { true, &ctx };
        struct gguf_context * g = gguf_init_from_file(argv[1], gp);
        if (!g) { printf("gguf=FAILED\n"); return 1; }
        int idx = gguf_find_key(g, "tokenizer.ggml.model");
        printf("gguf version=%u n_kv=%d tokenizer=%s\n", gguf_get_version(g), gguf_get_n_kv(g),
               idx >= 0 ? gguf_get_val_str(g, idx) : "?");
        gguf_free(g);
        if (ctx) ggml_free(ctx);
    }

    llama_model * spm = load_vocab(argv[1]);
    llama_model * bpe = load_vocab(argv[2]);
    if (!spm || !bpe) { printf("vocab=FAILED\n"); return 1; }
    printf("n_vocab spm=%d bpe=%d\n", llama_n_vocab(spm), llama_n_vocab(bpe));
    print_tokens(spm, "spm", " Hello world");
    print_tokens(bpe, "bpe", "Hello world");

    {
        llama_context * ctx = llama_new_context_with_model(spm, llama_context_default_params());
        grammar_parser::parse_state st = grammar_parser::parse("root ::= [0-9]+");
        std::vector<const llama_grammar_element *> rules = st.c_rules();
        llama_grammar * gr = llama_grammar_init(rules.data(), rules.size(), st.symbol_ids["root"]);
        const char * words[] = { "1", "a", "23" };
        std::vector<llama_token_data> cand;
        for (const char * w : words) {
            llama_token t;
            int n = llama_tokenize(spm, w, (int) std::string(w).size(), &t, 1, false, false);
            if (n < 1) { std::vector<llama_token> tt(8); n = llama_tokenize(spm, w, (int) std::string(w).size(), tt.data(), 8, false, false); t = tt[n - 1]; }
            cand.push_back({ t, 0.0f, 0.0f });
        }
        llama_token_data_array arr = { cand.data(), cand.size(), false };
        llama_sample_grammar(ctx, &arr, gr);
        printf("grammar-sample keep=");
        for (auto & c : cand) printf("%d", c.logit == -INFINITY ? 0 : 1);
        printf("\n");
        llama_grammar_free(gr);
        llama_free(ctx);
    }
    llama_free_model(spm);
    llama_free_model(bpe);
    return 0;
}
