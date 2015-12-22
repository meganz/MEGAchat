#ifndef CLIST_GUI_H
#define CLIST_GUI_H
#include <ui_clistitem.h>

class ContactList: public QListWidget
{
protected:
    Ui::ContactList* ui;
};
