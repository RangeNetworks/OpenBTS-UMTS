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

#include "Vector.h"
#include "Logger.h"
#include "URRCDefs.h"

namespace UMTS {
void rateMatchComputeUlEini(int insize, int outsize, TTICodes tticode,int *einis);
void rateMatchComputeEplus(int nin, int nout, int *eplus, int *eminus);


// This is for downlink with multiple TrCh.  The eplus,eminus are precomputed from the largest TF
// rather from the current TrCh.
template <class Type>
void rateMatchFunc2(Vector<Type> &in,Vector<Type> &out, int eplus, int eminus, int eini)
{
	int nin = in.size();
	int nout = out.size();
	if (nout == nin) {
		in.copyTo(out);
		return;
	}
	float e = eini;
	int m; // index of current bit, except zero-based as opposed to spec that is 1 based.
	Type *inp = in.begin();		// cheating just a bit for efficiency.
	Type *outp = out.begin();
	Type *outend = out.end();
	if (nout < nin) {
		// Puncture bits as necessary.
		// Note from spec: loop termination Xi == Xij == number of bits before rate matching == nin.
		for (m=0; m < nin && outp < outend; m++) {
			e = e - eminus;
			if (e <= 0) {
				e = e + eplus;
				continue;		// skip the bit.
			}
			*outp++ = inp[m];
		}
	} else {
		// Repeat bits as necessary.
		for (m=0; m < nin && outp < outend; m++) {
			e = e - eminus;
			while (e <= 0) {
				if (outp >= outend) goto failed;
				*outp++ = inp[m];	// repeat the bit.
				e = e + eplus;
			}
			*outp++ = inp[m];
		}
	}
	if (m != nin || outp != outend) {
		failed:
		LOG(ERR) << "rate matching mis-calculation, results:"
			<<LOGVAR(nin)<<LOGVAR(m)<<LOGVAR(nout)<<LOGVAR2(outp,outp-out.begin())
			<<LOGVAR(e)<<LOGVAR(eplus)<<LOGVAR(eminus)<<LOGVAR(eini);
	}
}

// 3GPP 25.212 4.2.7.5 Rate matching pattern determination.
// This version for be uplink channels and downlink when only one TF; use rateMatchFunc2 for downlink with multiple TF.
// This is the function that adds or elides bits from the bits to change the size.
// The in and out vectors should be the desired sizes.
// Note that the duplicate operation allows the output vector to be any
// arbitrary amount larger than the input vector; bits can be doubled, tripled, whatever.
template <class Type>
void rateMatchFunc(Vector<Type> &in,Vector<Type> &out, int eini)
{
	int nin = in.size();
	int nout = out.size();
	if (nout == nin) {
		in.copyTo(out);
		return;
	}
	//int eplus = 2 * nin;				// eplus = a * Ni,j
	//int eminus = 2 * (nout - nin);	// eminus = a * abs(deltaNi,j)
	//if (eminus < 0) { eminus = - eminus; }
	int eplus,eminus;
	rateMatchComputeEplus(nin,nout,&eplus,&eminus);
	rateMatchFunc2(in, out, eplus, eminus, eini);
}
};
