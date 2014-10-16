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

#ifndef UMTSTRANSFER_H
#define UMTSTRANSFER_H

#include <BitVector.h>
#include <ByteVector.h>
#include "UMTSCommon.h"
#include "UMTSCodes.h"
#include "URRCDefs.h"


namespace UMTS {

class UEInfo;

/** The size of the BCH tranport block used for SIBs. */
static const unsigned sSIBTrBlockSize = 246;


/**
	Interlayer primitives.
	RLC, MAC, PHY, etc. are implied by context.
	We don't provide the full req-conf-ind-ack handshake because we
	don't always need it in such a tighly integrated system, so
	our primitive set is simple.
*/
enum Primitive {
	ESTABLISH,		///< channel establihsment
	RELEASE,		///< normal channel release
	DATA,			///< multiframe data transfer
	UNIT_DATA,		///< datagram-type data transfer
	STATUS,			///< channel status
	CONFIG,			///< configuration control
	ERROR,			///< channel error
	HARDRELEASE		///< forced release after an assignment
};





/**
	Class to represent a pre-spread tx slot.
*/
class TxBitsBurst : public BitVector {

	protected:

	unsigned mSF;		///< spreading factor
	unsigned mLog2SF;       ///< log2 of spreading factor
	unsigned mCodeIndex;    ///< code number, i.e. index into code tree
	Time mTime;		///< the time when this burst is to be transmitted
	bool mDCH;		///< indicates if burst is a DCH (true) or CCH (false)
	bool mAICH;
	bool mRightJustified;   ///< bits should be right justified w.r.t slot boundary

        public:

	TxBitsBurst(size_t wSF, size_t wCodeIndex, const Time& wTime, bool wDCH, bool wRightJustified=true)
		:BitVector(gSlotLen/wSF),
		mSF(wSF),mCodeIndex(wCodeIndex),
		mTime(wTime),mDCH(wDCH),mRightJustified(wRightJustified)
	{         
	  mLog2SF = 0;
	  while (wSF > 1) {mLog2SF++; wSF = wSF >> 1;}
	  mAICH = false;
        }

	/** Create a TxBurst by copying from an existing BitVector. */
	TxBitsBurst(const BitVector& bits, size_t wSF, size_t wCodeIndex, const Time& wTime, bool wRightJustified=false);

	unsigned SF() const { return mSF; }

	unsigned log2SF() const { return mLog2SF; }

	unsigned codeIndex() const { return mCodeIndex; } 

	const Time& time() const { return mTime; }

	bool DCH() const { return mDCH; }

	void DCH(bool wDCH) {mDCH = wDCH;}
	
	bool AICH() const {return mAICH;}

	void AICH(bool wAICH) {mAICH = wAICH;}

	bool rightJustified() const { return mRightJustified; }

	std::ostream& text(std::ostream&os) const;
	friend std::ostream& operator<<(std::ostream& os, const TxBitsBurst&tbb);
	friend std::ostream& operator<<(std::ostream& os, const TxBitsBurst*ptbb);


	/** comparison operator, used for sorting */
	bool operator>(const TxBitsBurst& other) const {return mTime > other.mTime;}

};


std::ostream& operator<<(std::ostream& os, const TxBitsBurst&);
std::ostream& operator<<(std::ostream& os, const TxBitsBurst*);




class RxChipsBurst;

/**
	Class to represent a post-de-spread received burst with soft decoding.
	This class represents only the I-part or Q-part, not a full complex signal.
*/
class RxBitsBurst : public SoftVector {

	protected:

	// pat says: Do we need the spreading factor in here?  I'm not using it.
	unsigned mSFI;		///< spreading factor index (log2 of the SF)
	Time mTime;			// Must be provided, or havoc will ensue.
	float mTimingError;	// Pats todo: accumulate this in the UEInfo struct.
	float mRSSI;		// Pats todo: accumulate this in the UEInfo struct.
	public:
	// (pats comment) There are a maximum of two tfcibits in each burst,
	// the exact number is determined by the slot format chosen from the many
	// options in UMTSPhCh.cpp.  Then there are 15 (gFrameSlot) bursts
	// in each radio frame, making a total of 30 (gUlTfciSize) tfci bits/radio frame,
	// then the 30 bits are condensed down using sTfciCodes or findTfci
	// to a small TFCI code (usually length 2, 4, or 8, we dont use 10)
	// that are necessary to choose the TFC, which then specifies the TF for
	// all TrCh simultaneously.
	float mTfciBits[2];	// The (max) two tfcibits from this burst, in the range 0..1.


	/** Wrap an RxDataBurst around an existing float array. */
	RxBitsBurst(size_t wSFI, float* bits, const UMTS::Time &wTime, float wTimingError, int wRSSI)
		:SoftVector(bits,gSlotLen/(1<<wSFI)),mSFI(wSFI),mTime(wTime),
		mTimingError(wTimingError),mRSSI(wRSSI)
	{
		mTfciBits[0] = -1;	// Mark as invalid so we can detect that.
	}

        RxBitsBurst(size_t wSFI, float* bits, size_t sz, const UMTS::Time &wTime, float wTimingError, int wRSSI)
                :SoftVector(bits,sz),mSFI(wSFI),mTime(wTime),
                mTimingError(wTimingError),mRSSI(wRSSI)
        {
                mTfciBits[0] = -1;      // Mark as invalid so we can detect that.
        }

	unsigned SFI() const { return mSFI; }
	unsigned SF() const { return 1<<mSFI; }

	Time time() const { return mTime; }

	void time(const Time& wTime) { mTime = wTime; }
	
	float RSSI() const { return mRSSI; }

	float timingError() const { return mTimingError; }

	/** Given chips and a spreading sequence, despread this received bit vector. */
	void despread(const char* code, const RxChipsBurst& source);

	/** Apply a scrambling code. */
	void descramble(const char* code);

	friend std::ostream& operator<<(std::ostream& os, const RxBitsBurst&);
};

std::ostream& operator<<(std::ostream& os, const RxBitsBurst&);

/**
	Class to represent a pre-de-spread received burst with soft decoding.
	This class represents only the I-part or Q-part, not a full complex signal.
*/
class RxChipsBurst : public SoftVector {

	protected:

	Time mTime;
	float mTimingError;
	float mRSSI;

	public:

	/** Wrap an RxChipsBurst around an existing float array. */
	RxChipsBurst(float* chips, size_t wSz, const Time &wTime, float wTimingError, int wRSSI)
		:SoftVector(chips,wSz),mTime(wTime),
		mTimingError(wTimingError),mRSSI(wRSSI)
	{ }

	Time time() const { return mTime; }

	void time(const Time& wTime) { mTime = wTime; }
	
	float RSSI() const { return mRSSI; }

	float timingError() const { return mTimingError; }

	friend std::ostream& operator<<(std::ostream& os, const RxChipsBurst&);

};

std::ostream& operator<<(std::ostream& os, const RxChipsBurst&);




/**
	The transport block is passed on the simple L1/MAC interface,
	which supports only one TrCh per L1 connection per RadioFrame.
	This class is over-ridden in the MAC engine to aid in TransportBlock creation,
	but only the information here is needed to be passed on the L1/MAC interface.
*/
class TransportBlock : public BitVector {

	UMTS::Time mTime;	///< the time when this block was receive or is to be transmitted.
	public:
	bool mScheduled;	///< if false, ignore mTime
	// (pat) No, the tfci does not go in the TB, it goes in the TBS.
	//unsigned mTfci;		///< the TFCI [Transport Format Combination Index] assigned by MAC.
	std::string mDescr;	///< Optional description of what is in it.


	TransportBlock(const BitVector& bits)
		:BitVector(bits),mScheduled(false)//,mTfci(0)
	{ }

	TransportBlock(const BitVector& bits, const UMTS::Time& wTime)
		:BitVector(bits),
		mTime(wTime),mScheduled(true)//,mTfci(0)
	{ }

	TransportBlock(size_t sz)
		:BitVector(sz),mScheduled(false)//,mTfci(0)
	{ }

	TransportBlock(size_t sz, const UMTS::Time& wTime)
		:BitVector(sz),
		mTime(wTime),mScheduled(true)//,mTfci(0)
	{ }

	TransportBlock(const TransportBlock& block, const UMTS::Time& wTime)
		:BitVector(block),
		mTime(wTime),mScheduled(true)//,mTfci(0)
	{ }

	void setSchedule(unsigned framenum) { mTime = Time(framenum); mScheduled = true; }

	bool scheduled() const { return mScheduled;}
	
	const UMTS::Time& time() const { assert(mScheduled); return mTime; }
	void time(const UMTS::Time& wTime) { mTime = wTime; mScheduled=true; }

	void encodeParity(Parity& parity, BitVector& bBits) const;

	friend std::ostream& operator<<(std::ostream& os, const TransportBlock&);
	friend std::ostream& operator<<(std::ostream& os, const TransportBlock*);
	void text(std::ostream &os) const;
};

std::ostream& operator<<(std::ostream& os, const TransportBlock&);
std::ostream& operator<<(std::ostream& os, const TransportBlock*);

// The MacTbs is passed on the L1/MAC interface, when it is necessary to support
// multiple Trch or multiple blocks per RadioFrame.
// MAC has already encoded the logical channels so all that is left here is
// the TransportBlock data per TrCh, and the selected TFC to be used by L1.
// FYI: UMTS has four important flow identifiers:
// o RAB-id is the CN level identifier.
//   For PS [packet switched] channels it is in the range 5..15
// o RB-id is the RRC level identifier.
//   RB-id 1-4 are reserved as SRB1 - SRB4 [Signaling Radio Bearer] for communication with RRC.
//   SRB0 is not an official channel in that no RB-id will ever be 0,
//   but it used for CCCH messages that do not have an RB-id assigned yet.
//   The other RB-id can be assigned by RRC.  Each RAB-id has one or more RB-id assigned,
//   called RAB subflows.
// o The logical channel id is used by the MAC header to distinguish RB-ids on the PHY interface.
// o The TrCh distinguishes Transport Channels, of course.
//   Each RB-id sub-flow can be assigned either a unique TrCh with no mac multiplexing,
//   or unique logical channel ids for mac multiplexing on a shared TrCh.
// We currently use the RB-id and the logical channel id, and for CS channels,
// we use one-to-one mapping of RAB-id onto RB-id, ie, RAB 5-15 are RB 15.
// For voice channels, there should be just one RAB-id which will use sub-flows RB5,6,7
// for the AMR data.
// Notes:
// DCCH carries SRB1,2,3,4, and DTCH carries RB5 for IP data, or RB5,6,7 for voice data.
// From 25.331 [RRC] 8.5.21 it looks to me like DTCH and DCCH need to both be on
// the same physical channel, either DCH or RACH/FACH (more precisely, PRACH/SCCPCH.)
// Therefore, either the lower layer is going to have to support multiple TrCh or
// the TFCS will have to specify just one TrCh at a time and let MAC do the switching.
class RrcTfc;
class MacTbs : public virtual RrcDefs
{	public:
	RrcTfc *mTfc;	// The TFC selected by MAC.
	UMTS::Time mTime; // time MacTbs to be transmitted, used when number of blocks is zero
	// The transport channels here are indexed starting at 0,
	// which is the transport channel number minus 1.
	struct TbList {
		unsigned mNumTb;
		TransportBlock *mTb[maxTbPerTrCh];
		TbList() : mNumTb(0) { memset(mTb,0,sizeof(mTb)); }
		void addTb1(TransportBlock *tb) { assert(mNumTb < maxTbPerTrCh); mTb[mNumTb++] = tb; }
	} mTrChTb[maxTrCh];

	// Add a transport block for the specified TrCh.
	// Note that we used 0 based TrCh id here.
	void addTb(TransportBlock *tb, TrChId tcid = 0) { mTrChTb[tcid].addTb1(tb); }

	unsigned getNumTb(TrChId tcid = 0) { return mTrChTb[tcid].mNumTb; }

	void clear(void) { 
		for (unsigned i = 0; i < maxTrCh; i ++) { 
			for (unsigned j = 0; j < mTrChTb[i].mNumTb; j++) delete mTrChTb[i].mTb[j]; 
		} 
	}

	// Return the specified TB or null.
	TransportBlock *getTB(unsigned tbid, TrChId tcid) const {
		assert(tcid < maxTrCh);
		assert(tbid < maxTbPerTrCh);
		return tbid < mTrChTb[tcid].mNumTb ? mTrChTb[tcid].mTb[tbid] : NULL;
	}

	// Assuming there is only one TransportBlock in this TBS, return it, or NULL.
	TransportBlock *getOneTB() const { return getTB(0,0); }

	MacTbs(RrcTfc *wTfc) : mTfc(wTfc) { }

	~MacTbs() {
		/*for (int i = 0; i < maxTrCh; i++) {
			for (int j = 0; j < mTrChTb[i].mNumTb; j++) delete mTrChTb[i].mTb[j];
		}*/
	}

};




#if 0	// DABs start on this:

class MACSDU : public BitVector {

	public:

	MACSDU(const BitVector& bits):BitVector(bits) {}

};


/**
	(DAB) The MAC-PDU is passed between the MAC and the RLC.
	From 3GPP 25.321 9.1.2: A MAC PDU consists of an optional
	MAC header and a MAC Service Data Unit (MAC SDU), see figure 9.1.2.1.
	Both the MAC header and the MAC SDU are of variable size.  The
	content and the size of the MAC header depends on the type of the
	logical channel, and in some cases none of the parameters in the MAC
	header are needed.  The size of the MAC-SDU depends on the size of
	the RLC-PDU, which is defined during the setup procedure.
*/
class MACPDU : public BitVector {

	public:

	// This only applies to FACH.
	// If there is a FACH_MACPDU class, they should probably be there instead of here.
	// The size of this field depends on the channel type and the data itself.
	virtual size_t TCTFBase() const { return 0; }
	virtual size_t TCTFSize() const =0;
	virtual unsigned TCTF() const { return peekField(TCTFBase(),TCTFSize()); }

	// The size of this field depend on the channel type.
	virtual size_t UEIdTypeBase() const { return TCTFBase()+TCTFSize(); }
	virtual size_t UEIdTypeSize() const =0;
	virtual unsigned UEIdType() const { return peekField(UEIdBase(),UEIdSize()); }

	// The size of this field depends on the value of UEIdType().
	virtual size_t UEIdBase() const { return TCTFBase()+TCTFSize(); }
	virtual size_t UEIdSize() const;
	virtual unsigned UEId() const { return peekField(UEIdBase(),UEIdSize()); }

	virtual size_t CTBase() const { return UEIdBase() + UEIdSize(); }
	virtual size_t CTSize() const { return 4; }
	virtual unsigned CT() const { return peekField(CTBase(),CTSize()); }

	size_t SDUBase() const { return CTBase()+CTSize(); }
	const MACSDU SDU() const { return MACSDU(tail(SDUBase())); }
};


/** MAC PDU with SDU only. */
class SimpleMACPDU : public MACPDU {

	public:

	size_t TCHFSize() const { return 0; }
	unsigned TCTF() const { assert(0); }

	size_t UEIdTypeSize() const { return 0; }
	unsigned UEIdType() { assert(0); }

	size_t UEIdSize() const { return 0; }
	unsigned UEId() { assert(0); }

	size_t CTSize() const { return 0; }
	unsigned CT() { assert(0); }

};



/** A full MAC PDU as seen on the FACH. */
class FACH_MACPDU : public MACPDU {

	public:

	size_t TCTFSize() const;

	size_t UEIdTypeSize() const { return 2; }
};


/** A MAC PDU with TCTF+SDU only, as seen on the FACH. */
class FACH_TCTFOnlyMACPDU : public FACH_MACPDU {

	public:

	size_t UEIdTypeSize() const { return 0; }
	unsigned UEIdType() { assert(0); }

	size_t UEIdSize() const { return 0; }
	unsigned UEId() { assert(0); }

	size_t CTSize() const { return 0; }
	unsigned CT() { assert(0); }
};


// (pat) Having an alternate format PDU is kind of irrelevant on DCH.
// When using DCH, the primary information needed is the transport channel
// so we can identify the UE, and that is not in the MACPDU.
/** 3GPP 25.321 9.2.1.1 case (a) */
class DTCH_DCH_MACPDU : public SimpleMACPDU { };
class DCCH_DCH_MACPDU : public SimpleMACPDU { };
// We dont support case (b)
/** 3GPP 25.321 9.2.1.1 case (c) */
class DTCH_FACH_MACPDU : public FACH_MACPDU { };
class DCCH_FACH_MACPDU : public FACH_MACPDU { };

// These are davids originals, which I didnt want to delete.
// But I dont know what DTCCH & DCCCH are - maybe deprecated channel names?
// And the case (b) is not right for either case b or c, so I am fixing, above
/** 3GPP 25.321 9.2.1.1 case (a) */
class DTCCH_DCH_MACPDU : public SimpleMACPDU { };
class DTCH_DCH_MACPDU : public DTCCH_DCH_MACPDU { };
class DCCCH_DCH_MACPDU : public SimpleMACPDU { };
class DCCH_DCH_MACPDU : public DCCCH_DCH_MACPDU { };

/** 3GPP 25.321 9.2.1.1 case (b) */
// (pat) This is not right for any of the cases, so I changed:
class DTCCH_FACH_MACPDU : public FACH_TCTFOnlyMACPDU { };
class DTCH_FACH_MACPDU : public DTCCH_DCH_MACPDU { };
class DCCH_FACH_MACPDU : public DCCH_DCH_MACPDU { };

/** 3GPP 25.321 9.2.1.2
	(pat) There is no header because the UE must look
	inside the message for the gigantic IE to determine ownership.
*/
class BCCH_BCH_MACPDU : public SimpleMACPDU { };

/** 3GPP 25.321 9.2.1.2 */
class BCCH_FACH_MACPDU : public FACH_TCTFOnlyMACPDU { };

/** 3GPP 25.321 9.2.1.3 */
class PCCH_PCH_MACPDU : public SimpleMACPDU { };

/** 3GPP 25.321 9.2.1.4 */
class CCCH_FACCH_MACPDU : public FACH_TCTFOnlyMACPDU { };

class RLCSDU : public BitVector {
	public:
	RLCSDU(const BitVector& bits):BitVector(bits) { };
};


/**
	The RLC-PDU is passed between the RLC and upper layers.
	3GPP 25.322
*/
class RLCPDU : public BitVector {
	public:
	RLCPDU(const BitVector& bits):BitVector(bits) { };
};


/** RLC control PDUs 3GPP 9.2.1.5-9.2.1.7 */
class ControlRLCPDU : public RLCPDU {

	public:

	ControlRLCPDU(const BitVector& bits):RLCPDU(bits) { }

	unsigned DC() const { return peekField(0,1); }
	unsigned type() const { return peekField(2,3); }

};


class STATUS_RLCPDU : public ControlRLCPDU {

	public:

	STATUS_RLCPDU(const BitVector& bits):ControlRLCPDU(bits) { }

	unsigned RSN() const { return peekField(4,1); }
	unsigned R1() const { return peekField(5,3); }

};



/** RLC data PDUs */
class DataRLCPDU : public BitVector {

	public:

	size_t SNBase() const { return 0; }
	virtual size_t SNLen() const { assert(0); }

	virtual size_t LIBase() const { return SNBase()+SNLen(); }
	virtual size_t LILen() const;
	virtual unsigned LI() const;

	virtual size_t SDUBase() const =0;
	virtual size_t SDULen() const =0;
	virtual const RLCSDU SDU() const { return RLCSDU(segment(SDUBase(),SDULen())); }

};



/** Transparent data, 3GPP 25.322 9.2.1.2 */
class TMD_RLCPDU : public DataRLCPDU {

	public:

	size_t SDUBase() const { return 0; }
	size_t SDULen() const { return size(); }
};


/** Unacknowledged mode data, 3Gpp 25.332 9.2.1.3 */
class UMD_RLCPDU : public DataRLCPDU {

	public:

	size_t SNLen() const { return 8; }
	unsigned SN() const { return peekField(SNBase(),7); }

	size_t SDUBase() const { return LIBase()+LILen(); }
	size_t SDULen() const { return LI()*8; }
};

/** Acknowledged mode data, 3Gpp 25.332 9.2.1.4 */
class AMD_RLCPDU : public DataRLCPDU {

	public:

	size_t SNLen() const { return 16; }
	unsigned DC() const { return peekField(SNBase(),1); }
	unsigned SN() const { return peekField(SNBase()+1,12); }
	unsigned P() const { return peekField(SNBase()+13,1); }
	unsigned HE() const { return peekField(SNBase()+14,2); }

	size_t SDUBase() const { return LIBase()+LILen(); }
	size_t SDULen() const { return LI()*8; }

	size_t piggybackBase() const { return SDUBase() + SDULen(); }
	const STATUS_RLCPDU piggyback() const { return tail(piggybackBase()); }
};
#endif






} // namespace UMTS


#endif
