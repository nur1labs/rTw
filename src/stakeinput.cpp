// Copyright (c) 2010-2019 AriA developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulators.h"
#include "chain.h"
#include "primitives/zerocoin.h"
#include "main.h"
#include "stakeinput.h"
#include "wallet.h"

int CZAriAStake::GetChecksumHeightFromMint()
{
    int nHeightChecksum = chainActive.Height() - Params().Zerocoin_RequiredStakeDepth();
    //nHeightChecksum -= (nHeightChecksum % 10);

    //Need to return the first occurance of this checksum in order for the validation process to identify a specific
    //block height
    uint32_t nChecksum = 0;
    nChecksum = ParseChecksum(chainActive[nHeightChecksum]->nAccumulatorCheckpoint, mint.GetDenomination());
    return GetChecksumHeight(nChecksum, mint.GetDenomination());
}

int CZAriAStake::GetChecksumHeightFromSpend()
{
    return GetChecksumHeight(nChecksum, denom);
}

uint32_t CZAriAStake::GetChecksum()
{
    return nChecksum;
}

// The zAriA block index is the first appearance of the accumulator checksum that was used in the spend
// note that this also means when staking that this checksum should be from a block that is beyond 60 minutes old and
// 100 blocks deep.
CBlockIndex* CZAriAStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;

    int nHeightChecksum = 0;

    if (fMint)
        nHeightChecksum = GetChecksumHeightFromMint();
    else
        nHeightChecksum = GetChecksumHeightFromSpend();

    if (nHeightChecksum < Params().Zerocoin_StartHeight()) {
        pindexFrom = nullptr;
    } else {
        //note that this will be a nullptr if the height DNE
        pindexFrom = chainActive[nHeightChecksum];
    }

    return pindexFrom;
}

CAmount CZAriAStake::GetValue()
{
    return denom * COIN;
}

//Use the first accumulator checkpoint that occurs 60 minutes after the block being staked from
bool CZAriAStake::GetModifier(uint64_t& nStakeModifier)
{
    CBlockIndex* pindex = GetIndexFrom();
    if (!pindex)
        return false;

    int64_t nTimeBlockFrom = pindex->GetBlockTime();
    while (true) {
        if (pindex->GetBlockTime() - nTimeBlockFrom > 60*60) {
            nStakeModifier = pindex->nAccumulatorCheckpoint.Get64();
            return true;
        }

        if (pindex->nHeight + 1 <= chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            return false;
    }
}

CDataStream CZAriAStake::GetUniqueness()
{
    //LogPrintf("%s serial=%s\n", __func__, bnSerial.GetHex());
    //The unique identifier for a ZAriA is the serial
    CDataStream ss(SER_GETHASH, 0);
    ss << bnSerial;
    return ss;
}

bool CZAriAStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    LogPrintf("%s\n", __func__);
    CZerocoinSpendReceipt receipt;
    int nSecurityLevel = 100;
    if (!pwallet->MintToTxIn(mint, nSecurityLevel, hashTxOut, txIn, receipt, libzerocoin::SpendType::STAKE))
        return error("%s\n", receipt.GetStatusMessage());

    return true;
}

bool CZAriAStake::CreateTxOuts(CWallet* pwallet, vector<CTxOut>& vout)
{
    LogPrintf("%s\n", __func__);
    //todo: add mints here
    //Create an output returning the zAriA that was staked
    CTxOut outReward;
    libzerocoin::CoinDenomination denomStaked = libzerocoin::AmountToZerocoinDenomination(this->GetValue());
    libzerocoin::PrivateCoin coin(Params().Zerocoin_Params(), denomStaked);
    if (!pwallet->CreateZAriAOutPut(denomStaked, coin, outReward))
        return error("%s: failed to create zAR output", __func__);
    vout.emplace_back(outReward);

    for (unsigned int i = 0; i < 3; i++) {
        CTxOut outReward;
        libzerocoin::PrivateCoin coinReward(Params().Zerocoin_Params(), libzerocoin::CoinDenomination::ZQ_ONE);
        if (!pwallet->CreateZAriAOutPut(libzerocoin::CoinDenomination::ZQ_ONE, coinReward, outReward))
            return error("%s: failed to create zAR output", __func__);
        vout.emplace_back(outReward);
    }

    return true;
}

bool CZAriAStake::GetTxFrom(CTransaction& tx)
{
    LogPrintf("%s\n", __func__);
    return false;
}

bool CZAriAStake::MarkSpent(CWallet *pwallet)
{
    mint.SetUsed(true);
    CWalletDB walletdb(pwallet->strWalletFile);
    return walletdb.WriteZerocoinMint(mint);
}

//!AriA Stake
bool CAriAStake::SetInput(CTransaction txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CAriAStake::GetTxFrom(CTransaction& tx)
{
    tx = txFrom;
    return true;
}

bool CAriAStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom.GetHash(), nPosition);
    return true;
}

CAmount CAriAStake::GetValue()
{
    return txFrom.vout[nPosition].nValue;
}

bool CAriAStake::CreateTxOuts(CWallet* pwallet, vector<CTxOut>& vout)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom.vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
        LogPrintf("CreateCoinStake : failed to parse kernel\n");
        return false;
    }

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
        return false; // only support pay to public key and pay to address

    CScript scriptPubKey;
    if (whichType == TX_PUBKEYHASH) // pay to address type
    {
        //convert to pay to public key type
        CKey key;
        if (!pwallet->GetKey(uint160(vSolutions[0]), key))
            return false;

        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;
    } else
        scriptPubKey = scriptPubKeyKernel;

    vout.emplace_back(CTxOut(0, scriptPubKey));
    return true;
}

bool CAriAStake::GetModifier(uint64_t& nStakeModifier)
{
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
    GetIndexFrom();
    if (!pindexFrom)
        return error("%s: failed to get index from", __func__);

    if (!GetKernelStakeModifier(pindexFrom->GetBlockHash(), nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false))
        return error("CheckStakeKernelHash(): failed to get kernel stake modifier \n");

    return true;
}

CDataStream CAriAStake::GetUniqueness()
{
    //The unique identifier for a AriA stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << nPosition << txFrom.GetHash();
    return ss;
}

//The block that the UTXO was added to the chain
CBlockIndex* CAriAStake::GetIndexFrom()
{
    uint256 hashBlock = 0;
    CTransaction tx;
    if (GetTransaction(txFrom.GetHash(), tx, hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom.GetHash().GetHex());
    }

    return pindexFrom;
}
