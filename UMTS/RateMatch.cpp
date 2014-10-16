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

#include "BitVector.h"
#include "Logger.h"
#include "URRCDefs.h"
//#include "UMTSL1FEC.h"
#include "RateMatch.h"
#include "Utils.h"
#include <math.h>	// for ceil, round

namespace UMTS {

#if RATE_MATCH_TEST
// Define some funcs we need so we dont have to link with the entire OpenBTS-UMTS
unsigned TTICode2NumFrames(TTICodes ttiCode)	// return 1,2,4,8
{
	switch (ttiCode) {
	case TTI10ms: return 1;
	case TTI20ms: return 2;
	case TTI40ms: return 4;
	case TTI80ms: return 8;
	default: assert(0);
	}
}
int gcd(int x, int y)
{
	if (x > y) {
		return x % y == 0 ? y : gcd(y, x % y);
	} else {
		return y % x == 0 ? x : gcd(x, y % x);
	}
}
#endif

	// 25.212 4.2.5.2 table 4: Inter-Column permutation pattern for 1st interleaving:
	// TODO: Dont duplicate this table from the FEC stack.
	static char sInter1Perm[4][8] = {
		{0}, // TTI = 10ms
		{0, 1}, // TTI = 20ms
		{0, 2, 1, 3}, // TTI = 40ms
		{0, 4, 2, 6, 1, 5, 3, 7} // TTI = 80ms
	};

char *inter1Perm(TTICodes tticode)
{
	assert(tticode >= 0 && tticode <= 3);
	return sInter1Perm[(unsigned)tticode];
}

// 3GPP 25.212 4.2.7 Rate Matching.
// We only support convolutionally encoded vectors, not turbo-coded.
	// Star notation:
	//		X* = Y ::= for all x do Xx = Y;
	//		Y = X* ::= for any x do Y = Xx
	// Ni,j for uplink: bits in radio frame before rate matching on TrCh i, TFC j
	//		for downlink: a non-integer intermediate calculation variable step 1/8
	// deltaNi,j Number of repeated/dropped bits on TrCh i TFC j
	// Ndata,j Total bits in CCTrCh in a radio frame for TFC j.
	// NTTIi,j (TTI is superscript; i,j is subscript): bits in TTI before rate matching.
	// Fi - number of radio frames in the transmission time interval for TrCh i.
	// I = number of transport channels.
	// P1(ni) = Column permutation function of first interleaver from 4.2.5.2 table 4.
	// P1F(ni) = inverse of P1(), the original position of column number x after permutation.
	// S[n] = shift of puncturing or repetition for radio frame ni when n = P1Fi(ni).
	// Table is so small, here it is:
	// 4.2.5.2 table 4: Inter-column permutation patterns for 1st interleaving:
	//	TTI=10ms F=1  P1C(0) = <0>
	//	TTI=20ms F=2  P1C(n) for n=0,1 = <0,1>
	//	TTI=40ms F=4  P1C(n) for n=0..3 = <0,2,1,3>
	//	TTI=80ms F=8  P1C(n) for n=0..7 = <0,4,2,6,1,5,3,7>
	//
	// 4.2.7: Rate Matching
	// Equation 1:
	// Zi,j = big equation, but if RM is the same for all TrCh, simplifies to
	// distributing the available bits over TrCh evenly based on the number
	// of bits Ni,j available to go in the TrCh.
	// If only one TrCh:
	//	Z1,j = Ndata,j  (I is number of TrCh in CCTrCh)
	//	deltaN1,j = Ndata,j - Ni,j
	// If two TrCh:
	//	Z1,j = NData,j * N1,j / sum(N*,j)  ie, sum of Ni,j for all i
	// and: deltaNi,j = Ndata,j/I - Ni,j
	//	Z2,j = NData,j

	// so the rate-matching is evenly distributed among all TrCh, as you would expect.
	// so: Zi,j - Z(i-1),j = Ndata,j/I
	//
	// UPLINK RATE MATCHING PARAMETERS:
	// 4.2.7.1.1 Determination of SF and number of PhCh needed.
	// This is a no-op for us because RM is same for all TrCh.
	// If use use SF=256, then SET0 for RACH is just N256, and there is nothing more to say.
	// RACH SET0 is allowed to be {N256,N128,N64,N32}
	// Since RM is invariant, the SET1 equation reduces to:
	//		SET1 = { Ndata in SET0 such that Ndata - sum(Nx,j for all TrCh x) > 0 }
	// in other words, Ndata,j = the smallest SF (biggest SF number) in which the data will
	// fit with no puncturing allowed.
	// 4.2.7.1.2 Determination of Parameters needed for calculating the rate matching pattern.
	// We can ignore this.
	// 4.2.7.1.2.1 Convolutionally coded TrCh.
	// R = deltaNi,j positive mod Ni,j 
	// Note that the the mod only does something if delta > Ni,j, ie, bits repeated twice.
	//
	// DOWNLINK RATE MATCHING PARAMETERS:
	//	In downlink, you dont use Rate-matching to expand the the bits to
	//	fit the radio frame, you use DTX from 4.2.9 instead.
	//	So you only use rate-matching for puncturing those TFC that are too big to fit,
	//	and we wont do that, so we're done.
	//	The descriptions are confusing because you calculate Ni,max, which is the
	//	Ni of the largest TFC in the TFS, but that is correct, because it is only
	//	the ones bigger than that that need to be punctured.
	// Downlink calculation is independent TFC (j) so in notation below:
	//		Ndata,j is replaced by NData,* and Ni,j is replaced by Ni,*
	// 4.2.7.2: Determination of rate matching parameters in downlink.
	//	Ndata,* is just the number of bits in the radio frame.
	// 4.2.7.2.1.1: Calculation of deltaNi,max for normal mode and ...
	//	Ni,* is the number of bits in the largest TFS, per radio frame (part of a TTI)
	//	deltaNi,max is diff in the largest TFS per entire TTI.
	// 4.2.7.2.1.3 Determination of rate matching paramters for convolutionally coded TrCh
	//		TODO

#if 0
void rateMatchFunc(BitVector &in,BitVector &out, int eini)
{
	int nin = in.size();
	int nout = out.size();
	if (nout == nin) {
		in.copyTo(out);
		return;
	}
	int eplus = 2 * nin;				// eplus = a * Ni,j
	int eminus = 2 * (nout - nin);	// eminus = a * abs(deltaNi,j)
	if (eminus < 0) { eminus = - eminus; }
	float e = eini;
	int m; // index of current bit, except zero-based as opposed to spec that is 1 based.
	char *inp = in.begin();		// cheating just a bit for efficiency.
	char *outp = out.begin();
	char *outend = out.end();
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
#endif

// Compute eplus and eminus for TrCh that compute it from the current TF vector sizes.
void rateMatchComputeEplus(int nin, int nout, int *eplus, int *eminus) {
	*eplus = 2 * nin;				// eplus = a * Ni,j
	*eminus = 2 * (nout - nin);	// eminus = a * abs(deltaNi,j)
	if (*eminus < 0) { *eminus = - *eminus; }
}

// Uplink only.  In downlink, eini == 1.
// Output goes into: int einis[numRadioFrames]
// The insize and outsize are for each radio frame, which would be the same sizes
// fed to the rateMatchFunc, not the the total number of bits in whole the TTI.
void rateMatchComputeUlEini(int insize, int outsize, TTICodes tticode,int *einis)
{
	int numRadioFrames = (int) TTICode2NumFrames(tticode);	// return 1,2,4,8
	int debug = 0;
	// 4.2.7.1.2.1 verbatim.
	// This is massive way overkill, because for the common cases we could just use:
	// If TrCh=1, TTI=10ms, eini = 1;
	// If TrCh=1, TTI=20ms, eini = 1 for first tti, eini = ? for second tti;
	int Nij = insize;
	int deltaNij = outsize - insize;

	if (insize == 0 && outsize == 0) { 
          for (int ni = 0; ni < numRadioFrames; ni++) einis[ni] = 0;
	  return;
        }


	// Compute R
	int R = deltaNij % Nij;
	while (R < 0) {R += Nij;}

	// Compute q
	int q;
	if (R && 2*R <= Nij) {
		q = ceil((double)Nij/R);
	} else {
		q = ceil((double)Nij/(R-Nij));
	}

	// Compute qprime
	double qprime;
	int qpos = q > 0 ? q : - q;
	// Compute greatest common divisor.
	int gcdq;
	if ((qpos&1) == 0) { // if q is even
		gcdq = gcd(abs(q),numRadioFrames);
		qprime = q + (double)gcdq/numRadioFrames;
	} else {
		qprime = q;
	}

	if (debug) { printf("R=%d q=%d gcd=%d qprime=%g\n",R,q,gcdq,qprime); }

	// Compute S
	// For TTI=10 and 20, it is an identity function.
	int S[8];
	for (int x = 0; x < numRadioFrames; x++) {
		int tmp1 = abs((int) floor(x*qprime));
		int tmp3 = tmp1 % numRadioFrames;
		S[tmp3] = tmp1 / numRadioFrames;
		if (debug) printf("S[%d]=%d ",tmp3,tmp1/numRadioFrames);
	}
	if (debug) printf("\n");

	// Get P1Fi(ni)
	char *P1F = inter1Perm(tticode);

	// Compute the eini for each radio frame ni.
	for (int ni = 0; ni < numRadioFrames; ni++) {
		int p1f = P1F[ni];
		int a = 2;
		einis[ni] = (a * S[p1f] * abs(deltaNij) + 1) % (a * Nij);
	}
}

};	// namespace

#if RATE_MATCH_TEST
#include <stdlib.h>	// for rand
#include "Configuration.h"
using namespace UMTS;

// Return rand in the range 0..maxval inclusive.
static int rangerand(int minval, int maxval, unsigned *pseed)
{
	int range = maxval - minval;
	// Note: Should be /(RAND_MAX+1) but that overflows, but we will post-bound.
	double randfrac = ((double)rand_r(pseed) / RAND_MAX); // range 0..1
	int result = minval + (int) round(randfrac * range);
	return RN_BOUND(result,minval,maxval);	// Double check that it is in range.
}

static void printdiff(BitVector &v1, BitVector &v2)
{
	int diffbit, v1size = v1.size();
	for (diffbit=0; diffbit < v1size; diffbit++) {
		if (v1.bit(diffbit) != v2.bit(diffbit)) { break; }
	}
	if (diffbit < v1size) {
		//printf("vectors size %d differ at bit %d\n",v1.size(),diffbit);
		//printf("1= %s\n",v1.hexstr().c_str());
		//printf("2= %s\n",v2.hexstr().c_str());
		printf("d=");
		for (int i = 0; i < v1size; i++) {
			if (v1.bit(i) != v2.bit(i)) {
				printf("%c", '0' + v1.bit(i));
			} else {
				printf("-");
			}
		}
		printf("\n");
	}
}

static void testRateMatchFunc(int insize, int outsize, int eini)
{
	//ByteVector *svectors[sMaxTestVectors];
	int numTestVectors = 100;
	int ntests = 0;
	unsigned sseed = 1;
	for (int n = 0; n < numTestVectors; n++) {
		// Generate a random sized test vector.
		//int len = rangerand(2,40,&sseed);	// 2..100 bytes.
		int len = insize;
		printf("vector %d size %d\n",n,len);
		BitVector vtest(len);
		for (int j = 0; j < len; j++) {
			int val;
			switch (n%4) {	// Lets do 4 different test vectors.
			case 0: val = 0; break;
			case 1: val = 1; break;
			case 2: val = j & 1; break;
			case 3: val = !(j&1); break;
			}
			vtest.fillField(j,val,1);
		}
		//svectors[n] = new ByteVector(len);
		//svectors[n]->setField(0,n,16);
		//for (int j = 2; j < len; j++) {
			//svectors[n]->setByte(j,j); // Fill the rest of the test vec with numbers.
		//}

		// Now test with various puncturing and repetition...
		int size = vtest.size();
		//for (int diff = -size/2+1; diff < size; diff++) {
		for (int diff = outsize-size; diff <= outsize-size; diff++) {
			ntests++;
			BitVector there(size+diff);
			BitVector andBackAgain(size);
			UMTS::rateMatchFunc(vtest,there,eini);
			UMTS::rateMatchFunc(there,andBackAgain,eini);
			if (!(vtest == andBackAgain)) {
				printf("vector %d differs insize=%d outsize=%d %s %d\n",
					n,vtest.size(),there.size(),diff < 0 ? "puncture" : "repeat",diff);
				if (1) {
					std::cout<<"1="<<vtest<<"\n";
					std::cout<<"2="<<there<<"\n";
					std::cout<<"3="<<andBackAgain<<"\n";
				} else {
					printf("1=%s\n",vtest.hexstr().c_str());
					printf("2=%s\n",there.hexstr().c_str());
					printf("3=%s\n",andBackAgain.hexstr().c_str());
				}
				printdiff(vtest,andBackAgain);
			}
		}
	}
	printf("Ran %d tests\n",ntests);
}


static void testOneEini(int insize, int outsize, TTICodes tticode, int *eini)
{
	UMTS::rateMatchComputeUlEini(insize, outsize,tticode,eini);
	int numRadioFrames = (int) TTICode2NumFrames(tticode);
	printf("in=%d out=%d tti=%d ",insize,outsize,(int)tticode);
	printf("eini=");
	for (int i = 0; i < numRadioFrames; i++) {
		printf(" %d",eini[i]);
	}
	printf("\n");
}

static void testEini()
{
	for (int size = 8; size < 100; size++) {
		for (int diff = -size/2+1; diff < size; diff++) {
			for (int tti = 0; tti <= 3; tti++) {
				int eini[8];
				testOneEini(size,size+diff,(TTICodes)tti,eini);
			}
		}
	}
}


ConfigurationTable gConfig;	// geesh
int main(int argc, char **argv)
{
	int eini[8];
	int insize = 5904;
	int outsize = 9600;
	testOneEini(insize,outsize,TTI10ms,eini);
	//testOneEini(144,150,TTI20ms);
	testRateMatchFunc(insize,outsize,eini[0]);
	//testEini();
	return 0;
}
#endif
