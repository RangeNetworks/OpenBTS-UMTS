/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef TRXMANAGER_H
#define TRXMANAGER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <Threads.h>
#include <Sockets.h>
#include <Interthread.h>
#include "UMTSCommon.h"
#include "UMTSTransfer.h"
#include <list>

#include "UMTSRadioModem.h"


/* Forward refs into the UMTS namespace. */
namespace UMTS {

class L1Decoder;

};

class ARFCNManager;


/**
	The TransceiverManager processes the complete transcevier interface.
	There is one of these for each access point.
 */
class TransceiverManager {

	private:

	/// the ARFCN manangers under this TRX
	std::vector<ARFCNManager*> mARFCNs;		

	/// set true when the first CLOCK packet is received
	volatile bool mHaveClock;
	/// socket for clock management messages
	UDPSocket mClockSocket;		
	/// a thread to monitor the global clock socket
	Thread mClockThread;	


	public:

	/**
		Construct a TransceiverManager.
		@param numARFCNs Number of ARFCNs supported by the transceiver.
		@param wTRXAddress IP address of the transceiver.
		@param wBasePort The base port for the interface, as defined in README.TRX.
	*/
	void TransceiverManagerInit(int numARFCNs,
		const char* wTRXAddress, int wBasePort);

	/**@name Accessors. */
	//@{
	ARFCNManager* ARFCN(unsigned i) { assert(i<mARFCNs.size()); return mARFCNs.at(i); }
	//@}

	unsigned numARFCNs() const { return mARFCNs.size(); }

	/** Block until the clock is set over the UDP link. */
	//void waitForClockInit() const;

	/** Start the clock management thread and all ARFCN managers. */
	void trxStart();

	/** Clock service loop. */
	friend void* ClockLoopAdapter(TransceiverManager*);

	private:

	/** Handler for messages on the clock interface. */
	void clockHandler();
};




void* ClockLoopAdapter(TransceiverManager *TRXm);


/**
	The ARFCN Manager processes transceiver functions for a single ARFCN.
	When we do frequency hopping, this will manage a full rate radio channel.
*/
class ARFCNManager {

	private:

	TransceiverManager &mTransceiver;

	Mutex mDataSocketLock;			///< lock to prevent contentional for the socket
	UDPSocket mDataSocket;			///< socket for data transfer
	Mutex mControlLock;				///< lock to prevent overlapping transactions
	UDPSocket mControlSocket;		///< socket for radio control

	Thread mRxThread;				///< thread to receive data from rx

	Thread mTxThread;

	// The GSM demux table is replaced by gNodeB::mPhyChanMap
	/**@name The demux table. */
	//@{
	//Mutex mTableLock;
	// FIXME -- This should be a map of <hashedIndex>:<L1Decoder*>
	//UMTS::L1Decoder* mDemuxTable;		///< the demultiplexing table for received bursts
	//@}

	unsigned mUARFCN;			///< the current UARFCN

	// (pat) 1-5-2013 There was a constructor race here:
	// This line was starting the TransceiverManager above, which did a read on one of the sockets above,
	// but they are not guaranteed to be initialized before the RadioModem constructor.
	// Fixed by moving the RadioModem startup code to ARFCNManager::ARFCNManager().  What a great language.
	// fyi AFRCNManager is created from TransceiverManagerInit()
	UMTS::RadioModem mRadioModem;

	public:

	ARFCNManager(const char* wTRXAddress, int wBasePort, TransceiverManager &wTRX, unsigned wCId);

	unsigned mCId;	// (pat) An id for error messages, eg, 0 for C0.

	/** Start the uplink thread. */
	void arfcnManagerStart();
	const char *getId() { static char buf[8]; sprintf(buf,"C%d",mCId); return buf; }

	unsigned UARFCN() const { return mUARFCN; }

	void writeHighSide(UMTS::TxBitsBurst* burst);

	/**@name Transceiver controls. */
	//@{

	/**
		Tune to a given UARFCN.
		@param wUARFCN Target for tx/rx tuning.
		@return true on success.
	*/
	bool tune(unsigned wUARFCN);

	/**
		Tune to a given ARFCN, but with rx and tx on the same (downlink) frequency.
		@param wARFCN Target for tuning, using downlink frequeny.
		@return true on success.
	*/
	bool tuneLoopback(int wARFCN);

	/**
		Turn on the transceiver.
	*/
	bool powerOn();


	/** Turn off the transceiver. */
	bool powerOff();

	/** Reset Timestamps. */
	bool resetTimestamps();

        /***/
        bool resetFx3();

	/** Turn off the transceiver. */
	bool sysPowerOff();

	/**
		Turn on the transceiver.
		@param warn Warn if the transceiver fails to start
	*/
	bool sysPowerOn(bool warn);

	/** Turn off the transceiver. */
	bool radioPowerOff();

	/**
		Turn on the transceiver.
		@param warn Warn if the transceiver fails to start
	*/
	bool radioPowerOn(bool warn);

	/** Turn off the transmitter. */
	bool txPowerOff();

	/** Turn on the transmitter. */
	bool txPowerOn(unsigned band);

	/** Just test if the transceiver is running without printing alarming messages. */
	bool trxRunning() {return false;}

        /**     
		Set maximum expected delay spread.
		@param km Max network range in kilometers.
		@return true on success.
        */
        bool setMaxDelay(unsigned km);

        /**     
                Set radio receive gain.
                @param new desired gain in dB.
                @return new gain in dB.
        */
        signed setRxGain(signed dB);

        /**     
                Set radio transmit attenuation
                @param new desired attenuation in dB.
                @return new attenuation in dB.
        */
        signed setTxAtten(signed dB);

        /**     
                Set radio frequency offset
                @param new desired freq. offset
                @return new freq. offset
        */
        signed setFreqOffset(signed offset);

        /***/
        signed setTxNco(signed dB);

        /***/
        signed setRxDdcTune(signed dB);

        /**
                Get noise level as RSSI.
                @return current noise level.
        */
        signed getNoiseLevel(void);

        /***/
        signed getTemperature(void);

        /**
                Get the board state.
                @return current board bringup state
        */
        signed getState(void);

        /**
                Get the Tx Power.
                @return current Tx Power
        */
        signed readTxPwr(void);

        /**
                Get the Rx Power.
                @return current Rx Power
        */
        signed readRxPwrCoarse(void);

        /**
                Get the Rx Power.
                @return current Rx Power
        */
        long long readRxPwrFine();

	/**
	Get factory calibration values.
	@return current eeprom values.
	*/
	signed getFactoryCalibration(const char * param);

	/**
	Get factory calibration values.
	@return current eeprom values.
	*/
	signed getFactoryCalibrationVal(unsigned param);

	/**
	Get factory calibration values.
	@return current eeprom values.
	*/
	signed getFactoryCalibrationStr(unsigned param, unsigned index);

        /**
                Set the TX QMC gain and phase.
                @return true on success.
        */
        bool setTxQmcGain(unsigned gain_A, unsigned gain_B, unsigned phase);

        /**
                Set the TX QMC offset.
                @return true on success.
        */
        bool setTxQmcOffset(unsigned offset_A, unsigned offset_B);

        /**
                Set the RX QMC gain and phase.
                @return true on success.
        */
        bool setRxQmcGain(unsigned gain_A, unsigned gain_B, unsigned phase);

        /**
                Set the RX QMC offset.
                @return true on success.
        */
        bool setRxQmcOffset(unsigned offset_A, unsigned offset_B);

	/**
		Set power wrt full scale.
		@param dB Power level wrt full power.
		@return true on success.
	*/
	bool setPower(int dB);

	/**
		Set TSC for all slots on the ARFCN.
		@param TSC TSC to use.
		@return true on success.
	*/
	bool setTSC(unsigned TSC);

 	/**
		Set the BSIC (including setting TSC for all slots on the ARFCN).
		@param BSIC BSIC to use.
		@return true on success.
	*/
	bool setBSIC(unsigned BSIC);

	/**
		Describe the channel combination on a given slot.
		@param TN The timeslot number 0..7.
		@param combo Channel combination, GSM 05.02 6.4.1, 0 for inactive.
		@return true on success.
	*/
	bool setSlot(unsigned TN, unsigned combo);

	/**
		Set the given slot to run the handover burst correlator.
		@param TN The timeslot number.
		@return true on succes.
	*/
	bool setHandover(unsigned TN);

	/**
		Clear the given slot to run the handover burst correlator.
		@param TN The timeslot number.
		@return true on success.
	*/
	bool clearHandover(unsigned TN);

	//@}

	/** Install a decoder on this ARFCN. */
	void installDecoder(UMTS::L1Decoder* wL1);


	private:

        /** Receive loop; runs in the receive thread. */
        void transmitLoop();

	/** Receive loop; runs in the receive thread. */
	void receiveLoop();

	/** Demultiplex and process a received burst. */
	void receiveBurst(const UMTS::RxChipsBurst&);

        /** Receiver loop. */
        friend void* TransmitLoopAdapter(ARFCNManager*);

	/** Receiver loop. */
	friend void* ReceiveLoopAdapter(ARFCNManager*);

	/**
		Send a command packet and get the response packet.
		@param command The NULL-terminated command string to send.
		@param response A buffer for the response packet, assumed to be char[MAX_PACKET_LENGTH].
		@return Length of the response or -1 on failure.
	*/
	int sendCommandPacket(const char* command, char* response);

	/**
		Send a command with a parameter.
		@param command The command name.
		@param param The parameter for the command.
		@param index Another parameter for the command.
		@param responseParam Optional parameter returned
		@return The status code, 0 on success, -1 on local failure.
	*/
	int sendCommand(const char*command, int param, int index, int *responseParam);

	/**
		Send a command with a parameter.
		@param command The command name.
		@param param The parameter for the command.
		@param responseParam Optional parameter returned
		@return The status code, 0 on success, -1 on local failure.
	*/
	int sendCommand(const char*command, const char*param, int *responseParam);

        int sendCommand(const char*command, long long *responseParam);

	/**
		Send a command with a parameter.
		@param command The command name.
		@param param The parameter for the command.
                @param responseParam Optional parameter returned
		@return The status code, 0 on success, -1 on local failure.
	*/
	int sendCommand(const char* command, int param, int *responseParam=NULL);

	/**
		Send a command with a string parameter.
		@param command The command name.
		@param param The string parameter(s).
		@return The status code, 0 on success, -1 on local failure.
	*/
	int sendCommand(const char* command, const char* param);


	/**
		Send a command with no parameter.
		@param command The command name.
		@return The status code, 0 on success, -1 on local failure.
	*/
	int sendCommand(const char* command);


};

/** C interface for ARFCNManager threads. */
void* TransmitLoopAdapter(ARFCNManager*);

/** C interface for ARFCNManager threads. */
void* ReceiveLoopAdapter(ARFCNManager*);

#endif // TRXMANAGER_H
// vim: ts=4 sw=4
