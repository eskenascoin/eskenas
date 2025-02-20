// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "editzaddressdialog.h"
#include "ui_editzaddressdialog.h"

#include "zaddresstablemodel.h"
#include "guiutil.h"

#include <QDataWidgetMapper>
#include <QMessageBox>

EditZAddressDialog::EditZAddressDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditZAddressDialog),
    mapper(0),
    mode(_mode),
    model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    ui->labelEdit->setPlaceholderText("z-sapling");
    ui->labelOriginal->setPlaceholderText("z-sapling");

    switch(mode)
    {
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving z-address"));
        ui->addressEdit->setEnabled(false);
        ui->addressEdit->setVisible(false);
        ui->label_2->setVisible(false);
        break;
    case NewSendingAddress:
        setWindowTitle(tr("New sending z-address"));
        ui->addressEdit->setVisible(true);
        ui->label_2->setVisible(true);
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving z-address"));
        ui->addressEdit->setEnabled(false);
        ui->addressEdit->setVisible(true);
        ui->label_2->setVisible(true);
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending z-address"));
        ui->addressEdit->setVisible(true);
        ui->label_2->setVisible(true);
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    //Hide mapped elements
    ui->labelOriginal->setVisible(false);
    ui->addressOriginal->setVisible(false);
}

EditZAddressDialog::~EditZAddressDialog()
{
    delete ui;
}

void EditZAddressDialog::setModel(ZAddressTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    mapper->setModel(_model);
    mapper->addMapping(ui->labelOriginal, ZAddressTableModel::Label);
    mapper->addMapping(ui->addressOriginal, ZAddressTableModel::Address);
}

void EditZAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);

    //Set editable elements to mapped elements
    ui->labelEdit->setText(ui->labelOriginal->text());
    ui->addressEdit->setText(ui->addressOriginal->text());
}

bool EditZAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    //Set mapped elements to final values
    ui->labelOriginal->setText(ui->labelEdit->text());
    ui->addressOriginal->setText(ui->addressEdit->text());

    switch(mode)
    {
    case NewReceivingAddress:
    case NewSendingAddress:
        address = model->addRow(
                mode == NewSendingAddress ? ZAddressTableModel::Send : ZAddressTableModel::Receive,
                ui->labelEdit->text(),
                ui->addressEdit->text());
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditZAddressDialog::accept()
{
    if(!model)
        return;

    //Set mapped elements to final values
    ui->labelOriginal->setText(ui->labelEdit->text());
    ui->addressOriginal->setText(ui->addressEdit->text());

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case ZAddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case ZAddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case ZAddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid Eskenas z-address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case ZAddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is already in the address book.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case ZAddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case ZAddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;

        }
        return;
    }
    QDialog::accept();
}

QString EditZAddressDialog::getAddress() const
{
    return address;
}

void EditZAddressDialog::setAddress(const QString &_address)
{
    this->address = _address;
    ui->addressEdit->setText(_address);
    ui->addressOriginal->setText(_address);
}
