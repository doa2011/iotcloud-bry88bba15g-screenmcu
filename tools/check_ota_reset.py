#!/usr/bin/env python3
"""Check compiled reset instruction ordering (not a physical reset simulation)."""
import re
import sys
from pathlib import Path

asm = Path(sys.argv[1]).read_text().lower()
assert "<board_ota_restart>:" in asm, asm
assert re.search(r"\bcpsid\s+i\b", asm), asm
# ARM CMSIS reset inlines DSB and an AIRCR write; require the barriers
# after interrupt masking and a bounded non-tick delay (no board_delay call).
tail = re.split(r"\bcpsid\s+i\b", asm, maxsplit=1)[1]
assert len(re.findall(r"\bdsb\b", tail)) >= 3, asm
assert not re.search(r"\bblx?\b[^\n]*<board_delay>", tail), asm
assert re.search(r"\bstr(?:\.w)?\s", tail), asm
print("Compiled OTA reset: IRQ masking, barriers, register writes, no tick wait after mask")
