#!/usr/bin/env bash
set -euo pipefail
out="tests/helpers_generated.h"
{
  echo '#pragma once'
  echo '#include <windows.h>'
  echo '#include <string>'
  echo '#include <cstdint>'
  awk '/\/\/ === HELPERS BEGIN ===/{f=1;next} /\/\/ === HELPERS END ===/{f=0} f' lock-keys-notifier.wh.cpp
} > "$out"
echo "wrote $out"
