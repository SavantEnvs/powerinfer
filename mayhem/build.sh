#!/usr/bin/env bash
#
# mayhem/build.sh — build the PowerInfer (llama.cpp fork) fuzz harnesses and the oracle suite.
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem; re-runnable and offline.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# SanCov instrumentation of the project itself (not only the harness TU), also for the no-sanitizer build.
FUZZ_CFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"

# 1) The project, sanitized + instrumented. Only the llama and common libraries are needed.
#    LLAMA_NATIVE=OFF: no -march=native, so the binary does not depend on the build host's CPU.
cmake -S . -B build-fuzz \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS="$FUZZ_CFLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_CFLAGS" \
    -DLLAMA_NATIVE=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target llama common

# 2) Harnesses: each linked twice — libFuzzer binary and a run-once standalone reproducer.
HARNESS="$SRC/mayhem/harnesses"
LIBS=(build-fuzz/common/libcommon.a build-fuzz/libllama.a -lpthread -lm)
INC=(-I"$SRC" -I"$SRC/common" -I"$HARNESS")

# Leak detection off at build time (ASan + UBSan stay on).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build-fuzz/lsan_off.o
# The standalone driver is C: compile it as C so clang++ does not mangle LLVMFuzzerTestOneInput.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o build-fuzz/standalone_main.o

# Vocabularies the tokenizer/grammar harnesses spill to a scratch file at start-up. `ld -r -b binary`
# (not .incbin) so the object is rebuilt from the file contents on every run.
embed_vocab() { # <gguf relative to $SRC> <out.o>
    ld -r -b binary -z noexecstack -o "$2" "$1"
}
embed_vocab models/ggml-vocab-llama.gguf build-fuzz/vocab_llama.o
embed_vocab models/ggml-vocab-falcon.gguf build-fuzz/vocab_falcon.o

build_harness() { # <name> <source> <extra objects/flags...>
    local name="$1" src="$2"; shift 2
    $CXX $FUZZ_CFLAGS -std=c++17 "${INC[@]}" "$@" \
        "$src" build-fuzz/lsan_off.o "${LIBS[@]}" $LIB_FUZZING_ENGINE -o "$SRC/$name"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -std=c++17 "${INC[@]}" "$@" \
        "$src" build-fuzz/standalone_main.o build-fuzz/lsan_off.o "${LIBS[@]}" -o "$SRC/$name-standalone"
}

build_unchecked() { # <name> <source>: run-once reproducer without the header pre-check (see scratch.h)
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -std=c++17 "${INC[@]}" -DMAYHEM_NO_HEADER_CHECK \
        "$2" build-fuzz/standalone_main.o build-fuzz/lsan_off.o "${LIBS[@]}" -o "$SRC/$1-standalone-unchecked"
}

vocab_defs() { # <symbol stem>
    echo "-DVOCAB_START=_binary_models_ggml_vocab_$1_gguf_start -DVOCAB_END=_binary_models_ggml_vocab_$1_gguf_end"
}

build_harness fuzz_gguf_parse  "$HARNESS/fuzz_gguf_parse.cpp"
build_harness fuzz_llama_load  "$HARNESS/fuzz_llama_load.cpp"
build_unchecked fuzz_gguf_parse "$HARNESS/fuzz_gguf_parse.cpp"
build_unchecked fuzz_llama_load "$HARNESS/fuzz_llama_load.cpp"
build_harness fuzz_tokenize_spm "$HARNESS/fuzz_tokenize.cpp" $(vocab_defs llama)  build-fuzz/vocab_llama.o
build_harness fuzz_tokenize_bpe "$HARNESS/fuzz_tokenize.cpp" $(vocab_defs falcon) build-fuzz/vocab_falcon.o
build_harness fuzz_grammar     "$HARNESS/fuzz_grammar.cpp"   $(vocab_defs llama)  build-fuzz/vocab_llama.o
build_harness fuzz_sampling    "$HARNESS/fuzz_sampling.cpp"  $(vocab_defs llama)  build-fuzz/vocab_llama.o
# -fno-sanitize=function (this TU only): the harness calls ggml's C kernels through the type-erased
# ggml_type_traits_t pointers (const void *), which UBSan's function check flags at the call site.
build_harness fuzz_ggml_quants "$HARNESS/fuzz_ggml_quants.cpp" -fno-sanitize=function

# 3) The oracle suite, built with the project's NORMAL flags (no sanitizers, no -gdwarf-3).
cmake -S . -B build-tests \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
    -DLLAMA_NATIVE=OFF -DLLAMA_BUILD_TESTS=ON -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF
cmake --build build-tests -j"$MAYHEM_JOBS" --target \
    test-grammar-parser test-llama-grammar test-sampling test-quantize-fns test-rope \
    test-tokenizer-0-llama test-tokenizer-1-llama llama common

# Known-answer probe: dynamically linked, prints values computed by the real library.
$CXX -O1 $COVERAGE_FLAGS -std=c++17 -I"$SRC" -I"$SRC/common" "$SRC/mayhem/kat/kat_probe.cpp" \
    build-tests/common/libcommon.a build-tests/libllama.a -lpthread -lm -o build-tests/kat_probe
