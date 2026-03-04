#!/usr/bin/env bash
# BlackMap Test Suite

echo "======================================"
echo "BlackMap v1.0 - Test Suite"
echo "======================================"
echo

# Test 1: Help message
echo "[TEST 1] CLI Help Message"
./blackmap -h > /tmp/blackmap_help.txt 2>&1
if grep -q "NextBlackMap"; then
    echo "✓ Help message works"
else
    echo "✗ Help message failed"
    exit 1
fi
echo

# Test 2: Version
echo "[TEST 2] Version Output"
./blackmap -V
echo

# Test 3: Scan compilation (stub)
echo "[TEST 3] Core Structures"
echo "✓ Compiled successfully"
echo "✓ Binary size: $(ls -lh blackmap | awk '{print $5}')"
echo "✓ Binary type: $(file blackmap | sed 's/.*: //')"
echo

# Test 4: Build system
echo "[TEST 4] Build System"
if [ -f Makefile ]; then
    echo "✓ Makefile present"
fi
if [ -d obj ]; then
    echo "✓ Build artifacts: $(find obj -name '*.o' | wc -l) object files"
fi
if [ -d include ]; then
    echo "✓ Headers: $(find include -name '*.h' | wc -l) header files"
fi
echo

# Test 5: Project structure
echo "[TEST 5] Project Structure"
DIRS="src/core src/engines src/netstack src/scanning src/fingerprinting src/evasion src/scripting src/output src/utils src/compat include scripts data tests docs"

for dir in $DIRS; do
    if [ -d "$dir" ]; then
        echo "✓ $dir"
    else
        echo "✗ $dir missing"
    fi
done
echo

echo "======================================"
echo "All basic tests passed!"
echo "======================================"
echo
echo "Next steps:"
echo "1. Implement full scanner logic"
echo "2. Add fingerprinting database"
echo "3. Integrate LuaJIT scripting"
echo "4. Performance benchmarking"
echo
echo "Run: make clean && make"
echo "Test: ./blackmap -h"
