// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATIONINTERFACE_H
#define BITCOIN_VALIDATIONINTERFACE_H

#include <boost/signals2/signal.hpp>

#include "zcash/IncrementalMerkleTree.hpp"

class CBlock;
class CBlockIndex;
struct CBlockLocator;
class CTransaction;
class CValidationInterface;
class CValidationState;
class uint256;

// These functions dispatch to one or all registered wallets

/** Register a wallet to receive updates from core */
void RegisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister a wallet from core */
void UnregisterValidationInterface(CValidationInterface* pwalletIn);
/** Unregister all wallets from core */
void UnregisterAllValidationInterfaces();
/** Push an updated transaction to all registered wallets */
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, const int nHeight);
/** Erase a transaction from all registered wallets */
void EraseFromWallets(const uint256 &hash);
/** Rescan all registered wallets */
void RescanWallets();

class CValidationInterface {
protected:
    virtual void UpdatedBlockTip(const CBlockIndex *pindex) {}
    virtual void SyncTransaction(const CTransaction &tx, const CBlock *pblock, const int nHeight) {}
    virtual bool EraseFromWallet(const uint256 &hash) { return true; }
    virtual void RescanWallet() {}
    virtual void ChainTip(const CBlockIndex *pindex, const CBlock *pblock, SproutMerkleTree sproutTree, SaplingMerkleTree saplingTree, bool added) {}
    virtual void UpdatedTransaction(const uint256 &hash) {}
    virtual void Inventory(const uint256 &hash) {}
    virtual void ResendWalletTransactions(int64_t nBestBlockTime) {}
    virtual void BlockChecked(const CBlock&, const CValidationState&) {}
    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
};

struct CMainSignals {
    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void (const CBlockIndex *)> UpdatedBlockTip;
    /** Notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void (const CTransaction &, const CBlock *, const int nHeight)> SyncTransaction;
    /** Notifies listeners of an erased transaction (currently disabled, requires transaction replacement). */
    boost::signals2::signal<void (const uint256 &)> EraseTransaction;
    /** Notifies listeners of the need to rescan the wallet. */
    boost::signals2::signal<void ()> RescanWallet;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<void (const uint256 &)> UpdatedTransaction;
    /** Notifies listeners of a change to the tip of the active block chain. */
    boost::signals2::signal<void (const CBlockIndex *, const CBlock *, SproutMerkleTree, SaplingMerkleTree, bool)> ChainTip;
    /** Notifies listeners about an inventory item being seen on the network. */
    boost::signals2::signal<void (const uint256 &)> Inventory;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (int64_t nBestBlockTime)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
};

CMainSignals& GetMainSignals();

#endif // BITCOIN_VALIDATIONINTERFACE_H
