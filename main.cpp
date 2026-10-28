#include <cstring>
#include <iostream>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>
#include <format>
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
	    "  stats BLOB"
	    ");");

	auto insert = db.prepare("INSERT INTO hands (id, multiplier, holecards, stats) VALUES (?, ?, ?, ?);");

	db.withTransaction([&] {
		for (auto &row : rows) {
			insert.bind(1, row.id)
			      .bind(2, row.multiplier)
			      .bind(3, row.holecards)
			      .bind(4, &row.stats, sizeof(UltimateTexasHoldem::Stats));
			insert.execute();
		}
	});
}

std::vector<UltimateTexasHoldem::Row> readAllRows(Database& db) {
    std::vector<UltimateTexasHoldem::Row> rows;

    auto stmt = db.prepare("SELECT id, multiplier, holecards, stats FROM hands;");
    stmt.query([&](Database::Statement& r) {
        UltimateTexasHoldem::Row row;
        row.id    = r.getInt(0);
        row.multiplier  = r.getInt(1);
        row.holecards = r.getText(2);
        r.getBlob(3, &row.stats, sizeof(UltimateTexasHoldem::Stats));
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

// compute every row's stats and write each one back as soon as it's done. Rows that already have
// stats (nonzero total) are skipped, so a run that gets interrupted can just be restarted.
void fillRows(const std::string &filename) {
	UltimateTexasHoldem game;

	Database db(filename);
	db.execute("PRAGMA journal_mode=WAL;");
	db.execute("PRAGMA synchronous=NORMAL;");   // safe under WAL, avoids an fsync on every commit
	auto rows = readAllRows(db);

	std::vector<size_t> todo;
	for (size_t i = 0; i < rows.size(); i++) {
		if (rows[i].stats.getTotal() == 0) todo.push_back(i);
	}
	cout << "totalRows: " << rows.size() << ", remaining: " << todo.size() << endl;

	Hand warmup;   // force Hand's lazily-initialized static tables to build once, single-threaded, before spawning workers

	// each row is a long computation, so threads pull rows one at a time from a shared counter
	// instead of taking fixed chunks -- keeps all threads busy until the end
	std::atomic<size_t> next{0};
	std::mutex coutMutex;
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 1;
	numThreads = std::min<size_t>(numThreads, todo.size());
	std::vector<std::thread> threads;
	for (unsigned int t = 0; t < numThreads; t++) {
		threads.emplace_back([&] {
			Database threadDb(filename);
			auto update = threadDb.prepare("UPDATE hands SET stats = ? WHERE id = ?;");
			const std::vector<Card> emptyBoard;
			for (size_t k = next++; k < todo.size(); k = next++) {
				auto &row = rows[todo[k]];
				// the slow part, with no DB transaction (and no write lock) held
				game.getStats(getCards(row.holecards), emptyBoard, row.stats);
				double ev = row.stats.getEV();
				// one row per commit: rows take far longer to compute than to write
				update.bind(1, &row.stats, sizeof(UltimateTexasHoldem::Stats))
				      .bind(2, row.id);
				update.execute();
				std::lock_guard<std::mutex> lock(coutMutex);
				cout << std::format("[{}/{}] id: {:3}, {}, EV: {}", k + 1, todo.size(), row.id, row.holecards, ev) << endl;
			}
		});
	}
	for (auto &th : threads) th.join();
}

void evalGame(const std::string &filename) {
	UltimateTexasHoldem game;
	Database db(filename);
	auto rows = readAllRows(db);
	UltimateTexasHoldem::Stats stats;
	stats.clear();
	for (const auto &row : rows) {
		stats.add(row.stats, row.multiplier);
	}
	double ev;
	ev = stats.getEV(true);
	std::cout << "EV: " << ev << std::endl;
	std::cout << "total: " << stats.getTotal() << std::endl;
}

int main() {
	//buildStartingHands(poker::FULL_DECK, "bb_uth3x.sqlite3");
	fillRows("bb_uth3x.sqlite3");
	//evalGame("bb_uth3x.sqlite3");
}