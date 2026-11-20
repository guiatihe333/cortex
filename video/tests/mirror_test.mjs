// CORTEX VIDEO: the mirror's WebAssembly (build/mirror/mirror.wasm) replays what the simulated FM-1 sent
// (tests/scenarios/link.cvx's log): every frame's sum must agree, as tests/follow_test.c checks natively.
//   node tests/mirror_test.mjs build/mirror/mirror.wasm OUT/link.syx
import { readFileSync } from "node:fs";
const [wasmPath, logPath, wantRec = "2"] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), {});
const x = instance.exports;
if (x._initialize) x._initialize();
const log = readFileSync(logPath);
let start = 0, frames = 0, recTurns = 0, wasRec = 0;
for (let i = 0; i < log.length; i++) {
  if (log[i] === 0xf0) start = i;
  else if (log[i] === 0xf7) {
    const m = log.subarray(start, i + 1);
    new Uint8Array(x.memory.buffer, x.mx_in(), m.length).set(m);
    const r = x.mx_msg(m.length);
    if (r === 1) {
      frames++;
      const rec = x.mx_stat(1);
      if (rec !== wasRec) { recTurns++; wasRec = rec; }
    }
  }
}
const s = i => x.mx_stat(i);
console.log(`mirror_test: ${s(6)} syncs, ${s(3)} frames, ${s(4)} gaps, ${s(5)} disagreeing, ${s(7)} bad, REC turned ${recTurns} times`);
const ok = s(6) >= 1 && s(3) >= 40 && !s(4) && !s(5) && !s(7) && recTurns === +wantRec;
console.log(`mirror_test: ${ok ? "ok" : "FAILED"}`);
process.exit(ok ? 0 : 1);
