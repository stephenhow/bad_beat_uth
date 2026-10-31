#include "poker.h"
#include <sstream>
#include <cstring>

using namespace poker;

bool poker::Hand::initialized = false;
std::uint8_t Hand::hammingWeight[16384] = {};
rank_mask_t Hand::highest5[16384] = {};
rank_mask_t Hand::highest4[16384] = {};
rank_mask_t Hand::highest3[16384] = {};
rank_mask_t Hand::highest2[16384] = {};
rank_mask_t Hand::highest1[16384] = {};
Card::Rank Hand::strTable[16384] = {};
Card::Rank Hand::wildStrTable[16384] = {};
Card::Rank Hand::highestRank[16384] = {};
Card::Rank Hand::secondRank[16384] = {};
Card::Rank Hand::thirdRank[16384] = {};
Card::Rank Hand::lowestRank[16384] = {};

const char* Card::rankNames[14] = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A", "*"};
const char* Card::suitNames[4] = {"c", "d", "h", "s"};

const char* Hand::rankNames[11] = {"High Card", "One Pair", "Two Pairs", "Three-of-a-Kind", "Straight", "Flush", "Full House", "Four-of-a-Kind", "Straight Flush", "Royal Flush", "Five-of-a-Kind"};

void Hand::initTables() {
	if (!initialized) {
		for (rank_mask_t mask=0; mask<(0x1u << 13); mask++) {
			hammingWeight[mask] = getHammingWeight(mask);
			highest5[mask] = getHighestRanks(mask,5);
			highest4[mask] = getHighestRanks(mask,4);
			highest3[mask] = getHighestRanks(mask,3);
			highest2[mask] = getHighestRanks(mask,2);
			highest1[mask] = getHighestRanks(mask,1);
			strTable[mask] = getStraightRank(mask,0);
			wildStrTable[mask] = getStraightRank(mask,1);
			highestRank[mask] = getHighestRank(mask,0);
			secondRank[mask] = getHighestRank(mask,1);
			thirdRank[mask] = getHighestRank(mask,2);
			lowestRank[mask] = getLowestRank(mask);
		}
		initialized = true;
	}
}

Hand::Hand() {
	initTables();
	clear();
}

Hand::Hand(const std::vector<Card> &src) : Hand() {
	this->add(src);
	this->eval();
}

void Hand::copyFrom(const Hand& src) {
	memcpy(this->suitMask, src.suitMask, 4*sizeof(rank_mask_t));
	memcpy(this->rankCnt, src.rankCnt, 14*sizeof(char));
	this->rank = src.rank;
	this->r0 = src.r0;
	this->r1 = src.r1;
	this->strRank = src.strRank;
	this->kickerMask = src.kickerMask;
	this->singletons = src.singletons;
	this->pairs = src.pairs;
	this->trips = src.trips;
	this->quads = src.quads;
}

Hand::Hand(const std::string desc) : Hand() {
	std::istringstream iss(desc);
    while (iss) {
        Card c;
        std::string sub;
        iss >> sub;
        if (sub.length() > 0) {
            c.rank = Card::char_to_rank(sub[0]);
            c.suit = (c.rank == Card::JOKER) ? Card::CLUBS : Card::char_to_suit(sub[1]);
            this->add(c);
        }
    }
}

void Hand::clear() {
	memset(suitMask, 0, 4*sizeof(rank_mask_t));
	memset(rankCnt, 0, 14*sizeof(char));
}

void Hand::add(Card c) {
	if (!is_joker(c)) {
		suitMask[c.suit] |= (0x1u << c.rank);
	}
	rankCnt[c.rank]++;
}

void Hand::add(const std::vector<Card> &cards) {
	for (auto &c : cards) {
		this->add(c);
	}
}

void Hand::remove(Card c) {
	if (!is_joker(c)) {
		suitMask[c.suit] &= ((-1) ^ (0x1u << c.rank));
	}
	rankCnt[c.rank]--;
}

Card Hand::remove(Card::Rank r) {
	Card c;
	c.rank = r;
	c.suit = Card::CLUBS;	// initialize
	if (r == Card::JOKER) {
		rankCnt[r]--;
	} else {
		for (int s=0; s<4; s++) {
			if ((this->suitMask[s]>>r)&1) {
				c.suit = (Card::Suit) s;
				this->remove(c);
				return c;
			}
		}
	}
	return c;
}

void Hand::remove(const std::vector<Card> &cards) {
	for (const auto &card : cards) {
		this->remove(card);
	}
}

void Hand::eval() {
	singletons = 0x0;
	pairs = 0x0;
	trips = 0x0;
	quads = 0x0;
	
	for (int r=Card::ACE; r>=Card::DEUCE; r--) {
		switch (rankCnt[r]) {
		case 0:
			break;
		case 1:
			singletons |= (0x1u << r);
			break;
		case 2:
			pairs |= (0x1u << r);
			break;
		case 3:
			trips |= (0x1u << r);
			break;
		case 4:
			quads |= (0x1u << r);
			break;
		}
	}

	for (int s=0; s<4; s++) {
		this->suitCnt[s] = hammingWeight[suitMask[s]];
	}

	this->rankMask = this->suitMask[0] | this->suitMask[1] | this->suitMask[2] | this->suitMask[3];
	this->pairsCnt = hammingWeight[pairs];
	this->tripsCnt = hammingWeight[trips];

	int *ptr = std::max_element(suitCnt, suitCnt+4);
	maxSuit = static_cast<Card::Suit>(ptr - this->suitCnt);
	maxSuitCnt = *ptr;
	bool isFlush;
	Card::Rank sfRank=Card::Rank(0);
	rank_mask_t mask;

	this->hasJoker = (this->rankCnt[Card::JOKER] > 0);
	isFlush = (maxSuitCnt >= (hasJoker ? 4 : 5));

	if (isFlush) {
		rank_mask_t sfMask;
		sfMask = this->suitMask[maxSuit];
		sfRank = (this->hasJoker ? wildStrTable[sfMask] : strTable[sfMask]);
		if (sfRank == Card::ACE) {
			this->rank = Hand::ROYAL_FLUSH;
			return;
		} else if (sfRank) {
			this->rank = Hand::STRAIGHT_FLUSH;
			this->r0 = sfRank;
			return;
		} else {
			this->rank = Hand::FLUSH;
			mask = this->suitMask[maxSuit];
			if (this->hasJoker) mask = improveKickersWithJoker(mask);
			this->kickerMask = highest5[mask];
			return;
		}
	}

	if (this->hasJoker && (this->quads != 0x0)) {
		this->rank = Hand::FIVE_OF_A_KIND;
		this->r0 = highestRank[this->quads];
		return;
	}

	if (this->quads) {
		this->rank = Hand::FOUR_OF_A_KIND;
		this->r0 = highestRank[this->quads];
		mask = this->rankMask ^ quads;
		this->kickerMask = highest1[mask];
		return;
	} else if (this->hasJoker && (this->trips != 0x0)) {
		this->rank = Hand::FOUR_OF_A_KIND;
		this->r0 = highestRank[this->trips];
		mask = this->rankMask ^ (0x1u << this->r0);
		this->kickerMask = highest1[mask];
		return;
	}

	if (!this->hasJoker && (hammingWeight[this->trips] == 2)) {
		this->rank = Hand::FULL_HOUSE;
		this->r0 = highestRank[this->trips];
		this->r1 = secondRank[this->trips];
		return;
	} else if ((this->trips != 0x0) && (this->pairs != 0x0)) {
		this->rank = Hand::FULL_HOUSE;
		this->r0 = highestRank[this->trips];
		this->r1 = highestRank[this->pairs];
		return;
	} else if (this->hasJoker && (hammingWeight[this->pairs] >= 2)) {
		this->rank = Hand::FULL_HOUSE;
		this->r0 = highestRank[this->pairs];
		this->r1 = secondRank[this->pairs];
		return;
	}

	this->strRank = (this->hasJoker ? wildStrTable[this->rankMask] : strTable[this->rankMask]);
	if (this->strRank) {
		this->rank = Hand::STRAIGHT;
		this->r0 = this->strRank;
		return;
	}

	if (trips) {
		this->rank = Hand::THREE_OF_A_KIND;
		this->r0 = highestRank[this->trips];
		mask = this->rankMask;
		mask ^= (0x1u << this->r0);
		this->kickerMask = highest2[mask];
		return;
	} else if (this->hasJoker && (this->pairs != 0x0)) {
		this->rank = Hand::THREE_OF_A_KIND;
		this->r0 = highestRank[this->pairs];
		mask = this->rankMask;
		mask ^= (0x1u << this->r0);
		this->kickerMask = highest2[mask];
		return;
	}

	if (hammingWeight[this->pairs] >= 2) {
		this->rank = Hand::TWO_PAIRS;
		this->r0 = highestRank[this->pairs];
		this->r1 = secondRank[this->pairs];
		mask = this->rankMask;
		mask ^= (0x1u << this->r0);
		mask ^= (0x1u << this->r1);
		this->kickerMask = highest1[mask];
		return;
	}

	if (pairs) {
		this->rank = Hand::ONE_PAIR;
		this->r0 = highestRank[this->pairs];
		mask = this->rankMask;
		mask ^= (0x1u << this->r0);
		this->kickerMask = highest3[mask];
		return;
	}

	if (this->hasJoker) {
		this->rank = Hand::ONE_PAIR;
		this->r0 = highestRank[this->rankMask];
		mask = this->rankMask;
		mask ^= (0x1u << this->r0);
		this->kickerMask = highest3[mask];
		return;
	}

	// else
	this->rank = Hand::HIGH_CARD;
	this->kickerMask = highest5[this->rankMask];
	return;
}

rank_mask_t Hand::improveKickersWithJoker(rank_mask_t before) {
	rank_mask_t after=before;
	for (int i=Card::ACE; i>=Card::DEUCE; i--) {
		if (((before>>i)&0x1) == 0) {
			after |= (0x1u << i);
			break;
		}
	}
	return after;
}

bool Hand::operator>(const Hand &other) const {
	if (this->rank > other.rank) return true;
	else if (this->rank < other.rank) return false;
	else {
		switch (this->rank) {
		case FIVE_OF_A_KIND:
		case STRAIGHT_FLUSH:
		case STRAIGHT:
			return (this->r0 > other.r0);
		case ROYAL_FLUSH:
			return false;
		case FOUR_OF_A_KIND:
		case THREE_OF_A_KIND:
		case ONE_PAIR:
			if (this->r0 > other.r0) return true;
			else if (this->r0 < other.r0) return false;
			else return (this->kickerMask > other.kickerMask);
		case FULL_HOUSE:
			if (this->r0 > other.r0) return true;
			else if (this->r0 < other.r0) return false;
			else return (this->r1 > other.r1);
		case FLUSH:
		case HIGH_CARD:
			return (this->kickerMask > other.kickerMask);
		case TWO_PAIRS:
			if (this->r0 > other.r0) return true;
			else if (this->r0 < other.r0) return false;
			if (this->r1 > other.r1) return true;
			else if (this->r1 < other.r1) return false;
			else return (this->kickerMask > other.kickerMask);
		default:
			return false;
		}
	}
}

bool Hand::operator==(const Hand &other) const {
	return (!(*this > other) && !(other > *this));
}

Card::Rank Hand::getLowestSingleton() {
	return lowestRank[this->kickerMask];
}

Card::Rank Hand::getHighestSingleton() {
	return highestRank[this->singletons];
}

Card::Rank Hand::getSecondSingleton() {
	return secondRank[this->singletons];
}

Card::Rank Hand::getThirdSingleton() {
	return thirdRank[this->singletons];
}

Card::Rank Hand::getHighestRank() {
	return highestRank[this->rankMask];
}

Card::Rank Hand::getLowestRank(rank_mask_t mask) {
	for (int r=Card::DEUCE; r<=Card::ACE; r++) {
		if ((mask>>r)&1) return Card::Rank(r);
	}
	return Card::Rank(0);
}

Card::Rank Hand::getHighestRank(rank_mask_t mask, int n) {
	int k=0;
	for (int r=Card::ACE; r>=Card::DEUCE; r--) {
		if ((mask>>r)&1) {
			if (k == n) return Card::Rank(r);
			k++;
		}
	}
	return Card::Rank(0);
}


bool Hand::containsJoker() {
	return this->hasJoker;
}

int Hand::getHammingWeight(rank_mask_t mask) {
	int w=0;
	rank_mask_t c = mask;
	while (c) {
		if (c&1) w++;
		c = c>>1;
	}
	return w;
}

rank_mask_t Hand::getHighestRanks(rank_mask_t mask, int n) {
	rank_mask_t c=-1;
	while (hammingWeight[mask&c] > n) {
		c = (c<<1);
	}
	return (mask&c);
}

Card::Rank Hand::getStraightRank(rank_mask_t mask, int wilds) {
	rank_mask_t strMask, shifted;
	strMask = (mask << 1) | ((mask>>Card::ACE)&1);
	for (int i=9; i>=0; i--) {
		shifted = (strMask >> i)&0x1f;
		if (hammingWeight[shifted] >= (5-wilds)) {
			return Card::Rank(i+3);
		}
	}
	return Card::DEUCE;
}

Hand::Rank Hand::getRank() const {
	return this->rank;
}

int Hand::getRankCnt(Card::Rank r) const {
	return this->rankCnt[r];
}

std::vector<Card> Hand::getCards() const {
	std::vector<Card> cards;
	for (int i=0; i<this->rankCnt[Card::JOKER]; i++) cards.push_back(Card{Card::JOKER, Card::CLUBS});
	for (int r=Card::ACE; r>=Card::DEUCE; r--) {
		for (int s=3; s>=0; s--) {
			if ((this->suitMask[s]>>r)&1) {
				cards.push_back({Card::Rank(r),Card::Suit(s)});
			}
		}
	}
	return cards;
}

std::string poker::cards_to_str(const std::vector<Card> &cards) {
	std::stringstream ss;
	ss << cards;
	return ss.str();
}


int Hand::getMaxSuitCnt() {
	return this->maxSuitCnt;
}

Card::Suit Hand::getMaxSuit() {
	return this->maxSuit;
}

int Hand::getSuitCnt(Card::Suit s) {
	return this->suitCnt[static_cast<int>(s)];
}

std::string Hand::toString(int level) const {
	std::string description;
	std::vector<Card> cards = this->getCards();
	/*
	for (int i=0; i<this->rankCnt[Card::JOKER]; i++) {
		description += "* ";
	}
	*/
	for (auto it = cards.begin(); it != cards.end(); it++) {
		if (it != cards.begin()) description += " ";
		description += it->toString();
	}
	if (level == 0) return description;
	description += "; ";
	description += std::string(Hand::rankNames[this->getRank()]);
	description += ", ";
	switch (this->rank) {
	case FIVE_OF_A_KIND:
	case THREE_OF_A_KIND:
	case ONE_PAIR:
		description += ("r0: ");
		description += Card::rankNames[this->r0];
		break;
	case STRAIGHT_FLUSH:
	case STRAIGHT:
		description += ("strRank: ");
		description += Card::rankNames[this->r0];
		break;
	case FULL_HOUSE:
	case TWO_PAIRS:
		description += ("r0: ");
		description += Card::rankNames[this->r0];
		description += (", r1: ");
		description += Card::rankNames[this->r1];
		break;
	default:
		break;
	}
	bool first=true;
	switch (this->rank) {
	case FOUR_OF_A_KIND: case THREE_OF_A_KIND: case TWO_PAIRS: case ONE_PAIR: case HIGH_CARD: case FLUSH:
		description += ", kickers: ";
		for (int r=Card::ACE; r>=Card::DEUCE; r--) {
			if ((this->kickerMask>>r)&1) {
				if (!first) description += ", ";
				description += Card::rankNames[r];
				first = false;
			}
		}
		break;
	default:
		break;
	}
	return description;
}

void poker::init_deck(std::vector<Card> &deck) {
	deck.clear();
	for (int s=0; s<4; s++) {
		for (int r=Card::DEUCE; r<=Card::ACE; r++) {
			deck.push_back({Card::Rank(r),Card::Suit(s)});
		}
	}
}

std::vector< std::vector<int>> poker::generate_combinations(int N, int r) {
	std::vector <std::vector<int>> combinations;
    std::vector<int> bitmask(r, 1);         // r leading 1's
    bitmask.resize(N, 0);                   // N - r trailing 0's

    do {
        std::vector<int> combination;
        for (int i = 0; i < N; ++i) {
            if (bitmask[i]) combination.push_back(i);
        }

        combinations.push_back(combination);

    } while (std::prev_permutation(bitmask.begin(), bitmask.end()));
    return combinations;
}

std::string Card::toString() const {
    return std::string(rankNames[rank]) + std::string((this->rank == Card::JOKER) ? " " : suitNames[suit]);
}

std::string Hand::getRankName() const {
	return std::string(rankNames[this->getRank()]);
}

Card::Rank Hand::getR0() const {
	return this->r0;
}

Card::Rank Hand::getR1() const {
	return this->r1;
}

rank_mask_t Hand::getKickerMask() const {
	return this->kickerMask;
}

rank_mask_t Hand::getRankMask(Card::Suit s) const {
	return this->suitMask[s];
}

rank_mask_t Hand::getRankMask() const {
	return this->rankMask;
}

Card::Rank Hand::getStraightRank(rank_mask_t r) {
	return strTable[r];
}

Card::Rank Hand::getWildStrRank(rank_mask_t r) {
	return wildStrTable[r];
}

Card::Rank Hand::getKickerRank(int pos) {
	int n=0;
	for (Card::Rank r=Card::ACE; r>=Card::DEUCE; r=static_cast<Card::Rank>(r-1)) {
		if ((kickerMask>>r)&1) {
			if (n == pos) return r;
			n++;
		}
	}
	return Card::DEUCE;
}

int Hand::getPairsCnt() const {
	return this->pairsCnt;
}

int Hand::getTripsCnt() const {
	return this->tripsCnt;
}

int Hand::getQuadsCnt() const {
	return hammingWeight[this->quads];
}

Card::Rank Hand::getHighestTrips() {
	return this->highestRank[this->trips];
}

Card::Rank Hand::getHighestPair() {
	return this->highestRank[this->pairs];
}

Card::Rank Hand::getSecondPair() {
	return this->secondRank[this->pairs];
}

Card::Rank Hand::getHighestQuads() {
	return this->highestRank[this->quads];
}

std::vector<Card>  poker::getCards(const std::string desc) {
	std::vector<Card> cards;
	std::istringstream iss(desc);
    while (iss) {
        Card c;
        std::string sub;
        iss >> sub;
        if (sub.length() > 0) {
            c.rank = Card::char_to_rank(sub[0]);
            c.suit = (c.rank == Card::JOKER) ? Card::CLUBS : Card::char_to_suit(sub[1]);
            cards.push_back(c);
        }
    }
    return cards;
}
