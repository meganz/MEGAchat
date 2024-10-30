#ifndef _KARERE_DB_H
#define _KARERE_DB_H

#include <sqlite3.h>
#include <assert.h>
#include "buffer.h"
#include "karereCommon.h"

namespace karere
{
    class IApp;
}

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
    karere::IApp &mApp;
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
        {
            return false;
        }
        else
        {
            // returns zero if autocommit mode is disabled, otherwise return non-zero
            if (!sqlite3_get_autocommit(mDb))
            {
                // autocommit mode is disabled by a BEGIN statement (if there's an opened transaction)
                simpleQuery("COMMIT TRANSACTION");
            }
            else
            {
                // mHasOpenTransaction is true, but there's not an opened transaction in db
                KR_LOG_ERROR("db error: trying to commit a transaction, but there's not an opened transaction in db");
                assert(false);
            }
        }

        mHasOpenTransaction = false;
        mLastCommitTs = time(NULL);
        return true;
    }
public:
    SqliteDb(karere::IApp &app)
        : mApp(app)
    {}
    bool open(const char* fname, bool commitEach=true)
    {
        assert(!mDb);
        int ret = sqlite3_open(fname, &mDb);
        if (!mDb)
        {
            KR_LOG_ERROR("Karere log error: db error: mDb was null after sqlite3_open(), ret=%d", ret);
            return false;
        }
        if (ret != SQLITE_OK)
        {
            KR_LOG_ERROR("Karere log error: db error: sqlite3_open() returned %d", ret);
            sqlite3_close(mDb);
            mDb = nullptr;
            return false;
        }

        ret = sqlite3_exec(mDb, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        if (ret != SQLITE_OK)
        {
            KR_LOG_ERROR("Karere log error: db error: sqlite3_exec() returned %d (foreign_keys)", ret);
            sqlite3_close(mDb);
            mDb = nullptr;
            return false;
        }

        ret = sqlite3_exec(mDb, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        if (ret != SQLITE_OK)
        {
            KR_LOG_ERROR("Karere log error: db error: sqlite3_exec() returned %d (journal_mode)", ret);
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

        KR_LOG_DEBUG("Karere log debug: db opened: %s", fname);
        return true;
    }
    void close()
    {
        if (!mDb)
            return;
        if (!mCommitEach)
            commitTransaction();
        if (int err = sqlite3_close(mDb); err)
        {
            KR_LOG_ERROR("sqlite3_close error: %d", err);
        }
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
            // there was an open transaction --> commit
            commitTransaction();
        }
        else if (!mHasOpenTransaction)
        {
            beginTransaction();
        }
    }
    bool commitEach() { return mCommitEach; }   // false for transactional
    void setCommitInterval(uint16_t sec) { mCommitInterval = sec; }
    bool hasOpenTransaction() const { return !mHasOpenTransaction; }
    operator sqlite3*() { return mDb; }
    operator const sqlite3*() const { return mDb; }
    template <class... Args>
    inline bool query(const char* sql, Args&&... args);
    void simpleQuery(const char* sql);
    void commit()
    {
        if (mCommitEach)
            return;

        if (commitTransaction())
        {
            beginTransaction();
        }
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
    SqliteStmt& bind(int col, const char* val, size_t size) { retCheck(sqlite3_bind_text(mStmt, col, val, static_cast<int>(size), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const void* val, size_t size) { retCheck(sqlite3_bind_blob(mStmt, col, val, static_cast<int>(size), SQLITE_STATIC), "bind"); return *this; }
    SqliteStmt& bind(int col, const StaticBuffer& buf) { retCheck(sqlite3_bind_blob(mStmt, col, buf.buf(), static_cast<int>(buf.dataSize()), SQLITE_STATIC), "bind"); return *this; }
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
    std::string stringCol(int num)
    {
        const unsigned char* data = sqlite3_column_text(mStmt, num);
        if (!data)
            return std::string();
        size_t size = static_cast<size_t>(sqlite3_column_bytes(mStmt, num));
        return std::string((const char*)data, size);
    }
    bool hasBlobCol(int num)
    {
        return sqlite3_column_blob(mStmt, num) != nullptr;
    }
    void blobCol(int num, Buffer& buf)
    {
        const void* data = sqlite3_column_blob(mStmt, num);
        size_t size = static_cast<size_t>(sqlite3_column_bytes(mStmt, num));
        if (!data || !size)
        {
            buf.clear();
        }
        buf.assign(data, size);
    }
    void blobCol(int num, StaticBuffer& buf)
    {
        size_t size = static_cast<size_t>(sqlite3_column_bytes(mStmt, num));
        if (buf.dataSize() < size)
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
        size_t size = static_cast<size_t>(sqlite3_column_bytes(mStmt, num));
        if (size > buflen)
            throw std::runtime_error("blobCol: Insufficient buffer space for blob: required "+
                std::to_string(size)+", provided "+std::to_string(buflen));
        memcpy(buf, data, size);
        return size;
    }

    /**
     * @brief Generic method to get an integer from the num-th column.
     *
     * The method checks depending on the input which method is more appropriate to get the data
     * (sqlite3_column_int or sqlite3_column_int64).
     *
     * Example:
     *     char a = smtp.integralCol<char>(5);
     *     uint64_t a = smtp.integralCol<uint64_t>(4);
     *
     * @tparam T The output type. It must be an integral type or an enum (which relies on an
     * integral type)
     * @param num The index of the column to read from.
     * @return The value cast to the give T.
     */
    template<typename T>
    T integralCol(int num)
    {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "T must be an integral or an enum type");
        if constexpr (std::is_enum_v<T>)
        {
            using UnderlyingType = std::underlying_type_t<T>;
            return static_cast<T>(integralCol<UnderlyingType>(num));
        }
        if constexpr (std::numeric_limits<T>::digits <= std::numeric_limits<int>::digits)
        {
            return static_cast<T>(sqlite3_column_int(mStmt, num));
        }
        else if constexpr (std::numeric_limits<T>::digits <= std::numeric_limits<int64_t>::digits + 1)
        {
            // We allow casting from int64 to uint64
            return static_cast<T>(sqlite3_column_int64(mStmt, num));
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported type");
        }
    }

    bool isNullColumn (int num) const { return sqlite3_column_type(mStmt, num) == SQLITE_NULL; }
    int getColumnBytes (int num) const { return sqlite3_column_bytes(mStmt, num); }
private:
    template<class T> struct always_false : std::false_type {};
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

#endif
