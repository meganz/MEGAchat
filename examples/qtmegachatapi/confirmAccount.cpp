#include "confirmAccount.h"
#include "ui_confirmAccount.h"

ConfirmAccount::ConfirmAccount() :
    QDialog(nullptr),
    ui(new Ui::ConfirmAccount)
{
    ui->setupUi(this);
    connect(ui->bOk, SIGNAL(clicked()), this, SLOT(onOkButton()));
    connect(ui->bCancel, SIGNAL(clicked()), this, SLOT(onCancelButton()));
}

ConfirmAccount::~ConfirmAccount()
{
    delete ui;
}

void ConfirmAccount::onOkButton()
{
    if (ui->confirmPassword->text() == ui->newPassword->text())
    {
        emit onConfirmAccount(ui->email->text().toStdString(), ui->newPassword->text().toStdString());
    }
}

void ConfirmAccount::onCancelButton()
{
    emit onCancel();
}
