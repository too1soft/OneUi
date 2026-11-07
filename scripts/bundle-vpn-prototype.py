#!/usr/bin/env python3
"""Build the standalone SDK review artifact from its own source files."""
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'docs/prototypes/vpn'
source = (root / 'index.template.html').read_text(encoding='utf-8')
marker = '/* ONEUI_VPN_DEMO */'
if source.count(marker) != 1:
    raise RuntimeError('Expected exactly one simulation marker')
(root / 'index.html').write_text('<!-- Generated review artifact. Edit index.template.html and demo-session.js. -->\n' + source.replace(marker, (root / 'demo-session.js').read_text(encoding='utf-8')), encoding='utf-8')
