// Scratch-file helper for harnesses whose library API only takes a path.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static int scratch_fd = -1;
static std::string scratch_path;

static void scratch_init() {
    const char * dir = getenv("TMPDIR");
    std::string tmpl = std::string(dir && *dir ? dir : "/tmp") + "/mayhem-fuzz-XXXXXX";
    std::string buf = tmpl;
    scratch_fd = mkstemp(&buf[0]);
    if (scratch_fd < 0) abort();
    scratch_path = buf;
}

// Reject GGUF inputs whose declared lengths cannot fit in the file: it walks the header, KV table and
// tensor infos with the parser's layout and compares every count, string length, array length and n_dims
// against the bytes remaining (bounds only, no value validation). Without this the target keeps
// re-finding gguf_init_from_file's unchecked malloc/calloc(len) instead of reaching deeper code.
// -DMAYHEM_NO_HEADER_CHECK builds the raw `*-standalone-unchecked` reproducers used by the known-findings.
#ifndef MAYHEM_NO_HEADER_CHECK
struct gguf_walk {
    const uint8_t * p;
    size_t size, pos;
    size_t left() const { return pos < size ? size - pos : 0; } // pos starts at 8, past a 4..7-byte input
    bool skip(uint64_t n) { if (n > left()) return false; pos += (size_t) n; return true; }
    bool skip_n(uint64_t n, uint64_t each) { return each == 0 || n <= left() / each ? skip(n * each) : false; }
    bool u32(uint32_t * v) { if (left() < 4) return false; memcpy(v, p + pos, 4); pos += 4; return true; }
    bool u64(uint64_t * v) { if (left() < 8) return false; memcpy(v, p + pos, 8); pos += 8; return true; }
    bool str() { uint64_t n; return u64(&n) && skip(n); }
};

// Scalar sizes by gguf_type; 0 = not a fixed-size scalar (string/array/unknown).
static size_t gguf_scalar_size(uint32_t t) {
    static const size_t sz[] = { 1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8 };
    return t < sizeof(sz) / sizeof(sz[0]) ? sz[t] : 0;
}
#endif

static bool gguf_header_fits(const uint8_t * data, size_t size) {
#ifdef MAYHEM_NO_HEADER_CHECK
    (void) data; (void) size;
    return true;
#else
    gguf_walk w = { data, size, 8 }; // after the 4-byte magic and u32 version
    uint64_t n_tensors, n_kv;
    if (!w.u64(&n_tensors) || !w.u64(&n_kv)) return false;
    if (n_kv > w.left() / 12 || n_tensors > w.left() / 24) return false; // min KV entry / tensor info sizes

    for (uint64_t i = 0; i < n_kv; i++) {
        uint32_t type;
        if (!w.str() || !w.u32(&type)) return false;
        if (type == 8) { // string
            if (!w.str()) return false;
        } else if (type == 9) { // array
            uint32_t at; uint64_t n;
            if (!w.u32(&at) || !w.u64(&n)) return false;
            if (at == 8) {
                if (n > w.left() / 8) return false; // each string is >= 8 bytes
                for (uint64_t j = 0; j < n; j++) if (!w.str()) return false;
            } else if (gguf_scalar_size(at)) {
                if (!w.skip_n(n, gguf_scalar_size(at))) return false;
            } else if (at == 9 || at == 13) {
                return true; // the parser asserts on these; leave it to the target
            } // else: unknown element type, the parser reads no payload and carries on
        } else if (gguf_scalar_size(type)) {
            if (!w.skip(gguf_scalar_size(type))) return false;
        } else if (type == 13) {
            return true; // the parser asserts; leave it to the target
        } // else: unknown KV type, the parser reads no payload and carries on
    }

    for (uint64_t i = 0; i < n_tensors; i++) {
        uint32_t n_dims;
        if (!w.str() || !w.u32(&n_dims)) return false;
        if (!w.skip_n(n_dims, 8) || !w.skip(4 + 8)) return false; // ne[] + type(u32) + offset(u64)
    }
    return true;
#endif
}

// Replace the scratch file's contents with the fuzzer input and return its path.
static const char * scratch_write(const uint8_t * data, size_t size) {
    if (ftruncate(scratch_fd, 0) != 0) abort();
    size_t off = 0;
    while (off < size) {
        ssize_t w = pwrite(scratch_fd, data + off, size - off, off);
        if (w <= 0) abort();
        off += (size_t) w;
    }
    return scratch_path.c_str();
}
