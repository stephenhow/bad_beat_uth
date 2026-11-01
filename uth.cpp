#include "uth.h"
#include <iostream>
#include <array>
#include <sstream>

using namespace poker;

uint64_t UltimateTexasHoldem::Outcomes::getTotal() const {
	uint64_t total = 0;
	const uint64_t *src = reinterpret_cast<const uint64_t *>(this);
	for (int i=0; i<elements(); i++) {
		total += src[i];
	}
	return total;
}

int UltimateTexasHoldem::Outcomes::elements() {
	return (sizeof(Outcomes)/sizeof(uint64_t));
}

void UltimateTexasHoldem::Outcomes::add(const Outcomes &other, int mult) {
	// treat both structs as flat arrays of uint64_t counters
	static_assert(sizeof(Outcomes) % sizeof(uint64_t) == 0, "Outcomes must contain only uint64_t members");
	uint64_t *dst = reinterpret_cast<uint64_t *>(this);
	const uint64_t *src = reinterpret_cast<const uint64_t *>(&other);
	for (int i=0; i<elements(); i++) {
		dst[i] += mult*src[i];
	}
}

uint64_t UltimateTexasHoldem::Stats::getTotal() const {
	uint64_t total = 0;
	for (int i=0; i<=2; i++) {
		total += outcomes[i].getTotal();
	}
	return total;
}

void UltimateTexasHoldem::Stats::clear() {
	memset(this, 0, sizeof(Stats));
}

UltimateTexasHoldem::Outcomes UltimateTexasHoldem::Stats::getFlattened() const {
	Outcomes outcomes;
	outcomes.clear();
	for (int p=2; p>=0; p--) {
		outcomes.add(this->outcomes[p]);
	}
	return outcomes;
}

void UltimateTexasHoldem::Stats::addOutcomes(int n, const Outcomes &src) {
	outcomes[n].add(src);
}

void UltimateTexasHoldem::Stats::add(const Stats &src, int mult) {
	for (int p=2; p>=0; p--) {
		this->outcomes[p].add(src.outcomes[p], mult);
	}
}

void UltimateTexasHoldem::Outcomes::clear() {
	memset(this, 0, sizeof(Outcomes));
}

double UltimateTexasHoldem::Outcomes::getEV(int play) const {
	double ev=0, outcome;
	uint64_t n;
	for (int r=Hand::ROYAL_FLUSH; r>=Hand::HIGH_CARD; r--) {
		Hand::Rank rank = static_cast<Hand::Rank>(r);
		for (int q=1; q>=0; q--) {
			// wins
			n = this->wins[r][q];
			outcome = getWin(play, q, rank);
			ev += (double)n*outcome;
			// losses
			n = this->losses[r][q];
			outcome = getLoss(play, q, rank);
			ev += (double)n*outcome;
		}
	}
	// ties
	n = this->ties;
	ev /= (double)getTotal();
	return ev;
}

double UltimateTexasHoldem::Stats::getEV() const {
	double ev=0;
	int play;
	uint64_t n;
	// 2x / 1x
	for (int p=2; p>=1; p--) {
		play = getPlay(p);
		n = this->outcomes[p].getTotal();
		if (n) {
			ev += (double)n*this->outcomes[p].getEV(play);
		}
	}
	// 0x
	uint64_t folds = this->outcomes[0].getTotal();
	ev += (double)folds*evFold();
	ev /= getTotal();
	return ev;
}

double UltimateTexasHoldem::Stats::getEV4x() const {
	Outcomes showdowns;
	showdowns = this->getFlattened();
	return showdowns.getEV(getPlay(BET_PREFLOP));
}

std::string UltimateTexasHoldem::getCSV(int play, Outcomes &outcomes, uint64_t total) {
	std::stringstream csv;
	uint64_t n;
	double freq, outcome;
	if (play == 0) {
		outcome = evFold();
		n = outcomes.getTotal();
		freq = (double)n/(double)total;
		csv << "Fold, " << n << ", " << freq << ", " << outcome << ", " << outcome*freq << std::endl;
	} else {
		for (int r=Hand::ROYAL_FLUSH; r>=Hand::HIGH_CARD; r--) {
			Hand::Rank rank = static_cast<Hand::Rank>(r);
			for (int q=1; q>=0; q--) {
				// wins
				n = outcomes.wins[r][q];
				outcome = getWin(play, q, rank);
				freq = (double)n/(double)total;
				if (n > 0) {
					csv << play << "x " << (q ? "qualified" : "non-qualified") << " win with " << Hand::rankNames[r] << ", " << n << ", " << freq << ", " << outcome << ", " << freq*outcome << std::endl;
				}
				// losses
				n = outcomes.losses[r][q];
				outcome = getLoss(play, q, rank);
				freq = (double)n/(double)total;
				if (n > 0) {
					csv << play << "x " << (q ? "qualified" : "non-qualified") << " loss with " << Hand::rankNames[r] << ", " << n << ", " << freq << ", " << outcome << ", " << freq*outcome << std::endl;
				}
			}
		}
		// ties
		csv << play << "x Ties, " << outcomes.ties << ", " << (double)outcomes.ties/(double)total << ", 0, 0" << std::endl;
	}
	return csv.str();
}

std::string UltimateTexasHoldem::getCSV(std::vector<Row> &rows) {
	std::stringstream csv;
	Outcomes outcomes4x = {};	// preflop play
	Stats stats0x = {};	// preflop check
	Outcomes showdowns;
	double ev4x, ev0x, ev=0;
	uint64_t total=0;
	int startingHands=0;
	csv << "Outcome, Combinations, Frequency, Net, Return" << std::endl;
	for (auto const &row : rows) {
		showdowns = row.stats.getFlattened();
		total += row.multiplier*showdowns.getTotal();
		startingHands += row.multiplier;
		ev4x = row.stats.getEV4x();
		ev0x = row.stats.getEV();
		if ((ev4x - ev0x) > 0.00) {
			outcomes4x.add(showdowns, row.multiplier);
			ev += ev4x*row.multiplier;
		} else {
			stats0x.add(row.stats, row.multiplier);
			ev += ev0x*row.multiplier;
		}
	}
	// 4x
	csv << getCSV(getPlay(BET_PREFLOP), outcomes4x, total);
	// 2x, 1x, fold
	for (int p=2; p>=0; p--) {
		csv << getCSV(getPlay(p), stats0x.outcomes[p], total);
	}
	ev /= (double)startingHands;
	csv << "Total, " << total << ", 1, ," << ev << std::endl; 
	return csv.str();
}

UltimateTexasHoldem::UltimateTexasHoldem() {
	int remaining;
	for (int i=1; i<=6; i++) {
		// preflop
		remaining = 52 - 2*i;
		draws3[remaining] = generate_combinations(remaining, 3);
		// flop
		remaining = 52 - 2*i - 3;
		draws2[remaining] = generate_combinations(remaining, 2);
		// river
		remaining = 52 - 2*i - 5;
		draws2[remaining] = generate_combinations(remaining, 2);
	}
}

int UltimateTexasHoldem::getPlay(int p) {
	switch (p) {
	case 3: return PREFLOP_PLAY_X;
	case 2: return 2;
	case 1: return 1;
	default:
	case 0: return 0;
	}
}

double UltimateTexasHoldem::getWin(int play, bool qual, Hand::Rank r) {
	//if (play == 0) return evFold();
	return (play + qual + blindWinPayout(r)); 
}

double UltimateTexasHoldem::getLoss(int play, bool qual, Hand::Rank r) {
	//if (play == 0) return evFold();
	return (badBeatPayout(r) - play - qual); 
}

double UltimateTexasHoldem::blindWinPayout(Hand::Rank r) {
	switch (r) {
	case Hand::ROYAL_FLUSH: return 500;
	case Hand::STRAIGHT_FLUSH: return 50;
	case Hand::FOUR_OF_A_KIND: return 10;
	case Hand::FULL_HOUSE: return 3;
	case Hand::FLUSH: return 1.5;
	case Hand::STRAIGHT: return 1;
	default: return 0;
	}
}

double UltimateTexasHoldem::badBeatPayout(Hand::Rank r) {
	if (!ENABLE_BAD_BEAT) return -1;
	switch (r) {
	case Hand::STRAIGHT_FLUSH: return 500;
	case Hand::FOUR_OF_A_KIND: return 25;
	case Hand::FULL_HOUSE: return 6;
	case Hand::FLUSH: return 5;
	case Hand::STRAIGHT: return 4;
	default: return -1;
	}
}

bool UltimateTexasHoldem::qualifies(Hand &dealer) {
	return (dealer.getRank() >= Hand::ONE_PAIR);
}

void UltimateTexasHoldem::getStats(const std::vector<Card> &playerHole, const std::vector<Card> &board, const std::vector<Card> &others, UltimateTexasHoldem::Stats &stats) {
	std::vector<Card> deck;
	init_deck(deck);
	remove_cards(deck, playerHole);
	remove_cards(deck, board);
	remove_cards(deck, others);
	int remaining = deck.size();
	double ev2x, ev1x, ev0x;
	Outcomes showdowns;
	Stats stats0x;	// check this street
	if (board.size() == 0) {
		stats.clear();
		// for each flop
		forEachCombination<Card>(deck, 3, [this, &playerHole, &others, &stats](const std::vector<Card> &flop) {
			getStats(playerHole, flop, others, stats);	// add stats for each flop
		});
	} else if (board.size() == 3) {
		stats0x.clear();
		// for each turn+river
		forEachCombination<Card>(deck, 2, [this, &playerHole, &board, &others, &stats0x](const std::vector<Card> &turnRiver) {
			std::vector<Card> board5(board);
			board5.insert(board5.begin(), turnRiver.begin(), turnRiver.end());
			getStats(playerHole, board5, others, stats0x);	// add stats for each turn&river
		});
		ev0x = stats0x.getEV();
		showdowns = stats0x.getFlattened();
		ev2x = showdowns.getEV(getPlay(BET_FLOP));
		if (ev0x > ev2x) {
			stats.addOutcomes(1, stats0x.outcomes[1]);
			stats.addOutcomes(0, stats0x.outcomes[0]);
		} else {
			stats.addOutcomes(2, showdowns);
		}
	} else if (board.size() == 5) {
		// showdowns
		Hand player(playerHole), dealer(board);
		player.add(board);
		player.eval();
		
		showdowns.clear();
		Card d0, d1;
		bool qual;
		Hand::Rank r;
		r = player.getRank();
		for (auto &holecards : draws2.at(remaining)) {
			d0 = deck[holecards.at(0)];
			d1 = deck[holecards.at(1)];
			dealer.add(d0);
			dealer.add(d1);
			dealer.eval();
			qual = qualifies(dealer);
			if (player > dealer) {
				showdowns.wins[r][qual]++;
			} else if (dealer > player) {
				showdowns.losses[r][qual]++;
			} else {
				showdowns.ties++;
			}
			dealer.remove(d0);
			dealer.remove(d1);
		}
		ev1x = showdowns.getEV(getPlay(BET_RIVER));
		if (ev1x > evFold()) {
			stats.addOutcomes(1, showdowns);
		} else {
			stats.addOutcomes(0, showdowns);
		}
	}
}

// Preflop 4x decisions for the standard game (ENABLE_BAD_BEAT false), as 13x13 hand charts indexed by rank
// (DEUCE = 0): pairs on the diagonal [r][r], suited hands above it [high][low], offsuit hands below it [low][high].
// Each entry is the maximum number of copies of the hand's ranks among the known cards for which to 4x;
// -1 means check even with 0 copies, 6 means 4x with any number of copies.
using Chart = std::array<std::array<int, Card::ACE + 1>, Card::ACE + 1>;

static const Chart basicChart = {{	// no information: basic strategy (from uth.sqlite3), -1 check / 6 4x
	//  2   3   4   5   6   7   8   9   T   J   Q   K   A
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  6 },	// 2
	{ -1,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  6 },	// 3
	{ -1, -1,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1,  6 },	// 4
	{ -1, -1, -1,  6, -1, -1, -1, -1, -1, -1, -1,  6,  6 },	// 5
	{ -1, -1, -1, -1,  6, -1, -1, -1, -1, -1, -1,  6,  6 },	// 6
	{ -1, -1, -1, -1, -1,  6, -1, -1, -1, -1, -1,  6,  6 },	// 7
	{ -1, -1, -1, -1, -1, -1,  6, -1, -1, -1,  6,  6,  6 },	// 8
	{ -1, -1, -1, -1, -1, -1, -1,  6, -1, -1,  6,  6,  6 },	// 9
	{ -1, -1, -1, -1, -1, -1, -1, -1,  6,  6,  6,  6,  6 },	// T
	{ -1, -1, -1, -1, -1, -1,  6,  6,  6,  6,  6,  6,  6 },	// J
	{ -1, -1, -1, -1,  6,  6,  6,  6,  6,  6,  6,  6,  6 },	// Q
	{  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6 },	// K
	{  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6 },	// A
}};

static const Chart known10Chart = {{	// 5 known players (10 cards): max copies for 4x from 6p_preflop_thresholds.md
	//  2   3   4   5   6   7   8   9   T   J   Q   K   A
	{  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  2 },	// 2
	{ -1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  2 },	// 3
	{ -1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1,  1,  2 },	// 4
	{ -1, -1, -1,  2, -1, -1, -1, -1, -1, -1,  0,  1,  3 },	// 5
	{ -1, -1, -1, -1,  2, -1, -1, -1, -1, -1,  0,  1,  3 },	// 6
	{ -1, -1, -1, -1, -1,  6, -1, -1, -1,  0,  0,  1,  3 },	// 7
	{ -1, -1, -1, -1, -1, -1,  6, -1,  0,  0,  1,  2,  3 },	// 8
	{ -1, -1, -1, -1, -1, -1,  0,  6,  0,  1,  1,  2,  3 },	// 9
	{ -1, -1, -1, -1, -1,  0,  0,  1,  6,  1,  2,  2,  4 },	// T
	{ -1, -1, -1, -1,  0,  0,  1,  1,  2,  6,  2,  3,  6 },	// J
	{  0,  0,  0,  1,  1,  1,  2,  2,  2,  2,  6,  3,  6 },	// Q
	{  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,  6,  6 },	// K
	{  2,  3,  3,  4,  4,  4,  4,  4,  4,  6,  6,  6,  6 },	// A
}};

// maxCopies[number of other players whose hole cards are known, 0..5]: only 0 (basic strategy) and 5 (10 known cards)
// are measured; 1-4 known players use basic strategy until measured. Untested hands in known10Chart keep their basic decision.
static const std::array<Chart, 6> maxCopies = [] {
	std::array<Chart, 6> charts;
	charts.fill(basicChart);
	charts[5] = known10Chart;
	return charts;
}();

bool UltimateTexasHoldem::isPlay4x(const std::vector<poker::Card> &playerHole, const std::vector<poker::Card> &others) {
	int copies=0;
	poker::Card::Rank p0, p1;
	p0 = std::max(playerHole.at(0).rank, playerHole.at(1).rank);
	p1 = std::min(playerHole.at(0).rank, playerHole.at(1).rank);
	bool pair = (p0 == p1);
	bool suited = (playerHole[0].suit == playerHole[1].suit);
	for (auto &c : others) {
		if (c.rank == p0) copies++;
		if (!pair && (c.rank == p1)) copies++;
	}
	// known players: 2 hole cards each; anything else (an odd others count, or more than 5 players) uses basic strategy
	const size_t known = others.size() % 2 == 0 && others.size() / 2 < maxCopies.size() ? others.size() / 2 : 0;
	return copies <= (suited || pair ? maxCopies[known][p0][p1] : maxCopies[known][p1][p0]);
}
