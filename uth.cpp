#include "uth.h"
#include <iostream>

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
	for (int i = 0; i < 4; i++) {
		total += outcomes[i].getTotal();
	}
	return total;
}

void UltimateTexasHoldem::Stats::clear() {
	memset(this, 0, sizeof(Stats));
}

UltimateTexasHoldem::Outcomes UltimateTexasHoldem::Stats::getFlattened() {
	Outcomes outcomes;
	outcomes.clear();
	for (int p=3; p>=0; p--) {
		outcomes.add(this->outcomes[p]);
	}
	return outcomes;
}

void UltimateTexasHoldem::Stats::addOutcomes(int n, const Outcomes &src) {
	outcomes[n].add(src);
}

void UltimateTexasHoldem::Stats::add(const Stats &src, int mult) {
	for (int p=3; p>=0; p--) {
		this->outcomes[p].add(src.outcomes[p], mult);
	}
}

void UltimateTexasHoldem::Outcomes::clear() {
	memset(this, 0, sizeof(Outcomes));
}

double UltimateTexasHoldem::Outcomes::getEV(int play, bool verbose) {
	double ev=0, outcome;
	uint64_t n;
	for (int r=Hand::ROYAL_FLUSH; r>=Hand::HIGH_CARD; r--) {
		Hand::Rank rank = static_cast<Hand::Rank>(r);
		for (int q=1; q>=0; q--) {
			// wins
			n = this->wins[r][q];
			outcome = getWin(play, q, rank);
			if (verbose && (n > 0)) {
				std::cout << play << "x " << (q ? "qualified" : "non-qualified") << " win with " << Hand::rankNames[r] << ", " << n << ", " << outcome << std::endl;
			}
			ev += (double)n*outcome;
			// losses
			n = this->losses[r][q];
			outcome = getLoss(play, q, rank);
			if (verbose && (n > 0)) {
				std::cout << play << "x " << (q ? "qualified" : "non-qualified") << " loss with " << Hand::rankNames[r] << ", " << n << ", " << outcome << std::endl;
			}
			ev += (double)n*outcome;
		}
	}
	// ties
	n = this->ties;
	if (verbose && (n > 0)) {
		std::cout << play << "x tie, " << n << ", 0" << std::endl;
	}
	ev /= (double)getTotal();
	return ev;
}

double UltimateTexasHoldem::Stats::getEV(bool verbose) {
	double ev=0;
	int play;
	uint64_t n;
	// 4x / 2x / 1x
	for (int p=3; p>=1; p--) {
		play = getPlay(p);
		n = this->outcomes[p].getTotal();
		if (n) {
			ev += (double)n*this->outcomes[p].getEV(play, verbose);
		}
	}
	// 0x
	uint64_t folds = this->outcomes[0].getTotal();
	if (verbose && (folds > 0)) {
		std::cout << "Folds, " << folds << ", " << evFold() << std::endl;
	}
	ev += (double)folds*evFold();
	ev /= getTotal();
	return ev;
}

UltimateTexasHoldem::UltimateTexasHoldem() {
	// all C(45,2) = 990 dealer hole card combos, as index pairs into the 45 remaining cards
	dealerHoles.reserve(990);
	for (int i = 0; i < 45; i++)
		for (int j = i + 1; j < 45; j++)
			dealerHoles.emplace_back(i, j);
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

void UltimateTexasHoldem::getStats(const std::vector<Card> &playerHole, const std::vector<Card> &board, UltimateTexasHoldem::Stats &stats) {
	std::vector<Card> deck;
	init_deck(deck);
	remove_cards(deck, playerHole);
	remove_cards(deck, board);
	double ev4x, ev2x, ev1x, ev0x;
	Outcomes showdowns;
	Stats stats0x;	// check this street
	if (board.size() == 0) {
		stats0x.clear();
		// for each flop
		forEachCombination<Card>(deck, 3, [this, &playerHole, &stats0x](const std::vector<Card> &flop) {
			getStats(playerHole, flop, stats0x);	// add stats for each flop
		});
		ev0x = stats0x.getEV();
		showdowns = stats0x.getFlattened();
		ev4x = showdowns.getEV(getPlay(3));
		if (ev0x > ev4x) {
			stats = stats0x;
		} else {
			stats.clear();
			stats.addOutcomes(3, showdowns);
		}
	} else if (board.size() == 3) {
		stats0x.clear();
		
		// for each turn+river
		forEachCombination<Card>(deck, 2, [this, &playerHole, &board, &stats0x](const std::vector<Card> &turnRiver) {
			std::vector<Card> board5(board);
			board5.insert(board5.begin(), turnRiver.begin(), turnRiver.end());
			getStats(playerHole, board5, stats0x);	// add stats for each turn&river
		});
		ev0x = stats0x.getEV();
		showdowns = stats0x.getFlattened();
		ev2x = showdowns.getEV(getPlay(2));
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
		for (auto &holecards : dealerHoles) {
			d0 = deck[holecards.first];
			d1 = deck[holecards.second];
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
		ev1x = showdowns.getEV(getPlay(1));
		if (ev1x > evFold()) {
			stats.addOutcomes(1, showdowns);
		} else {
			stats.addOutcomes(0, showdowns);
		}
	}
}