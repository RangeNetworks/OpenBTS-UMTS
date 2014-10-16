/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef TMSITABLE_H
#define TMSITABLE_H

#include <map>

#include <Timeval.h>
#include <Threads.h>
#include <string.h>


struct sqlite3;

namespace GSM {
class L3LocationUpdatingRequest;
class L3MobileStationClassmark2;
class L3MobileIdentity;
}


namespace Control {

class TMSITable {

	private:

	sqlite3 *mDB;			///< database connection


	public:

	TMSITable(const char*wPath);

	~TMSITable();

	/**
		Create a new entry in the table.
		@param IMSI	The IMSI to create an entry for.
		@param The associated LUR, if any.
		@return The assigned TMSI.
	*/
	unsigned assign(const char* IMSI, const GSM::L3LocationUpdatingRequest* lur=NULL);

	/**
		Find an IMSI in the table.
		This is a log-time operation.
		@param TMSI The TMSI to find.
		@return Pointer to IMSI to be freed by the caller, or NULL.
	*/
	char* IMSI(unsigned TMSI) const;

	/**
		Find a TMSI in the table.
		This is a linear-time operation.
		@param IMSI The IMSI to mach.
		@return A TMSI value or zero on failure.
	*/
	unsigned TMSI(const char* IMSI) const;

	/** Write entries as text to a stream. */
	void dump(std::ostream&) const;
	
	/** Clear the table completely. */
	void clear();

	/** Set the IMEI. */
	bool IMEI(const char* IMSI, const char* IMEI);

	/** Set the classmark. */
	bool classmark(const char* IMSI, const GSM::L3MobileStationClassmark2& classmark);

	/** Save a RAND/SRES pair. */
	void putAuthTokens(const char* IMSI, uint64_t upperRAND, uint64_t lowerRAND, uint32_t SRES);

	/** Get a RAND/SRES pair. */
	bool getAuthTokens(const char* IMSI, uint64_t &upperRAND, uint64_t &lowerRAND, uint32_t &SRES);

	/** Save Kc. */
	void putKc(const char* IMSI, std::string Kc);

	/** Get Kc. */
	std::string getKc(const char* IMSI);

	/** Get the next TI value to use for this IMSI or TMSI. */
	unsigned nextL3TI(const char* IMSI);

	private:

	/** Update the "accessed" time on a record. */
	void touch(unsigned TMSI) const;
};


}

#endif

// vim: ts=4 sw=4
