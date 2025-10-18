#include "dialog.h"
#include "ui_dialog.h"
#include "trianglewidget.h"

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);
}

Dialog::~Dialog()
{
    delete ui;
}

void Dialog::on_comboBox_currentTextChanged(const QString &arg1)
{
    ui->widget_2->combo_changed(arg1);
}

