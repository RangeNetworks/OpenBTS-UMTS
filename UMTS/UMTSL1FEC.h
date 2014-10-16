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

#ifndef UMTSL1FEC_H
#define UMTSL1FEC_H

#include <stdlib.h>
#include <BitVector.h>
#include <TurboCoder.h>
#include <Interthread.h>
#include "GSMCommon.h"
#include "UMTSCommon.h"
//#include "UMTSTransfer.h"
#include "UMTSL1Const.h"
#include "UMTSL1CC.h"
#include "MACEngine.h"
#include "TRXManager.h"
#include "UMTSPhCh.h"
#include "URRCDefs.h"


//class ARFCNManager;


namespace UMTS {

// Outside refs.
class UMTSConfig;
class MacEngine;
// Forward refs.
class L1CCTrCh;
class TrCHFEC;
class TrCHFECEncoder;
class TrCHFECDecoder;
class DCHFEC;
class TrChInfo;
class TrChConfig;
class RrcTfs;

// The list of currently in-use DCHFEC, maintained by RRC using the ChannelTree,
// to be used by the PHY layer.  There is a lock to prevent it from changing
// while you are trying to look at it.
class DCHListType : public std::list<DCHFEC*> {
	public:
	Mutex mLock;
	bool inTxUse,inRxUse;
};

extern DCHListType gActiveDCH;

const unsigned gUlTfciSize = 30;	// There are 30 tfci bits in an uplink radio frame.


/*
How this works: How TrCH classes are built.
 * There is a TrCH base class set: TrCHECEncoder, TrCHFECDecoder, TrCHFEC.
 * These base classes implement most of the mechanics of the TrCH,
   differing only in channel-specific parameters.
 * Channel-specific parameters are provided by virtual methods,
   defined differently in the different base classes.
 * Parameters passed down to the ARFCNManager in the TxBitsBurst objects
   define the PhCH on transmit.
 * A map of channels in the ARFCNManager give the PhCH processors access to PhCH parameters for decoding recevied bursts.
*/


/*
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

OK - SO HOW DO YOU GET HIGHER RATES?? HOW CAN YOU USE SF=4??
A: Use the full 5114-but code block and then expand it with rate-matching.
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

// Moved to UMTSL1Const.h:
//class TrCHConsts {
//
//	public:
//
//	// 25.212, 4.2.1
//	static const uint64_t mgcrc24 = 0x1800063;
//	static const uint64_t mgcrc16 = 0x11021;
//	static const uint64_t mgcrc12 = 0x180f;
//	static const uint64_t mgcrc8 = 0x19b;
//	static const unsigned sMaxTfci = 256;	// Maximum TFCI we will ever use is 8 bits.
//	static const char inter2Perm[30];
//	// TFCI can be up to 10 bits, but we wont use them all.
//	static uint32_t sTfciCodes[sMaxTfci];	// Table for up to 8 bit tfci, plenty for us.
//	static void initTfciCodes();
//
//	// These are the pre-computed pilot patterns for Npilot =2,4,8,16.
//	static uint16_t sDlPilotBitPattern[4][15];
//	static const bool reedMullerTable[32][10];
//	static void initPilotBitPatterns();
//	static bool oneTimeInit;
//
//	TTICodes mTTImsDiv10Log2;
//	unsigned int mInter1Columns;
//	char *mInter1Perm;
//
//	TrCHConsts(TTICodes wTTImsDiv10Log2);
//};


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
struct FecProgInfo {
	public:
	UInt_z mSF;			// Yes, this varies per-TFC for uplink channels.
	TTICodes mTTICode;		// For us this is static or semi-static.
	UInt_z mTBSz;		// The all important Transport Block Size in bits.
	UInt_z mPB;			// Parity bits.
	unsigned mNumTB;		// Number of TB (currently, 1)
	// (pat) Note that mCodeBkSz is coded size of one block and mCodedBkSz is coded size of all blocks.
	// I am renaming these in the new fec.
	UInt_z mCodedBkSz;		// Per-TTI, not per radio-frame.
	UInt_z mFillBits;		// Number of fill bits generated during code block segmentation
	UInt_z mCodeBkSz;		// size of code blocks (after code block segmentation, prior to encoding)
	UInt_z mRadioFrameSz;	// The number of bits from the radio frame allocated to this TrCh.
							// Needed for rate matching.  In the trivial case of one trch,
							// it is simply calculated from the SF and channel type but it
							// is needed when there are multiple TrCh with multiple TFC,
							// or if the RACH SF < 256.  For UL RACH the UE picks
							// the minimum UL SF that will hold all the bits in the TFC.
	bool mTurbo;		// true if turbo coding is used

	/** The transport block size used by this encoder. */
	unsigned trBkSz() { return mTBSz; }
	/** Actual code block size for this encoder/decoder, before rate-matching. */
	unsigned codedBkSz() { return mCodedBkSz; }
	unsigned fillBits() { return mFillBits; }
	unsigned getPB() { return mPB; }
	TTICodes getTTICode() const { return mTTICode; }
	unsigned getNumRadioFrames() const;
	bool isTurbo() { return mTurbo; }
	
	// mTBSz and mCodedBkSz and mFillBits must be filled in by the descendent class constructors.
	FecProgInfo(unsigned wSF,TTICodes wTTICode,unsigned wPB,unsigned wRadioFrameSz, bool wTurbo = false)
		: mSF(wSF),mTTICode(wTTICode),mPB(wPB),mNumTB(1),mRadioFrameSz(wRadioFrameSz),mTurbo(wTurbo)
		{}
};

// This class is used to init FecProgInfo for simple channels like BCH.
struct FecProgInfoSimple : public FecProgInfo {
	FecProgInfoSimple(unsigned wSF,TTICodes wTTICode,unsigned wPB,unsigned wRadioFrameSz);
};

// This class is used to init the FecProgInfo for a full support channels: rach/fach/dch.
// TODO: Need a different constructor for Turbo-coded.
struct FecProgInfoInit : public FecProgInfo {
	FecProgInfoInit(unsigned wSF,TTICodes wTTICode,unsigned wPB,
		unsigned wRadioFrameSz, unsigned wTBSz, bool wTurbo = false);
};



// Base class for TrCHFECEncoder and TrCHFECDecoder
class TrCHFECBase: public FecProgInfo
{	protected:
	/**@name Parameters fixed by the constructor, not requiring mutex protection. */
	//@{
	TrCHFEC* mParent;			///< a containing TrCHFEC processor, if any
	//TrCHConsts mConsts;
	//@}

	public:
	TrCHFECBase(TrCHFEC*wParent,FecProgInfo&fpi):
		FecProgInfo(fpi), mParent(wParent) //, mConsts(fpi.mTTICode)
		{}

	//unsigned inter1Columns() const { return mConsts.mInter1Columns; }
	//const char* inter1Perm() const { return mConsts.mInter1Perm; }
	unsigned inter1Columns() const { return TrCHConsts::inter1Columns[getTTICode()]; }
	const char* inter1Perm() const { return TrCHConsts::inter1Perm[getTTICode()]; }
	//TTICodes getTTICode() const { return mConsts.mTTImsDiv10Log2; }

	TrCHFEC* parent() const { return mParent; }

	/**@name Components of the channel description. */
	//@{
	PhCh *getPhCh() const;
	unsigned SpCode() const { return getPhCh()->SpCode(); }
	unsigned SrCode() const { return getPhCh()->SrCode(); }
	// Not used:
	//unsigned getDlRadioFrameSize() const { return getPhCh()->getDlRadioFrameSize(); }
	//unsigned getUlRadioFrameSize() const { return getPhCh()->getUlRadioFrameSize(); }
	// Number of radio frames based on TTI.
	// The rate-matching parameters for this TrCh per radio frame in the TTI.
	int mEini[8];
	//@}
};

// Return the transport block size to avoid rate matching by making
// the coded transport block size exactly match the radio frame.
// Update: These are useless in the face of rate-matching.
//extern unsigned singleTransportBlockSizeForLowRate(unsigned codedBkSz, unsigned pb);
//extern unsigned singleTransportBlockSizeForTurbo(unsigned codedBkSz,unsigned pb);

/**
	Abstract class for transport channel encoders.
	In most subclasses, writeHighSide() drives the processing.
*/
class TrCHFECEncoder : public TrCHFECBase
{
	protected:

	//ARFCNManager *mDownstream;  Moved to PhCh

	/**@name Multithread access control and data shared across threads. */
	//@{
	mutable Mutex mLock;
	//@}

	/**@ Internal state. */
	//@{
	UInt_z mTotalBursts;			///< total bursts sent since last open()
	// (pat) These times are used only by the beacon:
	UMTS::Time mPrevWriteTime;		///< timestamp of most recent generated burst
	UMTS::Time mNextWriteTime;		///< timestamp of next generated burst
	volatile bool mRunning;			///< true while the service loop is running
	Bool_z mActive;				///< true between open() and close()
	//unsigned mPB;
	//@}

	char mDescriptiveString[100];		///< a human-readable description of channel config

	public:

	/**
		The basic encoder constructor.
		@param wParent The containing TrCHFEC, for sibling access -- may be NULL.
	*/
	TrCHFECEncoder(TrCHFEC *wParent,FecProgInfo&fpi) : TrCHFECBase(wParent,fpi) {/*mDownstream = NULL;*/}

	virtual ~TrCHFECEncoder() {}

	/** Set the transceiver pointer.  */
	//virtual void setDownstream(ARFCNManager *wDownstream)
	//{
	//	assert(mDownstream==NULL);	// Don't call this twice.
	//	mDownstream=wDownstream;
	//}

	/**@name Accessors. */
	//@{

	/** 25.212 4.2.2: Z is defined as the maximum code block size for this encoder. */
	virtual unsigned getZ() const =0;

	UMTS::Time nextWriteTime() const { ScopedLock lock(mLock); return mNextWriteTime; }

	//@}

	/** Close the channel after blocking for flush.  */
	virtual void close();

	/** Open the channel for a new transaction.  */
	virtual void open();

	/**
		Returns true if the channel is in use by a transaction.
		For broadcast and unicast channels this is always true.
		For dedicated channels, this is taken from the sibling deocder.
	*/
	virtual bool active() const { ScopedLock lock(mLock); return mActive; }

	unsigned SF() const { return getPhCh()->getDlSF(); }

	/**
	  Process pending transport blocks and/or generate filler and enqueue the resulting timeslots.
	  This method may block briefly, up to about 1/2 second.
	  This method is meaningless for some suclasses.
	*/
	virtual void writeHighSide(const TransportBlock&);

	/** Start the service loop thread, if there is one.  */
	virtual void start() { mRunning=true; }

	const char* descriptiveString() const { return mDescriptiveString; }

	protected:

	/** Roll write times forward to the next positions. */
	void rollForward();

	/** Return pointer to paired TrCHFEC decoder, if any. */
	virtual TrCHFECDecoder* sibling();

	/** Return pointer to paired TrCHFEC decoder, if any. */
	virtual const TrCHFECDecoder* sibling() const;

	/** Apply the actual encoder. */
	virtual void encode(BitVector& in, BitVector& c) = 0;

	/** Make sure we're consistent with the current clock.  */
	void resync();

	/** Block until the NodeB clock catches up to mPrevWriteTime.  */
	void l1WaitToSend() const;

	/** Send a radio frame to the downstream as a sequence of TxBitsBursts. */
	void sendFrame(BitVector& frame, unsigned tfci);


	friend class TrCHFEC;

};


/**
	An abstract class for TrCHFEC decoders.
	writeLowSide() drives the processing.
*/
class TrCHFECDecoder : public TrCHFECBase, public L1ControlInterface
{
	public:
	MacEngine * mUpstream;
	protected:

	SoftVector *mD;			// One RadioFrame of data, accumulates 15 slots worth.
	SoftVector *mHDI;		// De-interleaving buffer.
	SoftVector *mRM;		// Rate-match  buffer.
	float mRawTfciAccumulator[gUlRawTfciSize];	// Incoming raw radio slot tfci bits.
	unsigned mDSlotIndex;	// Incoming slot number and index into mD and mTfciAccumulator.
	SoftVector *mDTti;		// A full TTI of data.
	unsigned mDTtiIndex;	// Incoming index in mDTtti in the range 0..8, depending on TTI


	/**@name Mutex-controlled state information. */
	//@{
	mutable Mutex mLock;				///< access control

	/**@name Timers */
	//@{
	//@}

#if OLD_CONTROL_IF
	Bool_z mActive;						///< true between open() and close()
	//@}

	/**@name Atomic volatiles, no mutex. */
	// Yes, I realize we're violating our own rules here. -- DAB
	//@{
	volatile bool mRunning;						///< true if all required service threads are started
	volatile float mFER;						///< current FER estimate
	static const int mFERMemory=20;				///< FER decay time, in frames
	//@}
	// TODO UMTS -- We need the correct values for these timers.
	GSM::Z100Timer mAssignmentTimer;
	GSM::Z100Timer mReleaseTimer;
#endif

	UMTS::Time mNextBurstTime;

	public:

	/**
		Constructor for an TrCHFECDecoder.
		@param wParent The containing TrCHFEC, for sibling access.
	*/
	TrCHFECDecoder(TrCHFEC* wParent,FecProgInfo &fpi);


	virtual ~TrCHFECDecoder() { }

	/**
		Clear the decoder for a new transaction.
	*/
	virtual void open();

	/**
		Call this at the end of a tranaction.
		Stop timers.
	*/
	virtual void close(bool hardRelease=false);

	/**
		Returns true if the channel is in use for a transaction.
	*/
#if OLD_CONTROL_IF
	//bool active() const { ScopedLock lock(mLock); return mActive; }
	bool active() const { return mActive; }

	/** Return true if any timer is expired. */
	bool recyclable() const;
#endif

	/** Connect the upstream MacEngine.  */
	// Return the old one, used for testing.
	MacEngine * l1SetUpstream(MacEngine * wUpstream, bool dontworrybehappy=false)
	{
		MacEngine *old = mUpstream;
		assert(mUpstream==NULL || dontworrybehappy);	// Only call this once.
		mUpstream=wUpstream;
		return old;
	}

	/** 25.212 4.2.2: Z is defined as the maximum code block size for this encoder. */
	virtual unsigned getZ() const =0;

#if OLD_CONTROL_IF
	/** Total frame error rate since last open(). */
	float FER() const { return mFER; }
#endif

	unsigned SF() const { return getPhCh()->getUlSF(); }

	/** Accept an RxBurst and process it into the deinterleaver. */
	virtual void l1WriteLowSide(const RxBitsBurst&);
	void writeLowSide1(const SoftVector &e, const float tfcibits[2]);
	void writeLowSide2(const SoftVector &e);
	void writeLowSide3(const SoftVector &);

	protected:
	/** Invoke the actual decoder. */
	virtual void decode(const SoftVector& c, BitVector& o) = 0;

	/** Return pointer to paired TrCHFEC encoder, if any. */
	virtual TrCHFECEncoder* sibling();

	/** Return pointer to paired TrCHFEC encoder, if any. */
	virtual const TrCHFECEncoder* sibling() const;

#if OLD_CONTROL_IF
	/** Mark the decoder as started.  */
	virtual void start() { mRunning=true; }

	void countGoodFrame();

	void countBadFrame();
#endif
};


/**
	The TrCHFEC encapsulates an encoder and decoder.
	(pat) 3-20-2012: Moved physical data to the PhCh class.
*/
class TrCHFEC {
	protected:
	PhCh *mPhCh;	// Referred to only in TrCHFECBase
	friend class TrCHFECBase;

	TrCHFECEncoder* mEncoder;
	TrCHFECDecoder* mDecoder;

	public:
	/**
		The TrCHFEC constructor is over-ridden for different channel types.
		But the default has no encoder or decoder.
	*/
	TrCHFEC(PhCh *wPhCh) :mPhCh(wPhCh), mEncoder(NULL),mDecoder(NULL) {}


	/** This is no-op because these channels should not be destroyed. */
	virtual ~TrCHFEC() {};

	/** Send in an RxBurst for decoding. */
	void l1WriteLowSide(const RxBitsBurst& burst)
		{ assert(mDecoder); mDecoder->l1WriteLowSide(burst); }

	/** Send in a TransportBlock for encoding and transmission. */
	void l1WriteHighSide(const TransportBlock& frame)
		{ assert(mEncoder); mEncoder->writeHighSide(frame); }

	/** Attach TrCHFEC to a downstream radio. */
	// (pat) Moved the radio pointer into PhCh and init when FEC is constructed.
	//void setDownstream(ARFCNManager* radio)
	//	{ assert(mEncoder); mEncoder->setDownstream(radio); }

	/** Attach TrCHFEC to an upstream MacEngine. */
	MacEngine *l1SetUpstream(MacEngine* mux, bool testmode=false)
		{ if (mDecoder) return mDecoder->l1SetUpstream(mux,testmode); else return 0;}

	/**@name Ganged actions. */
	//@{
	// (pat) These are unused except for DCHFEC.
	virtual void open() { assert(0); }
	virtual void close() { assert(0); }
	//@}
	

	/**@name Pass-through actions. */
	//@{
	float FER() const
		{ assert(mDecoder); return mDecoder->FER(); }

	bool recyclable() const
		{ assert(mDecoder); return mDecoder->recyclable(); }

	// Evidently no one calls active().
	//bool active() const { return mPhCh->phChActive(); }
	//unsigned SF() const { return mPhCh->SF(); }		// Dont use this.

	const char* descriptiveString() const
		{ assert(mEncoder); return mEncoder->descriptiveString(); }

	void l1WaitToSend() const
		{ assert(mEncoder); mEncoder->l1WaitToSend(); }

	UMTS::Time nextWriteTime() const
		{ assert(mEncoder); return mEncoder->nextWriteTime(); }

	// pat says: uplink and downlink trBkSz are nearly always different.
	unsigned l1GetDlTrBkSz() const
		{ assert(mEncoder); return mEncoder->trBkSz(); }
	unsigned l1GetUlTrBkSz() const
		{ assert(mDecoder); return mDecoder->trBkSz(); }

	unsigned l1GetDlNumRadioFrames(TrChId tcid) const
		{ assert(tcid == 0); return mEncoder->getNumRadioFrames(); }

	//unsigned codedBkSz() const
	//	{ assert(mEncoder); return mEncoder->codedBkSz(); }
	//@}


	TrCHFECDecoder* decoder() { return mDecoder; }
	TrCHFECEncoder* encoder() { return mEncoder; }

	void setEncoder(TrCHFECEncoder *wEncoder) { mEncoder = wEncoder; }
	void setDecoder(TrCHFECDecoder *wDecoder) { mDecoder = wDecoder; }

	L1ControlInterface *getControlInterface() { return decoder(); }


	virtual void generate() { assert(0); }

	//friend void* TrCHServiceLoop(TrCHFEC*);

}; 

//void* TrCHServiceLoop(TrCHFEC*);




// Single TransportBlock encoder.
// It is Rate 1/2 Convolutional, which is dictated for BCH, PCH, and one option for SCCPCH and DCH.
class TrCHFECEncoderLowRate : public TrCHFECEncoder
{	protected:
	ViterbiR2O9 mVCoder;

	public:
	TrCHFECEncoderLowRate(TrCHFEC *wParent,FecProgInfo&fpi): TrCHFECEncoder(wParent,fpi) {};

	void encode(BitVector& in, BitVector& c);
	unsigned getZ() const { return 504; }		// Max convolutional block size is a constant from 25.212 4.2.3
};


// It is Rate 1/2 Convolutional, which is dictated for RACH and one option for DCH.
class TrCHFECDecoderLowRate : public TrCHFECDecoder
{	protected:
	ViterbiR2O9 mVCoder;

	public:
	TrCHFECDecoderLowRate(TrCHFEC *wParent,FecProgInfo&fpi): TrCHFECDecoder(wParent,fpi) {};

	void decode(const SoftVector& c, BitVector& o);
	unsigned getZ() const { return 504; }		// Max convolutional block size is a constant from 25.212 4.2.3
};

// (pat) The interleaver is locked to the input size so I guess
// we will need one of these for every TransportBlock size.
class TrCHFECEncoderTurbo : public TrCHFECEncoder
{	protected:
	ViterbiTurbo mTCoder;
	TurboInterleaver mInterleaver;

	public:
	TrCHFECEncoderTurbo(TrCHFEC *wParent,FecProgInfo &fpi):
		TrCHFECEncoder(wParent,fpi),
		mInterleaver(fpi.mCodeBkSz)
	{ }

	void encode(BitVector& in, BitVector& c);
	unsigned getZ() const { return 5114; }		// Max Turbo encoder block size is a constant from 25.212 4.2.3
};


class TrCHFECDecoderTurbo : public TrCHFECDecoder
{	protected:
	ViterbiTurbo mTCoder;
	TurboInterleaver mInterleaver;
	public:
	TrCHFECDecoderTurbo(TrCHFEC *wParent,FecProgInfo &fpi):
		TrCHFECDecoder(wParent,fpi),
		mInterleaver(fpi.mCodeBkSz)
	{ }
	void decode(const SoftVector& c, BitVector& o);
	unsigned getZ() const { return 5114; }		// Max Turbo encoder block size is a constant from 25.212 4.2.3
};



// BCH - downlink only, not configurable, always 20 ms TTI
class BCHFEC : public PhChDownlink, public L1FEC_t
{
	Thread mServiceThread;
	public:
	BCHFEC(ARFCNManager *wRadio) :
		PhChDownlink(PCCPCHType,256,1,wRadio),			// Fixed by the UMTS spec
		L1FEC_t(this)
	{
		DEBUGF("construct BCHFEC\n");
#if USE_OLD_FEC
		// Radio frame size is 270.
		FecProgInfoSimple fpi(256,TTI20ms,16,270);
		// with parity=(246+16)=262; encoded=2*(262)+16 = 540;
		// PhCh is 18 bits/slot * 15 slots * 2 radio frames for TTI20ms = 540;
		assert(fpi.trBkSz() == 246);
		assert(fpi.codedBkSz() == 540);
		assert(fpi.getPB() == 16);
		assert(fpi.mRadioFrameSz == 270);
		TrCHFECEncoder *encoder = new TrCHFECEncoderLowRate(this, fpi);
		setEncoder(encoder);
#else
		L1CCTrChDownlink::fecConfigTrivial(256,TTI20ms,16,270);
		L1CCTrChDownlink::l1InstantiateDownlink();
#endif
	}

	void start();
	void generate();
};



// PCH - downlink only, configurable spreading factor
class PCHFEC : public PhChDownlink, public TrCHFEC
{
	public:

	// (pat) Downlink channels do not get a unique scrambling code.
	//PCHFEC(unsigned wSpCode, unsigned wSrCode):
	//	TrCHFEC(256, 16, wSpCode, wSrCode,TTI10ms)
	// This assumes that we are using a rate-1/2 convolutional coder.
	// This assumes that we are using slot format 0 (3GPP 25.211 5.3.3.4, table 18).
	// (pat) I dont think this is correct:
	// unsigned trBkSz() const { return 276; }


	// The PCH is on SCCPCH and goes through the 1/2 rate convolutional encoder.
	// 25.321 9.2.1.3 and I quote: "There is no MAC header for PCCH when mapped on PCH."
	PCHFEC(unsigned wSF, unsigned wSpCode,ARFCNManager *wRadio):
		PhChDownlink(SCCPCHType,wSF,wSpCode,wRadio),
		TrCHFEC(this)
	{
		// Fix parameters from the spec: TTI 10 ms.
		// FIXME: What is the radio frame size here?
		FecProgInfoSimple fpi(wSF,TTI10ms,16,getDlRadioFrameSize());
		TrCHFECEncoder* encoder = new TrCHFECEncoderLowRate(this,fpi);
		setEncoder(encoder);
	}

	void start();
	void generate();
};


// FACH - downlink only
// If you want to change the programming for this channel,
// look here: rrcInitCommonCh() and : configFachTrCh()
class FACHFEC : public PhChDownlink, public L1CCTrCh //L1FEC_t
{
	public:

	// (pat) Downlink channels do not get a scrambling code.
	FACHFEC(unsigned wSF, unsigned wSpCode, unsigned wPB, unsigned wTBSize,TTICodes tti,ARFCNManager *wRadio):
		PhChDownlink(SCCPCHType,wSF,wSpCode,wRadio),
		L1CCTrCh(this) //L1FEC_t(this)
	{
#if USE_OLD_FEC
		FecProgInfoInit fpi(wSF,tti,wPB,getDlRadioFrameSize(),wTBSize); // parity and tti not programmable?
		TrCHFECEncoder *encoder = new TrCHFECEncoderLowRate(this,fpi);
		setEncoder(encoder);
#else
	//L1CCTrChDownlink::fecConfigForOneTrCh(true,wSF,tti,wPB,getPhCh()->getDlRadioFrameSize(),wTBSize,1,1,false);
	//L1CCTrChDownlink::l1InstantiateDownlink();
#endif
	}
        void open() {
		//L1CCTrChDownlink::fecConfigForOneTrCh(true,wSF,tti,wPB,getPhCh()->getDlRadioFrameSize(),wTBSize,1,1,false);
        	//L1CCTrChDownlink::l1InstantiateDownlink();
	}
};


// RACHFEC - uplink only.
// If you want to change the programming for this channel,
// look here: rrcInitCommonCh() and : configRachTrCh()
class RACHFEC : public PhChUplink, public L1CCTrCh //L1FEC_t
{
	public:
	RACHFEC(unsigned wSF, unsigned wUplinkScramblingCode, unsigned wPB, unsigned wTBSize,TTICodes tti):
		PhChUplink(PRACHType,wSF,wUplinkScramblingCode),
                L1CCTrCh(this)
		//L1FEC_t(this) // parity and TTI not programmable
	{
#if USE_OLD_FEC
		//unsigned codedBkSz = RrcDefs::R2EncodedSize(wTBSize+16);
		FecProgInfoInit fpi(wSF,tti,wPB,getUlRadioFrameSize(),wTBSize);
		TrCHFECDecoder *decoder = new TrCHFECDecoderLowRate(this,fpi);
		// For SF=256 TTI=20ms PB=16, these are the hand-calculated params,
		// after quantizing to byte-align and rate-matching:
		/*assert(fpi.mRadioFrameSz == 150);
		assert(fpi.getTTICode() == TTI20ms);
		assert(fpi.trBkSz() == 120);
		assert(fpi.codedBkSz() == 288);
		assert(fpi.getPB() == 16);
		assert(decoder->mEini[0] == 1);
		assert(decoder->mEini[1] == 145);*/
		setDecoder(decoder);
#else
		//L1CCTrChUplink::fecConfigForOneTrCh(false,wSF,tti,wPB,getPhCh()->getUlRadioFrameSize(),wTBSize,1,2,false);
		//L1CCTrChUplink::l1InstantiateUplink();
#endif
	}
	void open() {/*L1CCTrChUplink::l1InstantiateUplink();*/}
};


// (pat 9-2012): The DCHFEC may use the old TrCHFEC which is single TB, single TFC,
// or the new L1CCTrChFEC, which is multiple TFC, multiple TB;
// set the pre-processor USE_OLD_DCH for which one you want.
#if USE_OLD_DCH
// DCH - configurable spreading factor, TTI 10 ms
class DCHFEC : public PhCh, public TrCHFEC	// single TB, single TFC.
{
	public:
	DCHFEC(unsigned wDlSF, unsigned wSpCode, unsigned wUlSF, unsigned wSrCode, ARFCNManager *wRadio) :
		PhCh(DPDCHType, wDlSF, wSpCode, wUlSF, wSrCode, wRadio)
		, TrCHFEC(this) 
		{}
	PhCh *getPhCh() { return this; }
	void fecConfig(TrChConfig &config, bool turbo=false);
	void open();
	void close();
};
#else
class DCHFEC : public PhCh , public L1CCTrCh	// multiple TFC, multiple TB
{
	public:
	DCHFEC(unsigned wDlSF, unsigned wSpCode, unsigned wUlSF, unsigned wSrCode, ARFCNManager *wRadio) :
		PhCh(DPDCHType, wDlSF, wSpCode, wUlSF, wSrCode, wRadio)
		, L1CCTrCh(this)
		{}
	void open();
	void close();
};
#endif

// TODO: Still need PICH, which is super special - no coding.



} // namespace UMTS



#endif


// vim: ts=4 sw=4
