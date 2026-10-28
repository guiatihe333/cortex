#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""AMB-1 FM6's installer as one page to open from a folder (no site to host it): fm1-base's web installer
(web/installer.html, fm1pkg.js, fm1ota.js) with the package inside it (base64), in Portuguese first (English and
Japanese a click away). Chrome or Edge, Web MIDI: a file opened from the disk is a secure context there.
With M-VAVE's FM-1 V15 (STOCK.fwsc, checked against fm1ota.js's STOCK_SHA256) inside it too: "back to the stock
firmware" needs no file, and an FM-1 on M-VAVE's Open firmware (which takes only V15) gets V15 first, then the
firmware, on the one click. V15_LINK is where the page links the V15 file for M-VAVE's own updater (from a V16).

  tools/make_installer.py APP_DIR PACKAGE.fwsc VERSION OUT.html [STOCK.fwsc V15_LINK]"""
import base64
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "base" / "tools"))
from build import read_conf  # noqa: E402
from make_site import product_of, strip_module  # noqa: E402

WEB = ROOT / "base" / "web"

PT = {
    "title": "Instalar o {{NAME}}",
    "intro": "Ligue o FM-1 no computador pelo USB e clique em Instalar. Funciona no Chrome ou no Edge; o navegador "
             "pede uma vez para usar o MIDI: permita.",
    "newtab": " (abre em outra aba)", "progress": "Progresso", "pct": ", {n} por cento",
    "warn": "Não desligue o cabo USB enquanto estiver gravando.",
    "beforeh": "Antes:",
    "before": "o {{NAME}} se instala sobre o firmware V15 da M-VAVE. Se o seu FM-1 estiver no firmware Open da M-VAVE, "
              "esta página põe a V15 sozinha antes, no mesmo clique. Se estiver na V16, volte primeiro para a V15 com "
              "o atualizador da M-VAVE (M-UPGRADE), usando este arquivo:",
    "tostock": "Este FM-1 está no firmware Open da M-VAVE: primeiro a V15, depois o {{NAME}}.",
    "open": "Este FM-1 está com outro firmware da M-VAVE. Volte para a V15 (\"Voltar ao firmware original\", abaixo) "
            "e clique em Instalar de novo.",
    "install": "Instalar", "other": "English",
    "manual": "O manual do {{NAME}}",
    "direct": "Ligue o cabo USB direto no computador (sem hub). Não precisa de nenhum outro aparelho.",
    "brick": "Se o FM-1 não ligar mais, um Mac consegue pôr o firmware original de volta:",
    "bricklink": "recuperar um FM-1 que não liga (em inglês)",
    "back": "Para voltar ao firmware oficial, use \"Voltar ao firmware original\" aqui embaixo, ou o atualizador da "
            "M-VAVE, o M-UPGRADE (Downloads → PC Software → M-UPGRADE).",
    "stockh": "Voltar ao firmware original",
    "stockwhy": "Grava de volta o FM-1 V15 da M-VAVE, pelo mesmo cabo USB: ele já vem nesta página, é só clicar no "
                "botão. Atenção: isso apaga tudo o que o {{NAME}} guardou no FM-1 (as peças, a fita gravada e os "
                "bancos DX7 que você mandou; os de fábrica voltam quando o {{NAME}} for instalado de novo).",
    "stock1": "Se quiser usar um arquivo seu, o FM-1 V15 da M-VAVE:",
    "stock2": "Escolha o arquivo FM-1_V15.fwsc (opcional):",
    "stock3": "Ligue o FM-1 pelo USB e clique no botão abaixo.",
    "stockinstall": "Instalar o firmware original",
    "stockok": "É o FM-1 V15 da M-VAVE. Clique no botão para instalar.",
    "stockbad": "Esse não é o arquivo do FM-1 V15 da M-VAVE. Escolha o FM-1_V15.fwsc.",
    "stockdone": "Pronto. O FM-1 reiniciou com o firmware original.",
    "nomidi": "Este navegador não consegue. Abra esta página no Chrome ou no Edge.", "loadfail": "Não deu para ler: ",
    "start": "Começando", "verify": "Conferindo", "loader": "Entrando no modo de atualização", "write": "Gravando",
    "reboot": "Reiniciando", "done": "Pronto! O FM-1 já está com o {{NAME}}.",
    "stopped": "Parou no meio. Desligue e ligue o cabo USB e clique em Instalar de novo.",
    "error": "Erro: ",
    "denied": "O navegador não deixou usar o MIDI (SysEx). Permita o MIDI nas configurações do site (o ícone na barra "
              "de endereço) e clique em Instalar de novo.",
    "notfound": "FM-1 não encontrado. Confira o cabo USB (precisa ser de dados), feche outros programas que usam MIDI "
                "e clique em Instalar de novo.",
    "model": "O aparelho ligado não é um FM-1.",
    "lost": "A conexão USB caiu. Ligue de novo e clique em Instalar.",
    "noloader": "O FM-1 em modo de atualização não foi encontrado. Ligue o cabo de novo e clique em Instalar (ele fica "
                "no modo de atualização até terminar).",
    "noreturn": "O FM-1 não voltou. Desligue e ligue ele.",
    "mismatch": "Gravado, mas o FM-1 informa outra versão: ",
    "badpkg": "O firmware desta página está danificado. Baixe a pasta de novo.",
    "leave": "Gravando. Não feche a página.",
    "license": "{{LICENSE}}, sem garantia. Código-fonte:",
    "reporth": "Avisar de um problema",
    "reportwhy": "Se o FM-1 travou, não desligue: ligue pelo USB e clique no botão abaixo. O registro do travamento "
                 "sobrevive a um reinício, mas não a desligar. Copie o que aparecer e mande, dizendo o que você estava "
                 "fazendo.",
    "reportread": "Ler o registro do travamento", "reportcopy": "Copiar", "reportlabel": "Registro do travamento",
    "reportwait": "Lendo…", "reportnone": "Sem resposta. Confira se o FM-1 está rodando o {{NAME}}.",
    "reportok": "Lido. Copie e mande.", "reportcopied": "Copiado.",
}


def rep(s, a, b):
    if s.count(a) != 1:
        raise SystemExit(f"installer.html: {a[:60]!r} found {s.count(a)} times (fm1-base changed?)")
    return s.replace(a, b)


NEW_EN = {
    "beforeh": "First:",
    "before": "{{NAME}} installs over M-VAVE's firmware V15. If your FM-1 runs M-VAVE's Open firmware, this page puts "
              "V15 back by itself first, on the same click. If it runs V16, go back to V15 first with M-VAVE's updater "
              "(M-UPGRADE), with this file:",
    "tostock": "This FM-1 runs M-VAVE's Open firmware: V15 first, then {{NAME}}.",
    "open": "This FM-1 runs another M-VAVE firmware. Go back to V15 (\"Back to the stock firmware\", below) and press "
            "Install again.",
}
STOCK_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"
BEFORE = """  <p data-t="warn">Do not unplug the USB cable while writing.</p>
  <p><strong data-t="beforeh"></strong> <span data-t="before"></span>
    <a href="V15LINK" download>FM-1_V15.fwsc</a></p>"""
RUN_A = """async function run(img, product, isStock) {
  let midi;"""
RUN_B = """async function run(img, product, isStock) {
  let midi, result = "fail";"""
TRY_A = """      await up.install(img, product, step);
    }
  } catch (e) {
    if"""
TRY_B = """      await up.install(img, product, step);
    }
    result = "ok";
  } catch (e) {
    if (e && e.code === "open" && !isStock && stock) result = "open";
    else if"""
END_A = """  $("go").disabled = !image;
  $("stockgo").disabled = !stock;
}"""
END_B = """  $("go").disabled = !image;
  $("stockgo").disabled = !stock;
  return result;
}"""
GO_A = """$("go").addEventListener("click", () => run(image, meta.product, false));"""
GO_B = """$("go").addEventListener("click", async () => {
  if (await run(image, meta.product, false) !== "open") return;
  sayK("tostock");                               /* M-VAVE's Open firmware: V15 first (it takes nothing else) */
  await sleep(2500);
  if (await run(stock.image, stock.product, true) !== "ok") return;
  await sleep(4000);
  await run(image, meta.product, false);
});"""
STOCK_A = """let stock = null;"""
STOCK_B = """let stock = null;
(async () => {                                   /* M-VAVE's FM-1 V15, inside the page: checked, then the button is on */
  const raw = Uint8Array.from(atob(STOCK_B64), (c) => c.charCodeAt(0));
  const hex = [...new Uint8Array(await crypto.subtle.digest("SHA-256", raw))].map((b) => b.toString(16).padStart(2, "0")).join("");
  if (hex !== STOCK_SHA256) return;
  stock = { image: logicalImage(raw), product: productOf(raw) };
  $("stockgo").disabled = writing;
})();"""


def with_stock(html, stock, v15_link):
    """M-VAVE's V15 inside the page: the stock button at once; an FM-1 on M-VAVE's Open firmware gets V15 first"""
    new = "".join(f"    {k}: {json.dumps(v, ensure_ascii=False)},\n" for k, v in NEW_EN.items())
    html = rep(html, '  ja: {\n', '  ja: {\n' + new)
    html = rep(html, '  en: {\n', '  en: {\n' + new)
    html = rep(html, BEFORE.split("\n")[0], BEFORE.replace("V15LINK", v15_link))
    html = rep(html, '{ $("bar").value = 100; sayK("stockdone"); }',   # (V15 names itself its own way: done)
               '{ $("bar").value = 100; sayK("stockdone"); result = "ok"; }')
    for a, b in ((RUN_A, RUN_B), (TRY_A, TRY_B), (END_A, END_B), (GO_A, GO_B), (STOCK_A, STOCK_B)):
        html = rep(html, a, b)
    return html


def main(app, pkg, version, out, stock=None, v15_link="FM-1_V15.fwsc"):
    app, pkg, out = Path(app).resolve(), Path(pkg), Path(out)
    sraw = Path(stock).read_bytes() if stock else b""
    if stock and hashlib.sha256(sraw).hexdigest() != STOCK_SHA256:
        raise SystemExit(f"{stock}: not M-VAVE's FM-1 V15")
    conf = read_conf(app / "fm1.toml")
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(re.escape(conf.get("id_prefix", "FM-1_0")) + r"\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: a release build, please")
    html = (WEB / "installer.html").read_text(encoding="utf-8")
    lib = strip_module((WEB / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((WEB / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": pkg.name})
    # Portuguese first, then English, then Japanese: the button names the next one, in it
    html = rep(html, '<html lang="en">', '<html lang="pt-BR">')
    html = rep(html, "<title>Install · {{NAME}}</title>", "<title>Instalar · {{NAME}}</title>")
    html = rep(html, "<h1>Install {{NAME}}</h1>", '<h1 data-t="title">Instalar o {{NAME}}</h1>')
    html = rep(html, '  ja: {\n', '  ja: {\n    title: "{{NAME}} をインストール",\n')
    html = rep(html, '  en: {\n', '  en: {\n    title: "Install {{NAME}}",\n')
    html = rep(html, '<button type="button" class="lang" id="lang" lang="ja">日本語</button>',
               '<button type="button" class="lang" id="lang" lang="en">English</button>')
    html = rep(html, "const TEXT = {\n", "const TEXT = {\n  pt: " + json.dumps(PT, ensure_ascii=False) + ",\n")
    html = rep(html, '    install: "インストール", other: "English",', '    install: "インストール", other: "Português",')
    html = rep(html, 'let lang = "en";', 'const LANGS = ["pt", "en", "ja"];\nlet lang = "pt";')
    html = rep(html, '''  $("lang").lang = lang === "ja" ? "en" : "ja";''',
               '''  $("lang").lang = LANGS[(LANGS.indexOf(lang) + 1) % LANGS.length];''')
    html = rep(html, '''$("lang").addEventListener("click", () => { lang = lang === "ja" ? "en" : "ja"; applyLang(); });''',
               '''$("lang").addEventListener("click", () => { lang = LANGS[(LANGS.indexOf(lang) + 1) % LANGS.length]; applyLang(); });''')
    # the package inside the page: no fetch (a page opened from the disk may not fetch)
    html = rep(html, '''    const r = await fetch(meta.pkg);
    if (!r.ok) throw new Error(`${meta.pkg}: HTTP ${r.status}`);
    const pkg = new Uint8Array(await r.arrayBuffer());''',
               '''    const pkg = Uint8Array.from(atob(PKG_B64), (c) => c.charCodeAt(0));''')
    html = rep(html, '<a href="../" data-t="manual">', '<a href="MANUAL-PT.html" data-t="manual">')
    html = rep(html, "Fonts: Barlow Semi Condensed (SIL OFL 1.1).", "Fonts: Terminus (SIL OFL 1.1).")
    # the source: in the folder the page comes in (codigo-fonte/), not a repository's address
    html = rep(html, '''<a href="{{REPO}}" target="_blank" rel="noopener">{{REPO_SHORT}}<span class="sr" data-t="newtab"> (opens in a new tab)</span></a>''',
               '''<a href="codigo-fonte/">codigo-fonte/</a>''')
    if stock:
        html = with_stock(html, stock, v15_link)
    repo, credits = conf.get("repo", ""), conf.get("credits", "")
    for key, val in (("NAME", "AMB-1 FM6"), ("REPO", repo), ("REPO_SHORT", re.sub(r"^https?://", "", repo)),
                     ("LICENSE", conf.get("licence", "MPL-2.0")), ("CREDITS", " " + credits if credits else "")):
        html = html.replace("{{" + key + "}}", val)
    html = rep(html, "/*LIB*/", lib + "\nconst PKG_B64 = \"" + base64.b64encode(raw).decode() + "\";" +
               ("\nconst STOCK_B64 = \"" + base64.b64encode(sraw).decode() + "\";" if stock else ""))
    html = rep(html, "/*META*/", meta)
    out.write_text(html, encoding="utf-8")
    print(f"installer: {out} ({product}{', with V15' if stock else ''}, {len(html) // 1024} KB)")


if __name__ == "__main__":
    if len(sys.argv) not in (5, 7):
        sys.exit(__doc__)
    main(*sys.argv[1:])
