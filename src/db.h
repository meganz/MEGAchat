#ifndef _KARERE_DB_H
#define _KARERE_DB_H

#include <sqlite3.h>

struct SqliteString
{
    char* mStr;
    SqliteString() = default;
    SqliteString(char* str): mStr(str){}
    ~SqliteString()
    {
        if (mStr)
            sqlite3_free(mStr);
    }
};
class SqliteStmt;

class SqliteDb
{
protected:
    friend class SqliteStmt;
    sqlite3* mDb = nullptr;
    bool mCommitEach = true;
    bool mHasOpenTransaction = false;
    uint16_t mCommitInterval = 20;
    time_t mLastCommitTs = 0;
    inline int step(SqliteStmt& stmt);
    void beginTransaction()
    {
        assert(!mHasOpenTransaction);
        simpleQuery("BEGIN TRANSACTION");
        mHasOpenTransaction = true;
    }
    bool commitTransaction()
    {
        if (!mHasOpenTransaction)
            return false;
        simpleQuery("COMMIT TRANSACTION");
        mHasOpenTransaction = false;
        mLastCommitTs = time(NULL);
        return true;
    }
public:
    SqliteDb(sqlite3* db=nullptr, uint16_t commitInterval=20)
    : mDb(db), mCommitInterval(commitInterval)
    {}
    bool open(const char* fname, bool commitEach=true)
    {
        assert(!mDb);
        int ret = sqlite3_open(fname, &mDb);
        if (!mDb)
            return false;
        if (ret != SQLITE_OK)
        {
            sqlite3_close(mDb);
            mDb = nullptr;
            return false;
        }
        mCommitEach = commitEach;
        if (!mCommitEach)
        {
            beginTransaction();
            mLastCommitTs = time(NULL);
        }
        return true;
    }
    void close()
    {
        if (!mDb)
            return;
        if (!mCommitEach)
            commitTransaction();
        sqlite3_close(mDb);
        mDb = nullptr;
        mLastCommitTs = 0;
    }
    bool isOpen() const { return mDb != nullptr; }
    void setCommitMode(bool commitEach)
    {
        if (commitEach == mCommitEach)
            return;
        mCommitEach = commitEach;
        if (commitEach)
        {
            commitTransaction();
        }
    }
    void setCommitInterval(uint16_t sec) { mCommitInterval = sec; }
    bool hasOpenTransaction() const { return !mHasOpenTransaction; }
    operator sqlite3*() { return mDb; }
    operator const sqlite3*() const { return mDb; }
    template <class... Args>
    inline bool query(const char* sql, Args&&... args);
    void simpleQuery(const char* sql)
    {
        SqliteString err;
        auto ret = sqlite3_exec(mDb, sql, nullptr, nullptr, &err.mStr);
        if (ret == SQLITE_OK)
            return;
        std::string msg("Error executing '");
        msg.append(sql);
        if (err.mStr)
            msg.append("': ").append(err.mStr);
        else
            msg+='\'';

        throw std::runtime_error(msg);
    }
    void commit()
    {
        if (mCommitEach)
            return;
        commitTransaction();
        beginTransaction();
    }
    bool rollback()
    {
        if (mCommitEach)
            return false;
        // the rollback may fail - in case of some critical errors, sqlite automatically
        // does a rollback. In such cases, we should ignore the error returned by
        // rollback, it's harmless
        sqlite3_exec(mDb, "ROLLBACK", nullptr, nullptr, nullptr);
        beginTransaction();
        return true;
    }
    bool timedCommit()
    {
        if (mCommitEach)
            return false;

        auto now = time(NULL);
        if (now - mLastCommitTs < mCommitInterval)
            return false;

        commit();
        return true;
    }
};

class SqliteStmt
{
protected:
    sqlite3_stmt* mStmt;
    SqliteDb& mDb;
    int mLastBindCol = 0;
    void retCheck(int code, const char* opname)
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
    SqliteStmt(SqliteDb& db, const char* sql):mDb(db)
    {
        if (sqlite3_prepare_v2(db, sql, -1, &mStmt, nullptr) != SQLITE_OK)
        {
            const char* errMsg = sqlite3_errmsg(mDb);
            if (!errMsg)
                errMsg = "(Unknown error)";
            throw std::runtime_error(std::string(
                "Error creating sqlite statement with sql:\n'")+sql+"'\n"+errMsg);
        }
        assert(mStmt);
    }
    SqliteStmt(SqliteDb& db, const std::string& sql)
        :SqliteStmt(db, sql.c_str()){}
    ~SqliteStmt()
    {
        if (mStmt)
            sqlite3_finalize(mStmt);
    }
    operator sqlite3_stmt*() { return mStmt; }
    operator const sqlite3_stmt*() const {return mStmt; }
    SqliteStmt& bind(int col, int val) { retCheck(sqlite3_bind_int(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, int64_t val) { retCheck(sqlite3_bind_int64(mStmt, col, val), "bind"); return *this; }
    SqliteStmt& bind(int col, const std::string& val) { retCheck(sqlite3_bind_text(mStmt, col, val.c_str(), (int)val.size(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const char* val, size_t size) { retCheck(sqlite3_bind_text(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const void* val, size_t size) { retCheck(sqlite3_bind_blob(mStmt, col, val, size, SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const StaticBuffer& buf) { retCheck(sqlite3_bind_blob(mStmt, col, buf.buf(), buf.dataSize(), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, uint64_t val) { retCheck(sqlite3_bind_int64(mStmt, col, (int64_t)val), "bind"); return *this; }
    SqliteStmt& bind(int col, unsigned int val) { retCheck(sqlite3_bind_int(mStmt, col, (int)val), "bind"); return *this; }
    SqliteStmt& bind(int col, const char* val) { retCheck(sqlite3_bind_text(mStmt, col, val, -1, SQLITE_TRANSIENT), "bind"); return *this; }
    template <class T, class... Args>
    SqliteStmt& bindV(T&& val, Args&&... args) { return bind(val).bindV(args...); }
    SqliteStmt& bindV() { return *this; }
    SqliteStmt& clearBind() { mLastBindCol = 0; retCheck(sqlite3_clear_bindings(mStmt), "clear bindings"); return *this; }
    SqliteStmt& reset() { retCheck(sqlite3_reset(mStmt), "reset"); return *this; }
    template <class T>
    SqliteStmt& bind(T&& val) { bind(++mLastBindCol, val); return *this; }
    template <class T>
    SqliteStmt& operator<<(T&& val) { return bind(val);}
    bool step()
    {
        int ret = mDb.step(*this);
        if (ret == SQLITE_DONE)
            return false;
        else if (ret == SQLITE_ROW)
            return true;
        else
            throw std::runtime_error(getLastErrorMsg(nullptr));
    }
    void stepMustHaveData(const char* opname=nullptr)
    {
        if (step()) return;
        std::string errmsg = "SqliteStmt::stepMustHaveData: No rows returned";
        if (opname)
            errmsg.append(" on operation ").append(opname);
        throw std::runtime_error(errmsg);
    }
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
    bool hasBlobCol(int num)
    {
        return sqlite3_column_blob(mStmt, num) != nullptr;
    }
    void blobCol(int num, Buffer& buf)
    {
        const void* data = sqlite3_column_blob(mStmt, num);
        int size = sqlite3_column_bytes(mStmt, num);
        if (!data || !size)
        {
            buf.clear();
        }
        buf.assign(data, size);
    }
    void blobCol(int num, StaticBuffer& buf)
    {
        int size = sqlite3_column_bytes(mStmt, num);
        if ((int)buf.dataSize() < size)
            throw std::runtime_error("blobCol: provided buffer has less space than required: has "+
            std::to_string(buf.dataSize())+", required: "+std::to_string(size));
        const void* data = sqlite3_column_blob(mStmt, num);
        if (!data)
        {
            buf.clear();
            return;
        }
        memcpy(buf.buf(), data, size);
        buf.setDataSize(size);
    }

    size_t blobCol(int num, char* buf, size_t buflen)
    {
        const void* data = sqlite3_column_blob(mStmt, num);
        if (!data)
            return 0;
        size_t size = sqlite3_column_bytes(mStmt, num);
        if (size > buflen)
            throw std::runtime_error("blobCol: Insufficient buffer space for blob: required "+
                std::to_string(size)+", provided "+std::to_string(buflen));
        memcpy(buf, data, size);
        return size;
    }

    uint64_t uint64Col(int num) { return (uint64_t)sqlite3_column_int64(mStmt, num);}
    unsigned int uintCol(int num) { return (unsigned int)sqlite3_column_int(mStmt, num);}
};

template <class... Args>
inline bool SqliteDb::query(const char* sql, Args&&... args)
{
    SqliteStmt stmt(*this, sql);
    stmt.bindV(args...);
    return stmt.step();
}

inline int SqliteDb::step(SqliteStmt& stmt)
{
    auto ret = sqlite3_step(stmt);
    if (ret == SQLITE_DONE)
    {
        timedCommit();
    }
    return ret;
}

class SqliteTransaction
{
protected:
    SqliteDb* mDb;
public:
    SqliteTransaction(SqliteDb& db): mDb(&db) { mDb->commit(); }
    void commit()
    {
        assert(mDb);
        mDb->commit();
        mDb = nullptr;
    }
    ~SqliteTransaction()
    {
        if (mDb)
            mDb->rollback();
    }
};

#endif
