#!/usr/bin/env python3
"""fix_musb_hops.py - repair the missing vendor MUSB hops hooks (E5 tree).

Evidence: artifacts_e5/incident/dmesg.log (5.15.211-g47380a7b3b56)
  Unable to handle kernel NULL pointer dereference at 0x0 / Oops [#1]
  Workqueue: k_sm_usb musb_sprd_otg_sm_work [musb_sprd]
  pc : 0x0    lr : musb_sprd_otg_start_host+0x290/0x368 [musb_sprd]

Disassembly: 134c: ldr x8,[x19,#0x2370] (= musb->hops.host_start)
             1350: blr x8                (= NULL -> jump to address 0)

This tree only *reads* musb->hops.*; the wiring the sibling UMS9620 tree does
in musb_host_alloc() (and any musb_host_start() implementation) is missing, so
the first switch to host mode killed the kernel.  Edits:
  1. musb_host.c      - wire up the two hooks that do exist here.
  2. musb_sprd.c      - guard hops.host_start like the sibling tree does.
  3. sprd_musbhsdma.c - guard the two hops.rx_dma_program call sites (no
                        implementation exists here, hook stays NULL).
Idempotent.  Run from the tree root: python3 artifacts_e5/fix_musb_hops.py
"""

import os
import re
import sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HOOK_ADD = """\t/*
\t * Vendor hooks.  The sprd port (musb_sprd.c, sprd_musbhsdma.c) calls
\t * into the host code through these pointers, so they have to be filled
\t * in here: a NULL one is not a no-op, it is a jump to address 0.
\t * Only the two functions that exist in this tree are wired up -
\t * hops.host_start and hops.rx_dma_program have no implementation here
\t * and stay NULL, so every caller has to test for that.
\t */
\tmusb->hops.advance_schedule = musb_advance_schedule;
\tmusb->hops.tx_dma_program = musb_tx_dma_program;
"""

HOOK_GUARD = """\t\t/*
\t\t * hops.host_start has no implementation in this tree (the vendor
\t\t * musb_host_start() was not carried over), so this call used to be
\t\t * a guaranteed Oops (pc=0x0) as soon as the port switched to host
\t\t * mode, i.e. the moment a USB device was plugged in.  The host
\t\t * controller is already running here - musb_host_setup() brought up
\t\t * the HCD and sprd_musb_enable() below does SESSION/HOST_FORCE_EN -
\t\t * so skipping the hook is safe.  The sibling UMS9620 tree guards
\t\t * the same call the same way.
\t\t */
\t\tif (musb->hops.host_start)
\t\t\tmusb->hops.host_start(musb);
"""

RX_GUARD_1 = ("if (is_in && musb->hops.rx_dma_program)", "sibling-style guard 1")
RX_GUARD_2 = ("if (musb->hops.rx_dma_program)", "sibling-style guard 2")

EDITS = [
    ("drivers/usb/musb/musb_host.c",
     "hops.advance_schedule = musb_advance_schedule",
     r"([ \t]*musb->hcd->has_tt = 1;\n)",
     r"\1\n" + HOOK_ADD,
     1),
    ("drivers/usb/musb/musb_sprd.c",
     HOOK_GUARD.strip(),
     r"[ \t]*musb->hops\.host_start\(musb\);\n",
     HOOK_GUARD,
     1),
    ("drivers/usb/musb/sprd_musbhsdma.c",
     RX_GUARD_1[0],
     r"(?P<i1>[ \t]*)if \(is_in\)(?P<body>\n[ \t]*musb->hops\.rx_dma_program"
     r"\(channel, musb, epnum, qh, urb,\n[ \t]*d->offset, d->length\);\n"
     r"[ \t]*return;\n)",
     r"\g<i1>if (is_in && musb->hops.rx_dma_program)\g<body>",
     1),
    ("drivers/usb/musb/sprd_musbhsdma.c",
     RX_GUARD_2[0],
     r"(?P<i2>[ \t]*)if \(is_in\)\n(?P<r1>[ \t]*musb->hops\.rx_dma_program"
     r"\(channel, musb, epnum, qh, urb,\n[ \t]*d->offset, d->length\);\n)"
     r"(?P<ind>[ \t]*)else\n(?P<r2>[ \t]*musb->hops\.tx_dma_program"
     r"\(musb->dma_controller, hw_ep,\n[ \t]*qh, urb, d->offset, d->length\);\n)",
     r"\g<i2>if (is_in) {\n\g<i2>\tif (musb->hops.rx_dma_program)\n"
     r"\g<r1>\g<i2>} else\n\g<r2>",
     1),
]

cache = {}


def read(path):
    if path not in cache:
        with open(os.path.join(BASE, path), "r", encoding="utf-8",
                  errors="surrogateescape") as fh:
            cache[path] = fh.read()
    return cache[path]


def main():
    changed = 0
    for path, marker, pattern, repl, count in EDITS:
        text = read(path)
        if marker in text:
            print("  %-38s already patched" % path)
            continue
        hits = len(re.findall(pattern, text))
        if hits != count:
            sys.exit("%s: pattern hit %d times, expected %d: %s"
                     % (path, hits, count, pattern[:70]))
        cache[path] = re.sub(pattern, lambda m: m.expand(repl), text)
        print("  %-38s patched" % path)
        changed += 1
    for path, text in cache.items():
        with open(os.path.join(BASE, path), "w", encoding="utf-8",
                  errors="surrogateescape") as fh:
            fh.write(text)
    print("%d file(s) patched" % changed)


if __name__ == "__main__":
    main()
