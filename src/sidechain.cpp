// Copyright (c) 2017-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidechain.h>

#include <clientversion.h>
#include <core_io.h>
#include <hash.h>
#include <streams.h>
#include <utilmoneystr.h>

#include <algorithm>
#include <sstream>

const uint32_t nType = 1;
const uint32_t nVersion = 1;

uint256 SidechainObj::GetHash(void) const
{
    uint256 ret;
    if (sidechainop == DB_SIDECHAIN_WT_OP)
        ret = SerializeHash(*(SidechainWT *) this);
    else
    if (sidechainop == DB_SIDECHAIN_WTPRIME_OP)
        ret = SerializeHash(*(SidechainWTPrime *) this);
    else
    if (sidechainop == DB_SIDECHAIN_DEPOSIT_OP)
        ret = SerializeHash(*(SidechainDeposit *) this);

    return ret;
}

std::string SidechainObj::ToString(void) const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    return str.str();
}

std::string SidechainWT::ToString() const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    str << "nSidechain=" << std::to_string(nSidechain) << std::endl;
    str << "destination=" << strDestination << std::endl;
    str << "amount=" << FormatMoney(amount) << std::endl;
    str << "mainchainFee=" << FormatMoney(mainchainFee) << std::endl;
    str << "status=" << GetStatusStr() << std::endl;
    str << "hashBlindWTX=" << hashBlindWTX.ToString() << std::endl;
    return str.str();
}

std::string SidechainWT::GetStatusStr(void) const
{
    if (status == WT_UNSPENT) {
        return "Unspent";
    }
    else
    if (status == WT_IN_WTPRIME) {
        return "Pending - in WT^";
    }
    else
    if (status == WT_SPENT) {
        return "Spent";
    }
    return "Unknown";
}

std::string SidechainWTPrime::ToString() const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    str << "nSidechain=" << std::to_string(nSidechain) << std::endl;
    str << "wtprime=" << CTransaction(wtPrime).ToString() << std::endl;
    str << "status=" << GetStatusStr() << std::endl;
    return str.str();
}

std::string SidechainWTPrime::GetStatusStr(void) const
{
    if (status == WTPRIME_CREATED) {
        return "Created";
    }
    else
    if (status == WTPRIME_FAILED) {
        return "Failed";
    }
    else
    if (status == WTPRIME_SPENT) {
        return "Spent";
    }
    return "Unknown";
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream str;
    str << "sidechainop=" << sidechainop << std::endl;
    str << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    str << "nSidechain=" << std::to_string(nSidechain) << std::endl;
    str << "keyID=" << keyID.ToString() << std::endl;
    str << "payout=" << FormatMoney(amtUserPayout) << std::endl;
    str << "mainchaintxid=" << dtx.GetHash().ToString() << std::endl;
    str << "n=" << std::to_string(n) << std::endl;
    str << "inputs:\n";
    for (const CTxIn& in : dtx.vin) {
        str << in.prevout.ToString() << std::endl;
    }
    return str.str();
}

SidechainObj* ParseSidechainObj(const std::vector<unsigned char>& vch)
{
    if (vch.size() == 0)
        return NULL;

    const char *vch0 = (const char *) &vch.begin()[0];
    CDataStream ds(vch0, vch0+vch.size(), SER_DISK, CLIENT_VERSION);

    if (*vch0 == DB_SIDECHAIN_WT_OP) {
        SidechainWT *obj = new SidechainWT;
        obj->Unserialize(ds);
        return obj;
    }
    else
    if (*vch0 == DB_SIDECHAIN_WTPRIME_OP) {
        SidechainWTPrime *obj = new SidechainWTPrime;
        obj->Unserialize(ds);
        return obj;
    }
    else
    if (*vch0 == DB_SIDECHAIN_DEPOSIT_OP) {
        SidechainDeposit *obj = new SidechainDeposit;
        obj->Unserialize(ds);
        return obj;
    }

    return NULL;
}

struct CompareWTMainchainFee
{
    bool operator()(const SidechainWT& a, const SidechainWT& b) const
    {
        return a.mainchainFee > b.mainchainFee;
    }
};

void SortWTByFee(std::vector<SidechainWT>& vWT)
{
    std::sort(vWT.begin(), vWT.end(), CompareWTMainchainFee());
}

struct CompareWTPrimeHeight
{
    bool operator()(const SidechainWTPrime& a, const SidechainWTPrime& b) const
    {
        return a.nHeight > b.nHeight;
    }
};

void SortWTPrimeByHeight(std::vector<SidechainWTPrime>& vWTPrime)
{
    std::sort(vWTPrime.begin(), vWTPrime.end(), CompareWTPrimeHeight());
}

void SelectUnspentWT(std::vector<SidechainWT>& vWT)
{
    vWT.erase(std::remove_if(vWT.begin(), vWT.end(),[](const SidechainWT& wt)
                {return wt.status != WT_UNSPENT;}), vWT.end());

}

CScript SidechainObj::GetScript(void) const
{
    CDataStream ds (SER_DISK, CLIENT_VERSION);
    if (sidechainop == DB_SIDECHAIN_WT_OP)
        ((SidechainWT *) this)->Serialize(ds);
    else
    if (sidechainop == DB_SIDECHAIN_WTPRIME_OP)
        ((SidechainWTPrime *) this)->Serialize(ds);
    else
    if (sidechainop == DB_SIDECHAIN_DEPOSIT_OP)
        ((SidechainDeposit *) this)->Serialize(ds);

    CScript scriptPubKey;

    std::vector<unsigned char> vch(ds.begin(), ds.end());

    // Add script header
    scriptPubKey.resize(5 + vch.size());
    scriptPubKey[0] = OP_RETURN;
    scriptPubKey[1] = 0xAC;
    scriptPubKey[2] = 0xDC;
    scriptPubKey[3] = 0xF6;
    scriptPubKey[4] = 0x6F;

    // Add vch (serialization)
    memcpy(&scriptPubKey[5], vch.data(), vch.size());

    return scriptPubKey;
}
