#include "db.h"
#include "IGui.h"

void SqliteDb::simpleQuery(const char *sql)
{
    SqliteString err;
    auto ret = sqlite3_exec(mDb, sql, nullptr, nullptr, &err.mStr);
    if (ret == SQLITE_OK)
        return;
    std::string msg("Error " + std::to_string(ret) + " executing '");
    msg.append(sql);
    if (err.mStr)
        msg.append("': ").append(err.mStr);
    else
        msg+='\'';

    if (ret == SQLITE_FULL || ret == SQLITE_IOERR)
    {
        mApp.onDbError(ret, msg);
        return;
    }

    throw std::runtime_error(msg);
}
