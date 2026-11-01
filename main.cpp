#include <cstring>
#include <iostream>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>
#include <format>
#include <chrono>
#include <random>
#include <algorithm>
#include "uth.h"
#include "database.h"

using namespace std;
using namespace poker;


// write the fully populated rows to the database
void createDB(std::string filename, const std::vector<UltimateTexasHoldem::Row> &rows) {
	Database db(filename);

	db.execute(
	    "CREATE TABLE hands ("
	    "  id INTEGER PRIMARY KEY,"
	    "  multiplier INTEGER,"
	    "  holecards TEXT,"
	    "  others TEXT,"
	    "  stats BLOB"
	    ");");

	auto insert = db.prepare("INSERT INTO hands (id, multiplier, holecards, others, stats) VALUES (?, ?, ?, ?, ?);");

	db.withTransaction([&] {
		for (auto &row : rows) {
			insert.bind(1, row.id)
			      .bind(2, row.multiplier)
			      .bind(3, row.holecards)
			      .bind(4, row.others)
			      .bind(5, &row.stats, sizeof(UltimateTexasHoldem::Stats));
			insert.execute();
		}
	});
}

std::vector<UltimateTexasHoldem::Row> readAllRows(Database& db) {
    std::vector<UltimateTexasHoldem::Row> rows;

    auto stmt = db.prepare("SELECT id, multiplier, holecards, others, stats FROM hands;");
    stmt.query([&](Database::Statement& r) {
        UltimateTexasHoldem::Row row;
        row.id    = r.getInt(0);
        row.multiplier  = r.getInt(1);
        row.holecards = r.getText(2);
        row.others = r.getText(3);
        r.getBlob(4, &row.stats, sizeof(UltimateTexasHoldem::Stats));
        rows.push_back(std::move(row));
    });
    return rows;
}

void initRows(std::vector<UltimateTexasHoldem::Row> &rows, std::vector<poker::StartingHand> &hands) {
	rows.clear();
	UltimateTexasHoldem::Row row;
	memset(&row.stats, 0, sizeof(row.stats));
	int id=1;
	for (auto &hand : hands) {
		row.id = id++;
		row.multiplier = hand.multiplier;
		row.holecards = poker::cards_to_str(hand.cards);
		row.others = "";
		rows.push_back(row);
	}
}

void buildStartingHands(const std::vector<poker::Card> &deck, std::string filename) {
	std::vector<poker::StartingHand> startingHands;
	std::vector<UltimateTexasHoldem::Row> rows;
	std::vector< std::vector<poker::Card>> initialHands;

	poker::forEachCombination<poker::Card>(deck, 2, [&initialHands](const std::vector<poker::Card> &hand5) { initialHands.push_back(hand5); });
	compressSuits(initialHands, startingHands);
	initRows(rows, startingHands);
	
	cout << "5-card Starting Hands: " << startingHands.size() << ", rows: " << rows.size() << endl;

	createDB(filename, rows);
}

// compute every row's stats and write each one back as soon as it's done, overwriting whatever
// stats the row already had.
void fillRows(const std::string &filename) {
	UltimateTexasHoldem game;

	Database db(filename);
	db.execute("PRAGMA journal_mode=WAL;");
	db.execute("PRAGMA synchronous=NORMAL;");   // safe under WAL, avoids an fsync on every commit
	auto rows = readAllRows(db);

	cout << "totalRows: " << rows.size() << " (computing all rows, overwriting any existing stats)" << endl;

	Hand warmup;   // force Hand's lazily-initialized static tables to build once, single-threaded, before spawning workers

	// each row is a long computation, so threads pull rows one at a time from a shared counter
	// instead of taking fixed chunks -- keeps all threads busy until the end
	std::atomic<size_t> next{0};
	std::mutex coutMutex;
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 1;
	numThreads = std::min<size_t>(numThreads, rows.size());
	std::vector<std::thread> threads;
	for (unsigned int t = 0; t < numThreads; t++) {
		threads.emplace_back([&] {
			Database threadDb(filename);
			auto update = threadDb.prepare("UPDATE hands SET stats = ? WHERE id = ?;");
			const std::vector<Card> emptyBoard;
			for (size_t k = next++; k < rows.size(); k = next++) {
				auto &row = rows[k];
				// the slow part, with no DB transaction (and no write lock) held
				game.getStats(getCards(row.holecards), emptyBoard, getCards(row.others), row.stats);
				double ev = row.stats.getEV();
				// one row per commit: rows take far longer to compute than to write
				update.bind(1, &row.stats, sizeof(UltimateTexasHoldem::Stats))
				      .bind(2, row.id);
				update.execute();
				std::lock_guard<std::mutex> lock(coutMutex);
				cout << std::format("[{}/{}] id: {:3}, {}, EV: {}", k + 1, rows.size(), row.id, row.holecards, ev) << endl;
			}
		});
	}
	for (auto &th : threads) th.join();
}

// Monte Carlo over random deals: each hand deals the player 2 hole cards and the other players
// 2*(players-1) known cards, plays the best preflop choice (4x vs check, then optimal flop/river
// play via getStats), and adds that EV to the running total. Runs on every hardware thread until
// maxHands hands are done (0 = run until stopped), printing the running mean and its standard error.
void simHands(int players, uint64_t maxHands = 0) {
	if (players < 1 || players > 6) {
		cerr << "simHands: players must be 1..6 (draw tables cover up to 6 players' hole cards)" << endl;
		return;
	}
	UltimateTexasHoldem game;	// read-only once built, so all threads share it
	Hand warmup;	// build Hand's lazily-initialized static tables once, single-threaded, before spawning workers

	std::mutex mtx;	// guards the totals and cout
	uint64_t hands = 0;
	double net = 0, netSq = 0;	// sum of per-hand EVs and of their squares, for the standard error
	std::atomic<uint64_t> started{0};

	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 1;
	if (maxHands) numThreads = std::min<uint64_t>(numThreads, maxHands);
	cout << "simHands: " << players << " players, " << numThreads << " threads, "
	     << (maxHands ? std::to_string(maxHands) + " hands" : std::string("until stopped")) << endl;

	std::vector<std::thread> threads;
	for (unsigned int t = 0; t < numThreads; t++) {
		threads.emplace_back([&, t] {
			// each thread shuffles with its own generator; poker::shuffle() shares one static generator, which isn't thread-safe
			std::mt19937_64 rng(std::random_device{}() ^ (0x9E3779B97F4A7C15ull * (t + 1)));
			std::vector<Card> deck;
			init_deck(deck);
			const std::vector<Card> emptyBoard;
			UltimateTexasHoldem::Stats stats;
			while (!maxHands || started++ < maxHands) {
				std::shuffle(deck.begin(), deck.end(), rng);
				std::vector<Card> hole(deck.begin(), deck.begin() + 2);
				std::vector<Card> others(deck.begin() + 2, deck.begin() + 2 * players);
				game.getStats(hole, emptyBoard, others, stats);
				double ev = std::max(stats.getEV4x(), stats.getEV());

				std::lock_guard<std::mutex> lock(mtx);
				hands++;
				net += ev;
				netSq += ev * ev;
				double mean = net / hands;
				double se = hands > 1 ? std::sqrt((netSq / hands - mean * mean) / (hands - 1)) : 0;
				cout << std::format("{:6} {} | others {:<17} | ev {:+.5f} | mean {:+.5f} ± {:.5f}",
				                    hands, cards_to_str(hole), cards_to_str(others), ev, mean, se) << endl;
			}
		});
	}
	for (auto &th : threads) th.join();
}

// Compute a total of 'rows' simulated hands for a given playerHole cards, others = 2*(players-1) dealt remaining cards (playerHole removed from deck),
// Hands are randomly dealt but filtered for only when 'others' contains a total of 'copies' of either of the player's hole cards
// examples:
//   if playerHole = "Kc 5s", and copies = 1, add row to database only when randomly dealt others contains a total of 1 King or Five.
//   if playerHole = "Ts 9s", and copies = 0, add row to database only when randomly dealt others contains no other Ten or Nine.
//   if playerHole = "Qs 8h", and copies = 2, add row to database only when randomly dealt others contains a total of 2 cards that are Queen or Eight
// The function exits when a total of 'rows' matching the (playerHole, others) requirement have been written to the database.
// The function should run getStats() on the seperate threads of the instance 
void addToDatabase(Database &db, int rows, const std::string &playerHole, int players, int copies) {
	const std::vector<Card> hole = getCards(playerHole);
	const int nOthers = 2 * (players - 1);
	// the most matching cards that can be among the others: 2 left of a paired rank, 3 + 3 otherwise
	const int available = std::min(nOthers, hole[0].rank == hole[1].rank ? 2 : 6);
	if (hole.size() != 2 || players < 1 || players > 6 || copies < 0 || copies > available) {
		cerr << "addToDatabase: need 2 hole cards, players 1..6, and copies 0.." << available << endl;
		return;
	}
	const auto matches = [&](const Card &c) { return c.rank == hole[0].rank || c.rank == hole[1].rank; };

	std::vector<Card> deck;
	init_deck(deck);
	remove_cards(deck, hole);

	UltimateTexasHoldem game;	// read-only once built, so all threads share it
	Hand warmup;	// build Hand's lazily-initialized static tables once, single-threaded, before spawning workers
	auto insert = db.prepare("INSERT INTO hands (multiplier, holecards, others, stats) VALUES (?, ?, ?, ?);");	// id assigned by SQLite
	std::mutex mtx;	// guards the shared connection, the insert statement and cout
	std::atomic<int> claimed{0};
	int written = 0;

	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 1;
	numThreads = std::min<unsigned int>(numThreads, rows);
	cout << std::format("addToDatabase: {} rows, hole {}, {} players, {} cards of rank {}{} among the {} others, {} threads",
	                    rows, cards_to_str(hole), players, copies, Card::rankNames[hole[0].rank], hole[0].rank == hole[1].rank ? "" : std::string("/") + Card::rankNames[hole[1].rank], nOthers, numThreads) << endl;

	std::vector<std::thread> threads;
	for (unsigned int t = 0; t < numThreads; t++) {
		threads.emplace_back([&, t] {
			std::mt19937_64 rng(std::random_device{}() ^ (0x9E3779B97F4A7C15ull * (t + 1)));
			std::vector<Card> shuffled(deck);
			const std::vector<Card> emptyBoard;
			UltimateTexasHoldem::Stats stats;
			while (true) {
				// rejection sampling: redeal the others until they hold exactly 'copies' matching cards,
				// which leaves every qualifying deal equally likely
				std::vector<Card> others;
				do {
					std::shuffle(shuffled.begin(), shuffled.end(), rng);
					others.assign(shuffled.begin(), shuffled.begin() + nOthers);
				} while (std::count_if(others.begin(), others.end(), matches) != copies);
				if (claimed++ >= rows) break;	// enough rows are done or in progress

				game.getStats(hole, emptyBoard, others, stats);
				double ev4x = stats.getEV4x(), ev0x = stats.getEV();

				std::lock_guard<std::mutex> lock(mtx);
				insert.bind(1, 1)	// every sampled deal counts once
				      .bind(2, cards_to_str(hole))
				      .bind(3, cards_to_str(others))
				      .bind(4, &stats, sizeof(UltimateTexasHoldem::Stats));
				insert.execute();
				written++;
				cout << std::format("[{}/{}] others {:<17} | 4x {:+.5f} | check {:+.5f} | {}", written, rows, cards_to_str(others), ev4x, ev0x, ev4x > ev0x ? "4x" : "check") << endl;
			}
		});
	}
	for (auto &th : threads) th.join();
}

void evalGame(const std::string &filename) {
	UltimateTexasHoldem game;
	Database db(filename);
	std::vector<Card> emptyBoard;
	auto rows = readAllRows(db);
	std::string csv;
	csv = UltimateTexasHoldem::getCSV(rows);
	std::cout << csv << std::endl;
}

// uses maximum available threads to run parallel Monte Carlo analysis where:
//   hands++
//   shuffle deck, deal 2 cards to playerHole, 2*(players-1) cards to others
//   playPreflop = isPlay4x(playerHole, others)
//   deal 3 card flop
//   call getStats(playerHole, flop, others, &stats)
//   evHand = (playPreflop ? stats.getEV4x() : stats.getEV())
//   net += evHand
// Each deal is also played without the known cards (basic strategy, getStats with no others); the difference is the
// gain from knowing them on that deal. The two share the deal's luck, so the gain converges far faster than evHand
// (per-deal SD about 0.34 vs 1.7 with 6 players). Total EV = the exact no-information EV + mean gain.
// The gain splits into postflopGain = EV(playerHole, flop, others) - EV(playerHole, flop, {}) on hands checked
// preflop (!playPreflop), from the flop/river decisions, and preflopGain = the rest, from the preflop decision.
// periodically print out stats
void simPostflop(int players, uint64_t maxHands = 0) {
	// exact standard-game EV per hand with basic strategy and no known cards (uth.sqlite3, evalGame total)
	const double basicEV = ENABLE_BAD_BEAT ? 0.029273 : -0.0218497;
	if (players < 1 || players > 6) {
		cerr << "simPostflop: players must be 1..6" << endl;
		return;
	}
	UltimateTexasHoldem game;	// read-only once built, so all threads share it
	Hand warmup;	// build Hand's lazily-initialized static tables once, single-threaded, before spawning workers

	std::mutex mtx;	// guards the totals and cout
	uint64_t hands=0, played4x=0, changed=0;
	double net=0, netSq=0, base=0, baseSq=0, gain=0, gainSq=0;	// sums and sums of squares, for standard errors
	double post=0, postSq=0, pre=0, preSq=0;

	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 1;
	cout << "simPostflop: " << players << " players, " << numThreads << " threads, ENABLE_BAD_BEAT " << (ENABLE_BAD_BEAT ? "true" : "false")
	     << ", " << (maxHands ? std::to_string(maxHands) + " hands" : std::string("until stopped")) << endl;
	const auto start = std::chrono::steady_clock::now();
	auto ci = [](double sum, double sumSq, uint64_t n) {	// 95% half-width of the mean
		double m = sum / n;
		return 1.96 * std::sqrt(std::max(0.0, (sumSq / n - m * m) / (n - 1)));
	};

	std::atomic<uint64_t> started{0};
	std::vector<std::thread> threads;
	for (unsigned int t = 0; t < numThreads; t++) {
		threads.emplace_back([&, t] {
			// each thread shuffles with its own generator; poker::shuffle() shares one static generator, which isn't thread-safe
			std::mt19937_64 rng(std::random_device{}() ^ (0x9E3779B97F4A7C15ull * (t + 1)));
			std::vector<Card> deck;
			init_deck(deck);
			const std::vector<Card> none;
			UltimateTexasHoldem::Stats stats, baseStats;
			while (!maxHands || started++ < maxHands) {
				std::shuffle(deck.begin(), deck.end(), rng);
				std::vector<Card> hole(deck.begin(), deck.begin() + 2);
				std::vector<Card> others(deck.begin() + 2, deck.begin() + 2 * players);
				std::vector<Card> flop(deck.begin() + 2 * players, deck.begin() + 2 * players + 3);
				bool playPreflop = game.isPlay4x(hole, others);
				stats.clear();	// getStats() adds the flop's outcomes into stats
				game.getStats(hole, flop, others, stats);
				double ev = playPreflop ? stats.getEV4x() : stats.getEV();
				// the same deal without the known cards
				bool basicPlay = game.isPlay4x(hole, none);
				baseStats.clear();
				game.getStats(hole, flop, none, baseStats);
				double evBase = basicPlay ? baseStats.getEV4x() : baseStats.getEV();

				std::lock_guard<std::mutex> lock(mtx);
				hands++;
				if (playPreflop) played4x++;
				if (playPreflop != basicPlay) changed++;
				net += ev; netSq += ev * ev;
				base += evBase; baseSq += evBase * evBase;
				gain += ev - evBase; gainSq += (ev - evBase) * (ev - evBase);
				// postflop part: only on hands checked preflop with the known cards (0 otherwise)
				double g = ev - evBase, gPost = playPreflop ? 0 : stats.getEV() - baseStats.getEV();
				post += gPost; postSq += gPost * gPost;
				pre += g - gPost; preSq += (g - gPost) * (g - gPost);
				if (hands % 10000 == 0) {
					double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
					cout << std::format("hands {:>9} | total EV {:+.6f} ± {:.6f} | gain {:+.6f} ± {:.6f} | preflopGain {:+.6f} ± {:.6f} | postflopGain {:+.6f} ± {:.6f} | direct EV {:+.5f} ± {:.5f} | no-info EV {:+.5f} ± {:.5f} | 4x {:.2f}% | preflop changed {:.2f}% | {:.0f}/s",
					                    hands, basicEV + gain / hands, ci(gain, gainSq, hands), gain / hands, ci(gain, gainSq, hands),
					                    pre / hands, ci(pre, preSq, hands), post / hands, ci(post, postSq, hands),
					                    net / hands, ci(net, netSq, hands), base / hands, ci(base, baseSq, hands),
					                    100.0 * played4x / hands, 100.0 * changed / hands, hands / secs) << endl;
				}
			}
		});
	}
	for (auto &th : threads) th.join();
}

int main() {
	//buildStartingHands(poker::FULL_DECK, "uth.sqlite3");
	//fillRows("bb_uth3x.sqlite3");
	//evalGame("bb_uth.sqlite3");
	simHands(6);
}