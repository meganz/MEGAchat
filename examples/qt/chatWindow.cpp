#include "chatWindow.h"
namespace karere
{
sqlite3* db = nullptr;
struct AutoInit
{
    AutoInit()
    {
        const char* homedir = getenv("HOME");
        if (!homedir)
            return;
        int ret = sqlite3_open((std::string(homedir)+"/.karere.db").c_str(), &db);
        if (ret != SQLITE_OK)
        {
            db = nullptr;
            return;
        }
    }
};

AutoInit init;
}
