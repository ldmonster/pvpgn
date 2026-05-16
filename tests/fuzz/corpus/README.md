# Fuzz Corpus

This directory contains seed inputs for LibFuzzer.

## Directory Structure

- `bnet/` — Seed inputs for BNet protocol codec fuzzer
- `d2save/` — Seed inputs for D2 save file codec fuzzer

## Running Fuzz Tests

### Prerequisites

- Clang compiler (GCC does not support LibFuzzer)
- CMake 3.15+

### Build with Fuzzing Enabled

```bash
cmake -B build-fuzz \
  -DPVPGN_ENABLE_FUZZING=ON \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer,address" \
  -DCMAKE_C_FLAGS="-fsanitize=fuzzer,address"

cmake --build build-fuzz
```

### Run Fuzzers

```bash
# BNet codec fuzzer
./build-fuzz/fuzz/fuzz_bnet_codec tests/fuzz/corpus/bnet/ -max_len=1024

# D2Save codec fuzzer
./build-fuzz/fuzz/fuzz_d2save_codec tests/fuzz/corpus/d2save/ -max_len=8192
```

### Useful LibFuzzer Options

- `-max_len=N` — Maximum input length (bytes)
- `-timeout=N` — Timeout per input (seconds)
- `-max_total_time=N` — Total fuzzing time (seconds)
- `-artifact_prefix=DIR/` — Save crashes to directory
- `-dict=FILE` — Use dictionary of interesting values
- `-jobs=N` — Parallel jobs (requires `-artifact_prefix`)

### Example: Parallel Fuzzing with Crash Collection

```bash
mkdir -p /tmp/bnet_crashes
./build-fuzz/fuzz/fuzz_bnet_codec \
  tests/fuzz/corpus/bnet/ \
  -artifact_prefix=/tmp/bnet_crashes/ \
  -jobs=4 \
  -workers=4 \
  -max_len=1024
```

## Corpus Files

### BNet Corpus

- `seed_null` — Minimal SID_NULL packet (4 bytes)
- `seed_ping` — SID_PING packet with cookie (8 bytes)

### D2Save Corpus

- `seed_header` — Minimal valid D2S file header

## Adding New Corpus Files

1. Create a new file in the appropriate subdirectory
2. Use binary format (raw bytes)
3. Name it descriptively (e.g., `seed_login_request`)
4. Commit to version control

LibFuzzer will automatically use all files in the corpus directory as starting points.

## Interpreting Results

When LibFuzzer finds a crash:

1. It saves the input to the artifact directory
2. The filename is the SHA1 hash of the input
3. Reproduce the crash by running:
   ```bash
   ./build-fuzz/fuzz/fuzz_bnet_codec /path/to/crash_file
   ```

## Sanitizer Output

With `-fsanitize=address`, AddressSanitizer will detect:

- Buffer overflows
- Use-after-free
- Memory leaks
- Invalid memory access

With `-fsanitize=undefined`, UBSan will detect:

- Integer overflow
- Null pointer dereference
- Out-of-bounds array access
- Signed integer overflow

## References

- [LibFuzzer Documentation](https://llvm.org/docs/LibFuzzer/)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
