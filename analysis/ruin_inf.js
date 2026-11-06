// Infinite-horizon ruin probability for Ultimate Texas Hold'em bankrolls (see bankroll_indefinite_play.md).
//
// usage: node analysis/ruin_inf.js <outcomes.csv> [upperBarrierUnits=6000]
//   e.g. node analysis/ruin_inf.js bb4x_uth.csv
//
// The CSV is getCSV()/evalGame() output (columns include Combinations and Net). The bankroll moves one hand at a
// time by the per-hand net result, on a half-unit grid (1.5:1 flush pays leave half units). psi(b), the probability
// of ever falling below 6 units (the most one hand can lose), solves
//     psi(b) = sum_j P(j) * psi(b + j),   psi = 1 below 6 units,   psi = 0 at the upper barrier,
// by Gauss-Seidel iteration; a barrier far above the bankrolls of interest makes this indefinite play.
// Only meaningful for a game with a positive edge; with a negative edge, ruin is certain.
const fs = require('fs');
const U = Number(process.argv[3] || 6000);
const MIN_BANKROLL = 6;

const lines = fs.readFileSync(process.argv[2], 'utf8').split('\n').map(l => l.split(',').map(x => x.trim()));
const head = lines[0], iN = head.indexOf('Combinations'), iNet = head.indexOf('Net');
const by = new Map(); let total = 0;
for (const r of lines.slice(1)) {
	if (r.length <= iNet || r[iNet] === '' || /^total$/i.test(r[0])) continue;
	const k = Math.round(parseFloat(r[iNet]) * 2), n = Number(r[iN]);
	by.set(k, (by.get(k) || 0) + n); total += n;
}
const J = [...by.keys()], P = J.map(k => by.get(k) / total);	// net results in half units, and their probabilities
const mean = J.reduce((s, k, i) => s + k / 2 * P[i], 0);
const variance = J.reduce((s, k, i) => s + (k / 2) ** 2 * P[i], 0) - mean ** 2;
if (mean <= 0) { console.log('mean', mean.toFixed(6), '<= 0: ruin is certain for any bankroll'); process.exit(0); }

// Lundberg exponent: E[exp(-R X)] = 1, R > 0 (bisection). Used as a cross-check and as the starting guess.
const f = R => J.reduce((s, k, i) => s + P[i] * Math.exp(-R * k / 2), 0) - 1;
let lo = 1e-9, hi = 1;
while (f(hi) < 0) hi *= 2;
for (let it = 0; it < 200; it++) { const m = (lo + hi) / 2; if (f(m) < 0) lo = m; else hi = m; }
const R = (lo + hi) / 2;
console.log(`per hand: mean ${mean.toFixed(6)}, variance ${variance.toFixed(3)}, Lundberg R ${R.toFixed(6)} (diffusion approx ${(2 * mean / variance).toFixed(6)})`);

const bot = MIN_BANKROLL * 2, top = U * 2;
const psi = new Float64Array(top);
for (let b = 0; b < bot; b++) psi[b] = 1;
for (let b = bot; b < top; b++) psi[b] = Math.exp(-R * (b / 2 - MIN_BANKROLL));
let sweeps = 0;
for (;; sweeps++) {
	let d = 0;
	for (let b = bot; b < top; b++) {
		let v = 0;
		for (let j = 0; j < J.length; j++) { const t = b + J[j]; v += P[j] * (t < bot ? 1 : t >= top ? 0 : psi[t]); }
		d = Math.max(d, Math.abs(v - psi[b])); psi[b] = v;
	}
	if (d < 1e-13) break;
}
console.log(`solved: ${sweeps} sweeps, upper barrier ${U} units`);

const at = u => psi[Math.round(u * 2)];
console.log('bankroll  P(ever bust)  Lundberg bound');
for (const u of [500, 1000, 1500, 2000, 2500]) if (u < U / 2) console.log(String(u).padStart(8), at(u).toExponential(4).padStart(13), Math.exp(-R * (u - MIN_BANKROLL)).toExponential(4).padStart(15));
for (const target of [0.05, 0.01, 0.001]) {
	let b = bot; while (b < top && psi[b] > target) b++;
	console.log(`P(ever bust) <= ${target}: bankroll >= ${b / 2} units`);
}
