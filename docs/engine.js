// Bad Beat Ultimate Texas Hold'em engine: hand evaluation, payouts, and optimal decisions.
// Mirrors uth.cpp (ENABLE_BAD_BEAT true, PREFLOP_PLAY_X 4). Cards are ints 0..51: rank = c >> 2 (0 = deuce .. 12 = ace), suit = c & 3.
(function (root) {
	'use strict';

	const RANKS = '23456789TJQKA';
	const SUITS = 'cdhs';
	// Hand categories, same order as poker::Hand::Rank
	const HAND_NAMES = ['High Card', 'One Pair', 'Two Pairs', 'Three of a Kind', 'Straight', 'Flush', 'Full House', 'Four of a Kind', 'Straight Flush', 'Royal Flush'];
	const ROYAL = 9, STRAIGHT_FLUSH = 8, QUADS = 7, FULL_HOUSE = 6, FLUSH = 5, STRAIGHT = 4, TRIPS = 3, TWO_PAIRS = 2, ONE_PAIR = 1;

	const PREFLOP_PLAY_X = 4;
	const EV_FOLD = -2;
	const BLIND_PAYS = { 9: 500, 8: 50, 7: 10, 6: 3, 5: 1.5, 4: 1 };	// blindWinPayout
	const BAD_BEAT_PAYS = { 8: 500, 7: 25, 6: 6, 5: 5, 4: 4 };		// badBeatPayout

	function blindWin(cat) { return BLIND_PAYS[cat] || 0; }
	function badBeat(cat) { return BAD_BEAT_PAYS[cat] !== undefined ? BAD_BEAT_PAYS[cat] : -1; }
	// net result in ante units, like getWin()/getLoss(); ties push everything
	function winAmount(play, qual, cat) { return play + (qual ? 1 : 0) + blindWin(cat); }
	function lossAmount(play, qual, cat) { return badBeat(cat) - play - (qual ? 1 : 0); }

	// ---- evaluation ----
	const rc = new Int8Array(13), sc = new Int8Array(4), sm = new Int32Array(4);

	// highest straight in a 13-bit rank mask, ace may play low; returns top rank or -1
	function strHigh(mask) {
		const m = (mask << 1) | ((mask >> 12) & 1);	// bit 0 = ace-low
		for (let h = 13; h >= 4; h--) {
			if (((m >> (h - 4)) & 31) === 31) return h - 1;
		}
		return -1;
	}
	function topBits(mask, n) {
		let v = 0, k = 0;
		for (let r = 12; r >= 0 && k < n; r--) if (mask & (1 << r)) { v = (v << 4) | r; k++; }
		while (k < n) { v <<= 4; k++; }
		return v;
	}

	// score of the best 5-card hand among n cards; higher wins. category = score >>> 20
	function evaluate(cards, n) {
		rc.fill(0); sc.fill(0); sm.fill(0);
		let all = 0;
		for (let i = 0; i < n; i++) {
			const c = cards[i], r = c >> 2, s = c & 3;
			rc[r]++; sc[s]++; sm[s] |= 1 << r; all |= 1 << r;
		}
		for (let s = 0; s < 4; s++) {
			if (sc[s] >= 5) {
				const h = strHigh(sm[s]);
				if (h >= 0) return ((h === 12 ? ROYAL : STRAIGHT_FLUSH) << 20) | h;
				return (FLUSH << 20) | topBits(sm[s], 5);
			}
		}
		let quad = -1, trip1 = -1, trip2 = -1, pair1 = -1, pair2 = -1, pair3 = -1;
		for (let r = 12; r >= 0; r--) {
			const k = rc[r];
			if (k === 4) quad = r;
			else if (k === 3) { if (trip1 < 0) trip1 = r; else if (trip2 < 0) trip2 = r; }
			else if (k === 2) { if (pair1 < 0) pair1 = r; else if (pair2 < 0) pair2 = r; else if (pair3 < 0) pair3 = r; }
		}
		if (quad >= 0) return (QUADS << 20) | (quad << 4) | topBits(all & ~(1 << quad), 1);
		if (trip1 >= 0 && (trip2 >= 0 || pair1 >= 0)) {
			const p = Math.max(trip2, pair1);
			return (FULL_HOUSE << 20) | (trip1 << 4) | p;
		}
		const h = strHigh(all);
		if (h >= 0) return (STRAIGHT << 20) | h;
		if (trip1 >= 0) return (TRIPS << 20) | (trip1 << 8) | topBits(all & ~(1 << trip1), 2);
		if (pair2 >= 0) return (TWO_PAIRS << 20) | (pair1 << 8) | (pair2 << 4) | topBits(all & ~(1 << pair1) & ~(1 << pair2), 1);
		if (pair1 >= 0) return (ONE_PAIR << 20) | (pair1 << 12) | topBits(all & ~(1 << pair1), 3);
		return topBits(all, 5);
	}
	function category(score) { return score >>> 20; }

	// ---- outcome counting ----
	// per-street tallies: wins[cat][qual], losses[cat][qual], ties -- same shape as UltimateTexasHoldem::Outcomes
	function newOutcomes() { return { wins: Array.from({ length: 10 }, () => [0, 0]), losses: Array.from({ length: 10 }, () => [0, 0]), ties: 0, total: 0 }; }
	function outcomesEV(o, play) {
		let ev = 0;
		for (let r = 0; r < 10; r++) for (let q = 0; q < 2; q++) {
			ev += o.wins[r][q] * winAmount(play, q, r) + o.losses[r][q] * lossAmount(play, q, r);
		}
		return ev / o.total;
	}
	function addOutcomes(dst, src) {
		for (let r = 0; r < 10; r++) for (let q = 0; q < 2; q++) { dst.wins[r][q] += src.wins[r][q]; dst.losses[r][q] += src.losses[r][q]; }
		dst.ties += src.ties; dst.total += src.total;
	}

	function remaining(used) {
		const u = new Uint8Array(52); used.forEach(c => { u[c] = 1; });
		const d = []; for (let c = 0; c < 52; c++) if (!u[c]) d.push(c);
		return d;
	}

	const p7 = new Int32Array(7), d7 = new Int32Array(7);
	// showdown tallies against all C(45,2) dealer hands, for a complete 5-card board
	function riverOutcomes(hole, board) {
		const o = newOutcomes();
		p7[0] = hole[0]; p7[1] = hole[1];
		for (let i = 0; i < 5; i++) { p7[i + 2] = board[i]; d7[i + 2] = board[i]; }
		const ps = evaluate(p7, 7), cat = category(ps);
		const deck = remaining([...hole, ...board]);
		for (let i = 0; i < deck.length; i++) {
			d7[0] = deck[i];
			for (let j = i + 1; j < deck.length; j++) {
				d7[1] = deck[j];
				const ds = evaluate(d7, 7);
				const q = category(ds) >= ONE_PAIR ? 1 : 0;
				if (ps > ds) o.wins[cat][q]++;
				else if (ds > ps) o.losses[cat][q]++;
				else o.ties++;
				o.total++;
			}
		}
		return o;
	}

	// river: Play 1x vs fold
	function riverDecision(hole, board) {
		const o = riverOutcomes(hole, board);
		const ev1x = outcomesEV(o, 1);
		return { ev: { bet: ev1x, check: EV_FOLD }, best: ev1x > EV_FOLD ? 'bet' : 'fold', outcomes: o };
	}

	// flop: Play 2x vs check (then best river play on each of the 1,081 turn/river boards).
	// Runs in slices so the page stays responsive; onDone receives the result.
	function flopDecision(hole, flop, onDone, onProgress) {
		const deck = remaining([...hole, ...flop]);
		const all = newOutcomes();
		let checkSum = 0, boards = 0, i = 0;
		const board = [flop[0], flop[1], flop[2], 0, 0];
		function slice() {
			const stop = Math.min(deck.length, i + 4);
			for (; i < stop; i++) {
				board[3] = deck[i];
				for (let j = i + 1; j < deck.length; j++) {
					board[4] = deck[j];
					const o = riverOutcomes(hole, board);
					addOutcomes(all, o);
					checkSum += Math.max(outcomesEV(o, 1), EV_FOLD);
					boards++;
				}
			}
			if (onProgress) onProgress(i / deck.length);
			if (i < deck.length) { setTimeout(slice, 0); return; }
			const ev2x = outcomesEV(all, 2), evCheck = checkSum / boards;
			onDone({ ev: { bet: ev2x, check: evCheck }, best: evCheck > ev2x ? 'check' : 'bet', boards, total: all.total });
		}
		slice();
	}
	// synchronous version (used by tests)
	function flopDecisionSync(hole, flop) {
		const deck = remaining([...hole, ...flop]);
		const all = newOutcomes(); let checkSum = 0, boards = 0;
		const board = [flop[0], flop[1], flop[2], 0, 0];
		for (let i = 0; i < deck.length; i++) {
			board[3] = deck[i];
			for (let j = i + 1; j < deck.length; j++) {
				board[4] = deck[j];
				const o = riverOutcomes(hole, board);
				addOutcomes(all, o);
				checkSum += Math.max(outcomesEV(o, 1), EV_FOLD);
				boards++;
			}
		}
		const ev2x = outcomesEV(all, 2), evCheck = checkSum / boards;
		return { ev: { bet: ev2x, check: evCheck }, best: evCheck > ev2x ? 'check' : 'bet', boards, total: all.total };
	}

	// ---- preflop: precomputed from bb_uth4x.sqlite3 ----
	// key -> [raise 4x?, EV of raising 4x, EV of checking (only known when checking is best)]
	let PREFLOP = {};
	function setPreflopTable(t) { PREFLOP = t; }
	function handKey(hole) {
		let a = hole[0] >> 2, b = hole[1] >> 2;
		if (a < b) { const t = a; a = b; b = t; }
		if (a === b) return RANKS[a] + RANKS[b];
		return RANKS[a] + RANKS[b] + ((hole[0] & 3) === (hole[1] & 3) ? 's' : 'o');
	}
	function preflopDecision(hole) {
		const k = handKey(hole), e = PREFLOP[k];
		return { key: k, best: e[0] ? 'bet' : 'check', ev: { bet: e[1], check: e[2] } };
	}

	// ---- settlement of an actual hand ----
	function settle(hole, board, dealer, play) {
		const ps = evaluate([...hole, ...board], 7), ds = evaluate([...dealer, ...board], 7);
		const cat = category(ps), dcat = category(ds), qual = dcat >= ONE_PAIR;
		const r = { playerCat: cat, dealerCat: dcat, qual, play, ante: 0, blind: 0, playBet: 0, result: 'tie', badBeat: false };
		if (play === 0) { r.result = 'fold'; r.ante = -1; r.blind = -1; }
		else if (ps > ds) { r.result = 'win'; r.ante = qual ? 1 : 0; r.blind = blindWin(cat); r.playBet = play; }
		else if (ds > ps) { r.result = 'loss'; r.ante = qual ? -1 : 0; r.blind = badBeat(cat); r.playBet = -play; r.badBeat = r.blind > 0; }
		r.net = r.ante + r.blind + r.playBet;
		return r;
	}

	function parseCards(s) { return s.trim().split(/\s+/).map(t => RANKS.indexOf(t[0]) * 4 + SUITS.indexOf(t[1])); }
	function cardName(c) { return RANKS[c >> 2] + SUITS[c & 3]; }

	const api = {
		RANKS, SUITS, HAND_NAMES, PREFLOP_PLAY_X, EV_FOLD, BLIND_PAYS, BAD_BEAT_PAYS,
		evaluate, category, riverDecision, riverOutcomes, flopDecision, flopDecisionSync, preflopDecision, setPreflopTable, handKey,
		settle, parseCards, cardName, outcomesEV,
	};
	if (typeof module !== 'undefined' && module.exports) module.exports = api; else root.UTH = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
