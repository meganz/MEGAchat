#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>
#include <QDateTime>
#include "megachatapi.h"
#include "ui_chatMessageWidget.h"

namespace Ui {
class ChatMessageWidget;
}

class ChatMessage: public QWidget
{
    Q_OBJECT
    protected:

        Ui::ChatMessageWidget *ui;
        megachat::MegaChatHandle chatId;
        //ChatWindow& mChatWindow;
        megachat::MegaChatMessage *message;
        megachat::MegaChatApi* megaChatApi;
        void updateToolTip();
        /*
        Q_PROPERTY(QColor msgColor READ msgColor WRITE setMsgColor)
        QColor msgColor() { return palette().color(QPalette::Base); }
        void setMsgColor(const QColor& color)
        {
            QPalette p(ui.mMsgDisplay->palette());
            p.setColor(QPalette::Base, color);
            ui.mMsgDisplay->setPalette(p);
        }*/
        friend class ChatWindow;
public:
    ChatMessage(QWidget *parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg);
    std::string managementInfoToString() const;
    void setTimestamp(int64_t ts);
    void setStatus(int status);
    void setAuthor();
    virtual ~ChatMessage();






    /*

    ChatMessage& setText(const chatd::Message& msg)
    {
        assert(!msg.isManagementMessage());
        auto& txt = *ui.mMsgDisplay;
        txt.setText(QString::fromUtf8(msg.buf(), msg.dataSize()));
        return *this;
    }
    ChatMessage& setText(const std::string& str)
    {
        ui.mMsgDisplay->setText(QString::fromStdString(str));
        return *this;
    }

    inline bool isMine() const;
    ChatMessage& setAuthor(karere::Id userid);
    ChatMessage& setTimestamp(uint32_t ts)
    {
        QDateTime t;
        t.setTime_t(ts);
        ui.mTimestampDisplay->setText(t.toString("hh:mm:ss"));
        return *this;
    }

    ChatMessage& setStatus(chatd::Message::Status status)
    {
        ui.mStatusDisplay->setText(chatd::Message::statusToStr(status));
        return *this;
    }

    ChatMessage& updateStatus(chatd::Message::Status newStatus)
    {
        ui.mStatusDisplay->setText(chatd::Message::statusToStr(newStatus));
        return *this;
    }
    QPushButton* startEditing()
    {
        assert(mMessage->userp);
        setBgColor(Qt::yellow);
        ui.mEditDisplay->hide();
        ui.mStatusDisplay->hide();

        auto btn = new QPushButton(this);
        btn->setText("Cancel edit");
        auto layout = static_cast<QBoxLayout*>(ui.mHeader->layout());
        layout->insertWidget(2, btn);
        this->layout();
        return btn;
    }
    ChatMessage& cancelEdit()
    {
        disableEditGui();
        ui.mEditDisplay->setText(mMessage->updated?tr("(edited)"): QString());
        return *this;
    }
    ChatMessage& disableEditGui(bool fadeToNormal=true)
    {
        if (fadeToNormal)
            fadeIn(Qt::yellow);
        auto header = ui.mHeader->layout();
        auto btn = header->itemAt(2)->widget();
        header->removeWidget(btn);
        ui.mEditDisplay->show();
        ui.mStatusDisplay->show();
        delete btn;
        return *this;
    }
    ChatMessage& setBgColor(const QColor& color)
    {
        QPalette p = ui.mMsgDisplay->palette();
        p.setColor(QPalette::Base, color);
        ui.mMsgDisplay->setPalette(p);
        return *this;
    }
    ChatMessage& fadeIn(const QColor& color, int dur=500, const QEasingCurve& curve=QEasingCurve::Linear)
    {
        auto a = new QPropertyAnimation(this, "msgColor");
        a->setStartValue(QColor(color));
        a->setEndValue(QColor(Qt::white));
        a->setDuration(dur);
        a->setEasingCurve(curve);
        a->start(QAbstractAnimation::DeleteWhenStopped);
        return *this;
    }
    ChatMessage& setEdited(const QString& txt=QObject::tr("(Edited)"))
    {
        ui.mEditDisplay->setText(txt);
        ui.mEditDisplay->setToolTip(tr("After %1 seconds").arg(mMessage->updated));
        return *this;
    }
    void msgDeleted();
    void removeFromList();
    */

};









#endif // CHATMESSAGE_H
