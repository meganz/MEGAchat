#ifndef MSGINPUTBOX
#define MSGINPUTBOX

#include <QKeyEvent>
#include <QTextEdit>
#include <QListWidget>
#include <QWheelEvent>
#include <QScrollBar>

class MsgInputBox: public QTextEdit
{
Q_OBJECT
signals:
    void sendMsg();
    void editLastMsg();
public:
    MsgInputBox(QWidget* parent):QTextEdit(parent){}
protected:
    virtual void keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Return)
        {
            if ((event->modifiers() & Qt::ShiftModifier) == 0)
            {
                event->accept();
                emit sendMsg();
                return;
            }
        }
        else if (event->key() == Qt::Key_Up)
        {
            if (toPlainText().isEmpty())
            {
                event->accept();
                emit editLastMsg();
                return;
            }
        }
        QTextEdit::keyPressEvent(event);
    }
};

class MyMessageList: public QListWidget
{
Q_OBJECT
public:
    using QListWidget::QListWidget;
signals:
    void requestHistory();
protected:
    uint32_t mLastHistReqTs = 0; //the mouse wheel generates a flood of events, so we need to ingore the ones after the firs
    void wheelEvent(QWheelEvent* event)
    {
        if (event->angleDelta().y() > 0 && verticalScrollBar()->value() == 0)
        {
            event->accept();
            auto now = time(NULL);
            if (now - mLastHistReqTs >= 2) //minimum two seconds between sequential hist fetches
            {
                mLastHistReqTs = static_cast<uint32_t>(now);
                emit requestHistory();
            }
        }
        else
        {
            QListWidget::wheelEvent(event);
        }
    }
};
#endif // MSGINPUTBOX

