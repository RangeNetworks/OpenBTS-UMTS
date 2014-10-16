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

#define UMTSL1_IMPLEMENTATION 1
#include "UMTSL1FEC.h"
#include "UMTSL1CC.h"
#include "MACEngine.h"
#include <assert.h>
#include <Configuration.h>
#include <Logger.h>
#include "UMTSConfig.h"
#include "URRCTrCh.h"
#include "URRC.h"
#include "RateMatch.h"
#include <iostream>
#include <fstream>

#define CANNEDBEACON 0
#if CANNEDBEACON
#include "cannedBeacon.h"
#endif 

#define RN_ABS(x) ((x)>=0?(x):-(x))		// absolute value
#define LOGFECINFO LOG(NOTICE)		// This is for the fec programming.
#define LOGDEBUG LOG(DEBUG)
#define LOG_DOWNLINK LOG(DEBUG)
#define LOG_UPLINK LOG(DEBUG)


using namespace std;
namespace UMTS {

extern ConfigurationTable gConfig;

extern int gFecTestMode;
#if SAVEME
int gFecTestMode = 0;
DCHListType gActiveDCH;
#endif

#if 0 // UNUSED
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


L1TrChProgInfo *L1FecProgInfo::getTCI() { return mInfoParent->getTCI(mCCTrChIndex); }
L1TrChProgInfo *L1FecProgInfo::getTCI() const { return mInfoParent->getTCI(mCCTrChIndex); }
unsigned L1FecProgInfo::getPB() const { return getTCI()->mPB; }
TTICodes L1FecProgInfo::getTTICode() const { return getTCI()->mTTICode; }
unsigned L1FecProgInfo::getNumRadioFrames() const { return TTICode2NumFrames(getTCI()->mTTICode); }

// A trivial TFCS can be handled by the old FEC code and the simple version MAC functions.
// It has only one TrCh and no TFC with more than one TB.
bool L1CCTrChInfo::isTrivial()
{
	// We assume there is only one TrCh.
	if (getNumTrCh() > 1) return false;
	switch (getNumTfc()) {
	case 2:
		if (getFPI(0,1)->getNumTB() > 1) { return false; }
		// Fall through to test tfc0
	case 1:
		if (getFPI(0,0)->getNumTB() > 1) { return false; }
		// Fall through.
	case 0:
		return true;	// this is a trivial TFCS
	default:
		return false;
	}
}

L1CCTrChInfo::L1CCTrChInfo() {
	for (TrChId i = 0; i < maxTrCh; i++) {
		for (TfcId j = 0; j < maxTfc; j++) {
			getFPI(i,j)->mInfoParent = this;
			getFPI(i,j)->mCCTrChIndex = i;
		}
	}
}

void L1FecProgInfo::text(std::ostream &os) const {
	os	<< LOGVARM(mTfi)<<LOGVARM(mTBSz)<<LOGVARM(mNumTB)<<LOGVARM(mCodedSz)<<LOGVARM(mCodeInBkSz)
		<<LOGVARM(mCodeFillBits)<<LOGVARM(mHighSideRMSz)<<LOGVARM(mLowSideRMSz)
		<<LOGVARM(mRFSegmentSize)<<LOGVARM(mRFSegmentOffset)
		<<LOGVAR2("PB",getPB())<<LOGVAR2("TTICode",getTTICode());
}
void L1CCTrChInfo::text(std::ostream &os) const {
	for (TrChId i = 0; i < mNumTrCh; i++) {
		for (TfcId j = 0; j < mNumTfc; j++) {
			os << "TFC("<<i<<","<<j<<"):";
			const_cast<L1CCTrChInfo*>(this)->getFPI(i,j)->text(os);	// const foo b.a.r. 
			os << "\n";
		}
	}
}

#define TESTEQL(what) (a->what == b->what) &&
void L1FecProgInfo::musteql(L1FecProgInfo *b) {
	L1FecProgInfo *a = this;
	if (
		TESTEQL(mTBSz)
		TESTEQL(mNumTB)
		TESTEQL(mCodedSz)
		TESTEQL(mCodeInBkSz)
		TESTEQL(mCodeFillBits)
		TESTEQL(mHighSideRMSz)
		TESTEQL(mLowSideRMSz)
		TESTEQL(mRFSegmentSize)
		TESTEQL(mRFSegmentOffset) 1 ) return;
	LOGDEBUG << "a:" << a->str() << endl;
	LOGDEBUG << "b:" << b->str() << endl;
	assert(0);
}

void L1CCTrChInfo::musteql(L1CCTrChInfo &other) {
	L1CCTrChInfo *a = this, *b = &other;
	assert(a->mNumTrCh == b->mNumTrCh);
	assert(a->mNumTfc == b->mNumTfc);
	for (TrChId i = 0; i < mNumTrCh; i++) {
		for (TfcId j = 0; j < mNumTfc; j++) {
			a->getFPI(i,j)->musteql(b->getFPI(i,j));
		}
	}
}

// Set the size for encoder/decoder channels.
// The size should be inited once and then never change, so we throw an assertion if it does.
static void initSize(BitVector &b, unsigned size)
{
	if (b.size() == 0) {
		b.resize(size);
	} else {
		assert(b.size() == size);
	}
}

// Set the size for encoder/decoder channels.
// The size should be inited once and then never change, so we throw an assertion if it does.
static void initSize(SoftVector &b, unsigned size)
{
	if (b.size() == 0) {
		b.resize(size);
	} else {
		assert(b.size() == size);
	}
}


// This duplicates functionality in fecComputeUlTrChSizes and fecComputeDlTrChSizes and is used for testing.
// Init the L1CCTrChInfo for a simplified full support channel: rach/fach/dch.
// For uplink: one TrCh, one TF.
// For downlink: one TrCh, one or more TB mapping to a simplified (unspecified) TFCS where each
// TFC j corresponds to j+1 TBs.  For eample, if numTB == 2, the (assumed) TFCS has two entries
// for 1x or 2x TBs.
// Return true if the result is possible without puncturing.
bool L1CCTrChInfo::fecConfigForOneTrCh(bool isDownlink, unsigned wSF,TTICodes wTTICode,unsigned wPB,
	unsigned wRadioFrameSz, unsigned wTBSz, unsigned wMinNumTB, unsigned wMaxNumTB, bool wTurbo)
{
	bool isPunctured = false;	// unused init to shut up gcc
	// CCTrCh info:
	mNumTfc = 0;
	mNumTrCh = 1;
	// TrCh info:
	getTCI(0)->mPB = wPB;
	getTCI(0)->mTTICode = wTTICode;
	getTCI(0)->mIsTurbo = wTurbo;

	int nout = wRadioFrameSz * l1GetNumRadioFrames(0);	// warning: uses mTTICode set just above.
	int nin = wTBSz+wPB;

	// TFC info, exclusive of rate-matching:
	int maxNTTIij = 0;	// Something needed for rate-matching.  Bogus init to shut up gcc.
	TfcId tfcj = 0;
	for (unsigned numTB = wMinNumTB; numTB <= wMaxNumTB; numTB++, tfcj++) {
		mNumTfc++;
		L1FecProgInfo *fpi = getFPI(0,tfcj);
		fpi->mTBSz = wTBSz;
		fpi->mNumTB = numTB;
		int totalsize =  nin * fpi->mNumTB;
		if (wTurbo) {
			fpi->mCodedSz = RrcDefs::TurboEncodedSize(totalsize,&fpi->mCodeInBkSz,&fpi->mCodeFillBits);
		} else {
			fpi->mCodedSz = RrcDefs::R2EncodedSize(totalsize,&fpi->mCodeInBkSz,&fpi->mCodeFillBits);
		}
		if (numTB == wMaxNumTB) {
			// This is the largest TF.
			maxNTTIij = fpi->mCodedSz;
		}
		// In downlink, the RFSize is a constant, so we set it to the full width even if there are no bits,
		// for debugging purposes, even though the value will not be used.
		// In uplink we set it to 0 if there are no bits.
		fpi->mRFSegmentSize = (isDownlink || numTB) ? wRadioFrameSz : 0;
		fpi->mRFSegmentOffset = 0;
		fpi->mTfi = tfcj;
		LOG(DEBUG) << format("tfcj=%u numTB=%u maxNTTIij=%u\n",tfcj,numTB,maxNTTIij);
	}
	assert((signed) l1GetLargestCodedSz(0) == maxNTTIij);

	// Now that we know the coded block sizes, we can do the rate-matching calculation:
	if (isDownlink) {
		// Compute rate matching for one TrCh.  This stuff simplies for one TrCh as follows:
		// 4.2.7:
		// equation 1 simplifies to: Zi,j = Ndata,j
		// equation 1b simplfies to: deltaNi,j = Ndata,j - Ni,j
		// Ndataj = nout;
		// 4.2.7.2.1.1
		// NiStar = 1.0/F * maxNTTIij;
		// deltaNistar = Ndataj - Nistar;	// by simplified equation 1b above.
		// deltaNimax = F * deltaNistar = F * (Ndataj - maxNTTIij/F) = F * Ndataj - maxNTTIij = nout - maxNTTIij
		int deltaNimax = nout - maxNTTIij;
		//getTCI(0)->mDlEplus = 2 * maxNTTIij;
		//getTCI(0)->mDlEminus = 2 * deltaNimax * (deltaNimax<0 ? -1 : 1);
		// Lets double check:
		//if (wMaxTB == 1) {
		//	assert(getTCI(0)->mDlEplus == 2 * nin);
		//	assert(getTCI(0)->mDlEminus == 2 * ((int)nout - (int)nin));
		//}
		// Fill in the rate match info for each TFC:
		for (TfcId tfcj = 0; tfcj < mNumTfc; tfcj++) {
			L1FecProgInfo *fpi = getFPI(0,tfcj);
			fpi->mHighSideRMSz = fpi->mCodedSz;
			// This is the last equation in 4.2.7.2.1.3
			// The rate-matching params are fixed by the largest TF, so for other TFs we have to see
			// how many bits pop out, and we will use DRX bit insertion to pad it out to the largest TF.
			int deltaNTTIij = ceil((fabs(deltaNimax) * fpi->mHighSideRMSz) / maxNTTIij);
			isPunctured = deltaNimax < 0;	// No puncturing please.
			fpi->mLowSideRMSz = fpi->mHighSideRMSz + (isPunctured ? (- deltaNTTIij) : deltaNTTIij);
			// double-check:
			if (tfcj == mNumTfc-1) {assert((int)fpi->mLowSideRMSz == nout);}
		}
	} else {	// uplink
		assert(wMaxNumTB == 1); // Way too complicated for multiple TF; use fecComputeUlTrChSizes().
		assert(nin % l1GetNumRadioFrames(0) == 0);
		for (TfcId tfcj = 0; tfcj < mNumTfc; tfcj++) {
			// The uplink rate-matching is per-radio-frame not per-TTI.
			L1FecProgInfo *fpi = getFPI(0,tfcj);
			fpi->mHighSideRMSz = fpi->mCodedSz / l1GetNumRadioFrames(0);;
			fpi->mLowSideRMSz = fpi->mRFSegmentSize;
			if (fpi->getNumTB() == 1) {
				isPunctured = fpi->mHighSideRMSz > fpi->mLowSideRMSz;
			}
		}
	}
	return !isPunctured;
}

// This duplicates functionality in fecConfigForOneTrCh and is used for testing.
// Program for super simple channels like BCH: one TrCh, one TF, no rate matching.
void L1CCTrChInfo::fecConfigTrivial(
	unsigned wSF,TTICodes wTTICode,unsigned wPB, unsigned wRadioFrameSz)
{
	// CCTrCh info:
	mNumTfc = 1;
	mNumTrCh = 1;
	// TrCh info:
	getTCI(0)->mPB = wPB;
	getTCI(0)->mTTICode = wTTICode;
	getTCI(0)->mIsTurbo = false;		// Trivial fec is never turbo.
	// TFC info:
	L1FecProgInfo *fpi = getFPI(0,0);
	// Back-compute the TB size that will just fit in the TTI.
	unsigned nout = wRadioFrameSz * l1GetNumRadioFrames(0);
	unsigned tbsz = RrcDefs::R2DecodedSize(nout) - wPB;
	unsigned check = RrcDefs::R2EncodedSize(tbsz + wPB, &fpi->mCodeInBkSz,&fpi->mCodeFillBits);
	assert(check == nout && fpi->mCodeInBkSz == tbsz+wPB && fpi->mCodeFillBits == 0);
	fpi->mNumTB = 1;
	fpi->mCodedSz = nout;
	fpi->mTBSz = tbsz;
	fpi->mRFSegmentSize = wRadioFrameSz;
	fpi->mRFSegmentOffset = 0;
	// No rate matching needed.
	fpi->mHighSideRMSz = nout;
	fpi->mLowSideRMSz = nout;
	fpi->mTfi = 0;
	fpi->mCCTrChIndex = 0;

	assert(wPB == 16);	// Required for the simple channels.
}

// Equation 1 is completely different for uplink and downlink.
// Uplink version of equation 1 is verbatim.
static void ulEquation1(
	UlTrChList *ul,
	int Ndataj[RrcDefs::maxTfc], 			// input, computed from SET1
	L1CCTrChInfo *result,		// input, for the number of num bits in each TrCh.
	int deltaNij[RrcDefs::maxTrCh][RrcDefs::maxTfc])	// output
{
	// These variables defined in 25.212 4.2.7:

	// Calculate RMm x Nm,j which is the first term in the numerator
	// and also used in denominator of eqn 1.
	RrcTfcs *ulTfcs = ul->getTfcs();
	unsigned numTrCh = ul->getNumTrCh();
	int ulSumRMmxNmj[RrcDefs::maxTrCh][RrcDefs::maxTfc];	// This term is used in numerator and denominator of eqn 1.
	for (TrChId trchm = 0; trchm < numTrCh; trchm++) {
		RrcTfs *tfs = ul->getTfs(trchm);
		for (TfcId tfcj = 0; tfcj < ulTfcs->getNumTfc(); tfcj++) {
			int Fm = result->l1GetNumRadioFrames(trchm);
			int Nmj = result->getFPI(trchm,tfcj)->mCodedSz / Fm;
			int RMm = tfs->getRM();
			ulSumRMmxNmj[trchm][tfcj] = (trchm ? ulSumRMmxNmj[trchm-1][tfcj] : 0) + RMm * Nmj;
		}
	}

	int Zij[RrcDefs::maxTrCh][RrcDefs::maxTfc];
	// Calculate Zij and deltaNij for each TrCh i as per equation 1.
	for (TrChId trchi = 0; trchi < numTrCh; trchi++) {
		for (TfcId tfcj = 0; tfcj < ulTfcs->getNumTfc(); tfcj++) {
			int ulSumFull = ulSumRMmxNmj[numTrCh-1][tfcj];
			Zij[trchi][tfcj] = ulSumFull ? floor(((double)ulSumRMmxNmj[trchi][tfcj] * Ndataj[tfcj]) / ulSumFull) : 0;
			//printf("%u %u %f\n",ulSumRMmxNmj[trchi][tfcj]* Ndataj[tfcj], ulSumRMmxNmj[numTrCh-1][tfcj], ((double)ulSumRMmxNmj[trchi][tfcj] * Ndataj[tfcj]) / ulSumRMmxNmj[numTrCh-1][tfcj]);
			int Fi = result->l1GetNumRadioFrames(trchi);
			int Nij = result->getFPI(trchi,tfcj)->mCodedSz / Fi;
			deltaNij[trchi][tfcj] = Zij[trchi][tfcj] - (trchi ? Zij[trchi-1][tfcj] : 0) - Nij;
			LOGFECINFO << format("ulEquation1[%u,%u]: ulSum=%u Ndataj=%u Zij=%u Fi=%u Nij=%u deltaNij=%u\n",trchi,tfcj,
				ulSumRMmxNmj[trchi][tfcj],Ndataj[tfcj],Zij[trchi][tfcj],Fi,Nij,deltaNij[trchi][tfcj]);
		}
	}
}

// Downlink version of equation 1 is verbatim but substituting Ni,* for Nm,j and Ndata,j is a single
// constant, because those variables do not vary with the TFC j.
static void dlEquation1(
	DlTrChList *dl,
	int Ndataj,							// number of bits in one output radio frame; depends on PhCh and SF.
	double Nistar[RrcDefs::maxTrCh],	// input
	double deltaNij[RrcDefs::maxTrCh])	// output
{
	int numTrCh = dl->getNumTrCh();
	// These variables defined in 25.212 4.2.7:

	// Calculate RMm x Nm,j which is the first term in the numerator and also used in denominator of eqn 1.
	double dlSumRMmxNmj[RrcDefs::maxTrCh];	// This term is used in numerator and denominator of eqn 1.
	for (int trchm = 0; trchm < numTrCh; trchm++) {
		//RrcTfs *tfs = dl->getTfs(trchm);
		double Nmj = Nistar[trchm];
		int RMm = dl->getRM(trchm); 
		dlSumRMmxNmj[trchm] = (trchm ? dlSumRMmxNmj[trchm-1] : 0) + RMm * Nmj;
	}


	// Calculate Zij and deltaNij for each TrCh i as per equation 1.
	int Zij[RrcDefs::maxTrCh];	// Invariant over all TF or TFC j for TrCh i, so j is not a subscript.
	for (int trchi = 0; trchi < numTrCh; trchi++) {
		// For downlink, the j in eqn 1 does not matter because neither Nmj nor Ndataj vary based on j.
		Zij[trchi] = floor(((double)dlSumRMmxNmj[trchi] * Ndataj) / dlSumRMmxNmj[numTrCh-1]);
		double Nij = Nistar[trchi];
		deltaNij[trchi] = Zij[trchi] - (trchi ? Zij[trchi-1] : 0) - Nij;
		LOGFECINFO << format("dlEquation1 TrCh %u Ndataj=%u Nistar=Nij=%g dlSum=%g Zij=%u deltaNij=%g\n",
			trchi,Ndataj, Nistar[trchi],dlSumRMmxNmj[trchi],Zij[trchi],deltaNij[trchi]);
	}
}

#if 0 // changed to fecComputeCommon
static int computeCodedSize(RrcTfs *tfs, int numTB, int tbSz, int paritybits)
{
	int totalsize = numTB * (paritybits + TBSz);
	switch (ssp->mTypeOfChannelCoding) {
	case Convolutional:
		// Dont even check the rate - we only support 1/2.
		return R2EncodedSize(totalsize);
		break;
	case Turbo:
		return TurboEncodedSize(totalsize);
		break;
	default: assert(0);
	}
}
#endif

// TODO: Need to check the UE capabilities:
// TODO: channel type == non-RACH is hard coded here; the difference is just that RACH can not go as fast
// ans so the SET0 and SET1 are limited to SF32.
// I quote: "For RACH if Ndata,j is not part of UE capabilities TFC j cannot be used.
static void fecComputeUlNdataj(UlTrChList *ul,
	bool isRach,	// RACH handled different from DCH.
	L1CCTrChInfo *result,
	int Ndataj[RrcDefs::maxTfc],
	int SF[RrcDefs::maxTfc])		// The output result.
{
	const int maxSF = 7;
	// RACH supports the first four uplink spreading factors.
	// Other channels support all 7 possible uplink spreading factors, but limited by the UE capabilities.
	int numSF = isRach ? 4 : maxSF;
	static int ulSF[maxSF] = { 256, 128, 64, 32, 16, 8, 4 };
	static int ulSET0[maxSF];
	for (int sfi = 0; sfi < maxSF; sfi++) {
		ulSET0[sfi] = gFrameLen / ulSF[sfi];
	}

	int ulNumTrCh = ul->getNumTrCh();

	// The min(l<=y<=I){RMy} term in the SET1 equation is a just constant.  Figure it out.
	int minRM = 9999999;
	for (int tc = 0; tc < ulNumTrCh; tc++) {
		int thisRM = ul->getTfs(tc)->getRM();
		if (thisRM < minRM) { minRM = thisRM; }
	}
	assert(minRM != 0);

	// In equation for SET1, x is the TrCh and j is the TFC.
	RrcTfcs *ulTfcs = ul->getTfcs();
	for (TfcId j = 0; j < ulTfcs->mNumTfc; j++) {
		//RrcTfc *tfc = ulTfcs->getTfc(j);
		int sumRMxNxj = 0;
		for (int trchx = 0; trchx < ulNumTrCh; trchx++) {	// I in equation is ulNumTrCh.
			//RrcTf *tf = tfc->getTf(trchx);
			int nf = ul->getTTINumFrames(trchx);
			int Nxj = result->getFPI(trchx,j)->mCodedSz/nf;		
			//int Nxj = tf->getTfTotalSize() / nf;
			unsigned RMx = ul->getTfs(trchx)->getRM();
			LOGFECINFO<< format("trchx: %d %d %d %u\n",trchx,nf,Nxj,RMx);
			sumRMxNxj += RMx * Nxj;
		}

		// Want Ndata in SET0 such that minRMyNdata - sumRMxNxj is non-negative.
		// We only want the minimum value in SET1, which is NDataj, so we dont keep the rest of SET1, only Ndataj.
		// In uplink, the Ndata for spreading factor sf = gFrameLen / sf;
		Ndataj[j] = 0;	// Impossible value, used to detect overflow.
		for (int sfi = 0; sfi < numSF; sfi++) {
			int Ndata = ulSET0[sfi];
			LOGFECINFO<<format("sfi: %d, %d %d %d\n",sfi, minRM, Ndata, sumRMxNxj);
			if (minRM * Ndata >= sumRMxNxj) {
				// "If SET1 is not empty and the smallest element of SET1 requires just one PhCh then:"
				// "Ndata,j = min SET1"
				// And we are not worrying about the multiple PhCh case.
				Ndataj[j] = Ndata;
				SF[j] = ulSF[sfi];
				break;
			}
		}
		// Check for overflow.
		if (Ndataj[j] == 0) {
			// And now what?
			assert(0);
		}
	}
}


static void fecComputeCommon(
	TrChList *trchlist,		// Either UlTrChList or DlTrChList
	L1CCTrChInfo *result)	// Fills in PB, TTI, numTB, TBSz and codedSz and codedBkSz.
{
	//result->mChList = trchlist;
	RrcTfcs *tfcs = trchlist->getTfcs();
	unsigned numTrCh = trchlist->getNumTrCh();
	result->mNumTfc = tfcs->getNumTfc();
	//std::cout << format("trchlist=%p tfcs=%p numTfc=%d\n",trchlist,tfcs,result->mNumTfc);
	result->mNumTrCh = numTrCh;
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		RrcTfs *tfs = trchlist->getTfs(tcid);
		result->getTCI(tcid)->mPB = tfs->getPB();
		result->getTCI(tcid)->mTTICode = tfs->getTTICode();
		result->getTCI(tcid)->mIsTurbo = tfs->getTurboFlag();
		RrcSemiStaticTFInfo *ssp = tfs->getSemiStatic();
		int tfci = 0;
		for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++, tfci++) {
			L1FecProgInfo *fpi = result->getFPI(tcid,tfci);
			RrcTf *tf = tfc->getTf(tcid);
			fpi->mNumTB = tf->getNumTB();
			fpi->mTBSz = tf->getTBSize();
			int totalsize = fpi->mNumTB * (fpi->mTBSz + fpi->getPB());
			//result->getFPI(tcid,tfci)->mCodedSz = computeCodedSize(tfs,numTB,TBSz,paritybits);
			switch (ssp->mTypeOfChannelCoding) {
			case RrcDefs::Convolutional:
				// Dont even check the rate - we only support 1/2.
				//LOGDEBUG  <<format("here, tfs->pb=%u pb=%u\n",tfs->getPB(),fpi->getPB());
				fpi->mCodedSz = RrcDefs::R2EncodedSize(totalsize,&fpi->mCodeInBkSz,&fpi->mCodeFillBits);
				break;
			case RrcDefs::Turbo:
				fpi->mCodedSz = RrcDefs::TurboEncodedSize(totalsize,&fpi->mCodeInBkSz,&fpi->mCodeFillBits);
				break;
			default: assert(0);
			}
			LOGFECINFO<<format("fecComputeCommon(%d,%d) TB=%ux%u coded=%u\n",tcid,tfci,fpi->mNumTB,fpi->mTBSz,fpi->mCodedSz);
		}
	}
}



static void fecComputeUlTrChSizes(UlTrChList *ul, L1CCTrChInfo *result, bool isDch=true)
{
	// We simplify by never using multiple PhCh.  Multiple PhCh would be needed
	// for higher bandwidth in the uplink direction which is not one of our requirements.

	// Copy parameters from UURC.
	// Compute mCodedSz.

	fecComputeCommon(ul,result);
#if 0
	int ulNumTrCh = ul()->getNumTrCh();
	RrcTfcs *ulTfcs = ul()->getTfcs();
	int tfci = 0;
	for (TrChId tcid = 0; tcid < ulNumTrCh; tcid++) {
		RrcTfs *tfs = ul()->getTfs
		result->getTCI(tcid)->mPB = tfs->getPB();
		result->getTCI(tcid)->mTTICode = tfs->getTTICode();
		result->getTCI(tcid)->mIsTurbo = tfs->getTurboFlag();
		RrcSemiStaticTFInfo *ssp = tfs->getSemiStatic();
		for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++, tfci++) {
			RrcTf *tf = tfc->getTf(tcid);
			int numTB = result->getFPI(tcid,tfci)->mNumTB = tf->getNumTB();
			int TBSz = result->getFPI(tcid,tfci)->mTBSz = tf->getTBSize();
			result->getFPI(tcid,tfci)->mCodedSz = computeCodedSize(tfs,numTB,TBSz,result->getTCI(tcid)->mPB);
		}
	}
#endif

	// 4.2.4 Radio Frame Equalisation, verbatim. Also see 4.2 figure 1.  Used only in uplink.
	unsigned ulNumTrCh = ul->getNumTrCh();
	RrcTfcs *ulTfcs = ul->getTfcs();
	for (TrChId tcid = 0; tcid < ulNumTrCh; tcid++) {
		for (TfcId tfci = 0; tfci != ulTfcs->getNumTfc(); tfci++) {
			L1FecProgInfo *fpi = result->getFPI(tcid,tfci);
			int Ei = fpi->mCodedSz;
			int Fi = ul->getTTINumFrames(tcid);
			int Ni = (Ei + Fi - 1) / Fi;
			//int Ti = Fi * Ni;
			// Post radio frame equalisation goes into the interleaver, then radio-frame-segmentation, then rate-matching.
			fpi->mHighSideRMSz = Ni;
			fpi->mTfi = tfci;		// This is just 1-to-1 in uplink.
		}
	}

	// 4.2.7.1.1 Determination of SF (and number of PhCh) needed for uplink.
	int Ndataj[RrcDefs::maxTfc];		// number of bits in radio-frame.
	int SF[RrcDefs::maxTfc];
	fecComputeUlNdataj(ul,!isDch,result,Ndataj,SF);

	// 4.2.7: Compute equation 1.
	int ulDeltaNij[RrcDefs::maxTrCh][RrcDefs::maxTfc];
	ulEquation1(ul,Ndataj,result, ulDeltaNij);

	// Copy back out to the result, and pre-compute rate-matching eini parameters.
	int rfoffset = 0;
	for (TrChId tcid = 0; tcid < ulNumTrCh; tcid++) {
		for (TfcId tfcj = 0; tfcj != ulTfcs->getNumTfc(); tfcj++) {
			L1FecProgInfo *fpi = result->getFPI(tcid,tfcj);
			fpi->mLowSideRMSz = fpi->mHighSideRMSz + ulDeltaNij[tcid][tfcj];
			fpi->mRFSegmentSize = fpi->mLowSideRMSz;	// Number of bits in radio frame for this TrCh.
			fpi->mRFSegmentOffset = rfoffset;
			rfoffset += fpi->mRFSegmentSize;
			fpi->mSFLog2 = round(log2(SF[tfcj]));
		}
	}
}

unsigned L1CCTrChInfo::l1GetLargestCodedSz(TrChId tcid)
{
	unsigned maxCodedSz = 0;
	for (TfcId tfci = 0; tfci != mNumTfc; tfci++) {
		L1FecProgInfo *fpi = getFPI(tcid,tfci);
		if (fpi->mCodedSz > maxCodedSz) {
			maxCodedSz = fpi->mCodedSz;
		}
	}
	return maxCodedSz;
}


// Downlink and uplink calculations are completely different because 1. uplink SF
// is a variable that is chosen as part of the calculation; 2. downlink data
// is expanded to fit the largest TF while uplink data is always minimized individually for each TFC.

// Figure out the number of bits send on the radio frame for each TrCh in the CCTrCh.
static void fecComputeDlTrChSizes(DlTrChList *dl,unsigned dlRadioFrameSize,L1CCTrChInfo *result)
{
	// Convolutional coding:
	// Lets compute the number of bits of the radio frame that will be allocated
	// to each TrCh for each TFC. See 25.212 4.2.7


	// ================= DOWNLINK ======================
	// The downlink parameters for each TrCh vary only for the TF in the TFS for that TrCh, they do not
	// differ among different TFC in the TFCS that use the same TF for a TrCh, but to make things easy
	// we are going to store the info per TFC, which means entries for TrCh using the same TF are duplicated.
	
	// Copy parameters from UURC.
	// Compute mCodedSz.
	int maxNTTIil[RrcDefs::maxTrCh] = {0,0,0,0};
#if 0
	int dlNumTrCh = dl()->getNumTrCh();
	RrcTfcs *dlTfcs = dl()->getTfcs();
	for (TrChId tcid = 0; tcid < dlNumTrCh; tcid++) {
		RrcTfs *tfs = dl()->getTfs
		result->getTCI(tcid)->mPB = tfs->getPB();
		result->getTCI(tcid)->mTTICode = tfs->getTTICode();
		result->getTCI(tcid)->mIsTurbo = tfs->getTurboFlag();
		RrcSemiStaticTFInfo *ssp = tfs->getSemiStatic();
		int tfci = 0;
		for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++, tfci++) {
			RrcTf *tf = tfc->getTf(tcid);
			int numTB = result->getFPI(tcid,tfci)->mNumTB = tf->getNumTB();
			int TBSz = result->getFPI(tcid,tfci)->mTBSz = tf->getTBSize();
			int codedSz = computeCodedSize(tfs,numTB,TBSz,result->getTCI(tcid)->mPB);
			result->getFPI(tcid,tfci)->mCodedSz = codedSz;
			result->getFPI(tcid,tfci)->mHighSideRMSz = codedSz;	// Same size used for rate-matching input.
			maxNTTIil[tcid] = max(maxNTTIil[tcid],codedSz);
		}
	}
#endif
	fecComputeCommon(dl,result);

	// Fill in RM high-side size, and save the max value over all TFC as MaxNTTIil per TrCh.
	// In downlink, RM high side equals the coded size of all concatenated blocks.
	unsigned numTrCh = dl->getNumTrCh();
	RrcTfcs *dlTfcs = dl->getTfcs();
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		maxNTTIil[tcid] = result->l1GetLargestCodedSz(tcid);
#if 0
		maxNTTIil[tcid] = 0;
		for (TrChId tfci = 0; tfci != dlTfcs->getNumTfc(); tfci++) {
			L1FecProgInfo *fpi = result->getFPI(tcif,tfcj);
			if (fpi->mCodedSz > maxNTTIil[tcid]) {
				result->getTCI(tcid)->?? = tcid;
				maxNTTIil[tcid] = fpi->mCodedSz;
			}
		}
#endif
	}

	// Duplicate info from the TF in the TFS into the TFC in the TFCS.
	//for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++, tfci++) {
	//	for (TrChId tcid = 0; tcid < dlNumTrCh; tcid++) {
	//		result[tcid]->perTFC[tfci] = perTFS[tcid][tfc->getTf(tcid)->mTfi];
	//	}
	//}

	// Now the stupendously complex rate matching.

	// 25.212 4.2.7.2.1  Compute Ni,*
	// "First an intermediate calculation variable "Ni,*" is calculated for all transport channels i:"
	// Note that Ni,* is not an integer, but a multiple of 1/8 [or 1/TTINumFrames].
	double Nistar[RrcDefs::maxTrCh]; // For downlink: intermediate variable with a step of 1/8.
	for (TrChId trchi = 0; trchi < numTrCh; trchi++) {
		RrcTfs *tfs = dl->getTfs(trchi);
		int Fi = tfs->getTTINumFrames();
		Nistar[trchi] = ((double)maxNTTIil[trchi]/Fi);
	}

	// Compute deltaNi,* from Ni,* for each TrCh i using 4.2.7 equation 1.
	double deltaNistar[RrcDefs::maxTfc];
	dlEquation1(dl,dlRadioFrameSize,Nistar,deltaNistar);

	// Compute delta Ni,max from Ni,star using 4.2.7 equation 1.
	// (pat) we ran equation one on radio frames, but we want bits per TTI, so now we have to compute back to TTIs.
	int deltaNimax[RrcDefs::maxTrCh];
	int rfsegmentsize[RrcDefs::maxTrCh];
	for (TrChId trchi = 0; trchi < numTrCh; trchi++) {
		int Fi = dl->getTTINumFrames(trchi);
		// This converts deltaNistar from double (with step 1/F) back to an integral value.
		deltaNimax[trchi] = round(Fi * deltaNistar[trchi]);
		// The RF segment is determined by the post-rate-matched size of the largest TF.
		rfsegmentsize[trchi] = maxNTTIil[trchi] + deltaNimax[trchi];
		LOGFECINFO << format("fecComputeDlTrChSizes TrCh %u Nistar=%g deltaNistar=%g deltaNimax=%u RFseg=%u\n",
				trchi,Nistar[trchi],deltaNistar[trchi],deltaNimax[trchi],rfsegmentsize[trchi]);
		// In downlink the output size from rate matching varies for each TF, but NOT for each TFC.
		// Note, these equations are equivalent to: eplus = nin; eminus = 2 * abs(nout-nin);
		// where nin and nout are for the largest TF on this TrCh.
		// Moved to encoder.
		//result->getTCI(trchi)->mDlEplus = 2.0 * maxNTTIil[trchi];
		//result->getTCI(trchi)->mDlEminus  = 2.0 * RN_ABS(deltaNimax[trchi]);
	}

	// 4.2.7.2.1.3 Determination of the rate matching parameters (which are for the whole TTI, not per radio frame).
	// (pat) The documentation is elucidated by recognizing that for downlink TrCh we always ship the
	// same size vector equal to the largest TF for each TrCh, so eplus and eminus are computed from the largest TF
	// for this TrCh.
	// Astonishingly, you use the same rate-matching eplus and eminus for all TF regardless of the number of bits in the frame.
	int rfoffset = 0;
	RrcTfcs *tfcs = dl->getTfcs();
	for (TrChId trchi = 0; trchi < numTrCh; trchi++) {
		for (TfcId tfcj = 0; tfcj < dlTfcs->getNumTfc(); tfcj++) {
			L1FecProgInfo *fpi = result->getFPI(trchi,tfcj);
			int deltaNi = deltaNimax[trchi];
			int Xi = fpi->mCodedSz;
			// deltaNTTIl is number of bits repeated/punctured each TTI.
			int deltaNTTIl = maxNTTIil[trchi] ? (int) ceil((double)RN_ABS(deltaNi) * Xi / maxNTTIil[trchi]) : 0;
			LOGFECINFO << format("fecComputeDlTrChSizes(%u,%u) deltaNi=%d coded=%u deltaNTTIl=%d maxNTTIil=%d\n",trchi,tfcj,deltaNi,fpi->mCodedSz,deltaNTTIl,maxNTTIil[trchi]);
			if (deltaNi < 0) { deltaNTTIl = - deltaNTTIl; } // above * sgn(delta Ni)
			fpi->mHighSideRMSz = fpi->mCodedSz;	// Same size used for rate-matching input.
			fpi->mLowSideRMSz = (int) fpi->mHighSideRMSz + deltaNTTIl;

			// In downlink these are the same for all TFC.
			fpi->mRFSegmentSize = rfsegmentsize[trchi];	// Number of bits in radio frame for this TrCh.
			fpi->mRFSegmentOffset = rfoffset;

			// Set the TFI for each TFC.
			RrcTfc *tfc = tfcs->getTfc(tfcj);
			fpi->mTfi = tfc->getTfIndex(trchi);
		}
		rfoffset += rfsegmentsize[trchi];
	}
} // fecComputeDlTrChSizes


L1TrChEncoder::L1TrChEncoder(L1CCTrCh *wParent, L1FecProgInfo *wfpi):
	mParent(wParent)
	/*, mFpi(wfpi)*/
{
	int nin = wfpi->mInfoParent->l1GetLargestCodedSz(wfpi->mCCTrChIndex);
	int nout = wfpi->mRFSegmentSize;
	rateMatchComputeEplus(nin, nout, &mDlEplus, &mDlEminus);
}

L1TrChDecoder::L1TrChDecoder(L1CCTrCh* wParent,L1FecProgInfo *wfpi):
			mParent(wParent),mUpstream(NULL)
{
	//unsigned frameSize = gFrameLen / getSF();
	unsigned nrf = wfpi->getNumRadioFrames();// number of radio frames per tti
	//mDTtiBuf = new SoftVector(mRadioFrameSz * nrf);
	// mRMBuf and mDTtiBuf are after rate-matching:
	initSize(mRMBuf,wfpi->mHighSideRMSz);	
	initSize(mDTtiBuf,wfpi->mHighSideRMSz * nrf);
	mDTtiIndex = 0;

	//rateMatchComputeEini(wfpi->mCodedBkSz/nrf,mRadioFrameSz,wfpi->getTTICode(),mEini);
	rateMatchComputeUlEini(wfpi->mHighSideRMSz,wfpi->mLowSideRMSz,wfpi->getTTICode(),mEini);
}


bool L1ControlInterface::recyclable() const
{
	ScopedLock lock(mLock);
	if (mAssignmentTimer.expired()) return true;
	if (mReleaseTimer.expired()) return true;
	return false;
}

void L1ControlInterface::controlOpen()
{
	ScopedLock lock(mLock);
	mActive = true;
	mAssignmentTimer.set();
	mReleaseTimer.reset();
}

void L1ControlInterface::controlClose(bool hardRelease)	// TODO: something with hard release.
{
	ScopedLock lock(mLock);
	mActive = false;
	mReleaseTimer.set();
}

void L1ControlInterface::countBadFrame()
{
	L1FER::countBadFrame();
}

void L1ControlInterface::countGoodFrame()
{
	ScopedLock lock(mLock);
	L1FER::countGoodFrame();
	mAssignmentTimer.reset();	// TODO: Should only reset if all TrCh are good?
}


void L1CCTrChDownlink::l1DownlinkOpen()
{
	mTotalBursts = 0;
	mPrevWriteTime = 0;
	mNextWriteTime = gNodeB.clock().get();
}


//void L1TrCHFECDecoder::close(bool hardRelease)
//{
//	mActive = false;
//}

//void L1CCTrCh::sendFrame(BitVector& frame, unsigned tfci)
//{
//	//TODO: This test now fails, correctly, due to rate matching.
//	//assert(frame.size()%gFrameSlots == 0);
//
//	// 25.212 4.2.7 Rate Matching.
//	// The RRC insures that codedBkSz < mRadioFrameSz, ie, we will never use puncturing, ever,
//	// for any TF [Transport Format] and therefore there is no rate-matching in downlink;
//	// DTX is used instead.
//	/*unsigned insize = this->mCodeInBkSz / this->getNumRadioFrames();
//	unsigned outsize = this->getRadioFrameSz();
//	assert(frame.size() == insize);
//	assert(insize <= outsize);	// If you get this error, go fix RRC; dont mess around here.
//	*/
//
//	// This has been moved in UMTSL1FEC.cpp
//	// (pat) I had it in the wrong place.
//	/*****
//	unsigned outsize = this->mRadioFrameSz;
//	unsigned insize = this->mCodeInBkSz/this->getNumRadioFrames();
//	// (pat) 6-19-2012: Re-added this assertion, which insures no puncturing:
//	assert(insize <= outsize);	// If you get this error, go fix RRC; dont mess around here.
//	//assert(mHDIBuf->size() == insize);
//	
//	if (insize != outsize) {
//		rateMatchFunc<char>(frame,this->mResultFrame,1);
//	        //LOG(INFO) << "SCCPCH: " << insize << " " << outsize;
//	}
//	else {
//		frame.copyTo(this->mResultFrame);
//	}
//	***********/
//}

// 25.212 4.2.8 TrCh Multiplexing.
// Incoming frame is the result of 25.212 4.2.6 Radio Frame Segmentation.
// Meaning that it is sent one radio frames worth of data on just one TrCh.
void L1CCTrChDownlink::l1Multiplexer(L1FecProgInfo *fpi, BitVector& frame, unsigned intraTTIFrameNum)
{
	// TODO: the multiplexor must work over all the radio frames in the TTI.
	// We have to gather up an entire TTI before we can send any.
	// This goes back to the MAC which may have to deal with different TrCh running at different rates?
	//int offset = timestamp.FN() - mNextWriteTime;
	//if (offset < 0) {
	//	LOG(ERR) << "Stale burst in multiplexer";
	//	return;
	//}

	// Gather up data from each trch, then send downward.
	assert(frame.size() == fpi->mRFSegmentSize);
	initSize(mMultiplexerBuf[intraTTIFrameNum],fpi->mRFSegmentSize);
	LOG_DOWNLINK << "l1Multplexer"<<LOGVAR2("RFSegSize",fpi->mRFSegmentSize)<<LOGVAR2("RFSegOff",fpi->mRFSegmentOffset)<<LOGVAR2("FN",intraTTIFrameNum);
	frame.copyToSegment(mMultiplexerBuf[intraTTIFrameNum],fpi->mRFSegmentOffset);
}


void L1CCTrChDownlink::l1SendFrame2(BitVector& frame, unsigned tfci)
{
	// 25.212 4.2.9 Insertion of Discontinuous Transmission (DTX) Indicators.
	// (pat) I believe the second insertion of DTX is only necessary if you are using
	// multiple PhCh, and we are not.

	BitVector h = frame.alias();	// You must NOT say h(frame) because it takes possession.
	//LOG(DEBUG) << "here:" << h.str();

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
	initSize(mYoutBuf,Ysize);
	if (padding == 0) {
		h.interleavingNP(C2, TrCHConsts::inter2Perm, mYoutBuf);
	} else {
		// Must pre-pad and post-strip bits.
		// The post-interleave pad bits are spread all over; easiest way to get rid
		// of them is to use a special marker value for the padding.
		const char padval = 4;	// We will pad with padbits set to this value.
		initSize(mYinBuf,Ysize);	// Temporary buffer
		h.copyTo(mYinBuf);
		memset(mYinBuf.begin()+hsize,padval,padding);	// Add the padding.
		mYinBuf.interleavingNP(C2, TrCHConsts::inter2Perm, mYoutBuf);
		// Strip out the padding bits.
		char *yp = mYoutBuf.begin(), *yend = mYoutBuf.end();
		for (char *cp = yp; cp < yend; cp++) {
			if (*cp != padval) { *yp++ = *cp; }
		}
		assert(yp == mYinBuf.begin() + hsize);
	}

	BitVector U(mYoutBuf.head(hsize));

	//if (gFecTestMode == 2) {
	//	gNodeB.mRachFec->decoder()->writeLowSide2(U);
	//	return;
	//}

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
	//assert(mDownstream);
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
				phch->getDlSF(), phch->getSpCode(), mNextWriteTime.slot(i),true
			);
			RN_MEMLOG(TxBitsBurst,out);
			phch->getRadio()->writeHighSide(out);
		}
	} else {

		int radioFrameSize = 2*(gFrameLen / phch->getDlSF());
		//BitVector radioFrame(radioFrameSize);
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

		initSize(mRadioSlotBuf,radioSlotSize);
		for (unsigned s=0; s<gFrameSlots; s++) {
			const unsigned dataStart = s*dataSlotSize;
			size_t wp = 0;

			// 25.211 4.3.5.1: Mapping of TFCI word in normal [non-compressed] mode:
			// The 32 TFCI code bits are stuck in the slots LSB first,
			// wrapping around to be reused as much as needed.
			switch (chType) {
			case SCCPCHType:
				//cout << "TFCI: " << tfci << " code: " << (tfciCode&tfciMask);
				// The SCCPCH radio slot contains: | TFCI | Data | Pilot |
				mRadioSlotBuf.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
				U.segment(dataStart,ndata1).copyToSegment(mRadioSlotBuf,wp,ndata1);
				wp += ndata1;
				mRadioSlotBuf.fillField(wp, TrCHConsts::sDlPilotBitPattern[pi][s], npilot);
				// There is no data2 for SCCPCH.
				break;
			case DPDCHType:
				// The DCH radio slot contains: 
				// Release 4 or later:  | Data1 |  TPC  | TFCI | Data2 | Pilot |
				// Release 99 or 3:     | TFCI  | Data1 | TPC  | Data2 | Pilot |
#ifdef RELEASE99 // defined in UMTSPhCh.cpp
                                mRadioSlotBuf.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
#endif
				if (ndata1 > 0) {
                                	U.segment(dataStart,ndata1).copyToSegment(mRadioSlotBuf,wp,ndata1);
                                	wp += ndata1;
				}
				// Lower layers are going to fill in TPC, we hope.
				// Toggle tpc bits between all ones and all zeros to keep phone at same tx power
			        mRadioSlotBuf.fill(0x7f,wp,ntpc); wp += ntpc;
#ifndef RELEASE99
				mRadioSlotBuf.writeFieldReversed(wp,tfciCode&tfciMask,ntfci);
#endif
				if (ndata2 > 0) {
					U.segment(dataStart+ndata1,ndata2).copyToSegment(mRadioSlotBuf,wp,ndata2);
					wp += ndata2;
				}
				mRadioSlotBuf.fillField(wp, TrCHConsts::sDlPilotBitPattern[pi][s], npilot);
				break;
			default: assert(0);
			}

			// rotate tfciCode by ntfci bits.  It is unsigned, which helps.
			tfciCode = (tfciCode>>ntfci) | (tfciCode<<(32-ntfci));

			TxBitsBurst *out = new TxBitsBurst(mRadioSlotBuf, phch->getDlSF(), phch->getSpCode(), mNextWriteTime.slot(s));
			if (chType==DPDCHType) out->DCH(true);
			RN_MEMLOG(TxBitsBurst,out);
			phch->getRadio()->writeHighSide(out);
		}
	}
	mPrevWriteTime = mNextWriteTime;
	++mNextWriteTime;
}


extern void getParity(const BitVector &in, BitVector &parity);
#if SAVEME
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
#endif

// Downlink entry function to L1 from MAC.
// Simplified version for BCH and maybe FACH.  Send just one TB.
void L1CCTrChDownlink::l1WriteHighSide(const TransportBlock &tb)
{
	assert(getNumTrCh() == 1);
	assert(isTrivial()); //getNumTfc() <= 1);
	int tfci = getNumTfc()-1;
	L1FecProgInfo *fpi = getFPI(0,tfci);
	assert(fpi->getNumTB() == 1);
	TransportBlock const *blocklist[1];	// It is not a very interesting list in this case.
	blocklist[0] = &tb;
        if (tb.scheduled()) mNextWriteTime = tb.time();
	this->mEncoders[0][tfci]->l1CrcAndTBConcatenation(fpi,blocklist);
	// Now send the result to the radio.
	// This function is called only for TrCh with no TFC, so it is TFC 0
	//LOG(INFO) << "Pushing BCH: " << tb << " at time " << mNextWriteTime;
	l1PushRadioFrames(tfci);
}

// Downlink entry function to L1 from MAC.
// Distribute the blocks in TBS out to the TrCh encoders.
void L1CCTrChDownlink::l1WriteHighSide(const MacTbs &tbs)
{
	TransportBlock const *blocks[RrcDefs::maxTbPerTrCh];
	RrcTfc *tfc = tbs.mTfc; // The TFC selected by MAC.
	unsigned numTrCh = tfc->getNumTrCh();
	bool firstblock = true;
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		// The number of Transport Blocks in the TBS must match what
		// is specified in the RrcTfc.
		RrcTf *tf = tfc->getTf(tcid);
		unsigned numTB = tf->getNumTB();
		// The number of blocks in the TBS better match the TFC.
		assert(numTB == tbs.mTrChTb[tcid].mNumTb);
		for (unsigned tbn = 0; tbn < numTB; tbn++) {
			blocks[tbn] = tbs.getTB(tbn,tcid);
			if (firstblock) {
				if (blocks[tbn]->scheduled()) mNextWriteTime = blocks[tbn]->time();
				firstblock = false;
			}
		}
		//this->mPerTfc[tfc->getTfcIndex()].mEncoder[tcid]->encWriteHighSide(blocks);
		//this->mPerTfc[tfc->getTfcIndex()].mEncoder[tcid]->encProcess(blocks);

		L1FecProgInfo *fpi = getFPI(tcid,tfc->getTfIndex(tcid)); //tfc->getTfcsIndex());
                //printf("tcid: %d, %d\n",tcid,tfc->getTfIndex(tcid));
		if (tfc->getTfIndex(tcid) > 0) {
	                //printf("tcid: %d, %d\n",tcid,tfc->getTfIndex(tcid));
			this->mEncoders[tcid][tfc->getTfIndex(tcid)]->l1CrcAndTBConcatenation(fpi,blocks);
		}
	}
	//if (firstblock) mNextWriteTime = tbs.mTime;

	// Now send the result to the radio.
	if (tfc->getTfcsIndex() > 0) l1PushRadioFrames(tfc->getTfcsIndex());
}

void L1CCTrChDownlink::l1PushRadioFrames(int tfci)
{
	// We are assuming TTI is the same for all TrCh.
	// The MAC would need more support otherwise.
	int numRF = l1GetNumRadioFrames(0);
	for (int i = 0; i < numRF; i++) {
		//LOG(DEBUG) << "calling l1SendFrame2"<<LOGVAR(i)<< mMultiplexerBuf[i].str();
		l1SendFrame2(mMultiplexerBuf[i],tfci);
	}
}



// The number of TransporBlocks passed in is an intrinsic part of the L1TrChEncoder available as getNumTB().
void L1TrChEncoder::l1CrcAndTBConcatenation(L1FecProgInfo *fpi, TransportBlock const *tblocks[RrcDefs::maxTbPerTrCh])
{
	unsigned numTB = fpi->getNumTB();
	unsigned tbsize = fpi->getTBSize();
	unsigned paritysize = fpi->getPB();

	// 24.212 4.2.2.1 Transport Block Concatenation.
	// catbuf is the result of concatenation.
	initSize(crcAndTBConcatenationBuf, numTB * (tbsize + paritysize));

	for (unsigned tbn = 0; tbn < numTB; tbn++) {

		const BitVector a = tblocks[tbn]->alias();
		assert(a.size() == tbsize);
		//OBJLOG(DEBUG) << "L1TrCHFECEncoder input " << a.size() << " " << a;
		LOG_DOWNLINK << "L1TrCHEncoder input " <<tblocks[tbn];
		
		// parity - 25.212, 4.2.1
		unsigned start = tbn * (tbsize + paritysize);
		a.copyToSegment(crcAndTBConcatenationBuf,start, tbsize);
		BitVector parityOfA = crcAndTBConcatenationBuf.segment(start+tbsize, paritysize);
		getParity(a, parityOfA);
	}
	//OBJLOG(DEBUG) << "with parity " << crcAndTBConcatenationBuf.size() << " " << crcAndTBConcatenationBuf;
	l1ChannelCoding(fpi,crcAndTBConcatenationBuf);
}


void L1TrChEncoder::l1ChannelCoding(L1FecProgInfo *fpi, BitVector &catbuf)
{
	BitVector c;		// The result after convolutional encoding.

	if (catbuf.size() == 0) { c = BitVector((size_t) 0); l1RateMatching(fpi,c); return;}

	unsigned Z = getZ();
	if (catbuf.size() <= Z) {
		// No code block segmentation required.
		BitVector o = catbuf.alias();

		// 24.212 4.2.3 Channel Coding.
		// convolutional coding - 25.212, 4.2.3.1
		if (isTurbo()) {
		  c = BitVector(3 * o.size() + 12);
		  encode(o,c);
		} else {
			BitVector in(catbuf.size() + 8);
			c = BitVector(2 * in.size());
			o.copyTo(in);
			in.fill(0, catbuf.size(), 8);
			encode(in,c);
		}
	} else {
		// 24.212 4.2.2.2 Code Block Segmentation.
		// 25.212 4.2.3.3 concatenation of encoded blocks
		// We just encode the blocks directly into the concatenated output c.
		// This code is for convolutional coding only!!
		unsigned Xi = catbuf.size();			// number of input bits
		unsigned Ci = (Xi+ Z-1) / Z;		// number of code blocks.
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
		//unsigned bindex = 0;
		for (unsigned r = 0; r < Ci; r++) {
			if (Yi && r == 0) {
				in.fill(0,0,Yi);		// First block has Yi filler bits.
				// Harvind changed:  (pats TODO: What about the Yi filler bits in first block?)
				catbuf.segment(0,Ki-Yi).copyToSegment(in,Yi);
				//catbuf.segment(bindex,Ki-Yi).copyToSegment(in,Yi);
				//bindex = Ki - Yi;
			} else {
				// Harvind changed:
				catbuf.segment(r*Ki-Yi,Ki).copyToSegment(in,0);
				// catbuf.segment(bindex,Ki).copyTo(in);
				// bindex += Ki;
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

	l1RateMatching(fpi,c);
}

void L1TrChEncoder::l1RateMatching(L1FecProgInfo *fpi, BitVector &c)
{
	// 25.212 4.2.4 Radio Frame Size Equalization
	// (pat) 25.212 4.2.4 and I quote:
	// "Radio frame size equalisation is only performed in the UL."
	// (pat) And that is because we use DTX instead of frame-size-equalisation in DL.

	initSize(rateMatchingBuf,fpi->mLowSideRMSz);

    // 25.212 4.2.7 Rate-matching
	unsigned insize = fpi->mHighSideRMSz; // old: this->mCodeInBkSz;
	unsigned outsize = fpi->mLowSideRMSz; // old: this->mRadioFrameSz*this->getNumRadioFrames();
    	BitVector g = rateMatchingBuf.alias();
	if (insize != outsize) {
		rateMatchFunc2<char>(c,g,mDlEplus,mDlEminus,1);
	} else {
		c.copyTo(g);
	}

	l1FirstDTXInsertion(fpi,g);
}

void L1TrChEncoder::l1FirstDTXInsertion(L1FecProgInfo *fpi, BitVector &g)
{
	const int nframes = fpi->getNumRadioFrames();
	const unsigned ttisize = fpi->mRFSegmentSize * nframes;

    // 4.2.9.1 First insertion of DTX indication.
	// We pad with a delta bit out to the largest TF for this TrCh.
	// The appended DTX bits are not supposed to be transmitted,
	// so eventually we should insert a special value that the transmitter can ignore,
	// but for now just pad with zeros.

	assert(g.size() <= ttisize);
	initSize(firstDtxBuf,ttisize);
	BitVector h = firstDtxBuf.alias();
	g.copyTo(h);
	//h.fillField(g.size(),0x7f,h.size() - g.size());
	h.fill(0x7f,g.size(),h.size() - g.size());
	LOG_DOWNLINK << "first dtx " << h.str();

	// first interleave - 25.212, 4.2.5

	initSize(firstInterleaveBuf,ttisize);
	BitVector q = firstInterleaveBuf.alias();
	h.interleavingNP(fpi->inter1Columns(), fpi->inter1Perm(), q);
	LOG_DOWNLINK << "interleaved " << q.str();

	//if (gFecTestMode == 1) {
	//	// (pat) For testing, send it back up through the decoder.
	//	// Jumper around the radio frame segmentation for now.
	//	// We are assuming it is RACH/FACH for now.
	//	SoftVector qdebug(q); // Convert to SoftVector.
	//	gNodeB.mRachFec->decoder()->writeLowSide3(qdebug);
	//	return;
	//}

	// radio frame segmentation - 25.212, 4.2.6
	const unsigned frameSize = fpi->mRFSegmentSize;
	assert(frameSize == q.size() / nframes);

	assert(q.size() % frameSize == 0);
	for (int i = 0; i < nframes; i++) {
		BitVector seg = q.segment(i * frameSize, frameSize);
		mParent->l1Multiplexer(fpi,seg,i);
	}
}

SoftVector *L1CCTrChUplink::l1FillerBurst(unsigned size)
{
	if (mFillerBurst.size() != size) {
		mFillerBurst.resize(size);
		for (unsigned i = 0; i < size; i++) {
			mFillerBurst[i] = 0.5f + 0.0001*(2.0*(float) (random() & 0x01)-1.0);
		}
	}
	return &mFillerBurst;
}

// (pat) The incoming burst is a single-slot data burst, one of 15 in a radio frame.
// Note that uplink separates data and control info, so the data burst is
// nothing but data - the control burst contains the pilot and tfci bits.
// This function just adds filler bursts for missing slot data.
void L1CCTrChUplink::l1WriteLowSide(const RxBitsBurst &burst)
{
	LOG_UPLINK "l1WriteLowSide "<<timestr()<<" RxBitsBurst:"<<burst.str();
	// TODO: Enable these assertions.
	//assert(burst.mTfciBits[0] >= 0 && burst.mTfciBits[0] <= 1);
	//assert(burst.mTfciBits[1] >= 0 && burst.mTfciBits[1] <= 1);

	// (pat) Why are we inserting filler bursts going upstream?
	// They will fail the parity check (hopefully!) and writeLowSide1 will just throw them away?
	// Answer: Because the radio frame un-segmentation needs the right number of
	// bursts or it will become permanently desynchronized.
	// We cannot use the burst time to synchronize because incoming RACH bursts are not
	// synchronized to the main radio clock; we just get 15/30 in a row starting anywhere.
	// Also, maybe David hopes the convolutional decoder will span the data.

	// This assumes frame boundary is at TN=0.
	float garbageTfci[2] = { 0.5, 0.5 };

        //unsigned receivedTTIIndex = ((unsigned) burst.time().FN() % numFramesTTI);

	unsigned receivedSlotIndex = burst.time().TN();
	//LOG(INFO) << "1: mReceiveTime: " << mReceiveTime << " burstTime: " << burst.time();
	if (receivedSlotIndex == 0) {
		// Special case: if we skipped just the last slot of previous frame, add a fillerframe and send it anyway.
		if (burst.time().FN() == mReceiveTime.FN()+1 && mReceiveTime.TN() == gFrameSlots-1) {
			LOG(NOTICE) << "Skipped 1 slot at "<<burst.time();
			l1AccumulateSlots(l1FillerBurst(burst.size()), garbageTfci);	// increments mReceiveTime.TN()
		}
		// Start a new radio frame.
		mReceiveTime = burst.time();
		mSlotSize = burst.size();


	}

	// Check for skipped frames.
	if (burst.time().FN() != mReceiveTime.FN()) {
		// Hopeless.  Any slots we saved previously are useless; discard them.
		LOG(NOTICE) << "Incoming skipped frames, expected="<<mReceiveTime<<" burst.time="<<burst.time();
		mReceiveTime = UMTS::Time(0,1);	// Set to garbage.
		return;
	}

	if (burst.size() != mSlotSize) {
		LOG(ERR) << "Incoming radio slot"<<LOGVAR2("size",burst.size())<<" but expected"<<LOGVAR(mSlotSize);
		// punt.
		mReceiveTime = UMTS::Time(0,1);	// Set to garbage.
		return;
	}
	// TODO: Something about skipped frames.

	// Fill in skipped slots.
	if (mReceiveTime.TN() < receivedSlotIndex) {
		int skipped = receivedSlotIndex - mReceiveTime.TN();
		LOG(NOTICE) << "Skipped "<<skipped<<" slot(s) at "<<burst.time();
	}
	while (mReceiveTime.TN() < receivedSlotIndex) {
		l1AccumulateSlots(l1FillerBurst(burst.size()), garbageTfci);	// increments mReceiveTime.TN()
	}
	l1AccumulateSlots(&burst,burst.mTfciBits);
        //LOG(INFO) << "2: mReceiveTime: " << mReceiveTime << " burstTime: " << burst.time();

}

void L1CCTrChUplink::l1WriteLowSideFrame(const RxBitsBurst &burst, float tfci[30])
{
        LOG_UPLINK "l1WriteLowSideFrame "<<timestr()<<" RxBitsBurst:"<<burst.str();

        // Start a new radio frame.
        mReceiveTime = burst.time();
        mSlotSize = burst.size()/gFrameSlots;

        // Check for skipped frames.
        if (burst.time().FN() != mReceiveTime.FN()) {
                // Hopeless.  Any slots we saved previously are useless; discard them.
                LOG(NOTICE) << "Incoming skipped frames, expected="<<mReceiveTime<<" burst.time="<<burst.time();
                mReceiveTime = UMTS::Time(0,1); // Set to garbage.
                return;
        }

	for (unsigned j = 0; j < gFrameSlots; j++) {
		SoftVector burstSeg(burst.segment(j*mSlotSize,mSlotSize));	
		float tfciSeg[2] = {tfci[j*2],tfci[j*2+1]};
        	l1AccumulateSlots(&burstSeg,tfciSeg);
	}
        //LOG(INFO) << "2: mReceiveTime: " << mReceiveTime << " burstTime: " << burst.time();

}


// (pat) Accumulate slots into one radio frame in mDataIn and tfcibits into mTfciAccumulator.
void L1CCTrChUplink::l1AccumulateSlots(const SoftVector *e, const float tfcibits[2])
{
	LOG_UPLINK << "L1TrCHFECDecoder input " << e->str();

	// Uplink slot size varies, so the mDSlotAccumulator is sized for the maximum TFC.
	int slotIndex = mReceiveTime.TN();
	int frameIndex = mReceiveTime.FN(); 
	// The SF and therefore the slot size can vary with each uplink TFC.
	unsigned fullsize = e->size() * gFrameSlots;
	if (mDSlotAccumulatorBuf.size() != fullsize) { mDSlotAccumulatorBuf.resize(fullsize); }
	e->copyToSegment(mDSlotAccumulatorBuf,slotIndex * e->size());
	mRawTfciAccumulator[2*slotIndex] = tfcibits[0];
	mRawTfciAccumulator[2*slotIndex+1] = tfcibits[1];
	mReceiveTime.incTN();
	if (mReceiveTime.TN() != 0) {return;}

	//OBJLOG(INFO) << "concatenated " << mDSlotAccumulatorBuf.size() << " " << mDSlotAccumulatorBuf;
	unsigned tfci = findTfci(mRawTfciAccumulator,mNumTfc);
	LOG(NOTICE) << "TFCI: " << tfci << " time: " << mReceiveTime;

	// 25.212 4.2.12 Physical Channel Mapping.
	// "In compressed mode..."  Nope.

	l1SecondDeinterleaving(mDSlotAccumulatorBuf, tfci, frameIndex);
}

// It is called the 2nd interleaving but in uplink it happens before the 1st interleaving.
void L1CCTrChUplink::l1SecondDeinterleaving(SoftVector &v, unsigned tfci, unsigned frameIndex)
{

	// 25.212 4.2.11 Second Interleaving.
	// The SF and therefore the incoming buffer size can vary with each uplink TFC.
	//const_cast<SoftVector&>(frame).deInterleavingNP(30, TrCHConsts::inter2Perm, *mHDIBuf);
	if (v.size() != mHDIBuf.size()) { mHDIBuf.resize(v.size()); }
	v.deInterleavingNP(30, TrCHConsts::inter2Perm, mHDIBuf);
	//assert(mHDIBuf.size() == frameSize);
	//OBJLOG(INFO) << "2nd deinterleaved " << mHDIBuf.size() << " " << mHDIBuf;

	// 25.212 4.2.10 Physical Channel Segmentation.
	// "When more than one PhCh is used..."  Nope.

	l1Demultiplexer(mHDIBuf,tfci, frameIndex);
}

// The input here is one radio-frame, ie, 15 slots worth.
// Chop it up into TrChs using the TFs specified by tfci, and send it up to rate matching.
void L1CCTrChUplink::l1Demultiplexer(SoftVector &frame, unsigned tfci, unsigned frameIndex)
{
	// 25.212 4.2.8 TrCh (De-)Multiplexing.
	if (tfci >= getNumTfc()) {
		// Invalid data.
		// TODO what?
		return;
	}
	unsigned loc = 0;
	for (unsigned tcid = 0; tcid < getNumTrCh(); tcid++) {
		//unsigned tfi = tfc->getTfIndex(tcid);
		L1FecProgInfo *fpi = getFPI(tcid,tfci);
		unsigned nbits = fpi->mLowSideRMSz;	// Number of bits to go to this TrCh.
		//printf("nbits: %d %d %u %u\n",nbits, fpi->mHighSideRMSz, loc,tfci);
		if (! nbits) { continue; }		// No bits for this TrCh this time.
		//printf("framesize: %u %u %u %u\n",frame.size(),nbits,loc,tfci);
		SoftVector tmp(frame.segment(loc,nbits));
		loc += nbits;
		mDecoders[tcid][tfci]->l1RateMatching(fpi,tmp, frameIndex);
	}
	//printf("loc: %d, frame.size: %d\n",loc,frame.size());
	if (loc==0) return;
	if (loc!=frame.size()) LOG(INFO) << "loc: " << loc << " " << frame.size();
	//assert(loc == frame.size());
}

void L1TrChDecoder::l1RateMatching(L1FecProgInfo *fpi, SoftVector &frame, unsigned frameIndex)
{
	// 25.212 4.2.7 Rate Matching.
	unsigned insize = fpi->mLowSideRMSz;
	assert(frame.size() == insize);
	unsigned outsize = fpi->mHighSideRMSz;	// this->mCodeInBkSz/this->l1GetNumRadioFrames();
	mDTtiIndex = frameIndex % fpi->getNumRadioFrames();
	if (insize == outsize) {
		l1RadioFrameUnsegmentation(fpi,frame);
	} else {
		rateMatchFunc<float>(frame,mRMBuf,this->mEini[mDTtiIndex]);
		OBJLOG(INFO) << "insize: " << insize << "outsize: " << outsize;
		//OBJLOG(INFO) << "rate-unmatched" << mRMBuf.size() << " " << mRMBuf;
		l1RadioFrameUnsegmentation(fpi,mRMBuf);
	}
}


void L1TrChDecoder::l1RadioFrameUnsegmentation(L1FecProgInfo *fpi, const SoftVector&frame)
{
	// 25.212 4.2.6 Radio Frame Un-Segmentation.
	// If not using TTI=10ms, At this point we have to reaccumulate the complete TTI data.
	TTICodes tticode = fpi->getTTICode();
	if (tticode == TTI10ms) { l1FirstDeinterleave(fpi,frame); return; }


	// Accumulate one ttis worth of Radio Frames into mDTtiBuf.
	frame.copyToSegment(mDTtiBuf,mDTtiIndex*frame.size());

	unsigned numFramesPerTti = fpi->getNumRadioFrames();
	if (mDTtiIndex < numFramesPerTti - 1) {return;}
	mDTtiIndex = 0;	// prep for next TTI
	l1FirstDeinterleave(fpi,mDTtiBuf);
}

// It is called the 1st interleaving but in uplink it happens after the 2nd interleaving.
void L1TrChDecoder::l1FirstDeinterleave(L1FecProgInfo *fpi, const SoftVector &d)
{
	// first interleave - 25.212, 4.2.5
	SoftVector t(d.size());
	const_cast<SoftVector&>(d).deInterleavingNP(fpi->inter1Columns(), fpi->inter1Perm(), t);
	//OBJLOG(INFO) << "deinterleaved " << t.size() << " " << t;

	// radio frame equalization - 25.212, 4.2.4
	// TODO
	SoftVector c = t.alias();

	l1ChannelDecoding(fpi,c);
}


// Input is post-radio-frame-segmentation, which means input is the accumulation
// of 1, 2, 4, 8 radio frames based on the TTI=10,20,40,80
void L1TrChDecoder::l1ChannelDecoding(L1FecProgInfo *fpi, const SoftVector &c)
{
	// FIXME -- This stuff assumes a rate-1/2 coder.

	//assert(c.size() <= 2 * getZ());
	BitVector b;	// The result
	if (0) {
		// old code does not handle concatenation/segmentation.
		BitVector o(c.size() / 2);
		decode(c,o);
		//OBJLOG(INFO) << "unconvoluted " << o.str();

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
		//unsigned Zenc = 2 * getZ() + 16;		// encoded size of Z.
		unsigned Zenc = isTurbo() ? (3*getZ()+12) : (2*getZ() + 16);   // encoded size of Z.
		unsigned Ci = (c.size() + Zenc-1)/Zenc;	// number of coded blocks.
		unsigned Kienc = c.size()/Ci;		// number of encoded bits per coded block.
		//unsigned Ki = Kienc/2 - 8;		// number of unencoded bits per coded block.
		unsigned Ki = isTurbo() ? ((Kienc-12)/3) : (Kienc/2 - 8);       // number of unencoded bits per coded block
		unsigned numFillBits = fpi->mCodeFillBits;		// number of filler bits in first coded block.
		assert(Kienc * Ci == c.size());
		initSize(decodingInBuf, Ci*Ki - numFillBits);
		b = decodingInBuf.alias();
		//BitVector o1(Kienc/2);
		initSize(decodingOutBuf, isTurbo() ? Ki : Ki+8);	// pats TODO: Harvind changed, is this right?
		BitVector o1 = decodingOutBuf.alias();
		for (unsigned r = 0; r < Ci; r++) {
			decode(c.segment(r*Kienc,Kienc),o1);
			if (numFillBits && (r == 0)) { // skip first fillBits, they aren't data
				o1.segmentCopyTo(b,numFillBits,Ki-numFillBits);
			} else {
				o1.copyToSegment(b,r*Ki-numFillBits,Ki);
			}
		}
	}
	//OBJLOG(INFO) << "de-filled " << b.size() << " " << b;
	//OBJLOG(INFO) << "de-filled last 100: " << b.segment(b.size()-100,100);
	l1Deconcatenation(fpi,b);
}

void L1TrChDecoder::l1Deconcatenation(L1FecProgInfo *fpi, BitVector &b)
{
	// TODO
	// parity - 25.212, 4.2.1
	unsigned pb = fpi->getPB();
	unsigned numTB = fpi->mNumTB;
	unsigned tbpbSz = fpi->mTBSz + fpi->getPB();
	bool frameGood = true;
	for (unsigned j = 0; j < numTB; j++) {
		BitVector a = b.segment(j*tbpbSz, tbpbSz - pb);
		BitVector gotParity = b.segment(j*tbpbSz + tbpbSz - pb, pb);
		initSize(expectParity,pb);		// TODO: preallocate buffer.
		BitVector bWithoutParity = b.segment(j*tbpbSz, tbpbSz - pb);
		getParity(bWithoutParity, expectParity);
		//bool parityOK = true;
		//for (int i = 0; i < pb && parityOK; i++) {
		//	parityOK = expectParity[i] == gotParity[i];
		//}
		bool parityOK = expectParity == gotParity;
		if (gotParity.sum() == 0) parityOK = false;
		OBJLOG(NOTICE) << "parity OK: " << parityOK << " " << expectParity << " " << gotParity;
		if (gFecTestMode) {
			LOGDEBUG<<"writeLowSide3"<<LOGBV(b) <<LOGBV(gotParity)<<LOGBV(expectParity)<<LOGVAR(parityOK)<<"\n";
		}
		if (!parityOK) {
			frameGood = false;
			continue;
		}
		//assert(mUpstream);
       		const TransportBlock tb(a);
        	//LOG(INFO) << "Sending up tb: " << a;
        	if (parityOK && mUpstream) 
			mUpstream->macWriteLowSideTb(tb,fpi-> mCCTrChIndex);
	}

	if (frameGood) {
		// TODO: Do we need to keep FER separately for each TrCh as well as for the CCTrCh?
		//countGoodFrame();
		mParent->countGoodFrame();
	} else {
		//countBadFrame();
		mParent->countBadFrame();
	}
}

void L1FER::countGoodFrame()
{
	//unnecessary: ScopedLock lock(mLock);
	static const float a = 1.0F / ((float)mFERMemory);
	static const float b = 1.0F - a;
	mFER *= b;
	LOG_UPLINK <<"L1TrChDecoder FER=" << mFER;
}

void L1FER::countBadFrame()
{
	static const float a = 1.0F / ((float)mFERMemory);
	static const float b = 1.0F - a;
	mFER = b*mFER + a;
	LOG_UPLINK <<"L1TrChDecoder FER=" << mFER;
}

// (pat) This is used by BCH only - see macServiceLoop for FACH and DCH.
// (pat) Could move to BCHFEC
void L1CCTrChDownlink::l1WaitToSend() const
{
	//DEBUGF("L1CCTrChDownlink::waitToSend\n");
	// Block until the NodeB clock catches up to the
	// mostly recently transmitted burst.
	//LOG(INFO) << "mPrevWriteTime: " << mPrevWriteTime << ", " << mNextWriteTime;
	unsigned fnbefore = gNodeB.clock().FN();
	gNodeB.clock().wait(mPrevWriteTime);
	unsigned fnafter = gNodeB.clock().FN();
	int diff = FNDelta(fnafter, fnbefore);
	// It is usual to wait 2 frames since BCH is TTI 20ms.
	// TODO: Occasionally it waits 3 frames preceded or followed by 1 frame - why?
	if (diff > 3 || diff < 0) { LOG(NOTICE) << "waitToSend waited "<<diff<<" frames"<<LOGVAR(fnbefore)<<LOGVAR(mPrevWriteTime)<<LOGVAR(fnafter); }
	//DEBUGF("L1CCTrChDownlink::waitToSend finished\n");
}




void L1TrChEncoderLowRate::encode(BitVector& in, BitVector& c)
{
	// convolutional coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	in.encode(mVCoder, c);
	LOG_DOWNLINK << "convoluted " << c.str();
}

void L1TrChDecoderLowRate::decode(const SoftVector& c, BitVector& o)
{
	c.decode(mVCoder, o);
	LOG_UPLINK << "unconvoluted " << c.str();	//<< c.size() << " " << c;
}

#if CANNEDBEACON
const TransportBlock *cannedBeaconBlocks[2048];
#endif

#if SAVEME
void BCHFEC::generate()
{
	waitToSend();
#if CANNEDBEACON
        const TransportBlock *tb = cannedBeaconBlocks[nextWriteTime().FN()>>1];
#else
	const TransportBlock *tb = gNodeB.getTxSIB(nextWriteTime().FN());
#endif
	writeHighSide(*tb);
}



void BCHFEC::start()
{
#if CANNEDBEACON
	for (int i = 0; i < 2048; i++) {
	        BitVector tmp(62*4);
		tmp.unhex(cannedBeacon[i]);
		cannedBeaconBlocks[i] = new TransportBlock(tmp.head(62*4-2));
		RN_MEMLOG(TransportBlock,cannedBeaconBlocks[i]);
	}
#endif
	mServiceThread.start((void* (*)(void*))TrCHServiceLoop, (void*)this);
}



void* TrCHServiceLoop(L1TrCHFEC* chan)
{
	while (true) {
		chan->generate();
	}
	return 0;
}
#endif


void L1TrChEncoderTurbo::encode(BitVector& in, BitVector &c)
{
	// coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	in.encode(mTCoder, c, mInterleaver);
	LOG_DOWNLINK << "turbo " << c.str();	//c.size() << " " << c;
}

void L1TrChDecoderTurbo::decode(const SoftVector&c, BitVector &o)
{
	// coding - 25.212, 4.2.3.1
	// concatenation of encoded blocks - 25.212, 4.2.3.3
	c.decode(mTCoder, o, mInterleaver);
	LOG_UPLINK << "turbo " << o.str();	//o.size() << " " << o;
}

// Create the encoder/decoders for this FEC class from the RRC programming.
// The turbo flag is not used here.
void L1CCTrCh::fecConfig(TrChConfig &config)
{
#if THE_BELOW_DOESNT_WORK
	// (pat) To Harvind:  If you have trouble here, for testing only, you can try using this code instead;
	// you will have to manually fill in the SF, PB and tbsize to exactly match what was programmed in RRC in configDchPS()
	int sf = 256;
	int pb = 16;
	int ultbsize = 340;
	int dltbsize = 340;
	bool isTurbo = false;
	L1CCTrChUplink::fecConfigForOneTrCh(false,sf,TTI10ms,pb,getPhCh()->getDlRadioFrameSize(),ultbsize,0,1,isTurbo);
	L1CCTrChUplink::l1InstantiateUplink();
	L1CCTrChDownlink::fecConfigForOneTrCh(true,sf,TTI10ms,pb,getPhCh()->getUlRadioFrameSize(),dltbsize,0,1,isTurbo);
	L1CCTrChDownlink::l1InstantiateDownlink();
#endif

	// Currently we support only one TrCh.
	//std::cout << "fecConfig UL\n";
	//config.tcdump();
	fecComputeUlTrChSizes(config.ul(),static_cast<L1CCTrChUplink*>(this),mPhCh->isDch());
	//std::cout << "fecConfig DL\n";
	//config.tcdump();
	if (!mPhCh->isRach()) fecComputeDlTrChSizes(config.dl(), getPhCh()->getDlRadioFrameSize(), static_cast<L1CCTrChDownlink*>(this));
	L1CCTrChUplink::l1InstantiateUplink();	
        if (!mPhCh->isRach()) L1CCTrChDownlink::l1InstantiateDownlink();
}

void L1CCTrChDownlink::l1InstantiateDownlink()
{
	L1CCTrCh *parent = static_cast<L1CCTrCh*>(this);
	for (TrChId i = 0; i < getNumTrCh(); i++) {
		// There is one encoder for each TF, not one per TFC.
		for (TfcId j = 0; j < getNumTfc(); j++) {
			L1FecProgInfo *fpi = getFPI(i,j);
			TfIndex tfi = fpi->mTfi;
			//if (fpi->mCodeInBkSz == 0) continue;
			if (!this->mEncoders[i][tfi]) {
				if (getTCI(i)->mIsTurbo) {
					// Any one of the TFC j can be used to init the encoder for TF tfi; the sizes are the same in all.
					this->mEncoders[i][tfi] = new L1TrChEncoderTurbo(parent,fpi);
				} else {
					this->mEncoders[i][tfi] = new L1TrChEncoderLowRate(parent,fpi);
				}
			}
		}
	}
#if 0
	if (config) {
		// Create trivial encoder set, assuming one TrCh, no explicit TFCS needed..
		assert(getNumTrCh() == 0);
		for (TfcId j = 0; j < getNumTfc(); j++) {
			// Assume it is 
			if (turbo) {
				// Any one of the TFC j can be used to init the encoder for TF tfi; the sizes are the same in all.
				this->mEncoders[i][j] = new L1TrChEncoderTurbo(this,getFPI(0,j));
			} else {
				this->mEncoders[i][j] = new L1TrChEncoderLowRate(this,getFPI(0,j));
			}
		}
	} else {
		RrcTfcs *tfcs = config->dl()->getTfcs();
		for (TrChId i = 0; i < getNumTrCh(); i++) {
			// There is one encoder for each TF, not one per TFC.
			// We did not bother to save the TF index for each TFC in the L1FecProgInfo because we never really need it;
			// We can find the tf index using the config.
			// The other place we might have needed tf index is in the incoming TBS, but that includes the tfc pointer.
			for (TfcId j = 0; j < getNumTfc(); j++) {
				RrcTfc *tfc = tfcs->getTfc(j);
				TfIndex tfi = tfc->getTfIndex(i);
				if (!this->mEncoders[i][tfi]) {
					if (turbo) {
						// Any one of the TFC j can be used to init the encoder for TF tfi; the sizes are the same in all.
						this->mEncoders[i][tfi] = new L1TrChEncoderTurbo(this,getFPI(i,j));
					} else {
						this->mEncoders[i][tfi] = new L1TrChEncoderLowRate(this,getFPI(i,j));
					}
				}
			}
		}
	}
#endif
}

void L1CCTrChUplink::l1InstantiateUplink()
{
	L1CCTrCh *parent = static_cast<L1CCTrCh*>(this);
	//initSize(mDSlotAccumulatorBuf,parent->getMaxUlRadioFrameSz());
	//initSize(mHDIBuf,parent->getMaxUlRadioFrameSz());

	for (TrChId i = 0; i < getNumTrCh(); i++) {
		for (TfcId j = 0; j < getNumTfc(); j++) {
			L1FecProgInfo *fpi = getFPI(i,j);
			assert(j == fpi->mTfi);
			// (pat) 12-8-2012: Harvind modified the turbocoder code to not abort if you send it a K of 0.
			if (getTCI(i)->mIsTurbo) {
				this->mDecoders[i][j] = new L1TrChDecoderTurbo(parent,fpi);
			} else {
				this->mDecoders[i][j] = new L1TrChDecoderLowRate(parent,fpi);
			}
		}
	}
}


#if 0
void L1DCHFEC::open()
{
	//ScopedLock lock(gActiveDCH.mLock);
	// The DCHFEC was already allocated from the ChannelTree by the caller.
	assert(phChAllocated());
	// TODO: If this is a voice channel it needs to go in mDTCHPool;
	gActiveDCH.push_back(this);	// No one uses this yet.
	LOGDEBUG << "Opening DCH" << endl;
	controlOpen();
}

// TODO: Do we want a time delay here somewhere before reusing the channel?
// TODO UMTS - If hardRelease, we need to force-expire that timer.
void L1DCHFEC::close(bool hardRelease)
{
	ScopedLock lock(gActiveDCH.mLock);
	gActiveDCH.remove(this);
	getPhCh()->phChClose();		// Allows reallocation in the ChannelTree.
	// downlinkClose();	no close needed.
	controlClose();
}
#endif

// Test fecConfigForOneTrCh against fecConfigTrivial.
static void testCCProgramBCH()
{
	// Radio frame size is 270.
	FecProgInfoSimple fpi(256,TTI20ms,16,270);
	// with parity=(246+16)=262; encoded=2*(262)+16 = 540;
	// PhCh is 18 bits/slot * 15 slots * 2 radio frames for TTI20ms = 540;
	L1CCTrChInfo info1, info2;
	info1.fecConfigTrivial(256,TTI20ms,16,270);
	info2.fecConfigForOneTrCh(true,256,TTI20ms,16, 270,246,1,1,false);
	info1.musteql(info2);
}

static void testCCProgramRach(int SF, TTICodes tticode,int PB)
{
	TrChConfig config;
	L1CCTrChInfo info1, info2;

	unsigned radioFrameSize = gFrameLen / SF;
	unsigned numRadioFrames = TTICode2NumFrames(tticode);
	unsigned ulTotalSize = radioFrameSize * numRadioFrames;
	unsigned requestedTBSize = RrcDefs::R2DecodedSize(ulTotalSize) - PB;
	unsigned tbsize = quantizeRlcSize(true,requestedTBSize);
	info1.fecConfigForOneTrCh(false,SF,tticode, PB, radioFrameSize,tbsize,1,1,false);

	config.configRachTrCh(SF,tticode,PB,0);
	fecComputeUlTrChSizes(config.ul(),&info2);

	LOGDEBUG << "RACH program test:"<<LOGVAR(SF)<<LOGVAR(tticode)<<LOGVAR(PB)<<LOGVAR(radioFrameSize)<<LOGVAR(ulTotalSize)
		<<LOGVAR(requestedTBSize)<<LOGVAR(tbsize) << "\n";
	LOGDEBUG << "manual  result:" << info1.str();
	LOGDEBUG << "compute result:" << info2.str();
	info1.musteql(info2);
}

static void testCCProgramFach(int SF, TTICodes tticode,int PB)
{
	TrChConfig config;
	L1CCTrChInfo info1, info2;

	unsigned radioFrameSize = getDlRadioFrameSize(SCCPCHType, SF);
	unsigned numRadioFrames = TTICode2NumFrames(tticode);
	unsigned ulTotalSize = radioFrameSize * numRadioFrames;
	unsigned requestedTBSize = RrcDefs::R2DecodedSize(ulTotalSize) - PB;
	unsigned tbsize = quantizeRlcSize(true,requestedTBSize);
	info1.fecConfigForOneTrCh(true,SF,tticode, PB, radioFrameSize,tbsize,1,1,false);

	config.configFachTrCh(SF,tticode,PB,0);
	fecComputeDlTrChSizes(config.dl(),radioFrameSize,&info2);

	LOGDEBUG << "FACH program test:"<<LOGVAR(SF)<<LOGVAR(tticode)<<LOGVAR(PB)<<LOGVAR(radioFrameSize)<<LOGVAR(ulTotalSize)
		<<LOGVAR(requestedTBSize)<<LOGVAR(tbsize) << "\n";
	LOGDEBUG << "manual  result:" << info1.str();
	LOGDEBUG << "compute result:" << info2.str();
	info1.musteql(info2);
}

static void testCCDch(unsigned sf, unsigned pb, TTICodes tticode, unsigned tbsize, bool isTurbo)
{
	LOGDEBUG << "test dch"<<LOGVAR(tbsize)<<LOGVAR(sf)<<LOGVAR(pb)<<LOGVAR(tticode)<<LOGVAR(isTurbo)<<"\n";
	L1CCTrChInfo info1ul, info1dl, infocul, infocdl;
	unsigned dlRFSize = getDlRadioFrameSize(DPDCHType, sf);
	unsigned ulRFSize = gFrameLen / sf;
	DCHFEC *dch = gChannelTree.chChooseBySF(sf);
	if (!dch) {
		LOGDEBUG << "debug: Could not allocate DCH of sf" <<sf << "\n";
		return;
	}
	// First test the single-TB config.
	// Start with a fresh config each time:
	TrChConfig *config = new TrChConfig;
	config->configDchPS(dch, tticode,pb,isTurbo,tbsize,tbsize);

	// test downlink:
	{
	fecComputeDlTrChSizes(config->dl(),dlRFSize,&infocdl);
	// For fecConfigForOneTrCh, dig out the values for the last TF computed by configDchPS.
	//unsigned dltbsize = tbsize ? tbsize : config->dl()->getTfs(0)->getMaxTBSize();
	RrcTfs *dltfs = config->dl()->getTfs(0);	// TFS for TrCh 0
	unsigned dltbsize = dltfs->getTBSize(dltfs->getNumTf()-1);
	unsigned dlnumtbs = dltfs->getNumTB(dltfs->getNumTf()-1);
	LOGDEBUG << format("dltfs numTf=%u numTBs=%u\n",dltfs->getNumTf(),dlnumtbs);
	info1dl.fecConfigForOneTrCh(true,sf,tticode, pb, dlRFSize,dltbsize,0,dlnumtbs,false);
	LOGDEBUG << "info1dl:" << info1dl.str();
	LOGDEBUG << "infocdl:" << infocdl.str();
	if (!isTurbo) info1dl.musteql(infocdl);
	}

	// test uplink:
	{
	fecComputeUlTrChSizes(config->ul(),&infocul);
	LOGDEBUG << "infocul:" << infocul.str();
	//unsigned ultbsize = tbsize ? tbsize : config->ul()->getTfs(0)->getMaxTBSize();
	RrcTfs *ultfs = config->ul()->getTfs(0);	// TFS for TrCh 0
	unsigned ultbsize = ultfs->getTBSize(ultfs->getNumTf()-1);
	unsigned ulnumtbs = ultfs->getNumTB(ultfs->getNumTf()-1);
	if (ulnumtbs <= 1) { // Too complicated for multiple TBs.
		info1ul.fecConfigForOneTrCh(false,sf,tticode, pb, ulRFSize,ultbsize,0,ulnumtbs,false);
		LOGDEBUG << "info1ul:" << info1ul.str();
		// They may not be equal because fecComputeUlTrChSizes changes the SF, which is detectable
		// by checking the uplink RFSegmentSize.  This is most likely to happen in the smallest non-zero TFC #1.
		int rf1size = info1ul.getFPI(0,1)->mRFSegmentSize;
		int rfcsize = infocul.getFPI(0,1)->mRFSegmentSize;
		if (rf1size == rfcsize) {
			if (!isTurbo) info1ul.musteql(infocul);
		} else {
			LOGDEBUG << format("Not checking info1ul because SF not equal %d != %d\n",rf1size,rfcsize);
		}
	}
	delete config;
	dch->phChClose();
	}
}

// Test AMR programming.
static void testCCAMR(unsigned sf)
{
	LOGDEBUG << "test AMR"<<LOGVAR(sf)<<"\n";
	L1CCTrChInfo infoamrul, infoamrdl;
	unsigned dlRFSize = getDlRadioFrameSize(DPDCHType, sf);
	//unsigned ulRFSize = gFrameLen / sf;
	DCHFEC *dch = gChannelTree.chChooseBySF(sf);
	if (!dch) {
		LOGDEBUG << "debug: Could not allocate DCH of sf" <<sf << "\n";
		return;
	}
	// First test the single-TB config.
	// Start with a fresh config each time:
	TrChConfig *config = new TrChConfig;
	config->defaultConfig3TrCh();
	LOGDEBUG << "AMR dl config:" << config->dl()->str() << "\n";
	LOGDEBUG << "AMR ul config:" << config->ul()->str() << "\n";

	// downlink:
	fecComputeDlTrChSizes(config->dl(),dlRFSize,&infoamrdl);
	LOGDEBUG << "infoamrdl:" << infoamrdl.str();

	// uplink:
	fecComputeUlTrChSizes(config->ul(),&infoamrul);
	LOGDEBUG << "infoamrul:" << infoamrul.str();
}

void testCCProgramDCH(unsigned tbsize)
{
	int SFs[7] = { 256, 128, 64, 32, 16, 8, 4 };
	int PBs[3] = { 16, 12, 8 };
	TTICodes tticode = TTI10ms;
	for (unsigned iSF = 0; iSF < sizeof(SFs)/sizeof(*SFs); iSF++) {
		int sf = SFs[iSF];
		for (unsigned iPB = 0; iPB < sizeof(PBs)/sizeof(*PBs); iPB++) {
			int pb = PBs[iPB];
			for (int isTurbo = 0; isTurbo <= 1; isTurbo++) {
				testCCDch(sf,pb,tticode,tbsize,isTurbo);
			}
		}
	}
}

// Just print out a bunch of sizes.
static void printCC(std::ostream &os)
{
	L1CCTrChInfo info1;
	int SFs[6] = { 256, 128, 64, 32, 16, 8 };
	int PBs[3] = { 16, 12, 8 };
	TTICodes TTIs[2] = { TTI10ms, TTI20ms };
	int TBSzs[1] = { 340 };	// Must pre-include the 4 MAC bits for logical channel multiplexing
	bool isTurbo = false;
	bool isDownlink = true;
	for (unsigned iSF = 0; iSF < sizeof(SFs)/sizeof(*SFs); iSF++) {
		int sf = SFs[iSF];
		for (unsigned iPB = 0; iPB < sizeof(PBs)/sizeof(*PBs); iPB++) {
			int pb = PBs[iPB];
			for (unsigned iTTI = 0; iTTI < sizeof(TTIs)/sizeof(*TTIs); iTTI++) {
				TTICodes tticode = TTIs[iTTI];
				for (unsigned iTBSz = 0; iTBSz < sizeof(TBSzs)/sizeof(*TBSzs); iTBSz++) {
					unsigned tbsize = TBSzs[iTBSz];
					//unsigned radioFrameSize = getDlRadioFrameSize(SCCPCHType, sf);
					unsigned radioFrameSize = getDlRadioFrameSize(DPDCHType, sf);
					for (unsigned ntbs = 1; ntbs < 8; ntbs++) {
						info1.fecConfigForOneTrCh(isDownlink,sf,tticode,pb,radioFrameSize,tbsize,1,ntbs,isTurbo);
						os << info1.str();
					}
				}
			}
		}
	}
	// Must test if it results in puncturing.
}


void testCCProgramming()
{
	// These are the parameters for these functions:
	// fecConfigForOneTrCh(bool isDownlink, unsigned wSF,TTICodes wTTICode,unsigned wPB,
	//		unsigned wRadioFrameSz, unsigned wTBSz, unsigned wMaxTB, bool wTurbo)
	// fecConfigTrivial(unsigned wSF,TTICodes wTTICode,unsigned wPB, unsigned wRadioFrameSz)

	//std::filebuf fb;
	//fb.open("l1cc.log",ios::out);
	//ostream os(&fb);
	ostream &os = std::cout;
	if (0) printCC(os);
	testCCProgramBCH();
	testCCProgramRach(256,TTI10ms,16);
	testCCProgramRach(256,TTI20ms,16);
	testCCProgramFach(256,TTI10ms,12);
	testCCProgramDCH(0);
	testCCProgramDCH(340);
	testCCAMR(256);
	testCCAMR(128);
	//fb.close();
}


}; // namespace

// vim: ts=4 sw=4
