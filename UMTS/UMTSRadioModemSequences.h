/**@file Radiomodem, for physical later processing bits <--> chips */

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

#ifndef UMTSRADIOMODEMSEQS_H
#define UMTSRADIOMODEMSEQS_H

#include <BitVector.h>
#include <UMTSCommon.h>


namespace UMTS {
// Sec. 4.3.3 of 3GPP 25.213
extern const BitVector gRACHSignatures[16];

//Sec. 5.3.3.7 of 3GPP 25.211
extern const BitVector gAICHSignatures[16];



/*
Table 2:
Slot Form at #i		N_pilot
       0		6
      0A		5
      0B		4
       1		8
       2		5
      2A		4
      2B		3
       3		7
*/

// Sec 5.2.2.1.3 of 3GPP 25.211
extern const BitVector gRACHMessagePilots[15];

//Sec 6.1.1 of 3GPP 25.214
// gRACHSubchannel[i][j] = access slot available for subchannel i and SFN % 8 = j, -1 means no slot available
extern const int gRACHSubchannels[12][8];


//Sec 5.2.1.1 of 3GPP 25.211
extern const BitVector gPilotPatterns[6][gFrameSlots];

//Sec. 5.2.3.2 of 3GPP 25.213
// Allocation SCH sequences for S-SCH channel
extern const int gSSCAllocations[64][gFrameSlots];

}

// Assuming one sample per chip.  
#endif
