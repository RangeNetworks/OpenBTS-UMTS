/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <stdint.h>
#include "ByteVector.h"

// This is the algorithm as defined in the spec.
uint32_t AlgorithmF9( uint8_t *key, int count, int fresh, int dir, uint8_t *data, int length );

class IntegrityProtect
{
	static const int sIpNumSrbs = 5;
	// For GSM subscribers (no USIM) IK is generated from Kc, after which we could discard Kc.
	uint64_t mKc;		// Used to generate mIK.
	uint8_t mIK[16];	// 128 bit Integrity Key; see setKc().
	// 8.5.10 has the list of messages that are integrity protected, but they might as well not have bothered -
	// it is just all messages on DCCH are integrity protected, and on other channels they are not.
	// The spec goes on and on about Integrity Protection being started or not, but it is massive overkill:
	// 33.102 6.4.5 specifies that Integrity Protection is mandatory except for the authentication procedure,
	// so we must use the exact ladder diagram in the SGSN, do NOT do Integrity Protection of the Layer3
	// authentication messages, and then DO Integrity Protection of the immediately following SecurityMode command.
	// The Integrity Protection is "started" by the first message that includes the IntegrityProtection IE,
	// which will be the SecurityMode command.  Integrity Protection is "stopped" when we enter idle mode,
	// which is a nonsensical statement because you cant send DCCH messages in idle mode.
	// Note that Integritry Protection control is in Layer2 but it uses the Kc set by the Authentication message
	// sent by Layer3.  If you send a new Authentication message, the Kc does not become effective for
	// Integrity Protection until Layer2 sends the SecurityMode command, but that is irrelevant to us also
	// because we always do those immediately after each other as a single procedure.
	// TODO: We are also supposed to stop all other activity during the SecurityCommand procedure,
	// which doesnt matter much because it is the first thing we do, but it may be needed if we timeout
	// during the procedure, but I think that case is caught and handled in the SGSN.
	bool mIntegrityStarted;		// Set when we are supposed to be integrity protecting.
	// The count-i per radio bearer.
	// I do not understand why they want one of these for SRB0, since that is defined
	// as used for CCCH, which is not integrity protected.
	uint32_t mDlCounti[sIpNumSrbs];	// For SRB0-SRB4.
	//uint32_t mUlCounti[sIpNumSrbs];// Same for uplink, except we are not going to bother with it.

	uint32_t mFresh;	// Yet another random number chosen by RRC.
	uint32_t mStart;	// 20-bit init value for RRC HFN.
	public:
	uint32_t getStart() { return mStart; }
	uint32_t getFresh() { return mFresh; }

	void setKc(uint64_t Kc);
	void setKcs(std::string kcs);
	uint32_t runF9(unsigned rbid, bool dir, ByteVector &msg);
	// They call the bottom 4 bits of COUNT-I the RRC SN [sequence number]
	unsigned getDlRrcSn(unsigned rbid) { return 0xf & mDlCounti[rbid]; }
	void advanceDlRrcSn(unsigned rbid) { mDlCounti[rbid]++; }

	void initIntegrity() {
		for (int rbid = 0; rbid < sIpNumSrbs; rbid++) {
			// 33.102 6.5.4.1 COUNT-I initialized from START as follows:
			mDlCounti[rbid] = (mStart << 12) & 0x0FFFFF000;
		}
	}
	void updateStartValue(uint32_t newStart) {
		mStart = newStart;
                for (int rbid = 0; rbid < sIpNumSrbs; rbid++) {
                        mDlCounti[rbid] = (mDlCounti[rbid] & 0x0FFF) | ((mStart << 12) & 0x0FFFFF000);
                }
	}

	// 13.4.10: Integrity Protection is turned off when entering/leaving RRC Connected Mode, ie, idle mode.
	// We are allowed to send the Security Mode Command multiple times to make sure it gets through,
	// and if we were doing that we would want to not increment anything so that it is the same each time,
	// but since we are not doing that, and since the Security Mode Command is the first command
	// that is protected (using the IK from the Kc from a previous Layer3 authentication command) we
	// are goint to set the integrityStarted variable immediately instead of waiting for the
	// IntegrityModeComplete from the UE.  TODO: That may not be right, in case we have to rerun something.
	void integrityStart() { initIntegrity(); mIntegrityStarted = true; }
	void integrityStop() { mIntegrityStarted = false; }
	bool isStarted() { return mIntegrityStarted; }	// Is security mode started?

	protected: void _initIntegrityProtect() {
		mIntegrityStarted = false;	// Not started yet.
		mStart = 0;	// A fine place to start.
		mFresh = 1;	// A fine random number.
	}
	public: IntegrityProtect() { _initIntegrityProtect(); }
};

