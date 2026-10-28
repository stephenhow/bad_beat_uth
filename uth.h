#ifndef UTH_H
#define UTH_H

#include "poker.h"
#include <utility>

#define ENABLE_BAD_BEAT true

class UltimateTexasHoldem {
protected:
	std::vector<std::pair<int, int>> dealerHoles;	// C(45,2) index pairs into the 45 remaining cards
public:
	struct Outcomes {
		uint64_t wins[poker::Hand::ROYAL_FLUSH+1][2], losses[poker::Hand::ROYAL_FLUSH+1][2], ties;
		uint64_t getTotal();
		double getEV(int play, bool verbose=false);
		void add(const Outcomes &src, int mult=1);
		static int elements();
		void clear();
	};
	struct Stats {
		Outcomes outcomes[4];	// [0x, 1x, 2x, 4x]
		//void addFrom(Stats &other);
		uint64_t getTotal();
		double getEV(bool verbose=false);
		void addOutcomes(int n, const Outcomes &src);
		Outcomes getFlattened();
		void add(const Stats &src, int mult=1);
		void clear();
	};
	struct Row {
		int id;
		int multiplier;
		std::string holecards;
		Stats stats;
	};
	UltimateTexasHoldem();
	static int getPlay(int);
	static double blindWinPayout(poker::Hand::Rank r);
	static double badBeatPayout(poker::Hand::Rank r);
	static double getWin(int play, bool qual, poker::Hand::Rank r);
	static double getLoss(int play, bool qual, poker::Hand::Rank r);
	static double evFold() { return -2; };
	void getStats(const std::vector<poker::Card> &playerHole, const std::vector<poker::Card> &board, Stats &stats);
	bool qualifies(poker::Hand &dealer);
};

#endif
