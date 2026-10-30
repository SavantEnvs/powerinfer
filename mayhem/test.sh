#!/usr/bin/env bash
# Oracle: runs the pre-built upstream test binaries (build-tests/bin) plus a known-answer probe that
# prints values computed by the real library and compares them to exact expected strings.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN=build-tests/bin
PROBE=build-tests/kat_probe
passed=0 failed=0

for exe in "$BIN/test-grammar-parser" "$BIN/test-llama-grammar" "$BIN/test-sampling" \
           "$BIN/test-quantize-fns" "$BIN/test-rope" "$PROBE"; do
  if [ ! -x "$exe" ]; then echo "MISSING $exe (build.sh should have produced it)" >&2; failed=$((failed+1)); fi
done

for t in test-grammar-parser test-llama-grammar test-sampling test-quantize-fns test-rope; do
  [ -x "$BIN/$t" ] || continue
  if "$BIN/$t" >"/tmp/$t.out" 2>&1; then echo "PASS $t"; passed=$((passed+1))
  else echo "FAIL $t"; tail -20 "/tmp/$t.out"; failed=$((failed+1)); fi
done

for pair in "test-tokenizer-0-llama:models/ggml-vocab-llama.gguf" "test-tokenizer-1-llama:models/ggml-vocab-llama.gguf"; do
  t="${pair%%:*}"; v="${pair#*:}"
  [ -x "$BIN/$t" ] || { echo "MISSING $BIN/$t" >&2; failed=$((failed+1)); continue; }
  if "$BIN/$t" "$v" >"/tmp/$t.out" 2>&1; then echo "PASS $t"; passed=$((passed+1))
  else echo "FAIL $t"; tail -20 "/tmp/$t.out"; failed=$((failed+1)); fi
done

if [ -x "$PROBE" ]; then
  out="$("$PROBE" models/ggml-vocab-llama.gguf models/ggml-vocab-falcon.gguf 2>&1)"
  echo "$out"
  expect() { # <line prefix> <exact line>
    if printf '%s\n' "$out" | grep -qxF -- "$1"; then echo "PASS kat: $1"; passed=$((passed+1))
    else echo "FAIL kat: expected '$1'"; failed=$((failed+1)); fi
  }
  expect "grammar symbols=8 rules=8 root=0 term=3"
  expect "gguf version=3 n_kv=17 tokenizer=llama"
  expect "n_vocab spm=32000 bpe=65024"
  expect "tokens[spm]=29871,15043,3186"
  expect "tokens[bpe]=9856,1079"
  expect "grammar-sample keep=101"
fi

emit_ctrf "powerinfer-tests" "$passed" "$failed" 0
