/**@file Objects for transferring between UMTS layers (frames, blocks, etc.) */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "UMTSTransfer.h"
#include "URRC.h"
#include <Logger.h>


namespace UMTS {


TxBitsBurst::TxBitsBurst(const BitVector& bits, size_t wSF, size_t wCodeIndex, const Time& wTime, bool wRightJustified)
		:BitVector(bits),mSF(wSF),mCodeIndex(wCodeIndex),mTime(wTime),mRightJustified(wRightJustified)
{
	mDCH = false;
	assert(size()/2 <= gSlotLen/wSF); 
	mLog2SF = 0;
	while (wSF > 1) {mLog2SF++; wSF = wSF >> 1;}
	mAICH = false;
}

std::ostream& TxBitsBurst::text(std::ostream&os) const
{
	os <<LOGVAR(mTime) <<LOGVAR(mSF) <<LOGVAR(mLog2SF) <<LOGVAR(mCodeIndex) <<LOGVAR(mRightJustified) <<" ";
	textBitVector(os);
	return os;
}

void TransportBlock::text(std::ostream&os) const
{
	os <<LOGVARM(mTime) <<LOGVARM(mScheduled) <<LOGVARM(mDescr) <<" ";
	textBitVector(os);
};

std::ostream& operator<<(std::ostream& os, const TxBitsBurst&tbb) { tbb.text(os); return os;}
std::ostream& operator<<(std::ostream& os, const TxBitsBurst*ptbb) { ptbb->text(os); return os; }
std::ostream& operator<<(std::ostream& os, const TransportBlock&tbb) { tbb.text(os); return os; }
std::ostream& operator<<(std::ostream& os, const TransportBlock*ptbb) { ptbb->text(os); return os; }


#if 0
size_t MACPDU::UEIdSize() const
{
	unsigned t = UEIdType();
	switch (t) {
		case 0: return 32;
		case 1: return 16;
		default:
			LOG(WARNING) << "invalid UEIdType " << t;
			return 0;
	}
}

// 25.321 Table 9.2.1.2 Coding of the Target Channel Type Field on FACH for FDD
// This table is needed by the UE to decode the TCTF, but not by UTRAN except
// for humans to inspect the PDU.
size_t FACH_MACPDU::TCTFSize() const
{
	static const size_t sz[16] = {
		2, 2, 2, 2,	// 00XX for BCCH
		8, 8, 4, 4,	// 010X, 0110, 0111
		8, 8, 8, 8,	// 10XX
		2, 2, 2, 2	// 11XX DTCH/DCCH over FACH
	};

	// 3GPP 25.321 9.2.1, Table 9.2.1.2.
	const unsigned first4 = peekField(TCTFBase(),4);
	return sz[first4];
}
#endif


#if 0
size_t DataRLCPDU::LILen() const
{
	size_t base = LIBase();
	size_t rp = base+8;
	while (peekField(rp-1,1)==1) { rp += 8; }
	return rp - base;
}


unsigned DataRLCPDU::LI() const
{
	size_t base = LIBase();
	size_t rp = base+8;
	unsigned val = 0;
	while (peekField(rp-1,1)==1) {
		val = (val<<7) + peekField(rp-8,7);
		rp += 8;
	}
	return val;
}
#endif

};
