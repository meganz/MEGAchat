#ifndef _KARERE_DB_H
#define _KARERE_DB_H

#include <sqlite3.h>

class SqliteStmt
{
protected:
    sqlite3_stmt* mStmt;
    sqlite3* mDb;
    int mLastBindCol = 0;
    void check(int code, const char* opname)
    {
        if (code != SQLITE_OK)
            throw std::runtime_error(getLastErrorMsg(opname));
    }
    std::string getLastErrorMsg(const char* opname)
    {
        std::string msg("SqliteStmt error ");
        msg.append(std::to_string(sqlite3_errcode(mDb))).append(" on ");
        if (opname)
        {
            msg.append("operation '").append(opname).append("': ");
        }
        else
        {
            const char* sql = sqlite3_sql(mStmt);
            if (sql)
                msg.append("query\n").append(sql).append("\n");
        }
        const char* errMsg = sqlite3_errmsg(mDb);
        msg.append(errMsg?errMsg:"(no error message)");
        return msg;
    }
public:
    SqliteStmt(sqlite3* db, const char* sql) :mDb(db)
    {
        assert(db);
        check(sqlite3_prepare_v2(db, sql, -1, &mStmt, nullptr), "create statement");
        assert(mStmt);
    }
    SqliteStmt(sqlite3 *db, const std::string& sql):SqliteStmt(db, sql.c_str()){}
    ~SqliteStmt()
    {
        if (mStmt)
            sqlite3_finalize(mStmt);
    }
    operator sqlite3_stmt*() { return mStmt; }
    const operator sqlite3_stmt*() const {return mStmt; }
    SqliteStmt& bind(int col, int val) { check(sqlite3_bind_int(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, const int64_t& val) { check(sqlite3_bind_int64(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, const std::string& val) { check(sqlite3_bind_text(mStmt, col, val.c_str(), (int)val.size(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const char* val, size_t size) { check(sqlite3_bind_text(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const void* val, size_t size) { check(sqlite3_bind_blob(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const Buffer& buf) { check(sqlite3_bind_blob(mStmt, col, buf.buf(), buf.dataSize(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const uint64_t& val) { check(sqlite3_bind_int64(mStmt, col, (int64_t)val), "bind"); return *this; }
    SqliteStmt& bind(int col, unsigned int val) { check(sqlite3_bind_int(mStmt, col, (int)val), "bind"); return *this; }
    template <class T, class... Args>
    SqliteStmt& bindV(const T& val, Args... args) { return bind(val).bindV(args...); }
    SqliteStmt& bindV() { return *this; }
    SqliteStmt& clearBind() { mLastBindCol = 0; check(sqlite3_clear_bindings(mStmt), "clear bindings"); return *this; }
    SqliteStmt& reset() { check(sqlite3_reset(mStmt), "reset"); return *this; }
    template <class T>
    SqliteStmt& bind(const T& val) { bind(++mLastBindCol, val); return *this; }
    template <class T>
    SqliteStmt& operator<<(const T& val) { return bind(val);}
    bool step()
    {
        int ret = sqlite3_step(mStmt);
        if (ret == SQLITE_DONE)
            return false;
        else if (ret == SQLITE_ROW)
            return true;
        else
            throw std::runtime_error(getLastErrorMsg(nullptr));
    }
    void stepMustHaveData() { if (!step()) throw std::runtime_error("SqliteStmt::stepMustHaveData: No rows returned"); }
    int intCol(int num) { return sqlite3_column_int(mStmt, num); }
    int64_t int64Col(int num) { return sqlite3_column_int64(mStmt, num); }
    std::string stringCol(int num)
    {
        const unsigned char* data = sqlite3_column_text(mStmt, num);
        if (!data)
            return std::string();
        int size = sqlite3_column_bytes(mStmt, num);
        return std::string((const char*)data, size);
    }
    void blobCol(int num, Buffer& buf)
    {
        buf.clear();
        const void* data = sqlite3_column_blob(mStmt, num);
        if (!data)
            return;
        int size = sqlite3_column_bytes(mStmt, num);
        buf.assign(data, size);
    }
    uint64_t uint64Col(int num) { return (uint64_t)sqlite3_column_int64(mStmt, num);}
    unsigned int uintCol(int num) { return (unsigned int)sqlite3_column_int(mStmt, num);}
};

template <class... Args>
bool sqliteQuery(sqlite3* db, const char* sql, Args... args)
{
    SqliteStmt stmt(db, sql);
    stmt.bindV(args...);
    return stmt.step();
}

#endif
