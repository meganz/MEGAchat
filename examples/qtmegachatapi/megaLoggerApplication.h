#ifndef MEGALOGGERAPPLICATION_H
#define MEGALOGGERAPPLICATION_H
#include <fstream>
#include "megachatapi.h"



class MegaLoggerApplication : public mega::MegaLogger, public megachat::MegaChatLogger
{
    public:
        MegaLoggerApplication(const char *filename);
        virtual ~MegaLoggerApplication();
        std::ofstream *getOutputStream() { return &testlog; }
        void postLog(const char *message);
        bool getLogConsole() const;
        void setLogConsole(bool logConsole);

private:
        std::ofstream testlog;
        bool logConsole;

    protected:
        void log(const char *time, int loglevel, const char *source, const char *message);
        void log(int loglevel, const char *message);
};

#endif // MEGALOGGERAPPLICATION_H
