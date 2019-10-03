#ifndef REACTION_H
#define REACTION_H

#include <QWidget>
#include "chatMessage.h"

class ChatMessage;
namespace Ui {
class Reaction;
}

class Reaction : public QWidget
{
    Q_OBJECT

public:
    Reaction(ChatMessage *parent, const char *reactionString, int count = 0);
    ~Reaction();
    void contextMenuEvent(QContextMenuEvent *event);
    std::string getReactionString() const;
    void updateReactionCount(int mCount);
    void enterEvent(QEvent * event);
    int getCount() const;

private:
    Ui::Reaction *ui;
    ChatMessage *mChatMessage;
    std::string mReactionString;
    int mCount;

public slots:
    void onCopyReact();
};

#endif // REACTION_H
