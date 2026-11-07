// FITA.html's core outside the browser: a test sound (3 s: tones, a hit every 0.75 s) coded for the reel, decoded as
// the FM-1 decodes it (its quality checked), cut at its hits (4 splices), and the SysEx frames the page would send,
// written for the simulator (sysexfile).   node fita_test.js FITA.html OUT.syx
const fs = require('fs');
const html = fs.readFileSync(process.argv[2], 'utf8');
const core = html.slice(html.indexOf('// ------------------------------------------------------------------ CORE BEGIN'),
                        html.indexOf('// ------------------------------------------------------------------ CORE END'));
const M = new Function(core + '; return {encodeReel, decodeReel, makeMeta, cleanStarts, reelFrames, splitOnsets, splitEqual, splitBeats, RL_SR, RL_BLK};')();
let fails = 0;
const check = (ok, what) => { console.log((ok ? '  ok   ' : '  FAIL ') + what); if (!ok) fails++; };

const sr = M.RL_SR, n = 3 * sr, x = new Float32Array(n);
for (let i = 0; i < n; i++) {
  const t = i / sr, beat = t % 0.75;
  x[i] = 0.3 * Math.sin(2 * Math.PI * 220 * t) + 0.15 * Math.sin(2 * Math.PI * 331 * t) +
         (beat < 0.04 ? 0.6 * Math.exp(-beat * 80) * Math.sin(2 * Math.PI * 90 * t) : 0);
}
const r = M.encodeReel(x), y = M.decodeReel(r);
let se = 0, sx = 0;
for (let i = 0; i < n; i++) { se += (x[i] - y[i]) ** 2; sx += x[i] ** 2; }
const snr = 10 * Math.log10(sx / se);
check(snr > 20, `ADPCM as the FM-1 decodes it: ${snr.toFixed(1)} dB SNR`);
check(r.nib.length === n / 2 && r.hdr.length === 1016 * 4, `sizes: ${r.nib.length} bytes of samples, ${r.hdr.length} of states`);

const on = M.splitOnsets(x, sr, 4), cs = M.cleanStarts(on, n);
check(cs.length === 4 && cs.every((v) => v % M.RL_BLK === 0), `4 splices at the hits, on blocks: ${cs.join(', ')}`);
check(cs.slice(1).every((v) => Math.abs(((v / sr) % 0.75 + 0.75) % 0.75) < 0.06 || Math.abs(((v / sr) % 0.75) - 0.75) < 0.06),
      `each splice begins at a hit (${cs.map((v) => (v / sr).toFixed(3)).join(', ')} s)`);
const eq = M.cleanStarts(M.splitEqual(n, 8), n);
check(eq.length === 8, `8 equal splices: ${eq.length}`);
const meta = M.makeMeta(r.n, on), dv = new DataView(meta.buffer);
check(dv.getUint32(0, true) === 0x4C45455E && dv.getUint32(4, true) === n && dv.getUint32(8, true) === 4 &&
      dv.getUint32(48 + 12, true) === n, 'the reel\'s metadata (magic, length, 4 splices, the last to the end)');

const frames = M.reelFrames(r, meta);
check(frames.every((f) => f.bytes.length <= 640 && f.bytes.slice(1, -1).every((b) => b < 0x80)),
      `${frames.length} frames, each under 640 bytes and 7-bit`);
fs.writeFileSync(process.argv[3], Buffer.from([].concat(...frames.map((f) => f.bytes))));
console.log(`${process.argv[3]}: ${frames.length} frames`);
process.exit(fails ? 1 : 0);
