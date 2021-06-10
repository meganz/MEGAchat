#ifndef CONFIRMACCOUNT_H
#define CONFIRMACCOUNT_H

#include <QDialog>

namespace Ui {
class ConfirmAccount;
}

class ConfirmAccount : public QDialog
{
    Q_OBJECT

public:
    explicit ConfirmAccount();
    ~ConfirmAccount();

private:
    Ui::ConfirmAccount *ui;

public slots:
    void onOkButton();
    void onCancelButton();

signals:
    void onConfirmAccount(const std::string& email, const std::string& password);
    void onCancel();

};

#endif // CONFIRMACCOUNT_H
