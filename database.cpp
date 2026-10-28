#include "database.h"
#include <cstring>

// ─── Database ─────────────────────────────────────────────────────────────────

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_busy_timeout(db_, 5000);   // wait for a contending writer instead of throwing SQLITE_BUSY immediately
}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(err);
    }
}

ResultSet Database::query(const std::string& sql) {
    ResultSet results;
    char* errMsg = nullptr;

    auto callback = [](void* data, int colCount, char** colValues, char** colNames) -> int {
        auto* rs = static_cast<ResultSet*>(data);
        Row row;
        for (int i = 0; i < colCount; ++i)
            row[colNames[i]] = colValues[i] ? colValues[i] : "";
        rs->push_back(std::move(row));
        return 0;
    };

    if (sqlite3_exec(db_, sql.c_str(), callback, &results, &errMsg) != SQLITE_OK) {
        std::string err(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(err);
    }
    return results;
}

void Database::beginTransaction()    { execute("BEGIN TRANSACTION;"); }
void Database::commitTransaction()   { execute("COMMIT;");            }
void Database::rollbackTransaction() { execute("ROLLBACK;");          }

int64_t Database::lastInsertId() { return sqlite3_last_insert_rowid(db_); }
int     Database::affectedRows() { return sqlite3_changes(db_);           }

bool Database::tableExists(const std::string& name) {
    auto rs = query(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='" + name + "';"
    );
    return !rs.empty();
}

Database::Statement Database::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

// ─── Database::Statement ──────────────────────────────────────────────────────

Database::Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}

Database::Statement::Statement(Statement&& other) noexcept
    : db_(other.db_), stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Database::Statement::~Statement() {
    sqlite3_finalize(stmt_);
}

Database::Statement& Database::Statement::bind(int idx, int val) {
    sqlite3_bind_int(stmt_, idx, val);
    return *this;
}

Database::Statement& Database::Statement::bind(int idx, int64_t val) {
    sqlite3_bind_int64(stmt_, idx, val);
    return *this;
}

Database::Statement& Database::Statement::bind(int idx, double val) {
    sqlite3_bind_double(stmt_, idx, val);
    return *this;
}

Database::Statement& Database::Statement::bind(int idx, const std::string& val) {
    sqlite3_bind_text(stmt_, idx, val.c_str(), -1, SQLITE_TRANSIENT);
    return *this;
}

Database::Statement& Database::Statement::bind(int idx, const void* data, size_t len) {
    sqlite3_bind_blob(stmt_, idx, data, static_cast<int>(len), SQLITE_TRANSIENT);
    return *this;
}

Database::Statement& Database::Statement::bind(int idx, std::nullptr_t) {
    sqlite3_bind_null(stmt_, idx);
    return *this;
}

void Database::Statement::execute() {
    if (sqlite3_step(stmt_) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_reset(stmt_);
}

void Database::Statement::query(std::function<void(Statement&)> rowCallback) {
    while (sqlite3_step(stmt_) == SQLITE_ROW)
        rowCallback(*this);
    sqlite3_reset(stmt_);
}

int         Database::Statement::getInt   (int col) { return sqlite3_column_int(stmt_, col);    }
int64_t     Database::Statement::getInt64 (int col) { return sqlite3_column_int64(stmt_, col);                                          }
double      Database::Statement::getDouble(int col) { return sqlite3_column_double(stmt_, col);                                       }
std::string Database::Statement::getText  (int col) { return reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));          }
bool        Database::Statement::isNull   (int col) { return sqlite3_column_type(stmt_, col) == SQLITE_NULL;                         }
int         Database::Statement::blobBytes(int col) { return sqlite3_column_bytes(stmt_, col);                                     }

void Database::Statement::getBlob(int col, void* dst, size_t len) {
    int avail = sqlite3_column_bytes(stmt_, col);
    if (static_cast<size_t>(avail) != len)
        throw std::runtime_error(
            "Database::Statement::getBlob: size mismatch (expected " + std::to_string(len) +
            ", got " + std::to_string(avail) + ")");
    std::memcpy(dst, sqlite3_column_blob(stmt_, col), len);
}
