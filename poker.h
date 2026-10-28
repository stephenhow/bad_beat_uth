#ifndef POKER_H
#define POKER_H

#pragma once

#include <vector>
#include <algorithm>
#include <cstring>
#include <functional>
#include <cstdint>
#include <iostream>
#include <random>

namespace poker {

using rank_mask_t = std::uint16_t;

struct Card {
	enum Suit {CLUBS=0, DIAMONDS, HEARTS, SPADES};
	enum Rank {DEUCE=0, TREY, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, ACE, JOKER};
	static const char* rankNames[14];
    static const char* suitNames[4];

	Rank rank;
	Suit suit;

	// Default constructor
    Card() = default;

	Card(Rank r, Suit s) : rank(r), suit(s) {}

	bool operator>(const Card& other) const {
        return rank > other.rank;
    }

    // Static function to convert char to Rank
    static Rank char_to_rank(char r) {
        switch (r) {
            case '2': return DEUCE;
            case '3': return TREY;
            case '4': return FOUR;
            case '5': return FIVE;
            case '6': return SIX;
            case '7': return SEVEN;
            case '8': return EIGHT;
            case '9': return NINE;
            case 'T': return TEN;
            case 'J': return JACK;
            case 'Q': return QUEEN;
            case 'K': return KING;
            case 'A': return ACE;
            case '*': return JOKER;
            default: throw std::invalid_argument("Invalid rank character");
        }
    }

    static Suit char_to_suit(char s) {
    	switch (s) {
    		case 'c': return CLUBS;
    		case 'd': return DIAMONDS;
    		case 'h': return HEARTS;
    		case 's': return SPADES;
    		default: throw std::invalid_argument("Invalid suit character");
    	}
    }

    std::string toString() const;  // member function to describe the card

};

inline std::ostream& operator<<(std::ostream& os, const Card::Rank r) {
    return os << Card::rankNames[r];
}

inline std::ostream& operator<<(std::ostream& os, const Card::Suit s) {
    return os << Card::suitNames[s];
}

inline bool operator==(const Card& lhs, const Card& rhs) {
	if ((lhs.rank == Card::JOKER) && (rhs.rank == Card::JOKER)) return true;
    return lhs.rank == rhs.rank && lhs.suit == rhs.suit;
}

inline std::ostream& operator<<(std::ostream& os, const Card& card) {
	os << card.rank;
	if (card.rank != Card::JOKER) os << card.suit;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const std::vector<Card> &hand) {
	for (auto it=hand.begin(); it!=hand.end(); it++) {
		if (it != hand.begin()) os << " ";
		os << *it;
	}
	return os;
}

void init_deck(std::vector<Card> &deck);
inline const std::vector<poker::Card> FULL_DECK = [] {
	std::vector<poker::Card> deck;
	init_deck(deck);
	return deck;
}();

std::vector<Card> getCards(const std::string desc);
std::string cards_to_str(const std::vector<Card> &);

inline void add_joker(std::vector<Card> &deck) { deck.push_back(Card{Card::Rank::JOKER, Card::Suit::CLUBS}); }
inline const std::vector<poker::Card> JOKER_DECK = [] {
	std::vector<poker::Card> deck;
	init_deck(deck);
	add_joker(deck);
	return deck;
}();

inline void remove_cards(std::vector<Card> &dst, const std::vector<Card> &src) {
	for (const Card &c : src) {
		auto it = std::find(dst.begin(), dst.end(), c);
		if (it != dst.end()) {
			dst.erase(it);
		}
	}
}

inline bool is_joker(const Card &c) { return (c.rank == Card::JOKER); }

inline bool contains_rank(rank_mask_t mask, Card::Rank r) { return (mask>>r)&0x1; }

inline bool straight_contains(Card::Rank strRank, Card::Rank r) {
	return (r <= strRank) && ((static_cast<int>(strRank) - static_cast<int>(r)) <= 4);
}

inline void addStats(unsigned long dst[], const unsigned long src[], std::size_t size, int multiplier=1) {
	for (int i=0; i<(size/sizeof(unsigned long)); i++) {
		dst[i] += multiplier*src[i];
	}
}

#pragma once
inline unsigned long sumStats(const unsigned long src[], std::size_t size) {
	unsigned long total=0;
	for (int i=0; i<(size/sizeof(unsigned long)); i++) total += src[i];
	return total;
}

#pragma once
inline bool is_no_gap(Card::Rank a, Card::Rank b) {
	return (std::abs(a-b) == 1);
}

#pragma once
inline int hamming_weight(unsigned int k) {
	int ones=0;
	while (k) {
		ones += k&1;
		k = (k>>1);
	}
	return ones;
}

class Hand {
public:
	enum Rank {HIGH_CARD=0, ONE_PAIR, TWO_PAIRS, THREE_OF_A_KIND, STRAIGHT, FLUSH, FULL_HOUSE, FOUR_OF_A_KIND, STRAIGHT_FLUSH, ROYAL_FLUSH, FIVE_OF_A_KIND};
	static const char* rankNames[11];
protected:
	rank_mask_t suitMask[4];		// rankMask by suit
	static bool initialized;
	static Card::Rank strTable[16384], wildStrTable[16384], openEndedStr[16384];
	static rank_mask_t highest5[16384], highest4[16384], highest3[16384], highest2[16384], highest1[16384];	// maps of any rank_mask_t to its highest 5 ranks, 4 ranks, etc.
	static std::uint8_t hammingWeight[16384];
	static Card::Rank highestRank[16384], secondRank[16384], thirdRank[16384], lowestRank[16384];
	rank_mask_t singletons, pairs, trips, quads;	// rankMasks for each
	int pairsCnt, tripsCnt;
	char rankCnt[14];	// [DEUCE, JOKER]
	// hand value is described by these parameters:
	Rank rank;
	Card::Rank r0, r1, strRank;
	rank_mask_t rankMask, kickerMask;
	bool hasJoker;
	int suitCnt[4];
	Card::Suit maxSuit;
	int maxSuitCnt;
	rank_mask_t improveKickersWithJoker(rank_mask_t);
	int getHammingWeight(rank_mask_t);
	rank_mask_t getHighestRanks(rank_mask_t, int);
	Card::Rank getStraightRank(rank_mask_t, int);
	void initTables();
	Card::Rank getHighestRank(rank_mask_t, int);
	Card::Rank getLowestRank(rank_mask_t);
public:
	Hand();
	Hand(const std::string desc);
	Hand(const std::vector<Card> &src);
	//Hand(const Hand& src);
	void copyFrom(const Hand &src);
	void add(Card);
	void add(const std::vector<Card> &cards);
	void remove(Card);
	Card remove(Card::Rank r);
	void eval();
	void clear();
	
	bool operator>(const Hand& other) const;
	bool operator==(const Hand& other) const;
	Rank getRank() const;
	std::string getRankName() const;
	std::vector<Card> getCards() const;
	
	std::string toString(int level=0) const;
	// mostly for diagnostics
	int getMaxSuitCnt();
	Card::Suit getMaxSuit();
	int getSuitCnt(Card::Suit);
	Card::Rank getLowestSingleton();
	Card::Rank getHighestSingleton();
	Card::Rank getSecondSingleton();
	Card::Rank getThirdSingleton();
	Card::Rank getHighestRank();
	Card::Rank getHighestPair();
	Card::Rank getSecondPair();
	Card::Rank getHighestTrips();
	Card::Rank getHighestQuads();
	bool containsJoker();
	int getRankCnt(Card::Rank) const;
	
	Card::Rank getR0() const;
	Card::Rank getR1() const;
	rank_mask_t getKickerMask() const;
	rank_mask_t getRankMask(Card::Suit s) const;
	rank_mask_t getRankMask() const;
	static Card::Rank getStraightRank(rank_mask_t);
	static Card::Rank getWildStrRank(rank_mask_t);
	Card::Rank getKickerRank(int);
	int getTripsCnt() const;
	int getPairsCnt() const;
	int getQuadsCnt() const;
};

std::vector< std::vector<int>> generate_combinations(int N, int r);

inline uint64_t nChooseK(uint64_t n, uint64_t k) {
	if (k > n) return 0;
	k = std::min(k, n - k);
	uint64_t result = 1;
	for (uint64_t i = 0; i < k; i++) {
		result = result * (n - i) / (i + 1);
	}
	return result;
}

template <typename T>
inline void shuffle(std::vector<T> &v) {
    static std::mt19937 rng(std::random_device{}());
    std::shuffle(v.begin(), v.end(), rng);
}

template <typename T>
void forEachCombination(
    const std::vector<T>& items,
    int n,
    std::function<void(const std::vector<T>&)> callback)
{
    std::vector<T> current;
    std::vector<int> indices(n);

    std::function<void(int, int)> recurse = [&](int start, int depth) {
        if (depth == n) {
            callback(current);
            return;
        }
        for (int i = start; i <= (int)items.size() - (n - depth); i++) {
            current.push_back(items[i]);
            recurse(i + 1, depth + 1);
            current.pop_back();
        }
    };

    recurse(0, 0);
}

struct StartingHand {
	std::vector<Card> cards;
	int multiplier;
	uint32_t rankMask[4];

	StartingHand(const std::vector<Card> &cards) : cards(cards) {
		memset(rankMask, 0, sizeof(rankMask));
		for (const auto &card : cards) {
			if (!is_joker(card)) {
				rankMask[card.suit] |= (1 << card.rank);
			}
		}
		multiplier = 1;
	}

	inline bool isEquivalent(const StartingHand &other) const {
		bool found, used[4] = {};
		for (int s=0; s<4; s++) {
			found = false;
			for (int j=0; j<4; j++) {
				if (!used[j] && (this->rankMask[s] == other.rankMask[j])) {
					used[j] = true;
					found = true;
					break;
				}
			}
			if (!found) return false;
		}
		return true;
	}
};

inline void compressSuits(const std::vector< std::vector<Card>> &allHands, std::vector<StartingHand> &uniqueHands) {
	uniqueHands.clear();
	int n=0;
	for (auto const &hand : allHands) {
		if (n%1000 == 0) std::cout << n << std::endl;
		n++;
		//std::cout << hand << std::endl;
		StartingHand newHand(hand);
		auto it = std::find_if(uniqueHands.begin(), uniqueHands.end(), [&newHand](const StartingHand &uniqueHand) { return uniqueHand.isEquivalent(newHand); });
		if (it == uniqueHands.end()) {
			uniqueHands.push_back(newHand);
		} else {
			it->multiplier++;
		}
	}

}

}

#endif
