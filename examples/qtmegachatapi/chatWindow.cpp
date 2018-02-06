#include "chatWindow.h"
#include <iostream>
#include "chatMessage.h"

ChatWindow::ChatWindow(QWidget* parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatRoom *room)
    : QDialog(parent),
      ui(new Ui::ChatWindowUi)
{    
    histPos=0;
    this->chatItemWidget = (ChatItemWidget *) parent;
    chatRoomHandle=room;
    megaChatApi=mChatApi;
    ui->setupUi(this);
    ui->mSplitter->setStretchFactor(0,1);
    ui->mSplitter->setStretchFactor(1,0);
    ui->mMessageList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->mMsgSendBtn, SIGNAL(clicked()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(sendMsg()), this, SLOT(onMsgSendBtn()));
    connect(ui->mMessageEdit, SIGNAL(editLastMsg()), this, SLOT(editLastMsg()));
    connect(ui->mMessageList, SIGNAL(requestHistory()), this, SLOT(onMsgListRequestHistory()));
    connect(ui->mMembersBtn, SIGNAL(clicked(bool)), this, SLOT(onMembersBtn(bool)));
    connect(ui->mMessageList->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));

    #ifndef KARERE_DISABLE_WEBRTC
        connect(ui->mVideoCallBtn, SIGNAL(clicked(bool)), this, SLOT(onVideoCallBtn(bool)));
        connect(ui->mAudioCallBtn, SIGNAL(clicked(bool)), this, SLOT(onAudioCallBtn(bool)));
    #else
        ui->mAudioCallBtn->hide();
        ui->mVideoCallBtn->hide();
    #endif

    ui->mChatdStatusDisplay->hide();

    if (!chatRoomHandle->isGroup())
        ui->mMembersBtn->hide();
    else
        setAcceptDrops(true);

    setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    if (!chatRoomHandle->isActive())
        ui->mMessageEdit->setEnabled(false);

    QDialog::show();

    //We add ChatRoomListener
    this->megaChatRoomListenerDelegate =  new ::megachat::QTMegaChatRoomListener(megaChatApi, this);
}

void ChatWindow::openChatRoom()
{
    bool result = this->megaChatApi->openChatRoom(chatRoomHandle->getChatId(),megaChatRoomListenerDelegate);
    if(result)
        megaChatApi->loadMessages(chatRoomHandle->getChatId(),16);
}

ChatWindow::~ChatWindow()
{
    chatItemWidget->invalidChatWindowHandle();
    this->megaChatApi->closeChatRoom(chatRoomHandle->getChatId(),megaChatRoomListenerDelegate);
    delete megaChatRoomListenerDelegate;
    delete ui;
}


void ChatWindow::onChatRoomUpdate(megachat::MegaChatApi* api, megachat::MegaChatRoom *chat){}
void ChatWindow::onMessageReceived(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg){}
void ChatWindow::onMessageUpdate(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg){}
void ChatWindow::onMessageLoaded(megachat::MegaChatApi* api, megachat::MegaChatMessage *msg)
{
    if(msg==NULL)
    {
        std::cout << "[Este era el último]";
    }
    else
    {
        std::cout << "[Mensaje cargado] " << msg->getMsgId();
        //msg->getMsgIndex();
        addMsgWidget(msg->getMsgId(), false);
    }
}



QListWidgetItem* ChatWindow::addMsgWidget (megachat::MegaChatHandle msgId, bool first)
{                
    megachat::MegaChatHandle chatId = chatRoomHandle->getChatId();
    megachat::MegaChatMessage * msg = this->megaChatApi->getMessage(chatId, msgId);
    auto widget = new ChatMessage(this, megaChatApi, chatId, msg);

    //connect(widget->ui.mMsgDisplay, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onMessageCtxMenu(const QPoint&)));
    auto* item = new QListWidgetItem;

    //Pointer to Widget
    //msg.userp = item;

    item->setSizeHint(widget->size());
    if (msg->getStatus() == megachat::MegaChatMessage::STATUS_SENDING) //we need to add it to the actual end of the list
    {
        //GUI_LOG_DEBUG("Adding unsent message widget of msgxid %s", msg.id().toString().c_str());
        ui->mMessageList->addItem(item);
    }
    else
    {
        //if (first) {ui.mMessageList->insertItem(0, item);}
        //else {ui.mMessageList->insertItem(histPos, item);}

        ui->mMessageList->insertItem(histPos, item);
        histPos++;
    }
    ui->mMessageList->setItemWidget(item, widget);
    return item;
}
