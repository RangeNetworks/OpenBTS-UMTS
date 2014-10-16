/**@file L1 TrCH/PhCH FEC declarations for UMTS. */

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

#include "UMTSL1FEC.h"
#include "MACEngine.h"
#include <assert.h>
#include <Configuration.h>
#include <Logger.h>
#include "UMTSConfig.h"
#include "URRCTrCh.h"
#include "URRC.h"
#include "RateMatch.h"


using namespace std;
namespace UMTS {

int gFecTestMode = 0;
extern ConfigurationTable gConfig;

DCHListType gActiveDCH;

#if 0	// unused
/** From 3GPP 25.211 Table 11, 15-slot formats only */
unsigned SlotFormatDnNData1[] = {
 0, 0, 2, 2,		// 0-3
 2, 2, 2, 2,		// 4-7
 6, 6, 6, 6,		// 8-11
 12, 28, 56, 120,	// 12-15
 248			// 16
};

/** From 3GPP 25.211 Table 11, 15-slot formats only */
unsigned SlotFormatDnNData2[] = {
 4, 2, 14, 12,		// 0-3
 12, 10, 8, 6,		// 4-7
 28, 26, 24, 22,	// 8-11
 48, 112, 232, 488,	// 12-15
 1000			// 16
};

/** From 3GPP 25.211 Table 11, 15-slot formats only */
unsigned SlotFormatDnTPC[] = {
 2, 2, 2, 2,		// 0-3
 2, 2, 2, 2,		// 4-7
 2, 2, 2, 2,		// 8-11
 4, 4, 8, 8,		// 12-15
 8			// 16
};

/** From 3GPP 25.211 Table 11, 15-slot formats only */
unsigned SlotFormatDnTFCI[] = {
 0, 2, 2, 2,		// 0-3
 0, 2, 0, 2,		// 4-7
 0, 2, 0, 2,		// 8-11
 8, 8, 8, 8,		// 12-15
 16			// 16
};

/** From 3GPP 25.211 Table 11, 15-slot formats only */
unsigned SlotFormatDnPilot[] = {
 4, 4, 2, 4,		// 0-3
 4, 4, 8, 8,		// 4-7
 4, 4, 8, 8,		// 8-11
 8, 8, 16, 16,		// 12-15
 16			// 16
};
#endif



TrCHFECDecoder::TrCHFECDecoder(TrCHFEC* wParent,FecProgInfo &fpi):
			TrCHFECBase(wParent,fpi),
			mUpstream(NULL)
#if OLD_CONTROL_IF
			, mRunning(false),
			mFER(0.0F),
			mAssignmentTimer(10000),
			mReleaseTimer(10000)
#endif
{
	unsigned frameSize = gFrameLen / SF();
	mD = new SoftVector(frameSize);
	mHDI = new SoftVector(frameSize);
	mDSlotIndex = 0;
	unsigned nrf = fpi.getNumRadioFrames();// number of radio frames per tti
	//mDTti = new SoftVector(frameSize * nrf);
	// mRM and mDTti are after rate-matching:
	mRM = new SoftVector(fpi.mCodedBkSz/nrf);
	mDTti = new SoftVector(fpi.mCodedBkSz);
	mDTtiIndex = 0;

	rateMatchComputeUlEini(fpi.mCodedBkSz/nrf,mRadioFrameSz,fpi.getTTICode(),mEini);
}


#if OLD_CONTROL_IF
bool TrCHFECDecoder::recyclable() const
{
	ScopedLock lock(mLock);
	if (mAssignmentTimer.expired()) return true;
	if (mReleaseTimer.expired()) return true;
	return false;
}
#endif


PhCh * TrCHFECBase::getPhCh() const
{
	assert(mParent);
	return mParent->mPhCh;
}


unsigned FecProgInfo::getNumRadioFrames() const
{
	return TTICode2NumFrames(getTTICode());
}

FecProgInfoSimple::FecProgInfoSimple(unsigned wSF,TTICodes wTTICode,unsigned wPB,
	unsigned wRadioFrameSz) :
	FecProgInfo(wSF,wTTICode,wPB,wRadioFrameSz, false)
{
	assert(mPB == 16);	// Required for the simple channels.
	// assume no rate matching, convolutional coding, and compute TB size.
	mCodedBkSz = mRadioFrameSz * getNumRadioFrames();
	mTBSz = RrcDefs::R2DecodedSize(mCodedBkSz) - mPB;
}

// This class is used to init the FecProgInfo for a full support rach/fach/dch.
// Assumes convolutional coding.
// TODO: Need a different constructor for Turbo-coded.
FecProgInfoInit::FecProgInfoInit(unsigned wSF,TTICodes wTTICode,unsigned wPB,
	unsigned wRadioFrameSz, unsigned wTBSz, bool wTurbo) :
	FecProgInfo(wSF,wTTICode,wPB,wRadioFrameSz, wTurbo)
{
	assert(mPB == 24 || mPB == 16 || mPB == 12 || mPB == 8);
	assert(wTBSz != 0);
	mTBSz = wTBSz;	// Determined by RRC.
	if (wTurbo) {
        	mCodedBkSz = RrcDefs::TurboEncodedSize(mTBSz + mPB, &mCodeBkSz);
       		mFillBits = RrcDefs::TurboDecodedSize(mCodedBkSz) - (mTBSz+mPB);
	} else {
		mCodedBkSz = RrcDefs::R2EncodedSize(mTBSz + mPB, &mCodeBkSz);
		mFillBits = RrcDefs::R2DecodedSize(mCodedBkSz) - (mTBSz+mPB);
	}
	printf("ProgInfo: %u %u %u %u %u\n",wSF,wRadioFrameSz,wTBSz,(unsigned)mCodedBkSz,(unsigned)mFillBits);
}

TrCHFECDecoder* TrCHFECEncoder::sibling()
{
	assert(mParent);
	return mParent->decoder();
}

const TrCHFECDecoder* TrCHFECEncoder::sibling() const
{
	assert(mParent);
	return mParent->decoder();
}


TrCHFECEncoder* TrCHFECDecoder::sibling()
{
	assert(mParent);
	return mParent->encoder();
}

const TrCHFECEncoder* TrCHFECDecoder::sibling() const
{
	assert(mParent);
	return mParent->encoder();
}




void TrCHFECEncoder::open()
{
	mTotalBursts = 0;
	mPrevWriteTime = 0;
	mNextWriteTime = gNodeB.clock().get();
	mActive = true;
}


void TrCHFECEncoder::close()
{
	mActive = false;
}


void TrCHFECDecoder::open()
{
#if OLD_CONTROL_IF
	mActive = true;
	mAssignmentTimer.set();
	mReleaseTimer.reset();
#else
	controlOpen();
#endif
}


void TrCHFECDecoder::close(bool hardRelease)
{
#if OLD_CONTROL_IF
	mActive = false;
	mReleaseTimer.set();
	// TODO UMTS - If hardRelease, we need to force-expire that timer.
#else
	controlClose(hardRelease);
#endif
}

// Incoming frame is the result of 25.212 4.2.6 Radio Frame Segmentation.
void TrCHFECEncoder::sendFrame(BitVector& frame, unsigned tfci)
{
	//TODO: This test now fails, correctly, due to rate matching.
	//assert(frame.size()%gFrameSlots == 0);

	BitVector h = frame.alias();

	// 25.212 4.2.8 TrCh Multiplexing.
	// (pat) We only have one TrCh, so nothing to do.

	// 25.212 4.2.9 Insertion of Discontinuous Transmission (DTX) Indicators.
        // Since we only either the max. size of possible transport format combinatiors or transmit nothing, DTX is not needed.
	// (pat) This adds empty bits to fill up the radio frames.
	// The description is complicated by various types of compressed frames that we
	// will never support, but it is basically just right-padding with emptiness.

	// 25.212 4.2.10 Physical Channel Segmentation.
	// "When more than one PhCH is used..."  OK, can stop reading right there.

	// 25.212 4.2.11 Second Interleaving.
	// (pat) Number of columns fixed at 30, and number of rows is the minimum that will work.
	// The padding will only occur when supporting multiple TrCh, because the
	// radio frame is a multiple 150 which is divisible by 30.
	const unsigned C2 = 30;		// Number of columns;
	unsigned hsize = h.size();
	unsigned rows = (hsize + (C2 - 1))/C2;
	int padding = (C2 * rows) - hsize;
	assert(padding >= 0);
	unsigned Ysize = hsize + padding;	// Y is defined in 4.2.11 as padded interleave buf
	BitVector Yout(Ysize);
	if (padding == 0) {
		h.interleavingNP(C2, TrCHConsts::inter2Perm, Yout);
	} else {
		// Must pre-pad and post-strip bits.
		// The post-interleave pad bits are spread all over; easiest way to get rid
		// of them is to use a special marker value for the padding.
		const char padval = 4;	// We will pad with padbits set to this value.
		BitVector Yin(Ysize);	// Temporary buffer
		h.copyTo(Yin);
		memset(Yin.begin()+hsize,padval,padding);	// Add the padding.
		Yin.interleavingNP(C2, TrCHConsts::inter2Perm, Yout);
		// Strip out the padding bits.
		char *yp = Yout.begin(), *yend = Yout.end();
		for (char *cp = yp; cp < yend; cp++) {
			if (*cp != padval) { *yp++ = *cp; }
		}
		assert(yp == Yin.begin() + hsize);
	}

	BitVector U(Yout.head(hsize));

#if USE_OLD_FEC		// This makes assumptions about RACHFEC so does not compile with the new code.
	if (gFecTestMode == 2) {
		gNodeB.mRachFec->decoder()->writeLowSide2(U);
		return;
	}
#endif

	// 25.212 4.2.12 Physical Channel Mapping.
	// "In compressed mode..."  OK, can stop reading right there.

	// 25.212 4.3 Transport Format Detection Based on TFCI.
	// 25.212 4.3.3 Coding of TFCI.
	// (pat) We have to pre-encode the TFCI to go in the radio slots.
	// This encoding is static based only on on the tfci to be sent,
	// so it is done once on startup in class TrCHConsts,
	// and now we just index into it with the incoming tfci.
	assert(tfci < TrCHConsts::sMaxTfci);
	//tfci = 0;
	uint32_t tfciCode = TrCHConsts::sTfciCodes[tfci];

	// (pat) Okey Dokey, finished with spec 25.212.
	// Now we move on to another fine and wonderful spec:
	// 25.211: "Physical channels and mapping of transport channels onto physical channels (FDD)"
	PhCh *phch = getPhCh();
	PhChType chType = phch->phChType();
	assert(chType == DPDCHType || chType == SCCPCHType || chType == PCCPCHType);
	size_t dataSlotSize = U.size() / gFrameSlots;

	// NOTE: PCCPCHType sends only 18 bits per slot, not 20.
	if (chType == PCCPCHType) {
		// The PCCPCH radio slot contains: | Tx Off | Data (always 18 bits) |
		for (unsigned i=0; i<gFrameSlots; i++) {
			// (pat) Before I added the BitVector copy constructor, this allocated
			// a new Vector via constructor: Vector(const Vector<char>&other)
			const BitVector slotBits = U.segment(i*dataSlotSize,dataSlotSize);
			TxBitsBurst *out = new TxBitsBurst(
				slotBits,
				SF(), SpCode(), mNextWriteTime.slot(i),true
			);
			RN_MEMLOG(TxBitsBurst,out);
			phch->getRadio()->writeHighSide(out);
		}
	} else {

		int radioFrameSize = 2*(gFrameLen / SF());
		BitVector radioFrame(radioFrameSize);
		int radioSlotSize = radioFrameSize/15;

		SlotFormat *dlslot = phch->getDlSlot();
		const unsigned ndata1 = dlslot->mNData1;
		const unsigned ndata2 = dlslot->mNData2;
		const unsigned ntpc = dlslot->mNTpc;
		const unsigned ntfci = dlslot->mNTfci;
		const unsigned tfciMask = (1<<ntfci)-1;
		const unsigned npilot = dlslot->mNPilot;
		const unsigned pi = dlslot->mPilotIndex;
		// Double check this against channel parameters.
		assert(dataSlotSize == ndata1+ndata2);

		BitVector radioSlotBits(radioSlotSize);
		for (unsigned s=0; s<gFrameSlots; s++) {
			const unsigned dataStart = s*dataSlotSize;
			size_t wp = 0;
			unsigned int tpcField = 0;

			// 25.211 4.3.5.1: Mapping of TFCI word in normal [non-compressed] mode:
			// The 32 TFCI code bits are stuck in the slots LSB first,
			// wrapping around to be reused as much as needed.
			switch (chType) {
			case SCCPCHType:
				//cout << "TFCI: " << tfci << " code: " << (tfciCode&tfciMask);
				// The SCCPCH radio slot contains: | TFCI | Data | Pilot |
				radioSlotBits.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
				U.segment(dataStart,ndata1).copyToSegment(radioSlotBits,wp,ndata1);
				wp += ndata1;
				radioSlotBits.fillField(wp, TrCHConsts::sDlPilotBitPattern[pi][s], npilot);
				// There is no data2 for SCCPCH.
				break;
			case DPDCHType:
				// The DCH radio slot contains: 
				// Release 4 or later:     | Data1 |  TPC  | TFCI | Data2 | Pilot |
				// Early versions of R'99: | TFCI  | Data1 | TPC  | Data2 | Pilot |
#ifdef RELEASE99 // defined in UMTSPhCh.cpp
                                radioSlotBits.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
#endif
				if (ndata1 > 0) {
                                	U.segment(dataStart,ndata1).copyToSegment(radioSlotBits,wp,ndata1);
                                	wp += ndata1;
				}
				// Lower layers are going to fill in TPC, we hope.
				// Toggle tpc bits between all ones and all zeros to keep phone at same tx power
				// TODO: Will need to do better power control later
				tpcField = ((1 << ntpc)-1); // * ( (s+mNextWriteTime.FN()) % 2);
				radioSlotBits.writeField(wp,tpcField,ntpc);
#ifndef RELEASE99
				radioSlotBits.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
#endif
				if (ndata2 > 0) {
					U.segment(dataStart+ndata1,ndata2).copyToSegment(radioSlotBits,wp,ndata2);
					wp += ndata2;
				}
				radioSlotBits.fillField(wp, TrCHConsts::sDlPilotBitPattern[pi][s], npilot);
				break;
			default: assert(0);
			}

			// rotate tfciCode by ntfci bits.  It is unsigned, which helps.
			tfciCode = (tfciCode>>ntfci) | (tfciCode<<(32-ntfci));

			TxBitsBurst *out = new TxBitsBurst(radioSlotBits, SF(), SpCode(), mNextWriteTime.slot(s));
			RN_MEMLOG(TxBitsBurst,out);
			phch->getRadio()->writeHighSide(out);
		}
	}
	mPrevWriteTime = mNextWriteTime;
	++mNextWriteTime;	// The ++ does not matter for BCH because the TBs have scheduled() set, which overrides mNextWriteTime.
}

#if 0
//void TrCHConsts::initPilotBitPatterns()
//{
//	// 25.211 5.3.2 Table 12: Pilot bit patterns for downlink DPCCH with Npilot = 2, 4, 8 and 16
//	//
//	//      Npilot=2 Npilot=4   Npilot=8          Npilot=16
//	//Symbol   0       0  1    0  1  2  3     0  1  2  3  4  5  6  7
//	//   #
//	//Slot #0  11     11 11   11 11 11 10    11 11 11 10 11 11 11 10
//	//   1     00     11 00   11 00 11 10    11 00 11 10 11 11 11 00
//	//   2     01     11 01   11 01 11 01    11 01 11 01 11 10 11 00
//	//   3     00     11 00   11 00 11 00    11 00 11 00 11 01 11 10
//	//   4     10     11 10   11 10 11 01    11 10 11 01 11 11 11 11
//	//   5     11     11 11   11 11 11 10    11 11 11 10 11 01 11 01
//	//   6     11     11 11   11 11 11 00    11 11 11 00 11 10 11 11
//	//   7     10     11 10   11 10 11 00    11 10 11 00 11 10 11 00
//	//   8     01     11 01   11 01 11 10    11 01 11 10 11 00 11 11
//	//   9     11     11 11   11 11 11 11    11 11 11 11 11 00 11 11
//	//  10     01     11 01   11 01 11 01    11 01 11 01 11 11 11 10
//	//  11     10     11 10   11 10 11 11    11 10 11 11 11 00 11 10
//	//  12     10     11 10   11 10 11 00    11 10 11 00 11 01 11 01
//	//  13     00     11 00   11 00 11 11    11 00 11 11 11 00 11 00
//	//  14     00     11 00   11 00 11 11    11 00 11 11 11 10 11 01
//
//	// These are from columns 1,3,5,7 for nPilot==16; all the others columns are just copies.
//	// Table 19: Pilot Symbol Pattern for SCCPCH is identical.
//	static uint8_t sDlPilotBitPatternTable[4][gFrameSlots] = {
//		{ 3, 0, 1, 0, 2, 3, 3, 2, 1, 3, 1, 2, 2, 0, 0 },	// column 1
//		{ 2, 2, 1, 0, 1, 2, 0, 0, 2, 3, 1, 3, 0, 3, 3 },	// column 3
//		{ 3, 3, 2, 1, 3, 1, 2, 2, 0, 0, 3, 0, 1, 0, 2 },	// column 5
//		{ 2, 0, 0, 2, 3, 1, 3, 0, 3, 3, 2, 2, 1, 0, 1 }		// column 7
//		};
//
//	for (unsigned slot = 0; slot < gFrameSlots; slot++) {
//		uint16_t col1 = sDlPilotBitPatternTable[0][slot];
//		uint16_t col3 = sDlPilotBitPatternTable[1][slot];
//		uint16_t col5 = sDlPilotBitPatternTable[2][slot];
//		uint16_t col7 = sDlPilotBitPatternTable[3][slot];
//		uint16_t pat;
//		// Npilot=2
//		sDlPilotBitPattern[0][slot] = col1;
//		// Npilot=4
//		sDlPilotBitPattern[1][slot] = pat = (3<<2)|col1;
//		// Npilot=8
//		sDlPilotBitPattern[2][slot] = pat = (pat<<4)|(3<<2)|col3;
//		// Npilot=16
//		sDlPilotBitPattern[3][slot] = (pat<<8)|(3<<6)|(col5<<4)|(3<<2)|col7;
//	}
//}
//
//
//	// 25.212 4.2.11 table 7: Inter-Column permutation pattern for 2nd interleaving:
//const char TrCHConsts::inter2Perm[30] = {0,20,10,5,15,25,3,13,23,8,18,28,1,11,21,
//		6,16,26,4,14,24,19,9,29,12,2,7,22,27,17};
//
//	// (pat) 25.212 4.3.3 table 8: Magic for TFCI code encoding.
//	// The codes are 32 words of 10 bits each, but I am generating it
//	// form the original binary table in the spec (below)
//	// to avoid any transcription errors.
//const bool TrCHConsts::reedMullerTable[32][10] = {
//		// i Mi,0 Mi,1 Mi,2 Mi,3 Mi,4 Mi,5 Mi,6 Mi,7 Mi,8 Mi,9
//		/*0*/ {   1,   0,   0,   0,   0,   1,   0,   0,   0,   0 },
//		/*1*/ {   0,   1,   0,   0,   0,   1,   1,   0,   0,   0 },
//		/*2*/ {   1,   1,   0,   0,   0,   1,   0,   0,   0,   1 },
//		/*3*/ {   0,   0,   1,   0,   0,   1,   1,   0,   1,   1 },
//		/*4*/ {   1,   0,   1,   0,   0,   1,   0,   0,   0,   1 },
//		/*5*/ {   0,   1,   1,   0,   0,   1,   0,   0,   1,   0 },
//		/*6*/ {   1,   1,   1,   0,   0,   1,   0,   1,   0,   0 },
//		/*7*/ {   0,   0,   0,   1,   0,   1,   0,   1,   1,   0 },
//		/*8*/ {   1,   0,   0,   1,   0,   1,   1,   1,   1,   0 },
//		/*9*/ {   0,   1,   0,   1,   0,   1,   1,   0,   1,   1 },
//		/*10*/ {  1,   1,   0,   1,   0,   1,   0,   0,   1,   1 },
//		/*11*/ {  0,   0,   1,   1,   0,   1,   0,   1,   1,   0 },
//		/*12*/ {  1,   0,   1,   1,   0,   1,   0,   1,   0,   1 },
//		/*13*/ {  0,   1,   1,   1,   0,   1,   1,   0,   0,   1 },
//		/*14*/ {  1,   1,   1,   1,   0,   1,   1,   1,   1,   1 },
//		/*15*/ {  1,   0,   0,   0,   1,   1,   1,   1,   0,   0 },
//		/*16*/ {  0,   1,   0,   0,   1,   1,   1,   1,   0,   1 },
//		/*17*/ {  1,   1,   0,   0,   1,   1,   1,   0,   1,   0 },
//		/*18*/ {  0,   0,   1,   0,   1,   1,   0,   1,   1,   1 },
//		/*19*/ {  1,   0,   1,   0,   1,   1,   0,   1,   0,   1 },
//		/*20*/ {  0,   1,   1,   0,   1,   1,   0,   0,   1,   1 },
//		/*21*/ {  1,   1,   1,   0,   1,   1,   0,   1,   1,   1 },
//		/*22*/ {  0,   0,   0,   1,   1,   1,   0,   1,   0,   0 },
//		/*23*/ {  1,   0,   0,   1,   1,   1,   1,   1,   0,   1 },
//		/*24*/ {  0,   1,   0,   1,   1,   1,   1,   0,   1,   0 },
//		/*25*/ {  1,   1,   0,   1,   1,   1,   1,   0,   0,   1 },
//		/*26*/ {  0,   0,   1,   1,   1,   1,   0,   0,   1,   0 },
//		/*27*/ {  1,   0,   1,   1,   1,   1,   1,   1,   0,   0 },
//		/*28*/ {  0,   1,   1,   1,   1,   1,   1,   1,   1,   0 },
//		/*29*/ {  1,   1,   1,   1,   1,   1,   1,   1,   1,   1 },
//		/*30*/ {  0,   0,   0,   0,   0,   1,   0,   0,   0,   0 },
//		/*31*/ {  0,   0,   0,   0,   1,   1,   1,   0,   0,   0 }
//		};
//
//void TrCHConsts::initTfciCodes()
//{
//	// Pre-compute the tfci code for each possible tfci.
//	// Implements 4.3.3 verbatim.
//	// Yes, I realize we could turn the table sideways and do this quickly,
//	// but it is only done once, ever.
//	for (unsigned tfci = 0; tfci < sMaxTfci; tfci++) {
//		uint32_t result = 0;
//		for (unsigned i = 0; i <= 31; i++) {
//			unsigned bi = 0;
//			for (unsigned n = 0; n <= 9; n++) {
//				unsigned an = (tfci >> n) & 1;		// a0 is the lsb of tfci.
//				bi += (an & reedMullerTable[i][n]);	// b0 is the lsb of the result.
//			}
//			result |= ((bi&1) << i);
//		}
//		sTfciCodes[tfci] = result;
//		if (tfci < 4) printf("TFCI[%d] = 0x%x\n",tfci,sTfciCodes[tfci]);
//	}
//}
//
//// This is the wonderfully redundant redundant C++ way to declare declare declare static members.
//bool TrCHConsts::oneTimeInit = false;
//uint16_t TrCHConsts::sDlPilotBitPattern[4][15];
//uint32_t TrCHConsts::sTfciCodes[sMaxTfci];	// Table for up to 8 bit tfci, plenty for us.
//
//TrCHConsts::TrCHConsts(TTICodes wTTImsDiv10Log2)
//	:mTTImsDiv10Log2(wTTImsDiv10Log2)
//{
//	static int inter1Columns[4] = {
//		1, // TTI = 10ms
//		2, // TTI = 20ms
//		4, // TTI = 40ms
//		8 // TTI = 80ms
//	};
//
//	// 25.212 4.2.5.2 table 4: Inter-Column permutation pattern for 1st interleaving:
//	static char inter1Perm[4][8] = {
//		{0}, // TTI = 10ms
//		{0, 1}, // TTI = 20ms
//		{0, 2, 1, 3}, // TTI = 40ms
//		{0, 4, 2, 6, 1, 5, 3, 7} // TTI = 80ms
//	};
//
//
//	/**
//	static uint16_t reedMullerCode[32];	// They are 10 bits long.
//	for (unsigned w = 0; w < 32; w++) {
//		unsigned code = 0;
//		for (unsigned b = 0; b < 10; b++) {
//			code = (code << 1) | !!reedMullerTable[w][b];
//		}
//		ReedMullerCode[w] = code;
//	}
//	**/
//
//	if (!oneTimeInit) {
//		oneTimeInit = true;
//		initPilotBitPatterns();
//		initTfciCodes();
//	}
//
//	mInter1Columns = inter1Columns[mTTImsDiv10Log2];
//	mInter1Perm = inter1Perm[mTTImsDiv10Log2];
//}
//
//// In uplink each slot has 2 TFCI bits which are concatenated to form 30 bits,
//// from which we attempt to retrieve the original tfci.
//unsigned findTfci(SoftVector &radioTfciBits, unsigned numTfcis)
//{
//	assert(radioTfciBits.size() == 30);
//	float *bits = radioTfciBits.begin();
//
//	unsigned bestTfci = 0;
//	float bestMatch = 0;
//	assert(numTfcis <= TrCHConsts::sMaxTfci);
//	for (unsigned tfci = 0; tfci < numTfcis; tfci++) {
//		uint32_t tfciCode = TrCHConsts::sTfciCodes[tfci];
//		float thisMatch = 0;
//		for (unsigned b = 0; b < 30; b++) {
//			// The bits are transmitted in the slots LSB first, so start with LSB of tfciCode.
//			unsigned wantbit = tfciCode & 1; tfciCode >>= 1;
//			float havebit = RN_BOUND(bits[b],0.0,1.0);
//			if (wantbit) {
//				thisMatch += havebit;
//			} else {
//				thisMatch += 1.0 - havebit;
//			}
//		}
//		// A perfect match would be 30.
//		if (thisMatch > bestMatch) {
//			bestMatch = thisMatch;
//			bestTfci = tfci;
//		}
//	}
//	return bestTfci;	// TODO: Regardless of how poor it is?
//}
#endif

// parity - 25.212, 4.2.1
void getParity(const BitVector &in, BitVector &parity)
{
	int L = parity.size();
	if (in.size() > 0) {
		uint64_t gcrc;
		if (L == 24) {
			gcrc = TrCHConsts::mgcrc24;
		} else if (L == 16) {
			gcrc = TrCHConsts::mgcrc16;
		} else if (L == 12) {
			gcrc = TrCHConsts::mgcrc12;
		} else if (L == 8) {
			gcrc = TrCHConsts::mgcrc8;
		} else if (L == 0) {
			// no parity bits to add
		} else {
			assert(0);
		}
		if (L != 0) {
			Parity p(gcrc, L, L + in.size());
			p.writeParityWord(in, parity);
			parity.reverse();
			parity.invert(); // undo inversion done by parity generator
		}
	} else {
		parity.fill(0);
	}
}


void TrCHFECEncoder::writeHighSide(const TransportBlock &tblock)
{
	const BitVector a = tblock.alias();
        //PhCh *phch = getPhCh();
        //PhChType chType = phch->phChType();
	/*if (chType==SCCPCHType) {
          LOG(NOTICE) << "SCCPCH TrCHFECEncoder input " << a.size() << " " << a.hexstr();
	  LOG(NOTICE) << tblock.size() << "  " << trBkSz();
	}*/
	LOG(DEBUG) << "TrCHFECEncoder input " <<tblock;
	assert(tblock.size() == trBkSz());

	// parity - 25.212, 4.2.1
	BitVector b(a.size() + getPB());
	a.copyTo(b);
	BitVector parityOfA = b.segment(a.size(), getPB());
	getParity(a, parityOfA);
	//OBJLOG(DEBUG) << "with parity " << b.size() << " " << b;
	if (gFecTestMode) {
		std::cout<<"writeHighSide"<<LOGBV(a)<<LOGBV(parityOfA)<<"\n";
	}

	// 24.212 4.2.2.1 Transport Block Concatenation.
	// 		not yet

	BitVector c;		// The result after convolutional encoding.

	unsigned Z = getZ();
	if (b.size() <= Z) {
		// No code block segmentation required.
		BitVector o = b.alias();

		// 24.212 4.2.3 Channel Coding.
		// convolutional coding - 25.212, 4.2.3.1
		if (isTurbo()) {
			c = BitVector(3 * o.size() + 12);
			encode(o,c);
		} else {
			BitVector in(b.size() + 8);
			c = BitVector(2 * in.size());
			o.copyTo(in);
			in.fill(0, b.size(), 8);
			encode(in,c);
		}
	} else {
		// 24.212 4.2.2.2 Code Block Segmentation.
		// 25.212 4.2.3.3 concatenation of encoded blocks
		// We just encode the blocks directly into the concatenated output c.
		// This code is for convolutional coding only!!
		unsigned Xi = b.size();			// number of input bits
		unsigned Ci = (Xi+ Z-1)/ Z;		// number of code blocks.
		unsigned Ki = (Xi+Ci-1)/Ci;		// number of bits per block.
		unsigned Yi = Ci * Ki - Xi;		// number of filler bits.
		// Unnecessary assertion used as a warning that rate-matching
		// may be required.  The code below works fine, but the RRC
		// does not currently take these Yi filler bits into account
		// and may have botched the TB size calculation if Yi is non-zero.
		// Take this assertion out if you are sure.
		// (pat) 6-19-2012: Updated TrChConfig::configDchPS to take Yi into
		// account and removed this assertion.  We should still assert
		// elsewhere that any rate-matching is downward only.
		// assert(Yi == 0);
		const unsigned csize = isTurbo() ? 3*Ki+12 : 2*Ki+16;
		c = BitVector(Ci * csize);
		BitVector in(Ki+8);
		for (unsigned r = 0; r < Ci; r++) {
			if (Yi && (r == 0)) {
				in.fill(0,0,Yi);		// First block has Yi filler bits.
				b.segment(0,Ki-Yi).copyToSegment(in,Yi);
			} else {
				b.segment(r*Ki-Yi,Ki).copyToSegment(in,0);
			}
			// 24.212 4.2.3 Channel Coding,
			// convolutional coding - 25.212, 4.2.3.1
			// And implicit concatenation of encoded blocks.
                        BitVector csegment = c.segment(r*csize,csize);
			if (!isTurbo()) {
				in.fill(0,Ki,8);
				encode(in,csegment);
			} else {
				BitVector inTrunc = in.segment(0,Ki);
				encode(inTrunc,csegment);
			}
		}
	}

	// 25.212 4.2.4 Radio Frame Size Equalization
	// (pat) 25.212 4.2.4 and I quote:
	// "Radio frame size equalisation is only performed in the UL."
	// (pat) And that is because we use DTX instead of rate-matching in DL.
	BitVector g = c.alias();
	
	// Rate-matching
        // The RRC insures that codedBkSz < mRadioFrameSz, ie, we will never use puncturing, ever,
        // for any TF [Transport Format]
        unsigned outsize = this->mRadioFrameSz*this->getNumRadioFrames();
        unsigned insize = this->mCodedBkSz;
	BitVector h(outsize);
        if (insize != outsize) 
                rateMatchFunc<char>(g,h,1);
        else 
                g.copyTo(h);

	// not doing insertion of DTX  (pat) doesnt go here?
	// 4.2.9
        //BitVector h; // This is 3-valued, but for now, just pad with zeros.
	// Since we only either the max. size of possible transport format combinatiors or transmit nothing, DTX is not needed.
        
        /*if (insize == outsize) {
                h = frame;
        } else {
                h = BitVector(outsize);
                frame.copyTo(h);
                h.tail(frame.size()).zero();
        }*/

	// first interleave - 25.212, 4.2.5
	BitVector q(h.size());
	h.interleavingNP(inter1Columns(), inter1Perm(), q);
	//LOG(DEBUG) << "interleaved " <<LOGVAR2("descr",tblock.mDescr) << " "<< q.str();
	LOG(DEBUG) << "interleaved " << q.str();

#if USE_OLD_FEC		// This makes assumptions about RACHFEC so does not compile with the new code.
	if (gFecTestMode == 1) {
		// (pat) For testing, send it back up through the decoder.
		// Jumper around the radio frame segmentation for now.
		// We are assuming it is RACH/FACH for now.
		SoftVector qdebug(q); // Convert to SoftVector.
		gNodeB.mRachFec->decoder()->writeLowSide3(qdebug);
		return;
	}
#endif

	// radio frame segmentation - 25.212, 4.2.6
	// inter1Columns is the same as the number of 10 ms radio frames in the TTI.
	const int frames = inter1Columns();
	const int frameSize = q.size() / frames;
	assert(q.size() % frameSize == 0);
        if (tblock.scheduled()) mNextWriteTime = tblock.time();
	for (int i = 0; i < frames; i++) {
		BitVector seg = q.segment(i * frameSize, frameSize);
		sendFrame(seg,0 /*tblock.mTfci*/);
	}
}

// (pat) I assume the incoming burst is a single-slot data burst.
// Note that uplink separates data and control info, so the data burst is
// nothing but data - the control burst contains the pilot and tfci bits.
// This function just adds filler bursts for missing slot data.
void TrCHFECDecoder::l1WriteLowSide(const RxBitsBurst &burst)
{
	// TODO: Enable these assertions.
	//assert(burst.mTfciBits[0] >= 0 && burst.mTfciBits[0] <= 1);
	//assert(burst.mTfciBits[1] >= 0 && burst.mTfciBits[1] <= 1);

	const SoftVector f = burst.alias();

	// not doing rate matching
	SoftVector e = f.alias();

	// (pat) Why are we inserting filler bursts going upstream?
	// They will fail the parity check (hopefully!) and writeLowSide1 will just throw them away?
	// Answer: Because the radio frame un-segmentation needs the right number of
	// bursts or it will become permanently desynchronized.
	// We cannot use the burst time to synchronize because incoming RACH bursts are not
	// synchronized to the main radio clock; we just get 15/30 in a row starting anywhere.
	// Also, maybe he hopes the convolutional decoder will span the data.

	// This assumes frame boundary is at TN=0, and TTI boundary is at FN=0
	unsigned expectedTTISlotIndex = mDTtiIndex*gFrameSlots+mDSlotIndex;
	unsigned receivedTTISlotIndex = burst.time().TN();
	unsigned numFramesTTI = TTICode2NumFrames(getTTICode());
	unsigned receivedTTIIndex = ((unsigned) burst.time().FN() % numFramesTTI);
	receivedTTISlotIndex += (receivedTTIIndex*gFrameSlots);
	if (receivedTTISlotIndex <= expectedTTISlotIndex) {
		mDSlotIndex = burst.time().TN();
		mDTtiIndex = receivedTTIIndex;
		expectedTTISlotIndex = receivedTTISlotIndex;
	} 

	//LOG(INFO) << "time: " << burst.time() << " slot: " << mDSlotIndex << " frame " << mDTtiIndex;
	SoftVector fillerBurst;
	bool first = true;
	while (receivedTTISlotIndex > expectedTTISlotIndex) {
		if (first) {
			fillerBurst.resize(burst.size());
			for (unsigned i = 0; i < burst.size(); i++) 
				fillerBurst[i] = 0.5f + 0.0001*(2.0*(float) (random() & 0x01)-1.0);
			first = false;
		}
		float garbageTfci[2] = { 1.0, 1.0 };
		writeLowSide1(fillerBurst, garbageTfci);
		expectedTTISlotIndex++;
	}
	writeLowSide1(e,burst.mTfciBits);
}

// (pat) Accumulate slots into one radio frame in mD.
void TrCHFECDecoder::writeLowSide1(const SoftVector &e, const float tfcibits[2])
{
	//OBJLOG(INFO) << "TrCHFECDecoder input " << e.size() << " " << e;

	// (pat) If TTI != 10ms, we just concatenate radio frames until we have them all.
	// (pat) So accumulate the incoming fuzzy bits in mD until it is full.
	// radio frame segmentation - 25.212, 4.2.6
	unsigned frameSize = gFrameLen / SF();
	unsigned slotSize = frameSize/gFrameSlots;
	assert(e.size() == slotSize);

	e.copyToSegment(*mD,mDSlotIndex*slotSize);
	if (++mDSlotIndex < gFrameSlots) {return;}
	mDSlotIndex = 0;	// prep for next Radio Frame.

	// (pat) Previous code did alot of copying things around:
	//if (mD->size() == frameSize) mD->resize(0);
	//SoftVector *tmp = mD;
	//mD = new SoftVector(*mD, e);
	//delete tmp;
	//if (mD->size() < frameSize) return;

	assert(mD->size() == frameSize);
	//OBJLOG(INFO) << "concatenated " << mD->size() << " " << *mD;
	writeLowSide2(*mD);
}

// The input here is one radio-frame, ie, 15 slots worth.
void TrCHFECDecoder::writeLowSide2(const SoftVector &frame)
{
	unsigned frameSize = gFrameLen / SF();
	assert(frame.size() == frameSize);
	// 25.212 4.2.12 Physical Channel Mapping.
	// "In compressed mode..."  Nope.

	// TODO: These vectors can be allocated statically at initialization time.
	// 25.212 4.2.11 Second Interleaving.
	const_cast<SoftVector&>(frame).deInterleavingNP(30, TrCHConsts::inter2Perm, *mHDI);
	assert(mHDI->size() == frameSize);
	//OBJLOG(INFO) << "2nd deinterleaved " << mHDI->size() << " " << *mHDI;
	// 25.212 4.2.10 Physical Channel Segmentation.
	// "When more than one PhCh is used..."  Nope.

	// 25.212 4.2.8 TrCh (De-)Multiplexing.
	// Not yet.

	// 25.212 4.2.7 Rate Matching.
	unsigned insize = this->mRadioFrameSz;
	unsigned outsize = this->mCodedBkSz/this->getNumRadioFrames();
	assert(mHDI->size() == insize);
	SoftVector *result = mHDI;
	if (insize != outsize) {
		result = mRM;
		rateMatchFunc<float>(*mHDI,*mRM,this->mEini[mDTtiIndex]);
	}

	OBJLOG(INFO) << "insize: " << insize << "outsize: " << outsize;
        OBJLOG(INFO) << "rate-unmatched" << result->size() << " " << *result;

	// 25.212 4.2.6 Radio Frame Segmentation.
	// If not using TTI=10ms, At this point we have to reaccumulate the complete TTI data.
	TTICodes tticode = getTTICode();
	if (tticode == TTI10ms) { writeLowSide3(*result); return; }

	// Accumulate one ttis worth of Radio Frames into mDTti.
	result->copyToSegment(*mDTti,mDTtiIndex*outsize);
	mDTtiIndex++;

	unsigned numFramesPerTti = getNumRadioFrames();
	if (mDTtiIndex < numFramesPerTti) {return;}
	mDTtiIndex = 0;	// prep for next TTI
	writeLowSide3(*mDTti);
}

// Input is post-radio-frame-segmentation, which means input is the accumulation
// of 1, 2, 4, 8 radio frames based on the TTI=10,20,40,80
void TrCHFECDecoder::writeLowSide3(const SoftVector &dataTti)
{
	//OBJLOG(INFO) << "concatenated TTI: " << dataTti.size() << " " << dataTti;

	// first interleave - 25.212, 4.2.5
	SoftVector t(dataTti.size());
	const_cast<SoftVector&>(dataTti).deInterleavingNP(inter1Columns(), inter1Perm(), t);
	OBJLOG(INFO) << "deinterleaved " << t.size() << " " << t;

	// radio frame equalization - 25.212, 4.2.4
	SoftVector c = t.alias();

	// FIXME -- This stuff assumes a rate-1/2 coder.

	//assert(c.size() <= 2 * getZ());
	BitVector b;	// The result
	if (0) {
		// old code does not handle concatenation/segmentation.
		BitVector o(c.size() / 2);
		decode(c,o);
		//OBJLOG(INFO) << "unconvoluted " << o.size() << " " << o;

		// 24.212 4.2.2 Transport Block Concatenation and Code Block Segmentation.
		// concatenation - 25.212, 4.2.2.1
		// segmentation - 25.212, 4.2.2.2
		// nothing to do but remove 8 bits of fill
		//BitVector b(o.size() - 8);
		b = BitVector(o.size() - 8);
		o.copyToSegment(b, 0, o.size() - 8);
	} else {
		// convolutional coding - 25.212, 4.2.3.1
		// concatenation of encoded blocks - 25.212, 4.2.3.3
		unsigned Zenc = isTurbo() ? (3*getZ()+12) : (2*getZ() + 16);		// encoded size of Z.
		unsigned Ci = (c.size() + Zenc-1)/Zenc;	// number of coded blocks.
		unsigned Kienc = c.size()/Ci;		// number of encoded bits per coded block.
		unsigned Ki = isTurbo() ? ((Kienc-12)/3) : (Kienc/2 - 8);		// number of unencoded bits per coded block.
		unsigned numFillBits = fillBits();		// number of filler bits in first coded block.
		assert(Kienc * Ci == c.size());
		b = BitVector(Ci*Ki - numFillBits);
		BitVector o1(isTurbo() ? Ki : (Ki+8));
		//printf("%u %u %u %u\n",Zenc,Ci,Kienc,Ki);
		for (unsigned r = 0; r < Ci; r++) {
			decode(c.segment(r*Kienc,Kienc),o1);
			if (numFillBits && (r == 0)) {// skip first fillBits, they aren't data
				o1.segmentCopyTo(b,numFillBits,Ki-numFillBits);
			}
			else
				o1.copyToSegment(b,r*Ki-numFillBits,Ki);
		}
	}
	OBJLOG(INFO) << "de-filled " << b.size() << " " << b;
	OBJLOG(INFO) << "de-filled last 100: " << b.segment(b.size()-100,100);
	// parity - 25.212, 4.2.1
	unsigned pb = getPB();
	BitVector a = b.segment(0, b.size() - pb);
	BitVector gotParity = b.segment(b.size() - pb, pb);
	BitVector expectParity(pb);
	BitVector bWithoutParity = b.segment(0, b.size() - pb);
	getParity(bWithoutParity, expectParity);
	//bool parityOK = true;
	//for (int i = 0; i < pb && parityOK; i++) {
	//	parityOK = expectParity[i] == gotParity[i];
	//}
	bool parityOK = expectParity == gotParity;
	if (gotParity.sum() == 0) parityOK = false;
	OBJLOG(INFO) << "parity " << expectParity << " " << gotParity;
	if (gFecTestMode) {
		std::cout<<"writeLowSide3"<<LOGBV(b) <<LOGBV(gotParity)<<LOGBV(expectParity)<<LOGVAR(parityOK)<<"\n";
	}

	if (parityOK) {
		countGoodFrame();
	} else {
		countBadFrame();
		return;
	}

	assert(mUpstream);
	const TransportBlock tb(a);
	LOG(INFO) << "Sending up tb: " << a;
	mUpstream->macWriteLowSideTb(tb);
}

#if OLD_CONTROL_IF
void TrCHFECDecoder::countGoodFrame()
{
	ScopedLock lock(mLock);
	mAssignmentTimer.reset();
	static const float a = 1.0F / ((float)mFERMemory);
	static const float b = 1.0F - a;
	mFER *= b;
	OBJLOG(DEBUG) <<"L1Decoder FER=" << mFER;
}

void TrCHFECDecoder::countBadFrame()
{
	static const float a = 1.0F / ((float)mFERMemory);
	static const float b = 1.0F - a;
	mFER = b*mFER + a;
	OBJLOG(DEBUG) <<"L1Decoder FER=" << mFER;
}
#endif

// (pat) This is used only for BCH - see macServiceLoop for FACH and DCH.
//void TrCHFECEncoder::waitToSend() const
void TrCHFECEncoder::l1WaitToSend() const
{
	// Block until the NodeB clock catches up to the
	// mostly recently transmitted burst.
	//LOG(INFO) << "mPrevWriteTime: " << mPrevWriteTime << ", " << mNextWriteTime;
	// (pat) TODO: Add Transceiver.mTransmitLatency in here.
	gNodeB.clock().wait(mPrevWriteTime);
	//LOG(NOTICE) << "waitToSend "<<when<<" clock="<<gNodeB.clock().FN();
}




void TrCHFECEncoderLowRate::encode(BitVector& in, BitVector& c)
{
	// convolutional coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	in.encode(mVCoder, c);
	OBJLOG(DEBUG) << "convoluted " << c.str(); // c.size() << " " << c;
}

void TrCHFECDecoderLowRate::decode(const SoftVector& c, BitVector& o)
{
	c.decode(mVCoder, o);
	OBJLOG(DEBUG) << "unconvoluted " << c.str();	//c.size() << " " << c;
}


void BCHFEC::generate()
{
	//printf("BCHFEC::generate\n"); fflush(stdout);
	l1WaitToSend();
	const TransportBlock *tb = gNodeB.getTxSIB(nextWriteTime().FN());
	//printf("BCHFEC::generate calling writeHighSide\n"); fflush(stdout);
	//LOG(NOTICE) << "BCH TB.time="<<tb->time() <<" clock="<<gNodeB.clock().FN() <<" t="<< format("%.2f",timef());
	l1WriteHighSide(*tb);
}



static void* BCHServiceLoop(BCHFEC* chan)
{
	while (true) {
		chan->generate();
	}
	return 0;
}


void BCHFEC::start()
{
	mServiceThread.start((void* (*)(void*))BCHServiceLoop, (void*)this);
}



void TrCHFECEncoderTurbo::encode(BitVector& in, BitVector &c)
{
	// coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	in.encode(mTCoder, c, mInterleaver);
	OBJLOG(DEBUG) << "turbo " << c.str();	//c.size() << " " << c;
}

void TrCHFECDecoderTurbo::decode(const SoftVector&c, BitVector &o)
{
	// coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	c.decode(mTCoder, o, mInterleaver);
	OBJLOG(DEBUG) << "turbo " << o.str();	//o.size() << " " << o;
}

#if USE_OLD_DCH
// Create the encoder/decoders for this FEC class from the RRC programming.
void DCHFEC::fecConfig(TrChConfig &config, bool turbo)
{
	// Currently we support only one TrCh.
	assert(config.ul()->getNumTrCh() == 1);
	assert(config.dl()->getNumTrCh() == 1);

	// Uplink:
	{
		RrcTfs *tfs = config.ul()->getTfs(0);

		// Currently we do not support TFC, so only one TF can be non-empty.
		int ntf = tfs->getNumTF();
		int numNonEmpty = 0;
		for (int n = 0; n < ntf; n++) {
			int numtb = tfs->getNumTB(n);
			int tbsize = tfs->getTBSize(n);
			if (numtb == 0 || tbsize == 0) continue;
			if (numNonEmpty++) {assert(0);}	// There are multiple non-empty TF.
			assert(numtb == 1);
			FecProgInfoInit fpiul(this->getUlSF(),tfs->getTTICode(),tfs->getPB(),
				this->getUlRadioFrameSize(), tbsize, turbo);
			if (turbo) {
                        	TrCHFECDecoder *decoder = new TrCHFECDecoderTurbo(this,fpiul);
                        	this->setDecoder(decoder);
 			} else {
				TrCHFECDecoder *decoder = new TrCHFECDecoderLowRate(this,fpiul);
				this->setDecoder(decoder);
			}
		}
	}

	// Downlink:
	{
		RrcTfs *tfs = config.dl()->getTfs(0);

		// Currently we do not support TFC, so only one TF can be non-empty.
		int ntf = tfs->getNumTF();
		int numNonEmpty = 0;
		for (int n = 0; n < ntf; n++) {
			int numtb = tfs->getNumTB(n);
			int tbsize = tfs->getTBSize(n);
			if (numtb == 0 || tbsize == 0) continue;
			if (numNonEmpty++) {assert(0);}	// There are multiple non-empty TF.
			assert(numtb == 1);
			FecProgInfoInit fpidl(this->getDlSF(),tfs->getTTICode(),tfs->getPB(),
				this->getDlRadioFrameSize(), tbsize, turbo);
			if (turbo) {
                        	TrCHFECEncoder *encoder = new TrCHFECEncoderTurbo(this,fpidl);
                        	this->setEncoder(encoder);
			} else {
				TrCHFECEncoder *encoder = new TrCHFECEncoderLowRate(this,fpidl);
				this->setEncoder(encoder);
			}
		}
	}
	//this->setDownstream(mRadio);	radio pointer moved to PhCh

}
#endif

void DCHFEC::open()
{
	ScopedLock lock(gActiveDCH.mLock);
	// The DCHFEC was already allocated from the ChannelTree by the caller.
	assert(phChAllocated());
#if USE_OLD_DCH
	mEncoder->open();
	mDecoder->open();
#else
	controlOpen();
#endif
	gActiveDCH.push_back((DCHFEC*)this);
	cout << "Opening DCH" << endl;
}

// TODO: Do we want a time delay here somewhere before reusing the channel?
void DCHFEC::close()
{
	printf("waiting to remove...\n");
	while (gActiveDCH.inTxUse || gActiveDCH.inRxUse) usleep(1000);
	ScopedLock lock(gActiveDCH.mLock);
	gActiveDCH.remove((DCHFEC*)this);
	printf("removed\n");
#if USE_OLD_DCH
	mEncoder->close();
	mDecoder->close();
#else
	// Somebody needs to deallocate the encoder/decoders.
	controlClose();
#endif
	mPhCh->phChClose();		// Allows reallocation in the ChannelTree.
}


}; // namespace

// vim: ts=4 sw=4
