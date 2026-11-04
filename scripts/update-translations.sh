#!/usr/bin/env bash
# Updates translations/snapslash_en.ts from the texts in src/ (D-038): new
# texts come in unfinished, texts gone from the code are dropped. Translate
# the new ones (Qt Linguist, or by hand in the file), then commit the file.
# CI runs this and fails if the file changes, so it never falls behind.
set -euo pipefail
cd "$(dirname "$0")/.."

lupdate="${LUPDATE:-}"
if [ -z "$lupdate" ]; then
    lupdate="$(command -v lupdate || true)"
fi
if [ -z "$lupdate" ] && [ -x "$HOME/Qt/6.8.3/gcc_64/bin/lupdate" ]; then
    lupdate="$HOME/Qt/6.8.3/gcc_64/bin/lupdate"
fi
if [ -z "$lupdate" ]; then
    echo "lupdate not found; set LUPDATE=/path/to/lupdate" >&2
    exit 1
fi

# All of src/, also the Windows-only files, whatever this machine builds.
"$lupdate" -silent -source-language de -no-obsolete -locations none src -ts translations/snapslash_en.ts
