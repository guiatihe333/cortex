#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Make CORTEX's site parts for GitHub Pages (https://guiatihe333.github.io/cortex/, served from main's root; the
landing page and the simulator are kept in the tree by hand: index.html, simulator/). It makes:

  install/index.html          fm1-base's web installer (base/web/installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (fm1-base's tools/manual_html.py, from docs/MANUAL.md)
  words/                      the screen reader's words files (app/web/words, from app/words.tsv)
  firmware/cortex-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py app/build/cortex-1.5.5.fwsc 1.5.5 .      (into the repository's root)

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_76XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
BASE = SRC / "base"
sys.path.insert(0, str(BASE / "tools"))
from make_site import MBOOT_VERSION, product_of, read_conf, strip_module  # noqa: E402  (fm1-base's: the package format)

REPO = "https://github.com/guiatihe333/cortex"
NAME = "CORTEX"



def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_76\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"cortex-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer: fm1-base's page, filled in from app/fm1.toml
    conf = read_conf(SRC / "app" / "fm1.toml")
    page = (BASE / "web" / "installer.html").read_text(encoding="utf-8")
    lib = strip_module((BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((BASE / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    # m-bootloader (MIT), which the installer offers to install after AMB-1, with its licence (as fm1-base's make_site)
    boot = BASE / "web" / "m-bootloader" / f"fm1-uboot-{MBOOT_VERSION}.fwsc"
    boot_name = f"m-bootloader-{MBOOT_VERSION}.fwsc"
    shutil.copy(boot, out / "firmware" / boot_name)
    shutil.copy(BASE / "web" / "m-bootloader" / "LICENSE", out / "firmware" / "m-bootloader-LICENSE.txt")
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name,
                       "backup": bool(conf.get("backup")),
                       "boot": {"pkg": "../firmware/" + boot_name, "version": MBOOT_VERSION,
                                "sha256": hashlib.sha256(boot.read_bytes()).hexdigest()}})
    for mark in ("/*LIB*/", "/*META*/", "/*APP*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"base/web/installer.html must contain {mark} once")
    for key, val in (("NAME", NAME), ("REPO", REPO), ("REPO_SHORT", re.sub(r"^https?://", "", REPO)),
                     ("LICENSE", conf.get("licence", "MPL-2.0")), ("CREDITS", " " + conf.get("credits", ""))):
        page = page.replace("{{" + key + "}}", val)
    # the firmware's own steps around an install (fm1.toml install_app; base/web/installer.html APP): AMB-1 has none
    app_js = (SRC / "app" / conf["install_app"]).read_text(encoding="utf-8") if conf.get("install_app") else "null"
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta)
                                                .replace("/*APP*/", app_js), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(BASE / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html"), "--name", NAME, "--repo", REPO,
                    "--tagline", conf.get("tagline", "")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "app" / "web" / "words", out / "words")
    print(f"site: {out}: install/ ({product}), manual/, words/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
