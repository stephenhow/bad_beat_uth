#ifndef UTH_H
#define UTH_H

#include "poker.h"
#include "database.h"
#include <utility>

#define ENABLE_BAD_BEAT true
#define PREFLOP_PLAY_X 4

class UltimateTexasHoldem {
protected:
	std::map<int, std::vector< std::vector<int>>> draws3, draws2;	// C(remaining.size(),3) and C(remaining.size(),2) to speed up calculations
public:
	enum Plays {FOLD=0, BET_RIVER=1, BET_FLOP=2, BET_PREFLOP=3};
	struct Outcomes {
		uint64_t wins[poker::Hand::ROYAL_FLUSH+1][2], losses[poker::Hand::ROYAL_FLUSH+1][2], ties;
		uint64_t getTotal() const;
		double getEV(int play) const;
		void add(const Outcomes &src, int mult=1);
		static int elements();
		void clear();
	};
	struct Stats {
		Outcomes outcomes[3];	// [0x, 1x, 2x]
		//void addFrom(Stats &other);
		uint64_t getTotal() const;
		double getEV() const;
		void addOutcomes(int n, const Outcomes &src);
		Outcomes getFlattened() const;
		void add(const Stats &src, int mult=1);
		void clear();
		double getEV4x() const;
	};
	struct Row {
		int id;
		int multiplier;
		std::string holecards;
		std::string others;
		Stats stats;
		bool is4x();
	};
	UltimateTexasHoldem();
	static int getPlay(int);
	static double blindWinPayout(poker::Hand::Rank r);
	static double badBeatPayout(poker::Hand::Rank r);
	static double getWin(int play, bool qual, poker::Hand::Rank r);
	static double getLoss(int play, bool qual, poker::Hand::Rank r);
	static double evFold() { return -2; };
	void getStats(const std::vector<poker::Card> &playerHole, const std::vector<poker::Card> &board, const std::vector<poker::Card> &others, Stats &stats);
	bool qualifies(poker::Hand &dealer);
	static std::string getCSV(int play, Outcomes &outcomes, uint64_t total);
	static std::string getCSV(std::vector<Row> &rows);
};

#endif
