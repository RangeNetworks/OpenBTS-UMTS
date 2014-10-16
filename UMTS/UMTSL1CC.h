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

#ifndef UMTSL1_H
#define UMTSL1_H

#include <stdlib.h>
#include <BitVector.h>
#include <TurboCoder.h>
#include <Interthread.h>
#include "GSMCommon.h"
#include "UMTSCommon.h"
//#include "UMTSTransfer.h"
#include "MACEngine.h"
#include "TRXManager.h"
#include "UMTSPhCh.h"
#include "URRCDefs.h"
#include "UMTSL1Const.h"
#include "URRCTrCh.h"

#define DEBUGF(...) //printf(__VA_ARGS__)


//class ARFCNManager;


namespace UMTS {

// Outside refs.
class UMTSConfig;
class MacEngine;

class L1CCTrCh;
class L1TrChEncoder;
class L1TrChDecoder;
class TrChConfig;
class RrcTfs;

#if SAVEME
class DCHFEC;
// The list of currently in-use DCHFEC, maintained by RRC using the ChannelTree,
// to be used by the PHY layer.  There is a lock to prevent it from changing
// while you are trying to look at it.
// 9-2012 Update: the PHY layer does not use this, and no one calls open()/close() on DCH to populate this.
class DCHListType : public std::list<DCHFEC*> {
	public:
	Mutex mLock;
};
extern DCHListType gActiveDCH;
#endif

const unsigned gUlRawTfciSize = 30;	// There are 30 tfci bits in an uplink radio frame.
									// It is 2 * gFrameSlots

// 9-2012: New CCTrCh [Coded Composite Transport Channel] classes to support multiple TrCh.
// These classes are in addition to the old FEC classes; names begin with "L1" to distinguish them.

// Note that a DCH of a particular SF may be used for different purposes, eg, voice or PS,
// so allocation is three phase:  First we pre-allocate the DCH of various SF and put them
// in the ChannelTree (unchanged from before), then when a UE wants a DCH we allocate a DCH,
// which reserves that part of the ChannelTree, then apply the L1 programming corresponding
// to the intended use for DCH, and finally allocate the L1 encoder/decoders.
// Releasing a DCH will free the encoder/decoders, but we dont actually do that yet.

// The L1 programming is now completely separated from the L1 procedures defined in 25.212.
// The L1 procedures themselves are handled by a main L1CCTrCh class (replaces TrCHFEC) consisting
// of an L1CCTrChUplink and L1CCTrChDownlink, which handle the combined part of the L1 stack below the multiplexer,
// and each of which has an array of pointers to encoder/decoders to handle the individual TrCh above the multiplexer.
// The coder classes are locked to a particular coded block size, so there is one encoder/decoder for each size,
// which happens to be per-TFC in uplink and per-TF in downlink.  I also broke out the L1<->LogicalChannel interface
// into an L1ControlInterface class which worked well.

// The new L1 programming is encapsulated by L1CCTrChInfo class and descendents, which specify primarily the buffer
// size of each stage in the L1 stack.  There are three (count em) redundant sets of L1 stack programming procedures.
// Their output is a properly programmed L1CCTrChInfo object, of which there are two in a DCH (uplink and downlink.)
// The fecConfig() function is the full bore gruesomely complicated multi-trch procedure needed for AMR.
// To test that I wrote fecConfigForOneTrCh() which is a vast simplification for single-TrCh.
// And to test that, I wrote fecConfigTrivial() which does the single TB sub-case we were using previously.

// Eventually we could get rid of the old classes and map BCH, RACH and FACH into these, but there is no need.
// In downlink, these take a TBS [Transport Block Set], which specifies a TFCI and 0 or more blocks for each channel.
// The TBS encapsulates everything to be sent in one time period, which is currently a TTI period.
// Currently all TrCh in the CCTrCh must use the same TTI.
// In uplink, the top of the decoder connects directly to a simple MAC to send one TB [Transport Block] at a time,
// since there is no need for coordination between the channels.
//
// L1CCTrCh is the top level class.
// It incorporates L1CCTrChDownlink and L1CCTrChUplink to handle the downlink/uplink CCTrCh
// (the bottom part of 25.212 4.2 figure 1 and 2 where the TrCh have been combined),
// and a bunch of L1Encoder/L1Decoder to handle the TrCh (the top part of the two figures, which is per-TrCh.)
// Each encoder/decoder is locked to a particular size of coded bits.
// The downlink needs one encoder/decoder for each TF [Transport Format.]
// The uplink needs one encoder/decoder for each TFC [Transport Format Combination.]
// L1CCTrCh also has a pointer to a specific a PhCh class that knows about the physical channel.
// The programming for the above is horrendous, and is encapsulated by L1TrChProgInfo and related L1FecProgInfo and L1TrChProgInfo.
// Setting up a CCTrCh involves these steps:
// A DCHFEC (or other FEC) is a thin wrapper over an L1CCTrCh.
// The L1CCTrCh may be pre-allocated tied to a PhCh, which in downlink is locked to a particular SF,
// to create a DCHFEC,etc, with a specified bandwidth, but without knowing any other programming.
// When someone wants to use the DCHFEC, the RRC specifies the TrCh configuration, which may be different for CS or PS services.
// The RRC TrChConfig is used first to program the L1TrChProgInfo, then that is applied to the L1CCTrCh to allocate
// all the encoder/decoder classes.


/* 2011 comments:
Determining the constants.

From the standard we know:
 * All frame sizes for the BCH.
  * transport block is 246 bits
  * there are two radio frames, 270 bits each
  * TTI is 20 ms
  * SF is 256
  * parity word Li is 16 bits
 * For all downlink TrCH except BCH, the radio frame size is  2*38400/SF = 76800/SF.
  * For SF=256 that's 300.
  * For SF=4 that's 19200.
 * The maximum code block size for convulutional coding is 540 bits (25.212 4.2.2.2).
  * That corresponds to a radio frame size of 1080 bits, or a spreading factor of 71,
    meaning that the smallest spreading factor that can be used is 128.
  * 76800/128 = 600 coded bits -> roughly 300 data bits.
  * That corresponds to an input rate of roughly 30 kb/s at.
 * The maximum code block size for turbo coding is 5114 bits (25.212 4.2.2.2).
  * That corresponds to a radio frame size of 15342 bits, or a spreading factor of 5,
    meaning that the smallest spreading factor that can be used is 8.
  * 76800/8 = 9600 coded bits -> roughly 3200 data bits.
  * That corresponds to an input rate of roughly 320 kb/s.

(David) OK - SO HOW DO YOU GET HIGHER RATES?? HOW CAN YOU USE SF=4??
(pat) A: Use the full 5114-but code block and then expand it with rate-matching.
You still can't get the full ~640 kb/s implied by SF=4, but you get to ~500 kb/s.

(pat) A: They considered this problem.  See 25.212 4.2.2.2 Code block segmentation.
In Layer1, after transport block concatenation, you then simply chop the result up
into the largest pieces that can go through the encoder, then put them back together after.

From "higher layers" we are given:
 * SF: 4, 8, 16, 32, 64, 128, 256.
 * P: 24, 16, 8, 0 bits
 * TTI: 10, 20, 40 ms.

To simplify things, we set:
 * TTI 10 ms always on DCH and FACH, 20 ms on PCH and BCH
 * BCH and PCH are always rate-1/2 convolutional code
 * DCH and FACH are always rate-1/3 turbo code
 * no rate-matching, no puncturing
 * parity word is always 16 bits
 * So the only parameter than changes is spreading factor.

 * We will only support 15-slot (non-compressed) slot formats.


From our simplifications we also know:
  * For non-BCH/PCH TrCH there is one radio frame,
    76800/SF channel (coded) bits, per transport block.
  * DCH and FACH always use rate-1/3 turbo code,
    which has 12 terminating bits in the output.
  * For DCH and FACH, the transport block size is
    ((76800/SF - 12)/3) - P = (25600/SF) - 4 - P data bits,
    where P is the parity word size.
   * Fix P=16 for simplicity and transport block size is (25600/SF) - 20.
   * for SF=256, that's 80 bits.
   * for SF=16, that's 1580 bits.
   * for SF=8, that's 3180 bits.

 * For PCH there is one radio frame,
    76800/SF channel (coded) bits, per transport block.
 * SF=64, for that's 1200 channel bits.
 * It's a rate-1/2 conv code, so that's 1200/2 - 8 - P data bits.
 * P=16 so that's 1200/2 - 24 = 576 transport bits.  Really?

*/



// The Rrc determines the information necessary to program the TrCh encoder/decoder,
// and passes it to the TrChFec classes using this structure.
// The encoder/decoders dont particularly care if the information happens to
// be static, semi-static, or dynamic at the Rrc level, they just want all the info,
// so here it is.
// This info struct is per-TrCh, per TFC [Transport Format Combination].
	// The dynamic info in the TFS is the TB Size and the number of TB.
	// I see no reason to change TB Size per TrCh, ever, so for us it
	// will just be number of TB.  Why?  So when we support multiple TrCh
	// we can slice the high-rate channels into a number of TB and the
	// only thing we will change is how many of those TB go to each TrCh each time.
	// For example, there will a data-trch and a control-trch,
	// and the TFS will be:
	// 		TFS 0: 10 x data-trch + 0 x control-trch
	//		TFS 1:  9 x data-trch + 1 x control-trch.
	// For rate-matching, 4.2.7 equation 1 specifies Zi,j, which then specifies Ni,j,
	// the number of bits per trch before rate-matching.
	// If TB for different channels are different sizes, then the eini for
	// rate-matching the  ...
	// When we support multiple TrCh, The coded block size for this particular TrCh.
// This info is for just one encoder or decoder, and is typically different
// for uplink or downlink.
// ===== TrCh encoding sizes for steps ======
// 25.212 4.2 figure 1: TrCh for uplink.
// V CRC Attachment V
// V TrBk concatenation and Code block segmentation V
// V Channel coding V
// V Radio frame equalisation V
// 
// 
// 25.212 4.2 figure 2: TrCh for downlink.
// 			+ blocks to/from MAC +
// 			(common path)
// 				mTBSz * mNumTB
// 			V CRC Attachment V
//				uses mPB.
// 			V TrBk concatenation and Code block segmentation V
//				uses mCodeFillBits
// 			V Channel coding V
//				mCodeInBkSz, per-tti not per-radio-frame.
//						(uplink path)
//						V Radio frame equalisation (uplink only) V
//							new: uses mRadioFrameEqualisationFillBits;
//						V 1st interleaving (uplink location) V
//							(Xi is already guaranteed to be evenly divisible by Fi)
// 						V Radio frame segmentation V
//						V rate matching V
// (downlink path)
// V rate matching V
//	old:mRadioFrameSz new: mRmOutSz
// V 1st insertion of DTX indication (downlink only) V
//	new:
// V 1st interleaving V
// V Radio frame segmentation V
// 			(common path)
// 			V TrCh Multiplexing (Combine TrCh into CCTrCh) V
// (downlink path)
// V 2nd insertion of DTX indication (downlink only) V
// 			(common path)
// 			V Physical channel segmentation (we dont need) V
// 			V 2nd interleaving V
// 			V Physical channel mapping (we dont need) V
// There is one of these for each TrCh.


class L1CCTrChInfo;
struct L1TrChProgInfo;

class L1FecProgInfo : public RrcDefs, public Text2Str {
	friend class L1CCTrChInfo;
	friend class L1TrChEncoder;
	private:
	L1CCTrChInfo *mInfoParent;

	public:
	unsigned mCCTrChIndex;	// Which TrCh in the CCTrCh are we?
	unsigned mTBSz;		// The all important Transport Block Size in bits.
	unsigned mNumTB;		// Number of TB.
	unsigned mCodedSz;		// for the whole TTI, not just the radio-frame.
							// The mCodedSz may include some fill bits but we dont save them separately here.
	unsigned mCodeInBkSz;	// Size (low-side) of just one code block in the mCodedSz
	unsigned mCodeFillBits;	// Decoder only; Number of fill bits generated during code block segmentation
	// Rate-matcher for downlink flows mHighSideRMSz => mLowSideRMSz
	// Rate-matcher for uplink flows mLowSideRMSz => mHighSideRMSz
	// Note that we do not puncture, which terminology is confusing because we do remove bits
	// in uplink, however we are un-rate-matching what the UE did in uplink, which was rate-matching
	// by bit replication.  The distinction is important because turbo-coded puncturing is special.
	unsigned mHighSideRMSz;		// Rate-match high side.
								// For downlink: mCodedSz == mHighSideRMSz.
								// For uplink: radio frame equalisation intervenes and results in:
								//		mCodedSz <= mHighSideRMSz < mCodedSz + getNumRadioFrames()
	unsigned mLowSideRMSz;		// Rate-match low side, the result of the hideous 4.2.7 computation.

	// For downlink, the RF [Radio Frame] sizes are the same in all TFC:
	unsigned mRFSegmentSize;	// Number of bits in radio frame for this TrCh.
	unsigned mRFSegmentOffset;	// Offset of start of of bits in radio frame for this TrCh.
	UInt_z mSFLog2;			// Yes, this varies per-TFC for uplink channels.
					// Need this to decode DPDCH on uplink, since TFCI determines the TFC which in turn sets the SF. 

	TfIndex mTfi;		// The index into the mEncoders or mDecoders array.
						// For downlink there is only one encoder per TF, so each TFC mapping to that TF has the same mTfi.
						// For uplink, currently mTfi == TFC index, but when/if we start allowing multiple different TTI
						// in the same CCTrCh, and if the uplink multiple radio frame aggregation buffer is in the
						// decoder class, then all the TFC must map the longer TTI trch to the same decoder.
						// In that case, we may also have to save the TFCI across the shorter TTI border?
						// Not sure how the TFCI works in that case, if the second TFCI includes the TFC mapping
						// to the longer TTI or not.

	unsigned inter1Columns() const { return TrCHConsts::inter1Columns[getTTICode()]; }
	const char* inter1Perm() const { return TrCHConsts::inter1Perm[getTTICode()]; }
	unsigned getNumTB() const { return mNumTB; }
	unsigned getTBSize() const { return mTBSz; }
	unsigned getPB() const;
	TTICodes getTTICode() const;
	unsigned getNumRadioFrames() const;
	L1TrChProgInfo *getTCI();
	L1TrChProgInfo *getTCI() const;	// idiotic language
	void text(std::ostream &os) const;
	void musteql(L1FecProgInfo *other);
};


struct L1TrChProgInfo : RrcDefs {
	// These are semi-static, meaning they are constant per TrCh.
	UInt_z mPB;				// Parity bits.
	TTICodes mTTICode;		// We do not use 'dynamic' so for us this is semi-static.
	bool mIsTurbo;
	// In uplink, the rate-matching parameters vary per TFC, and eplus and eminus can be computed
	// on the fly from the mPreRmSz and mPostRmSz.
	// For downlink the eplus and eminus parameters are constants computed from the largest
	// TF rather than depending the sizes in each TF, so have to precompute them and save them here.
	L1FecProgInfo perTFC[maxTfc];
};


// There is one of these for uplink and one for downlink for each L1CCTrCh.
class L1CCTrChInfo : public RrcDefs, public Text2Str {
	L1TrChProgInfo mPerTrCh[maxTrCh];
	public:
	unsigned mNumTfc;
	unsigned mNumTrCh;
	unsigned l1GetNumRadioFrames(TrChId tcid) {
		return TTICode2NumFrames(mPerTrCh[tcid].mTTICode);
	}

	L1FecProgInfo *getFPI(TrChId i, TfcId j) {
		assert(i < maxTrCh && j < maxTfc);
		return &mPerTrCh[i].perTFC[j];
	}
	L1TrChProgInfo *getTCI(TrChId i) { return &mPerTrCh[i]; }

	// FIXME: This needs to be programmed somewhere.
	unsigned getTfciSize() const { return 2; }
	unsigned getNumTfc() { return mNumTfc; }
	unsigned l1GetLargestCodedSz(TrChId i);		// Return the largest CodedSz for this TrCh.

	unsigned getNumTrCh() { return mNumTrCh; }
	//RrcTfcs *getTfcs() const { return mChList->getTfcs(); }
	L1CCTrChInfo();
	void text(std::ostream &os) const;
	void musteql(L1CCTrChInfo &other);
	bool isTrivial();

	// This class is normally programmed by fecConfig.
	// These two simplified programming functions can be used to test fecConfig.
	void fecConfigTrivial(unsigned wSF,TTICodes wTTICode,unsigned wPB,unsigned wDlRadioFrameSz);
	bool fecConfigForOneTrCh(bool isDownlink, unsigned wSF,TTICodes wTTICode,unsigned wPB,
		unsigned wRadioFrameSz, unsigned wTBSz, unsigned wMinTB, unsigned wMaxTB, bool wTurbo);
};

class L1FER {
	/**@name Atomic volatiles, no mutex. */
	// Yes, I realize we're violating our own rules here. -- DAB
	//@{
	volatile float mFER;						///< current FER estimate
	static const int mFERMemory=20;				///< FER decay time, in frames
	//@}
	public:
	L1FER() : mFER(0.0) {}
	void countGoodFrame();
	void countBadFrame();
	/** Total frame error rate since last open(). */
	float FER() const { return mFER; }
};


/**
	Abstract class for transport channel encoders.
	In most subclasses, writeHighSide() drives the processing.
*/
class L1TrChEncoder
{
	protected:
		L1CCTrCh *mParent;
		int mDlEplus, mDlEminus;		// Downlink pre-computed rate matching parameters.

	/**
	  Process pending transport blocks and/or generate filler and enqueue the resulting timeslots.
	  This method may block briefly, up to about 1/2 second.  (pat) Dont think this is true.
	  This method is meaningless for some suclasses.
	*/
	private:
		BitVector crcAndTBConcatenationBuf;
		BitVector rateMatchingBuf;
		BitVector firstDtxBuf;
		BitVector firstInterleaveBuf;
	public:
		void l1CrcAndTBConcatenation(L1FecProgInfo *fpi, TransportBlock const *tblocks[RrcDefs::maxTbPerTrCh]);
		void l1ChannelCoding(L1FecProgInfo *fpi, BitVector &catbuf);
		void l1RateMatching(L1FecProgInfo *fpi, BitVector &catbuf);
		void l1FirstDTXInsertion(L1FecProgInfo *fpi, BitVector &g);

		/**
			The basic encoder constructor.
			@param wParent The containing class.
		*/
		L1TrChEncoder(L1CCTrCh *wParent, L1FecProgInfo *wfpi);

		virtual ~L1TrChEncoder() {}

	protected:
		// Interface to the convolutional or turbo coder:
		/** 25.212 4.2.2: Z is defined as the maximum code block size for this encoder. */
		virtual unsigned getZ() const =0;
		/** Apply the actual convolutional/turbo encoder. */
		virtual void encode(BitVector& in, BitVector& c) = 0;
		virtual bool isTurbo() const = 0;
};


/**
	An abstract class for L1TrCHFEC decoders.
	writeLowSide() drives the processing.
*/
class L1TrChDecoder : public L1FER
{
	protected:
		L1CCTrCh *mParent;
		//L1FecProgInfo *mFpi;
		SoftVector mRMBuf;		// Rate-match  buffer.
		SoftVector mDTtiBuf;		// A full TTI of data.
		unsigned mDTtiIndex;	// Incoming index in mDTtti in the range 0..8, depending on TTI
		int mEini[8];			// Uplink pre-computed rate matching parameters.


		/** Connect the upstream MacEngine.  */
		// Return the old one, used for testing.
		MacEngine * mUpstream;
		public:
		MacEngine * l1SetUpstream(MacEngine * wUpstream, bool dontworrybehappy=false)
		{
			MacEngine *old = mUpstream;
			assert(wUpstream==NULL || mUpstream==NULL || dontworrybehappy);	// Only call this once.
			mUpstream=wUpstream;
			return old;
		}

		/** Constructor for an L1TrCHFECDecoder.
			@param wParent The containing class.
		*/
		L1TrChDecoder(L1CCTrCh* wParent, L1FecProgInfo *fpi);

		virtual ~L1TrChDecoder() { }


	private:
		BitVector decodingInBuf;
		BitVector decodingOutBuf;
		BitVector expectParity;
	public:
		void l1RateMatching(L1FecProgInfo *fpi, SoftVector &f, unsigned frameIndex);
		void l1RadioFrameUnsegmentation(L1FecProgInfo *fpi, const SoftVector&e);
		void l1FirstDeinterleave(L1FecProgInfo *fpi, const SoftVector &d);
		void l1ChannelDecoding(L1FecProgInfo *fpi, const SoftVector &);
		void l1Deconcatenation(L1FecProgInfo *fpi, BitVector &);

	protected:
		// Interface to the convolutional or turbo coder:
		/** Invoke the actual decoder. */
		virtual void decode(const SoftVector& c, BitVector& o) = 0;
		virtual bool isTurbo() const = 0;
		/** 25.212 4.2.2: Z is defined as the maximum code block size for this encoder. */
		virtual unsigned getZ() const =0;
};


// Single TransportBlock encoder.
// It is Rate 1/2 Convolutional, which is dictated for BCH, PCH, and one option for SCCPCH and DCH.
class L1TrChEncoderLowRate : public L1TrChEncoder
{	protected:
	ViterbiR2O9 mVCoder;

	public:
	L1TrChEncoderLowRate(L1CCTrCh *wParent,L1FecProgInfo *wfpi) : L1TrChEncoder(wParent,wfpi) {}

	void encode(BitVector& in, BitVector& c);
	unsigned getZ() const { return 504; }		// Max convolutional block size is a constant from 25.212 4.2.3
	bool isTurbo() const { return false; }
};


// It is Rate 1/2 Convolutional, which is dictated for RACH and one option for DCH.
class L1TrChDecoderLowRate : public L1TrChDecoder
{	protected:
	ViterbiR2O9 mVCoder;

	public:
	L1TrChDecoderLowRate(L1CCTrCh *wParent,L1FecProgInfo *wfpi) : L1TrChDecoder(wParent,wfpi) {}

	void decode(const SoftVector& c, BitVector& o);
	unsigned getZ() const { return 504; }		// Max convolutional block size is a constant from 25.212 4.2.3
	bool isTurbo() const { return false; }
};

// (pat) The interleaver is locked to the input size so we need one of these for every transport set size.
class L1TrChEncoderTurbo : public L1TrChEncoder
{	protected:
	ViterbiTurbo mTCoder;
	TurboInterleaver mInterleaver;

	public:
	L1TrChEncoderTurbo(L1CCTrCh *wParent,L1FecProgInfo *wfpi) :
		L1TrChEncoder(wParent,wfpi),
		mInterleaver(wfpi->mCodeInBkSz)
	{ }

	void encode(BitVector& in, BitVector& c);
	unsigned getZ() const { return 5114; }		// Max Turbo encoder block size is a constant from 25.212 4.2.3
	bool isTurbo() const { return true; }
};


class L1TrChDecoderTurbo : public L1TrChDecoder
{	protected:
	ViterbiTurbo mTCoder;
	TurboInterleaver mInterleaver;
	public:
	L1TrChDecoderTurbo(L1CCTrCh *wParent,L1FecProgInfo *wfpi) :
		L1TrChDecoder(wParent,wfpi),
		mInterleaver(wfpi->mCodeInBkSz)
	{ }

	void decode(const SoftVector& c, BitVector& o);
	unsigned getZ() const { return 5114; }		// Max Turbo encoder block size is a constant from 25.212 4.2.3
	bool isTurbo() const { return true; }
};


class L1CCBase {
	protected:
		PhCh *mPhCh;
	public:
		L1CCBase() { DEBUGF("construct L1CCBase\n"); }
		PhCh *getPhCh() { return mPhCh; }
		//unsigned getSpCode() const { return mPhCh->SpCode(); }
		//unsigned getSrCode() const { return mPhCh->SrCode(); }
		//unsigned SF() const { return mPhCh->SF(); } // Dl or Ul?
		//unsigned getDlSF() const { return mPhCh->getDlSF(); }
};

class L1CCTrChUplink : public L1CCTrChInfo, public virtual L1CCBase {
	protected:
	unsigned mSlotSize;		// Size of slots in current accumulating radio vector.
	UMTS::Time mReceiveTime;// Time stamp of most recent incoming burst.
	float mRawTfciAccumulator[gUlRawTfciSize];	// Incoming raw radio slot tfci bits.
	SoftVector *mDTti;		// A full TTI of data.
	unsigned mDTtiIndex;	// Incoming index in mDTtti in the range 0..8, depending on TTI

	// We dont know the sizes of these until the TF are programmed.

	// In uplink, there is one decoder size for each TFC because the number
	// of bits varies per TFC.
	L1TrChDecoder* mDecoders[RrcDefs::maxTrCh][RrcDefs::maxTfc];

	public:

        UMTS::Time mLastTPCTime;
	bool mReceived; 		// false until we receive some data, i.e. until TFCI!=0	

	L1CCTrChUplink() {
		mLastTPCTime = UMTS::Time(0,0);
		mReceived = false;
		memset(mDecoders,0,sizeof(mDecoders));
		DEBUGF("construct L1CCTrChUplink\n");
	}

	/** Send in an RxBurst for decoding. */
	public:    void l1WriteLowSide(const RxBitsBurst& burst);
	public:    void l1WriteLowSideFrame(const RxBitsBurst &burst, float tfci[30]);
	private:   SoftVector mDSlotAccumulatorBuf;	// uplink data in
	protected: void l1AccumulateSlots(const SoftVector *e, const float tfcibits[2]);
	private:   SoftVector mHDIBuf;		// uplink 2nd De-interleaving buffer.
	protected: void l1SecondDeinterleaving(SoftVector &e, unsigned tfci, unsigned frameIndex);
	protected: void l1Demultiplexer(SoftVector &e, unsigned tfci, unsigned frameIndex);
	protected: SoftVector mFillerBurst;
	public:    SoftVector *l1FillerBurst(unsigned size);

	MacEngine * l1SetUpstream(MacEngine * wUpstream, TrChId tcid=0) {
		assert(tcid == 0);			// TODO: implement me
		assert(getNumTrCh() == 1); 	// TODO: implement me
		for (TfcId j = 0; j < RrcDefs::maxTfc; j++) {
			if (mDecoders[tcid][j]) { mDecoders[tcid][j]->l1SetUpstream(wUpstream); }
		}
		return NULL;
	}
	void l1InstantiateUplink();
};


class L1CCTrChDownlink : public L1CCTrChInfo, public virtual L1CCBase
{
	protected:
		// In downlink, there is one encoder size for each TF (not per TFC) because the largest
		// TF is always used for each TrCh.  (The L1FecProgInfo are still per TFC because
		// consequently the rate-matching buffer sizes change per TFC.)
		L1TrChEncoder* mEncoders[RrcDefs::maxTrCh][RrcDefs::maxTfPerTrCh];
		// The data from the multiple TrCh is saved up here.  We need 8 for
		// a TTI worth of write-ahead.
		UMTS::Time mNextWriteTime;		///< timestamp of next generated burst
		UMTS::Time mPrevWriteTime;		///< timestamp of most recent generated burst
		UInt_z mTotalBursts;			///< total bursts sent since last open()

	private:
		BitVector mMultiplexerBuf[8];
		BitVector mYinBuf, mYoutBuf;
		BitVector mRadioSlotBuf;

	public:
		L1CCTrChDownlink() {
			memset(mEncoders,0,sizeof(mEncoders));
			memset(mMultiplexerBuf,0,sizeof(mMultiplexerBuf));	// To catch bugs.
			DEBUGF("construct L1CCTrChDownlink\n");
		}
		void l1DownlinkOpen();

		// Downlink parts.
		void l1WriteHighSide(const TransportBlock &tb);	// For the channels without a TFS that send just one TB.
		void l1WriteHighSide(const MacTbs &tbs);	// For the channels that use non-trivial TFS
		void l1Multiplexer(L1FecProgInfo *fpi, BitVector& frame, unsigned intraTTIFrameNum);
		void l1SendFrame2(BitVector& frame, unsigned tfci);
		void l1PushRadioFrames(int tfci);

		UMTS::Time nextWriteTime() const { return mNextWriteTime; }	// Used by BCHFEC
		void l1WaitToSend() const;	// Used by BCHFEC

		unsigned l1GetDlTrBkSz() {	// Used for simple channels with 1 TrCh, not TFCS.
			LOG(NOTICE) << "getNumTrCh: " << getNumTrCh();
			assert(getNumTrCh() == 1);
			assert(isTrivial());
			return getFPI(0,0)->getTBSize();
		}
		void l1InstantiateDownlink();
};

// The interface to use these channels for voice.
class L1ControlInterface : public L1FER {
	mutable Mutex mLock;				///< access control
	// TODO UMTS -- We need the correct values for these timers.
	Bool_z mActive;				///< true between open() and close()
	GSM::Z100Timer mAssignmentTimer;
	GSM::Z100Timer mReleaseTimer;
	char mDescriptiveString[100];		///< a human-readable description of channel config
	public:
	L1ControlInterface() :
		mAssignmentTimer(10000),
		mReleaseTimer(10000)
		{
			DEBUGF("construct L1ControlInterface\n");
		}
	bool active() const { return mActive; }
	bool recyclable() const;
	// (pat) These are unused except for DCHFEC.
	void controlOpen();
	void controlClose(bool hardRelease=false);
	void countGoodFrame();
	void countBadFrame();
	const char* descriptiveString() const { return mDescriptiveString; }
};


class L1CCTrCh :
	public virtual L1CCBase, // Construct this before others.
	public L1CCTrChDownlink,
	public L1CCTrChUplink,
	public L1ControlInterface
	//public MacL1FecInterface
{
	public:
	int getMaxUlRadioFrameSz() { return 1000; }	// TODO
	L1CCTrCh(PhCh *wPhCh) { mPhCh = wPhCh; DEBUGF("construct L1CCTrCh\n");; }
	void fecConfig(TrChConfig &config);
	unsigned l1GetDlNumRadioFrames(TrChId tcid) { return L1CCTrChDownlink::l1GetNumRadioFrames(tcid); }
	L1CCTrChDownlink *l1dl() { return (L1CCTrChDownlink*)this; }
	L1CCTrChUplink *l1ul() { return (L1CCTrChUplink*)this; }
	L1ControlInterface *getControlInterface() { return this; }
};

// TODO: Encapsulates the service loop for the BCHFEC class.
#if 0
class L1Generator {
	//Thread mServiceThread;	// This should be in any descendent FEC class that needs it, not here.
	//volatile bool mRunning;			///< true while the service loop is running
	//void waitToSend() const
	//	{ assert(mEncoder); mEncoder->waitToSend(); }
	//UMTS::Time nextWriteTime() const
	//	{ assert(mEncoder); return mEncoder->nextWriteTime(); }

	void generate() { assert(0); }

	friend void* TrCHServiceLoop(TrCHFEC*);
};
#endif

//void* TrCHServiceLoop(L1CCTrCh*);



#if 0
class L1DCHFEC : public PhCh , public L1CCTrCh	// multiple TFC, multiple TB
{
	public:
	// Leave the encoders undefined until we know what the use of the channel
	// is going to be, then call fecConfig.
	L1DCHFEC(unsigned wDlSF, unsigned wSpCode, unsigned wUlSF, unsigned wSrCode, ARFCNManager *wRadio)
		: PhCh(DPDCHType, wDlSF, wSpCode, wUlSF, wSrCode, wRadio)
		, L1CCTrCh(this)
		{
		}
	void open();
	void close(bool hardRelease=false);
};
#endif



// TODO: Still need PICH, which is super special - no coding.



} // namespace UMTS



#endif


// vim: ts=4 sw=4
