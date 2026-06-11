#!/bin/sh
# Increments BUILD_NUMBER in Source/BuildNumber.h on every build.
# macOS/Xcode equivalent of ../VisualStudio2026/update_build_number.ps1
set -euo pipefail

HEADER_PATH="${SRCROOT}/../../Source/BuildNumber.h"

BUILD_NUMBER=0
if [ -f "$HEADER_PATH" ]; then
    EXISTING=$(grep -Eo '#define[[:space:]]+BUILD_NUMBER[[:space:]]+[0-9]+' "$HEADER_PATH" | grep -Eo '[0-9]+$' || true)
    if [ -n "$EXISTING" ]; then
        BUILD_NUMBER=$EXISTING
    fi
fi
BUILD_NUMBER=$((BUILD_NUMBER + 1))

cat > "$HEADER_PATH" <<EOF
// Auto-generated before each build by update_build_number.sh - do not edit manually.
#pragma once
#define BUILD_NUMBER $BUILD_NUMBER
EOF
