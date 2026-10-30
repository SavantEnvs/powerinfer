// Fuzz ggml's quantised-tensor kernels: dequantise raw blocks, dot-product two raw block rows, and
// quantise floats with ggml_quantize_chunk / the type's from_float functions.
// Input = [type selector][mode][payload]. Payload goes to exact-size heap buffers (aligned, ASan-redzoned).
#include "ggml.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static const enum ggml_type k_types[] = {
    GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1, GGML_TYPE_Q8_0, GGML_TYPE_Q8_1,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q5_K, GGML_TYPE_Q6_K,
};

struct buf {
    void * p;
    explicit buf(size_t n) : p(malloc(n ? n : 1)) { if (!p) abort(); }
    ~buf() { free(p); }
};

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
    // ggml_init builds the fp16 conversion tables the kernels read.
    struct ggml_init_params params = { /*mem_size =*/ 1 << 16, /*mem_buffer =*/ nullptr, /*no_alloc =*/ true };
    if (ggml_init(params) == nullptr) abort();
    return 0;
}

static int dequant_and_dot(enum ggml_type type, const uint8_t * payload, size_t len) {
    const ggml_type_traits_t tx = ggml_internal_get_type_traits(type);
    const ggml_type_traits_t ty = ggml_internal_get_type_traits(tx.vec_dot_type);
    if (!tx.to_float || !tx.vec_dot || !ty.to_float) return -1;

    const size_t bx = (size_t) tx.blck_size, by = (size_t) ty.blck_size;
    const size_t step = bx > by ? bx : by; // both are 32 or 256
    size_t n = (len / 2 / tx.type_size) * bx;
    n -= n % step;
    while (n > 0 && n / bx * tx.type_size + n / by * ty.type_size > len) n -= step;
    if (n == 0) return -1;

    const size_t xbytes = n / bx * tx.type_size, ybytes = n / by * ty.type_size;
    buf x(xbytes), y(ybytes), fx(n * sizeof(float)), fy(n * sizeof(float));
    memcpy(x.p, payload, xbytes);
    memcpy(y.p, payload + xbytes, ybytes);

    tx.to_float(x.p, (float *) fx.p, (int) n);
    ty.to_float(y.p, (float *) fy.p, (int) n);

    float s = 0.0f;
    tx.vec_dot((int) n, &s, x.p, y.p);
    return 0;
}

static int quantize_floats(enum ggml_type type, const uint8_t * payload, size_t len) {
    const ggml_type_traits_t tt = ggml_internal_get_type_traits(type);
    const size_t b = (size_t) tt.blck_size;
    size_t n = len / sizeof(float);
    n -= n % b;
    if (n == 0) return -1;

    buf src(n * sizeof(float));
    memcpy(src.p, payload, n * sizeof(float));
    const float * f = (const float *) src.p;
    for (size_t i = 0; i < n; i++) {
        if (!std::isfinite(f[i]) || std::fabs(f[i]) > 1e6f) return -1; // weights are finite and bounded
    }

    const size_t dbytes = n / b * tt.type_size;
    buf dst(dbytes), back(n * sizeof(float));
    int64_t hist[16] = {0};

    if (type != GGML_TYPE_Q8_1) {
        (void) ggml_quantize_chunk(type, f, dst.p, 0, (int) n, hist);
        if (tt.to_float) tt.to_float(dst.p, (float *) back.p, (int) n);
    }
    if (tt.from_float_reference) {
        tt.from_float_reference(f, dst.p, (int) n);
        if (tt.to_float) tt.to_float(dst.p, (float *) back.p, (int) n);
    }
    if (tt.from_float) {
        tt.from_float(f, dst.p, (int) n);
        if (tt.to_float) tt.to_float(dst.p, (float *) back.p, (int) n);
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size) {
    if (size < 3 || size > (1 << 16)) return -1;
    const enum ggml_type type = k_types[data[0] % (sizeof(k_types) / sizeof(k_types[0]))];
    const bool quantize = data[1] & 1;
    return quantize ? quantize_floats(type, data + 2, size - 2) : dequant_and_dot(type, data + 2, size - 2);
}
