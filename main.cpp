#include <cstring>
#include <iostream>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>
#include <format>
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

	auto insert = db.prepare("INSERT INTO hands (id, multiplier, holecards, stats) VALUES (?, ?, ?, ?);");

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

void evalGame(const std::string &filename) {
	UltimateTexasHoldem game;
	Database db(filename);
	std::vector<Card> emptyBoard;
	auto rows = readAllRows(db);
	std::string csv;
	csv = UltimateTexasHoldem::getCSV(rows);
	std::cout << csv << std::endl;
}

int main() {
	//buildStartingHands(poker::FULL_DECK, "uth.sqlite3");
	//fillRows("bb_uth3x.sqlite3");
	//evalGame("bb_uth.sqlite3");
	simHands(6);
}