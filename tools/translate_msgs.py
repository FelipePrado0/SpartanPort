#!/usr/bin/env python3
"""Create a reviewed-later Persian draft of a MSGS_TXT file.

Uses the public Google Translate web endpoint. This is intentionally a local tool:
translated game text is not written to the repository by this script.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import time
from urllib.parse import quote
from urllib.request import urlopen

TOKEN = re.compile(r"(\[[^\]]+\]|\{[^}]+\}|%\w+|\\n)")
MARKER = re.compile(r"^\*[^*]+\*\s*")


def protect(text: str):
    tokens = []
    def repl(match):
        key = f"ZXQ{len(tokens)}QXZ"
        tokens.append((key, match.group(0)))
        return key
    return TOKEN.sub(repl, text), tokens


def restore(text: str, tokens):
    for key, value in tokens:
        text = text.replace(key, value)
    return text


def translate_batch(lines):
    prepared = []
    token_sets = []
    for line in lines:
        safe, tokens = protect(line)
        prepared.append(safe)
        token_sets.append(tokens)
    joined = "\nZXQ_LINE_BREAK_QXZ\n".join(prepared)
    url = ('https://translate.googleapis.com/translate_a/single?client=gtx&sl=en&tl=fa&dt=t&q='
           + quote(joined))
    with urlopen(url, timeout=45) as response:
        data = json.load(response)
    translated = ''.join(part[0] for part in data[0])
    result = translated.split('ZXQ_LINE_BREAK_QXZ')
    if len(result) != len(lines):
        raise RuntimeError(f'translator returned {len(result)} lines for {len(lines)} inputs')
    return [restore(text.strip(), tokens) for text, tokens in zip(result, token_sets)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--batch-size', type=int, default=20)
    args = parser.parse_args()
    lines = args.source.read_text(encoding='ascii', errors='replace').splitlines(keepends=True)
    output = []
    pending = []
    pending_indices = []

    def flush():
        nonlocal pending, pending_indices
        if not pending:
            return
        for attempt in range(4):
            try:
                translated = translate_batch(pending)
                break
            except Exception:
                if attempt == 3:
                    raise
                time.sleep(2 ** attempt)
        for index, text in zip(pending_indices, translated):
            ending = '\r\n' if lines[index].endswith('\r\n') else '\n' if lines[index].endswith('\n') else ''
            output[index] = text.rstrip('\r\n') + ending
        pending, pending_indices = [], []

    output = list(lines)
    for index, original in enumerate(lines):
        text = original.rstrip('\r\n')
        if not text or MARKER.match(text) or text.startswith('------'):
            continue
        pending.append(text)
        pending_indices.append(index)
        if len(pending) >= args.batch_size:
            flush()
            args.output.write_text(''.join(output), encoding='utf-8', newline='')
            print(f'translated through line {index + 1}/{len(lines)}', flush=True)
    flush()
    args.output.write_text(''.join(output), encoding='utf-8', newline='')
    print(f'wrote {args.output} ({len(lines)} lines)', flush=True)


if __name__ == '__main__':
    main()
