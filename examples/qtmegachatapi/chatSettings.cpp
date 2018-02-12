#include "chatSettings.h"

#include <vector>

ChatSettings::ChatSettings(QMainWindow *parent)
    :QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
}

ChatSettings::~ChatSettings()
{
    delete ui;
}
