// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2010-2019 AriA developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ARIA_POW_H
#define ARIA_POW_H

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

// Define difficulty retarget algorithms
enum DiffMode {
    DIFF_DEFAULT = 0, // Default to invalid 0
    DIFF_BTC = 1,     // Retarget every x blocks (AriA style)
    DIFF_KGW = 2,     // Retarget using Kimoto Gravity Well
    DIFF_DGW = 3,     // Retarget using Dark Gravity Wave v3
};

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits);
uint256 GetBlockProof(const CBlockIndex& block);

#endif // ARIA_POW_H
