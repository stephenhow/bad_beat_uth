#pragma once

#include <sqlite3.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <cstddef>
#include <cstdint>

using Row       = std::map<std::string, std::string>;
using ResultSet = std::vector<Row>;

class Database {
public:
    // ─── Construction / Destruction ───────────────────────────────────────────
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&)                 = default;
    Database& operator=(Database&&)      = default;

    // ─── Core Query Interface ─────────────────────────────────────────────────
    void      execute(const std::string& sql);
    ResultSet query  (const std::string& sql);

    // ─── Transactions ─────────────────────────────────────────────────────────
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    template<typename Func>
    void withTransaction(Func&& func) {
        beginTransaction();
        try {
            func();
            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    // ─── Utility ──────────────────────────────────────────────────────────────
    int64_t lastInsertId();
    int     affectedRows();
    bool    tableExists(const std::string& name);

    // ─── Prepared Statement ───────────────────────────────────────────────────
    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();

        Statement(const Statement&)            = delete;
        Statement& operator=(const Statement&) = delete;
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&&)      = delete;

        // Bind parameters (1-based index)
        Statement& bind(int idx, int val);
        Statement& bind(int idx, int64_t val);
        Statement& bind(int idx, double val);
        Statement& bind(int idx, const std::string& val);
        Statement& bind(int idx, const void* data, size_t len);
        Statement& bind(int idx, std::nullptr_t);

        // Execute non-SELECT
        void execute();

        // Execute SELECT, invoke callback per row
        void query(std::function<void(Statement&)> rowCallback);

        // Column accessors (0-based index)
        int         getInt   (int col);
        int64_t     getInt64 (int col);
        double      getDouble(int col);
        std::string getText  (int col);
        bool        isNull   (int col);
        int         blobBytes(int col);
        void        getBlob  (int col, void* dst, size_t len);

    private:
        sqlite3*      db_;
        sqlite3_stmt* stmt_ = nullptr;
    };

    Statement prepare(const std::string& sql);

private:
    sqlite3* db_ = nullptr;
};
