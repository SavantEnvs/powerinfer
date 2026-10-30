#!/usr/bin/env python3
"""Regenerate the starter seeds under mayhem/<target>/testsuite (stdlib only).

Usage: python3 mayhem/tools/gen_seeds.py   (from the repo root)
"""
import os
import random
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
U32, I32, F32, BOOL, STR, ARR = 4, 5, 6, 7, 8, 9


def s(b):
    b = b.encode() if isinstance(b, str) else b
    return struct.pack("<Q", len(b)) + b


def kv(key, typ, val):
    out = s(key) + struct.pack("<I", typ)
    if typ == STR:
        out += s(val)
    elif typ == U32:
        out += struct.pack("<I", val)
    elif typ == I32:
        out += struct.pack("<i", val)
    elif typ == F32:
        out += struct.pack("<f", val)
    elif typ == ARR:
        et, items = val
        out += struct.pack("<IQ", et, len(items))
        for it in items:
            if et == STR:
                out += s(it)
            elif et == F32:
                out += struct.pack("<f", it)
            elif et == I32:
                out += struct.pack("<i", it)
    return out


def gguf(kvs, tensors):
    head = b"GGUF" + struct.pack("<IQQ", 3, len(tensors), len(kvs)) + b"".join(kvs)
    infos, data, off = b"", b"", 0
    for name, dims, payload in tensors:
        infos += s(name) + struct.pack("<I", len(dims)) + b"".join(struct.pack("<Q", d) for d in dims)
        infos += struct.pack("<IQ", 0, off)  # type 0 = f32
        data += payload + b"\0" * ((-len(payload)) % 32)
        off += len(payload) + ((-len(payload)) % 32)
    blob = head + infos
    blob += b"\0" * ((-len(blob)) % 32)
    return blob + data


def tensor(name, dims):
    n = 1
    for d in dims:
        n *= d
    return (name, dims, struct.pack("<%df" % n, *[(i % 7) / 7.0 for i in range(n)]))


def write(path, data, mode="wb"):
    path = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, mode) as f:
        f.write(data)


def llama_like(arch, extra, tokenizer, tokens, scores=None, types=None, merges=None):
    kvs = [
        kv("general.architecture", STR, arch),
        kv("general.name", STR, "tiny"),
        kv(arch + ".context_length", U32, 64),
        kv(arch + ".embedding_length", U32, 8),
        kv(arch + ".block_count", U32, 1),
        kv(arch + ".feed_forward_length", U32, 16),
        kv(arch + ".attention.head_count", U32, 2),
    ] + extra + [
        kv("tokenizer.ggml.model", STR, tokenizer),
        kv("tokenizer.ggml.tokens", ARR, (STR, tokens)),
    ]
    if scores:
        kvs.append(kv("tokenizer.ggml.scores", ARR, (F32, scores)))
    if types:
        kvs.append(kv("tokenizer.ggml.token_type", ARR, (I32, types)))
    if merges:
        kvs.append(kv("tokenizer.ggml.merges", ARR, (STR, merges)))
    return gguf(kvs, [tensor("token_embd.weight", [8, len(tokens)])])


spm_tokens = ["<unk>", "<s>", "</s>", "<0x0A>", "▁", "▁a", "▁b", "ab", "c"]
llama_seed = llama_like(
    "llama",
    [kv("llama.attention.head_count_kv", U32, 2), kv("llama.attention.layer_norm_rms_epsilon", F32, 1e-5),
     kv("llama.rope.dimension_count", U32, 4)],
    "llama", spm_tokens, [0.0, 0.0, 0.0, 0.0, -1.0, -2.0, -3.0, -4.0, -5.0], [2, 3, 3, 6, 1, 1, 1, 1, 1])
bpe_tokens = ["<|endoftext|>", "a", "b", "c", "ab", "bc", "abc", "Ġ", "Ġa"]
falcon_seed = llama_like(
    "falcon",
    [kv("falcon.attention.head_count_kv", U32, 1), kv("falcon.attention.layer_norm_epsilon", F32, 1e-5)],
    "gpt2", bpe_tokens, None, [1] * len(bpe_tokens), ["a b", "b c", "ab c", "Ġ a"])
for tgt in ("fuzz_gguf_parse", "fuzz_llama_load"):
    write("mayhem/%s/testsuite/tiny_llama" % tgt, llama_seed)
    write("mayhem/%s/testsuite/tiny_falcon" % tgt, falcon_seed)

text_samples = [
    "Hello world, this is a test of the tokenizer.",
    "def f(x):\n    return x * 2  # comment\n",
    "日本語のテキスト αβγ \U0001f600 123456789",
    "<s> special </s> tokens <unk> <0x0A> in text",
]
for tgt in ("fuzz_tokenize_spm", "fuzz_tokenize_bpe"):
    for i, t in enumerate(text_samples):
        write("mayhem/%s/testsuite/text%d" % (tgt, i), bytes([i & 3]) + t.encode())

rng = random.Random(1234)
for name in ("arithmetic", "c", "chess", "japanese", "json_arr", "json", "list"):
    g = open(os.path.join(ROOT, "grammars", name + ".gbnf"), "rb").read()
    write("mayhem/fuzz_grammar/testsuite/%s" % name, g + b"\0" + bytes(rng.randrange(256) for _ in range(48)))

# fuzz_sampling: 16 parameter bytes + float32 logits.
for i, n in enumerate((4, 16, 64, 200)):
    head = bytes([i * 37 & 255, 0xA5, (n // 2) & 255, 1, 200, 20, 220, 200, 100, 128, 140, 120, i % 5, 80, 60, 40])
    logits = struct.pack("<%df" % n, *[rng.uniform(-8.0, 8.0) for _ in range(n)])
    write("mayhem/fuzz_sampling/testsuite/logits%d" % i, head + logits)

# fuzz_ggml_quants: [type selector][mode] + raw blocks (mode 0) or float32 weights (mode 1), one per type.
for t in range(11):
    write("mayhem/fuzz_ggml_quants/testsuite/dequant_dot_t%02d" % t,
          bytes([t, 0]) + bytes(rng.randrange(256) for _ in range(2048)))
    write("mayhem/fuzz_ggml_quants/testsuite/quantize_t%02d" % t,
          bytes([t, 1]) + struct.pack("<512f", *[rng.gauss(0.0, 1.0) for _ in range(512)]))
