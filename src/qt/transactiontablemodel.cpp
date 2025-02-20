// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactiontablemodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactiondesc.h"
#include "transactionrecord.h"
#include "walletmodel.h"

#include "core_io.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include "main.h"
#include "wallet/wallet.h"
#include "wallet/rpcpiratewallet.h"

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QIcon>
#include <QList>
#include <QSettings>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter, /* status */
        Qt::AlignLeft|Qt::AlignVCenter, /* watchonly */
        Qt::AlignLeft|Qt::AlignVCenter, /* date */
        Qt::AlignLeft|Qt::AlignVCenter, /* type */
        Qt::AlignLeft|Qt::AlignVCenter, /* address */
        Qt::AlignRight|Qt::AlignVCenter /* amount */
    };

// Comparison operator for sort/binary search of model tx list
struct TxLessThan
{
    bool operator()(const TransactionRecord &a, const TransactionRecord &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const TransactionRecord &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const TransactionRecord &b) const
    {
        return a < b.hash;
    }
};

// Private implementation
class TransactionTablePriv
{
public:
    TransactionTablePriv(CWallet *_wallet, TransactionTableModel *_parent) :
        wallet(_wallet),
        parent(_parent)
    {
    }

    CWallet *wallet;
    TransactionTableModel *parent;

    /* Local cache of wallet.
     * As it is in the same order as the CWallet, by definition
     * this is sorted by sha256.
     */
    QList<TransactionRecord> cachedWallet;

    /* Query entire wallet anew from core.
     */
    void refreshWallet()
    {
        qDebug() << "TransactionTablePriv::refreshWallet";
        LogPrintf("Refreshing GUI Wallet from core\n");

        cachedWallet.clear();

        bool fIncludeWatchonly = true;
        {
            LOCK2(cs_main, wallet->cs_wallet);

            //Get all Archived Transactions
            uint256 ut;
            std::map<std::pair<int,int>, uint256> sortedArchive;
            for (map<uint256, ArchiveTxPoint>::iterator it = wallet->mapArcTxs.begin(); it != wallet->mapArcTxs.end(); ++it)
            {
                uint256 txid = (*it).first;
                ArchiveTxPoint arcTxPt = (*it).second;
                std::pair<int,int> key;

                if (!arcTxPt.hashBlock.IsNull() && mapBlockIndex.count(arcTxPt.hashBlock) > 0) {
                    key = make_pair(mapBlockIndex[arcTxPt.hashBlock]->GetHeight(), arcTxPt.nIndex);
                    sortedArchive[key] = txid;
                }
            }


            int nPosUnconfirmed = 0;
            for (map<uint256, CWalletTx>::iterator it = wallet->mapWallet.begin(); it != wallet->mapWallet.end(); ++it) {
                CWalletTx wtx = (*it).second;
                std::pair<int,int> key;

                if (wtx.GetDepthInMainChain() == 0) {
                    ut = wtx.GetHash();
                    key = make_pair(chainActive.Tip()->GetHeight() + 1,  nPosUnconfirmed);
                    sortedArchive[key] = wtx.GetHash();
                    nPosUnconfirmed++;
                } else if (!wtx.hashBlock.IsNull() && mapBlockIndex.count(wtx.hashBlock) > 0) {
                    key = make_pair(mapBlockIndex[wtx.hashBlock]->GetHeight(), wtx.nIndex);
                    sortedArchive[key] = wtx.GetHash();
                } else {
                    key = make_pair(chainActive.Tip()->GetHeight() + 1,  nPosUnconfirmed);
                    sortedArchive[key] = wtx.GetHash();
                    nPosUnconfirmed++;
                }
            }



            for (map<std::pair<int,int>, uint256>::reverse_iterator it = sortedArchive.rbegin(); it != sortedArchive.rend(); ++it)
            {

                uint256 txid = (*it).second;
                RpcArcTransaction arcTx;

                if (wallet->mapWallet.count(txid)) {

                    CWalletTx& wtx = wallet->mapWallet[txid];

                    if (!CheckFinalTx(wtx))
                        continue;

                    if (wtx.mapSaplingNoteData.size() == 0 && wtx.mapSproutNoteData.size() == 0 && !wtx.IsTrusted())
                        continue;

                    //Excude transactions with less confirmations than required
                    if (wtx.GetDepthInMainChain() < 0 )
                        continue;

                    getRpcArcTx(wtx, arcTx, fIncludeWatchonly, false);

                } else {
                    //Archived Transactions
                    getRpcArcTx(txid, arcTx, fIncludeWatchonly, false);

                    if (arcTx.blockHash.IsNull() || mapBlockIndex.count(arcTx.blockHash) == 0)
                      continue;

                }

                cachedWallet.append(TransactionRecord::decomposeTransaction(arcTx));

                if (cachedWallet.size() >= 200) break;
            }
        }
    }

    /* Update our model of the wallet incrementally, to synchronize our model of the wallet
       with that of the core.

       Call with transaction that was added, removed or changed.
     */
    void updateWallet(const uint256 &hash, int status, bool showTransaction)
    {

        // Find bounds of this transaction in model
        bool inModel = false;
        QList<TransactionRecord>::iterator i;
        for (i = cachedWallet.begin(); i != cachedWallet.end(); ++i) {
            TransactionRecord tRecord = *i;
            if (tRecord.hash == hash) {
                inModel = true;
            }
        }

        QList<TransactionRecord>::iterator lower = qLowerBound(
            cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        QList<TransactionRecord>::iterator upper = qUpperBound(
            cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
        int lowerIndex = (lower - cachedWallet.begin());
        int upperIndex = (upper - cachedWallet.begin());

        if(showTransaction && inModel)
            status = CT_UPDATED; /* In model, but want to show, do nothing */
        if(showTransaction && !inModel)
            status = CT_NEW; /* Not in model, but want to show, treat as new */
        if(!showTransaction && inModel)
            status = CT_DELETED; /* In model, but want to hide, treat as deleted */


        qDebug() << "    inModel=" + QString::number(inModel) +
                    " Index=" + QString::number(lowerIndex) + "-" + QString::number(upperIndex) +
                    " showTransaction=" + QString::number(showTransaction) + " derivedStatus=" + QString::number(status);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "TransactionTablePriv::updateWallet: Warning: Got CT_NEW, but transaction is already in model";
                break;
            }
            if(showTransaction)
            {
                LOCK2(cs_main, wallet->cs_wallet);
                // Find transaction in wallet
                bool isActiveTx = false;
                bool isArchiveTx = false;
                RpcArcTransaction arcTx;
                bool fIncludeWatchonly = true;

                //Try mapWallet first
                std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
                if(mi != wallet->mapWallet.end()) {
                    isActiveTx = true;
                    CWalletTx& wtx = wallet->mapWallet[hash];
                    getRpcArcTx(wtx, arcTx, fIncludeWatchonly, false);
                }

                //Try ArcTx is nor found in mapWallet
                if (!isActiveTx) {
                    std::map<uint256, ArchiveTxPoint>::iterator ami = wallet->mapArcTxs.find(hash);
                    if(ami != wallet->mapArcTxs.end()) {
                          isArchiveTx = true;
                          uint256 txid = hash;
                          getRpcArcTx(txid, arcTx, fIncludeWatchonly, false);
                          if (arcTx.blockHash.IsNull() || mapBlockIndex.count(arcTx.blockHash) == 0) {
                              isArchiveTx = false;
                          }
                    }
                }

                if (!isActiveTx && !isArchiveTx) {
                    qWarning() << "TransactionTablePriv::updateWallet: Warning: Got CT_NEW, but transaction is not in wallet";
                    parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
                    cachedWallet.erase(lower, upper);
                    parent->endRemoveRows();
                    break;
                }

                // Added -- insert at the right position
                QList<TransactionRecord> toInsert =
                        TransactionRecord::decomposeTransaction(arcTx);
                if(!toInsert.isEmpty()) /* only if something to insert */
                {
                    parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex+toInsert.size()-1);
                    int insert_idx = lowerIndex;
                    for (const TransactionRecord &rec : toInsert)
                    {
                        cachedWallet.insert(insert_idx, rec);
                        insert_idx += 1;
                    }
                    parent->endInsertRows();
                }
            }
            break;
        case CT_DELETED:
            if(!inModel)
            {
                qWarning() << "TransactionTablePriv::updateWallet: Warning: Got CT_DELETED, but transaction is not in model";
                break;
            }
            // Removed -- remove entire transaction from table
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedWallet.erase(lower, upper);
            parent->endRemoveRows();
            break;
        case CT_UPDATED:
            // Miscellaneous updates -- nothing to do, status update will take care of this, and is only computed for
            // visible transactions.
            for (int i = lowerIndex; i < upperIndex; i++) {
                TransactionRecord *rec = &cachedWallet[i];
                rec->status.needsUpdate = true;
            }
            break;
        }
    }

    int size()
    {
        return cachedWallet.size();
    }

    TransactionRecord *index(int idx)
    {
        if(idx >= 0 && idx < cachedWallet.size())
        {
            TransactionRecord *rec = &cachedWallet[idx];

            // Get required locks upfront. This avoids the GUI from getting
            // stuck if the core is holding the locks for a longer time - for
            // example, during a wallet rescan.
            //
            // If a status update is needed (blocks came in since last check),
            //  update the status of this transaction from the wallet. Otherwise,
            // simply re-use the cached status.
            TRY_LOCK(cs_main, lockMain);
            if(lockMain)
            {
                TRY_LOCK(wallet->cs_wallet, lockWallet);
                if(lockWallet && rec->statusUpdateNeeded())
                {
                    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);

                    if(mi != wallet->mapWallet.end())
                    {
                        rec->updateStatus(mi->second);
                    }
                }
            }
            return rec;
        }
        return 0;
    }

    QString describe(TransactionRecord *rec, int unit)
    {
        {
            LOCK2(cs_main, wallet->cs_wallet);
            std::map<uint256, ArchiveTxPoint>::iterator mi = wallet->mapArcTxs.find(rec->hash);
            if(mi != wallet->mapArcTxs.end())
            {
                return TransactionDesc::toHTML(wallet, rec, unit);
            }
        }
        return QString();
    }

    QString getTxHex(TransactionRecord *rec)
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);
        if(mi != wallet->mapWallet.end())
        {
            std::string strHex = EncodeHexTx(static_cast<CTransaction>(mi->second));
            return QString::fromStdString(strHex);
        }
        return QString();
    }
};

TransactionTableModel::TransactionTableModel(const PlatformStyle *_platformStyle, CWallet* _wallet, WalletModel *parent):
        QAbstractTableModel(parent),
        wallet(_wallet),
        walletModel(parent),
        priv(new TransactionTablePriv(_wallet, this)),
        fProcessingQueuedTransactions(false),
        platformStyle(_platformStyle)
{
    columns << QString() << QString() << tr("Date") << tr("Type") << tr("Label") << KomodoUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit());
    priv->refreshWallet();

    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    subscribeToCoreSignals();
}

TransactionTableModel::~TransactionTableModel()
{
    unsubscribeFromCoreSignals();
    delete priv;
}

/** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
void TransactionTableModel::updateAmountColumnTitle()
{
    columns[Amount] = KomodoUnits::getAmountColumnTitle(walletModel->getOptionsModel()->getDisplayUnit());
    Q_EMIT headerDataChanged(Qt::Horizontal,Amount,Amount);
}

void TransactionTableModel::updateTransaction(const QString &hash, int status, bool showTransaction)
{
    uint256 updated;
    updated.SetHex(hash.toStdString());

    priv->updateWallet(updated, status, showTransaction);
}

void TransactionTableModel::updateConfirmations()
{
    // Blocks came in since last poll.
    // Invalidate status (number of confirmations) and (possibly) description
    //  for all rows. Qt is smart enough to only actually request the data for the
    //  visible rows.
    Q_EMIT dataChanged(index(0, Status), index(priv->size()-1, Status));
    Q_EMIT dataChanged(index(0, ToAddress), index(priv->size()-1, ToAddress));
}

void TransactionTableModel::refreshWallet()
{
    return priv->refreshWallet();
}

int TransactionTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord *wtx) const
{
    QString status;
    if (!wtx->archiveType == ARCHIVED) {
        switch(wtx->status.status)
        {
        case TransactionStatus::OpenUntilBlock:
            status = tr("Open for %n more block(s)","",wtx->status.open_for);
            break;
        case TransactionStatus::OpenUntilDate:
            status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
            break;
        case TransactionStatus::Offline:
            status = tr("Offline");
            break;
        case TransactionStatus::Unconfirmed:
            status = tr("Unconfirmed");
            break;
        case TransactionStatus::Abandoned:
            status = tr("Abandoned");
            break;
        case TransactionStatus::Confirming:
            status = tr("Confirming (%1 of %2 recommended confirmations)").arg(wtx->status.depth).arg(TransactionRecord::RecommendedNumConfirmations);
            break;
        case TransactionStatus::Confirmed:
            status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
            break;
        case TransactionStatus::Conflicted:
            status = tr("Conflicted");
            break;
        case TransactionStatus::Immature:
            status = tr("Immature (%1 confirmations, will be available after %2)").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
            break;
        case TransactionStatus::MaturesWarning:
            status = tr("This block was not received by any other nodes and will probably not be accepted!");
            break;
        case TransactionStatus::NotAccepted:
            status = tr("Generated but not accepted");
            break;
        }
    } else {
        status = tr("Archived");
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord *wtx) const
{
    if(wtx->time)
    {
        return GUIUtil::dateTimeStr(wtx->time);
    }
    return QString();
}

/* Look up address in address book, if found return label (address)
   otherwise just return (address)
 */
QString TransactionTableModel::lookupAddress(const std::string &address, bool tooltip) const
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if(!label.isEmpty())
    {
        description += label;
    }
    if(label.isEmpty() || tooltip)
    {
        description += QString(" (") + QString::fromStdString(address) + QString(")");
    }
    return description;
}

QString TransactionTableModel::formatTxType(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::RecvWithAddress:
        return tr("Received with");
    case TransactionRecord::RecvWithAddressWithMemo:
        return tr("Received with Memo");
    case TransactionRecord::RecvFromOther:
        return tr("Received from");
    case TransactionRecord::SendToAddress:
        return tr("Sent to");
    case TransactionRecord::SendToAddressWithMemo:
        return tr("Sent with Memo");
    case TransactionRecord::SendToOther:
        return tr("Sent to");
    case TransactionRecord::SendToSelf:
        return tr("Internal");
    case TransactionRecord::SendToSelfWithMemo:
        return tr("Internal with Memo");
    case TransactionRecord::Generated:
        return tr("Mined");
    default:
        return QString();
    }
}

QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord *wtx) const
{
    switch(wtx->type)
    {
    case TransactionRecord::Generated:
        return QIcon(":/icons/tx_mined");
    case TransactionRecord::RecvWithAddress:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::RecvWithAddressWithMemo:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::RecvFromOther:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::SendToAddress:
        return QIcon(":/icons/tx_output");
    case TransactionRecord::SendToAddressWithMemo:
        return QIcon(":/icons/tx_output");
    case TransactionRecord::SendToOther:
        return QIcon(":/icons/tx_output");
    default:
        return QIcon(":/icons/tx_inout");
    }
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const
{
    QString watchAddress;
    if (tooltip) {
        // Mark transactions involving watch-only addresses by adding " (watch-only)"
        watchAddress = wtx->involvesWatchAddress ? QString(" (") + tr("watch-only") + QString(")") : "";
    }

    switch(wtx->type)
    {
    case TransactionRecord::RecvFromOther:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::RecvWithAddress:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::RecvWithAddressWithMemo:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::SendToAddress:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::SendToAddressWithMemo:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::Generated:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::SendToSelf:
        return QString::fromStdString(wtx->address) + watchAddress;
    case TransactionRecord::SendToSelfWithMemo:
        return QString::fromStdString(wtx->address) + watchAddress;
    default:
        return tr("(n/a)") + watchAddress;
    }
}

QVariant TransactionTableModel::addressColor(const TransactionRecord *wtx) const
{
    // Show addresses without label in a less visible color
    // switch(wtx->type)
    // {
    // case TransactionRecord::RecvWithAddress:
    // case TransactionRecord::SendToAddress:
    // case TransactionRecord::Generated:
    //     {
    //     QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
    //     if(label.isEmpty())
    //         return COLOR_BAREADDRESS;
    //     } break;
    // case TransactionRecord::SendToSelf:
    //     return COLOR_BAREADDRESS;
    // default:
    //     break;
    // }
    // return QVariant();

    return COLOR_BLACK;
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed, KomodoUnits::SeparatorStyle separators) const
{
    QString str = KomodoUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), wtx->credit + wtx->debit, false, separators);
    if(showUnconfirmed && !wtx->archiveType == ARCHIVED)
    {
        if(!wtx->status.countsForBalance)
        {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QVariant TransactionTableModel::txStatusDecoration(const TransactionRecord *wtx) const
{
    if (!wtx->archiveType == ARCHIVED) {
        switch(wtx->status.status)
        {
        case TransactionStatus::OpenUntilBlock:
        case TransactionStatus::OpenUntilDate:
            return COLOR_TX_STATUS_OPENUNTILDATE;
        case TransactionStatus::Offline:
            return COLOR_TX_STATUS_OFFLINE;
        case TransactionStatus::Unconfirmed:
            return QIcon(":/icons/transaction_0");
        case TransactionStatus::Abandoned:
            return QIcon(":/icons/transaction_abandoned");
        case TransactionStatus::Confirming:
            switch(wtx->status.depth)
            {
            case 1: return QIcon(":/icons/transaction_1");
            case 2: return QIcon(":/icons/transaction_2");
            case 3: return QIcon(":/icons/transaction_3");
            case 4: return QIcon(":/icons/transaction_4");
            default: return QIcon(":/icons/transaction_5");
            };
        case TransactionStatus::Confirmed:
            return QIcon(":/icons/transaction_confirmed");
        case TransactionStatus::Conflicted:
            return QIcon(":/icons/transaction_conflicted");
        case TransactionStatus::Immature: {
            int total = wtx->status.depth + wtx->status.matures_in;
            int part = (wtx->status.depth * 4 / total) + 1;
            return QIcon(QString(":/icons/transaction_%1").arg(part));
            }
        case TransactionStatus::MaturesWarning:
        case TransactionStatus::NotAccepted:
            return QIcon(":/icons/transaction_0");
        default:
            return COLOR_BLACK;
        }
    }
    return COLOR_BLACK;
}

QVariant TransactionTableModel::txWatchonlyDecoration(const TransactionRecord *wtx) const
{
    if (wtx->involvesWatchAddress)
        return QIcon(":/icons/eye");
    else
        return QVariant();
}

QString TransactionTableModel::formatTooltip(const TransactionRecord *rec) const
{
    QString tooltip = formatTxStatus(rec) + QString("\n") + formatTxType(rec);
    if(rec->type==TransactionRecord::RecvFromOther || rec->type==TransactionRecord::SendToOther ||
       rec->type==TransactionRecord::SendToAddress || rec->type==TransactionRecord::RecvWithAddress ||
       rec->type==TransactionRecord::SendToAddressWithMemo || rec->type==TransactionRecord::RecvWithAddressWithMemo)
    {
        tooltip += QString(" ") + formatTxToAddress(rec, true);
    }
    return tooltip;
}

QVariant TransactionTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
    TransactionRecord *rec = static_cast<TransactionRecord*>(index.internalPointer());

    switch(role)
    {
    case RawDecorationRole:
        switch(index.column())
        {
        case Status:
            return txStatusDecoration(rec);
        case Watchonly:
            return txWatchonlyDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;
    case Qt::DecorationRole:
    {
        QIcon icon = qvariant_cast<QIcon>(index.data(RawDecorationRole));
        return platformStyle->TextColorIcon(icon);
    }
    case Qt::DisplayRole:
        switch(index.column())
        {
        case Date:
            return formatTxDate(rec);
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Amount:
            return formatTxAmount(rec, true, KomodoUnits::separatorAlways);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch(index.column())
        {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return formatTxType(rec);
        case Watchonly:
            return (rec->involvesWatchAddress ? 1 : 0);
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return qint64(rec->credit + rec->debit);
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
    //     // Use the "danger" color for abandoned transactions
    //     if(rec->status.status == TransactionStatus::Abandoned && !rec->archiveType == ARCHIVED)
    //     {
    //         return COLOR_TX_STATUS_DANGER;
    //     }
    //     // Non-confirmed (but not immature) as transactions are grey
    //     if(!rec->status.countsForBalance && rec->status.status != TransactionStatus::Immature  && !rec->archiveType == ARCHIVED)
    //     {
    //         return COLOR_UNCONFIRMED;
    //     }
        if(index.column() == Amount && (rec->credit+rec->debit) < 0)
        {
            QSettings settings;
            if (settings.value("strTheme", "dark").toString() == "dark") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "pirate") {
                return COLOR_NEGATIVE;
            } else if (settings.value("strTheme", "dark").toString() == "piratemap") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "armada") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "treasure") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "treasuremap") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "ghostship") {
                return COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "night") {
                return COLOR_NEGATIVE_DARK;
            } else {
                return COLOR_NEGATIVE;
            }
        }
        if(index.column() == Amount && (rec->credit+rec->debit) > 0)
        {
            QSettings settings;
            if (settings.value("strTheme", "dark").toString() == "dark") {
                return COLOR_POSITIVE_DARK;
            } else if (settings.value("strTheme", "dark").toString() == "pirate") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "piratemap") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "armada") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "treasure") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "treasuremap") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "ghostship") {
                return COLOR_POSITIVE_PIRATE;
            } else if (settings.value("strTheme", "dark").toString() == "night") {
                return COLOR_POSITIVE_PIRATE;
            } else {
                return COLOR_POSITIVE;
            }
        }
    //     if(index.column() == ToAddress)
    //     {
    //         return addressColor(rec);
    //     }
        break;
    case TypeRole:
        return rec->type;
    case DateRole:
        return QDateTime::fromTime_t(static_cast<uint>(rec->time));
    case WatchonlyRole:
        return rec->involvesWatchAddress;
    case WatchonlyDecorationRole:
        return txWatchonlyDecoration(rec);
    case LongDescriptionRole:
        return priv->describe(rec, walletModel->getOptionsModel()->getDisplayUnit());
    case MemoDescriptionRole:
        return QString::fromStdString("Memo:<br>" + rec->memo + "<br><br>Memo Hex:<br>" + rec->memohex);
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));
    case AmountRole:
        return qint64(rec->credit + rec->debit);
    case TxIDRole:
        return rec->getTxID();
    case TxHashRole:
        return QString::fromStdString(rec->hash.ToString());
    case TxHexRole:
        return priv->getTxHex(rec);
    case TxPlainTextRole:
        {
            QString details;
            QDateTime date = QDateTime::fromTime_t(static_cast<uint>(rec->time));
            QString txLabel = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(rec->address));

            details.append(date.toString("M/d/yy HH:mm"));
            details.append(" ");
            details.append(formatTxStatus(rec));
            details.append(". ");
            if(!formatTxType(rec).isEmpty()) {
                details.append(formatTxType(rec));
                details.append(" ");
            }
            if(!rec->address.empty()) {
                if(txLabel.isEmpty())
                    details.append(tr("(no label)") + " ");
                else {
                    details.append("(");
                    details.append(txLabel);
                    details.append(") ");
                }
                details.append(QString::fromStdString(rec->address));
                details.append(" ");
            }
            details.append(formatTxAmount(rec, false, KomodoUnits::separatorNever));
            return details;
        }
    case ConfirmedRole:
        if (rec->archiveType == ARCHIVED) {
            return true;
        }
        return rec->status.countsForBalance;
    case FormattedAmountRole:
        // Used for copy/export, so don't include separators
        return formatTxAmount(rec, false, KomodoUnits::separatorNever);
    case StatusRole:
        return rec->status.status;
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case Watchonly:
                return tr("Whether or not a watch-only address is involved in this transaction.");
            case ToAddress:
                return tr("User-defined intent/purpose of the transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TransactionRecord *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    return QModelIndex();
}

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    updateAmountColumnTitle();
    Q_EMIT dataChanged(index(0, Amount), index(priv->size()-1, Amount));
}

// queue notifications to show a non freezing progress dialog e.g. for rescan
struct TransactionNotification
{
public:
    TransactionNotification() {}
    TransactionNotification(uint256 _hash, ChangeType _status, bool _showTransaction):
        hash(_hash), status(_status), showTransaction(_showTransaction) {}

    void invoke(QObject *ttm)
    {
        QString strHash = QString::fromStdString(hash.GetHex());
        qDebug() << "NotifyTransactionChanged: " + strHash + " status= " + QString::number(status);
        QMetaObject::invokeMethod(ttm, "updateTransaction", Qt::QueuedConnection,
                                  Q_ARG(QString, strHash),
                                  Q_ARG(int, status),
                                  Q_ARG(bool, showTransaction));
    }
private:
    uint256 hash;
    ChangeType status;
    bool showTransaction;
};

static bool fQueueNotifications = false;
static std::vector< TransactionNotification > vQueueNotifications;

static void NotifyTransactionChanged(TransactionTableModel *ttm, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    // Find transaction in wallet
    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
    // Determine whether to show transaction or not (determine this here so that no relocking is needed in GUI thread)
    bool inWallet = mi != wallet->mapWallet.end();
    bool showTransaction = (inWallet && TransactionRecord::showTransaction(mi->second));

    TransactionNotification notification(hash, status, showTransaction);

    if (fQueueNotifications)
    {
        vQueueNotifications.push_back(notification);
        return;
    }
    notification.invoke(ttm);
}

static void ShowProgress(TransactionTableModel *ttm, const std::string &title, int nProgress)
{
    if (nProgress == 0)
        fQueueNotifications = true;

    if (nProgress == 100)
    {
        fQueueNotifications = false;
        if (vQueueNotifications.size() > 10) // prevent balloon spam, show maximum 10 balloons
            QMetaObject::invokeMethod(ttm, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, true));
        for (unsigned int i = 0; i < vQueueNotifications.size(); ++i)
        {
            if (vQueueNotifications.size() - i <= 10)
                QMetaObject::invokeMethod(ttm, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, false));

            vQueueNotifications[i].invoke(ttm);
        }
        std::vector<TransactionNotification >().swap(vQueueNotifications); // clear
    }
}

void TransactionTableModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
}

void TransactionTableModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
}
