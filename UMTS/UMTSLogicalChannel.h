/**@file Logical Channel.  */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef LOGICALCHANNEL_H
#define LOGICALCHANNEL_H

#include <sys/types.h>
#include <pthread.h>

#include <iostream>

#include <TransactionTable.h>

#include <Logger.h>

#include <UMTSL1FEC.h>
#include <UMTSTransfer.h>
//#include <ControlCommon.h>
#include <GSMCommon.h>
// One of the above does a #define WARNING that kills the LOG facility.
#undef WARNING	// Fix it

class UDPSocket;

namespace GSM {
class L3Frame;
class L3Message;
}

class ARFCNManager;


namespace UMTS {

// Outside refs.
//class RLCEngine;

// Forward refs.
class DCCHLogicalChannel;

typedef InterthreadQueue<Control::TransactionEntry> TransactionFIFO;


/**
	A complete logical channel.
	Includes processors for L1, L2, L3, as needed.
	The layered structure of GSM is defined in GSM 04.01 7, as well as many other places.
	The concept of the logical channel and the channel types are defined in GSM 04.03.
	This is virtual class; specific channel types are subclasses.
	(pat) This class is used exclusively to interface to the existing GSM codebase
	and specifically for the L3 message processors.
	There is one of these for each UE.
	It is implemented by diverting L3 messages on UMTS SRB3 that have protocol
	descriptors for the CS domain into the DCCHLogicalChannel class defined below.
	This is done on the high side of the SRB3 RLC.
	One of the primary purposes of this class is to allow it to block until
	specific L3 messages are received from the UE, and this architecture.
	allows it to block without affecting any other L3 messages.
	For PS services, L3 messages go to the SGSN, so this is class is unused.
/
*/
class LogicalChannel
{
	// Queue for incoming messages on UMTS SRB3, until somebody attached to this
	// class deigns to read them, and an easy way to do the timeout.
	InterthreadQueue<GSM::L3Frame> mL3RxQ;
	
protected:	

	/**@name Contained layer processors. */
	//@{
	// pat says: The lower layer connection changes for the UE depending on the state it is in.
	// Instead, we want a pointer to the UEInfo, which is relatively immortal,
	// and map the GSM states into states of UEInfo.
	UEInfo *mUep;	// The ue this channel is for.
	// If the UE is in CELL_DCH state, return the DCHFEC, otherwise,
	// if the UE is in CELL_FACH state, return NULL.
	L1ControlInterface *getPhy() const;

	//TrCHFEC *mPHY;			///< L1 forward error correction, used for debug info on the channel
	//RLCEngine *mRLC;		///< data link layer state machine
	//MACEngine *mMAC;		///< MAC layer state machine
	//@}


	/**
		A FIFO of inbound transactions intiated in the SIP layers on an already-active channel.
		Unlike most interthread FIFOs, do *NOT* delete the pointers that come out of it.
	*/
	TransactionFIFO mTransactionFIFO;

	/** A DTCH has an associated DCCH, like the SACCH or FACCH in GSM. */
	DCCHLogicalChannel* mDCCH;

public:

	/**
		Blank initializer just nulls the pointers.
		Specific sub-class initializers allocate new components as needed.
	*/
	LogicalChannel():
		//mRLC(NULL),mMAC(NULL), mPHY(NULL)
		mUep(NULL), mDCCH(NULL)
	{ }


	
	/** The destructor doesn't do anything since logical channels should not be destroyed. */
	virtual ~LogicalChannel() {};
	

	/**@name Accessors. */
	//@{
	//@}

	// Called from rrcRecvL3Msg() for protocol descriptors the GSM stack wants.
	void l3writeHighSide(ByteVector &msg);


	/**@name Pass-throughs. */
	//@{

	/** Set L1 physical parameters from a RACH or pre-exsting channel. */
	// (pat) removed until need proven.  Phy changes as UE changes state.
	virtual void setPhy(float wRSSI, float wTimingError);

	/* Set L1 physical parameters from an existing logical channel. */
	// (pat) removed until need proven.
	// virtual void setPhy(const LogicalChannel&);

	/**@name L3 interfaces */
	//@{

	/**
		Read an L3Frame from SAP0 uplink, blocking, with timeout.
		The caller is responsible for deleting the returned pointer.
		The default 15 second timeout works for most L3 operations.
		@param timeout_ms A read timeout in milliseconds.
		@param SAPI The service access point indicator from which to read.
		@return A pointer to an L3Frame, to be deleted by the caller, or NULL on timeout.
	*/
	virtual GSM::L3Frame * recv(unsigned timeout_ms = 15000, unsigned SAPI=0);

	/**
		Send an L3Frame on downlink.
		This method will block until the message is transferred to the transceiver.
		@param frame The L3Frame to be sent.
		@param SAPI The service access point indicator.
		(pat) The L3Frame is a BitVector containing the L3 message.
	*/
	virtual void send(const GSM::L3Frame& frame, unsigned SAPI=0);

	/**
		Send "naked" primitive down the channel.
		@param prim The primitive to send.
		@pram SAPI The service access point on which to send.
	*/
	virtual void send(const GSM::Primitive& prim, unsigned SAPI=0);

	/**
		Initiate a transaction from the SIP side on an already-active channel.
	*/
	virtual void addTransaction(Control::TransactionEntry* transaction);

	/**
		Serialize and send [downstream] an L3Message with a given primitive.
		@param msg The L3 message.
		@param prim The primitive to use.
	*/
	virtual void send(const GSM::L3Message& msg,
			GSM::Primitive prim=GSM::DATA,
			unsigned SAPI=0);

	/**
		Block on a channel until a given primitive arrives.
		Any payload is discarded.  Block indefinitely, no timeout.
		@param primitive The primitive to wait for.
	*/
	void waitForPrimitive(GSM::Primitive primitive) {} ; // FIXME: stubbed out

	/**
		Block on a channel until a given primitive arrives.
		Any payload is discarded.  Block indefinitely, no timeout.
		@param primitive The primitive to wait for.
		@param timeout_ms The timeout in milliseconds.
		@return True on success, false on timeout.
	*/
	bool waitForPrimitive(GSM::Primitive primitive, unsigned timeout_ms) {return false;} //FIXME: stubbed out



	//@} // L3

	/**@name L1 interfaces */
	//@{

	/** Write a received radio burst into the "low" side of the channel. */
	//virtual void writeLowSide(const UMTS::RxBitsBurst& burst) { assert(mPHY); mPHY->writeLowSide(burst); }

	/** Return true if the channel is safely abandoned (closed or orphaned). */
	// (pat) This may not work unless we dedicate these channels for CS service...
	bool recyclable() const {
		return getPhy() ? getPhy()->recyclable() : 0;
		//assert(mPHY); return mPHY->recyclable();
	}

	/** Return true if the channel is active. */
	// (pat) Guess no one calls this...
	//bool active() const; { assert(mPHY); return mPHY->active(); }
	//bool active() const { return getPhy() ? getPhy()->active() : 0; }

	/**@name Channel stats from the physical layer */
	//@{
	/** Receive FER. */
	//float FER() const { assert(mPHY); return mPHY->FER(); }
	float FER() const { return getPhy() ? getPhy()->FER() : 0; }
	/** RSSI wrt full scale. */
	virtual float RSSI() const;
	/** Uplink timing error. */
	virtual float timingError() const;
	/** Actual MS uplink power. */
	virtual int actualMSPower() const;
	/** Actual MS uplink timing advance. */
	virtual int actualMSTiming() const;
	//@}

	//@} // L1

	/**@name L2 passthroughs */
	//@{
	// (pat) Take these out until someone needs them, and then find out why.
	//unsigned SF() const { return mPHY->SF(); }
	//unsigned SpCode() const { return mPHY->SpCode(); }
	//unsigned SrCode() const { return mPHY->SrCode(); }
	//@}

	//@} // passthrough

	DCCHLogicalChannel* DCCH() { return mDCCH; }

	/** Connect an ARFCN manager to link L1FEC to the radio. */
	void downstream(ARFCNManager* radio);

	/** Return the channel type. */
	virtual ChannelTypeL3 type() const =0;

	// (pat) This would change depending on UE state, eg: DCCH would change from FACH to DCH.
	//const PhyChanDesc& phyChanDesc() const;

	/**
		Make the channel ready for a new transaction.
		The channel is closed with primitives from L3.
	*/
	virtual void open();

	/**@ Debuging functions: will give access to all intermediate layers. */
	//@{
	//RLCEngine * debugGetL2(){ return mRLC; }
	//TrCHFEC * debugGetL1(){ return mPHY; }
	//@}

	//const char* descriptiveString() const { return mPHY->descriptiveString(); }
	const char* descriptiveString() const { return "DCCH for GSM"; }	// (pat) all it can be.

	protected:

	/**
		Make the normal inter-layer connections.
		Should be called from inside the constructor after
		the channel components are created.
	*/
	virtual void connect();

};


std::ostream& operator<<(std::ostream&, const UMTS::LogicalChannel&);


/**
	Dedicated control channel, used only to drive the GSM L3 message machine.
	Hopefully.
"
*/
class DCCHLogicalChannel : public LogicalChannel
{

	public:
	//GSM::L3Frame * recv(unsigned timeout_ms = 15000, unsigned SAPI=0);

	//void send(const GSM::L3Frame& frame, unsigned SAPI=0);
	
	DCCHLogicalChannel(UEInfo *wUep) { mUep = wUep; }

	ChannelTypeL3 type() const { return DCCHType; }

	bool multiframeMode(unsigned SAPI) const {return true;} // FIXME: stubbed out
};



/**
	Dedicated CS traffic channel.  Not used for PS services.
"
*/
class DTCHLogicalChannel : public LogicalChannel {

	public:
	
	DTCHLogicalChannel();

	ChannelTypeL3 type() const { return DTCHType; }

	void sendTCH(const unsigned char* frame) {}// FIXME: stubbed out

	unsigned char* recvTCH() { return NULL;} // FIXME: stubbed out

	unsigned queueSize() const { return 0; } // FIXME: stubbed out

	bool radioFailure() const {return false;} // FIXME: stubbed out
};










/**
	Common control channel.
	The "uplink" component of the CCCH is the RACH.
*/
class CCCHLogicalChannel : public LogicalChannel {

	protected:

	/*
		Because the CCCH is written by multiple threads,
		we funnel all of the outgoing messages into a FIFO
		and empty that FIFO with a service loop.
	*/

	Thread mServiceThread;		///< a thread for the service loop
	GSM::L3FrameFIFO mQ;		///< because the CCCH is written by multiple threads
	bool mRunning;			///< a flag to indication that the service loop is running

	public:

	CCCHLogicalChannel();

	void open();

	void send(const GSM::L3Message&) { assert(0); }

	/** This is a loop in its own thread that empties mQ. */
	void serviceLoop();

	/** Return the number of messages waiting for transmission. */
	unsigned load() const { return mQ.size(); }

	ChannelTypeL3 type() const { return CCCHType; }

	friend void *CCCHLogicalChannelServiceLoopAdapter(CCCHLogicalChannel*);

};

/** A C interface for the CCCHLogicalChannel embedded loop. */
void *CCCHLogicalChannelServiceLoopAdapter(CCCHLogicalChannel*);


//@}

}		// UMTS

typedef InterthreadQueue<UMTS::DCCHLogicalChannel> DCCHLogicalChannelFIFO;

extern DCCHLogicalChannelFIFO gDCCHLogicalChannelFIFO;


#endif


// vim: ts=4 sw=4
