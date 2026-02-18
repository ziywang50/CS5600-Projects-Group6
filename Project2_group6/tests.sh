#!/usr/bin/env bash
set -euo pipefail

echo "== Test 1: small =="
./Project2 6 5 80 1 4 | tail -n 5

echo "== Test 2: max constraints (should finish) =="
./Project2 30 13 30 2 5 > /dev/null
echo "OK (completed)"

echo "== Test 3: fairness smoke test (mixed load) =="
./Project2 20 10 60 3 4 > /dev/null
echo "OK (completed)"

echo "== Test 4: repeat seeds (deadlock-free check) =="
for s in 1 2 3; do
  ./Project2 20 13 40 "$s" 5 > /dev/null
  echo " seed $s OK"
done

echo "All tests passed."
