# SPDX-License-Identifier: MIT
"""Frozen historical function extractor; forward tests compile real classes."""

def extract(text, signature):
    """Extract a complete definition; skip braces in comments and literals."""
    if text.count(signature) != 1:
        raise ValueError('expected one definition: ' + signature)
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    state = 'code'
    i = brace
    while i < len(text):
        c = text[i]
        pair = text[i:i + 2]
        if state == 'line':
            if c == '\n':
                state = 'code'
        elif state == 'block':
            if pair == '*/':
                state = 'code'
                i += 1
        elif state in ('"', "'"):
            if c == '\\':
                i += 1
            elif c == state:
                state = 'code'
        elif pair == '//':
            state = 'line'
            i += 1
        elif pair == '/*':
            state = 'block'
            i += 1
        elif c in ('"', "'"):
            state = c
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    raise ValueError('unterminated function: ' + signature)

