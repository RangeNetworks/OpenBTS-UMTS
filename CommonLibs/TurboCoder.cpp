/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "BitVector.h"
#include "TurboCoder.h"
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <sstream>

using namespace std;


/** (pat) This is the old version.  There is a new version in BitVector.cpp
  Apply a Galois polymonial to a binary seqeunce.
  @param val The input sequence.
  @param poly The polynomial.
  @param order The order of the polynomial.
  @return Single-bit result.
*/
static unsigned applyPolyOld(uint64_t val, uint64_t poly)
{
	uint64_t prod = val & poly;
	prod = (prod ^ (prod >> 32));
	prod = (prod ^ (prod >> 16));
	prod = (prod ^ (prod >> 8));
	prod = (prod ^ (prod >> 4));
	prod = (prod ^ (prod >> 2));
	prod = (prod ^ (prod >> 1));
	return prod & 0x01;
}

int turboCoderConstituentEncoder(int &D, int inbit)
{
	int D0 = D;
	int D1 = D >> 1;
	int D2 = D >> 2;
	int nextin = (inbit ^ D1 ^ D2) & 1;
	int zk = (D2 ^ D0 ^ nextin) & 1;
	D = (D << 1) | nextin;
	return zk;
}



void turboCoderTrellisTermination(int &D, int &xk, int &zk)
{
	int D0 = D;
	int D1 = D >> 1;
	int D2 = D >> 2;
	xk = (D2 ^ D1) & 1;
	zk = (D2 ^ D0) & 1;
	D = D << 1;
}



void BitVector::encode(const ViterbiTurbo& coder, BitVector& target, TurboInterleaver& wInterleaver)
{
	assert(target.size() == size() * 3 + 12);
	int CE1 = 0;
	int CE2 = 0;
	char *outPtr = target.begin();
	vector<int> permutation = wInterleaver.permutation();
	for (unsigned i = 0; i < size(); i++) {
		int inbit1 = mStart[i];
		int inbit2 = mStart[permutation[i]];
		*outPtr++ = inbit1;
		*outPtr++ = turboCoderConstituentEncoder(CE1, inbit1);
		*outPtr++ = turboCoderConstituentEncoder(CE2, inbit2);
	}
	int xk, zk;
	for (int i = 0; i < 3; i++) {
		turboCoderTrellisTermination(CE1, xk, zk);
		*outPtr++ = xk;
		*outPtr++ = zk;
	}
	for (int i = 0; i < 3; i++) {
		turboCoderTrellisTermination(CE2, xk, zk);
		*outPtr++ = xk;
		*outPtr++ = zk;
	}
}



ViterbiTurbo::ViterbiTurbo()
{
	assert(mDeferral < 32);
	mCoeffs[0] = 0x01;
	mCoeffs[1] = 0x0b;
	computeStateTables(0);
	computeStateTables(1);
	computeGeneratorTable();
}




void ViterbiTurbo::initializeStates()
{
	for (unsigned i=0; i<mIStates; i++) clear(mSurvivors[i]);
	for (unsigned i=0; i<mNumCands; i++) clear(mCandidates[i]);
}



void ViterbiTurbo::computeStateTables(unsigned g)
{
	assert(g<mIRate);
	for (unsigned state=0; state<mIStates; state++) {
		// 0 input
		uint64_t inputVal = state<<1;
		mStateTable[g][inputVal] = applyPolyOld(inputVal, mCoeffs[g]);
		// 1 input
		inputVal |= 1;
		mStateTable[g][inputVal] = applyPolyOld(inputVal, mCoeffs[g]);
	}
}



void ViterbiTurbo::computeGeneratorTable()
{
	for (unsigned index=0; index<mIStates*2; index++) {
		mGeneratorTable[index] = (mStateTable[0][index]<<1) | mStateTable[1][index];
	}
}






void ViterbiTurbo::branchCandidates()
{
	// Branch to generate new input states.
	const vCand *sp = mSurvivors;
	for (unsigned i=0; i<mNumCands; i+=2) {
		// extend and suffix
		const uint32_t iState0 = (sp->iState) << 1;				// input state for 0
		const uint32_t iState1 = iState0 | 0x01;				// input state for 1
		const uint32_t oStateShifted = (sp->oState) << mIRate;	// shifted output
		const float cost = sp->cost;
		const char D1 = (sp->rState) >> 1;
		const char D2 = (sp->rState) >> 2;
		const char fb = (D1 ^ D2) & 1;  // feedback that affects the real next state
		const uint32_t rState0 = ((sp->rState << 1) ^ fb) << 0;
		const uint32_t rState1 = rState0 ^ 1;
		sp++;
		// 0 input extension
		mCandidates[i].cost = cost;
		mCandidates[i].oState = oStateShifted | (mGeneratorTable[0] & 2) | (mGeneratorTable[rState0 & mCMask] & 1);
		mCandidates[i].iState = iState0;
		mCandidates[i].rState = rState0;
		// 1 input extension
		mCandidates[i+1].cost = cost;
		mCandidates[i+1].oState = oStateShifted | (mGeneratorTable[1] & 2) | (mGeneratorTable[rState1 & mCMask] & 1);
		mCandidates[i+1].iState = iState1;
		mCandidates[i+1].rState = rState1;

	}
}



/*
 * Here's my thinking about decoding to soft decoder outputs:
 * It can't be exact because it's np-hard.  So I figure simulate digital
 * encoder inputs and encoder outputs, keep a rough estimate of confidence
 * when comparing simulated encoder outputs to decoder inputs, and use that
 * confidence to make the decoder output soft.  My rough estimate of
 * confidence in a simulated  encoder output (hard) to decoder input (soft) is exact match
 * (0 : 0.0 or 1 : 1.0) is 100%, exact mismatch (0 : 1.0 or 1 to 0.0) is 0%,
 * and interpolate linearly between them.  I combine those confidences by
 * averaging.  Here I add all the confidences, later to divide by how many were added.
 * The infrastructure was already in place for cost to minimize rather than
 * confidence to maximize, so I save cost = 1 - confidence.
 * See SoftVector::decode to see how these get turned into decoder outputs.
 */
void ViterbiTurbo::getSoftCostMetrics(const float *inSample)
{
	for (unsigned i=0; i<mNumCands; i++) {
		vCand& thisCand = mCandidates[i];
		// We examine input bits 2 at a time for a rate 1/2 coder.
		thisCand.cost += (
			((thisCand.oState & 1) ? (1.0 - inSample[0])  : inSample[0]) +
			((thisCand.oState & 2) ? (1.0 - inSample[-1]) : inSample[-1]));
	}
}


void ViterbiTurbo::pruneCandidates()
{
	const vCand* c1 = mCandidates;					// 0-prefix
	const vCand* c2 = mCandidates + mIStates;		// 1-prefix
	for (unsigned i=0; i<mIStates; i++) {
		if (c1[i].cost < c2[i].cost) mSurvivors[i] = c1[i];
		else mSurvivors[i] = c2[i];
	}
}


const ViterbiTurbo::vCand* ViterbiTurbo::minCost() const
{
	int minIndex = 0;
	float minCost = mSurvivors[0].cost;
	for (unsigned i=1; i<mIStates; i++) {
		const float thisCost = mSurvivors[i].cost;
		if (thisCost>=minCost) continue;
		minCost = thisCost;
		minIndex=i;
	}
	return mSurvivors + minIndex;
}


const ViterbiTurbo::vCand* ViterbiTurbo::step(const float *inSample)
{
	branchCandidates();
	getSoftCostMetrics(inSample);
	pruneCandidates();
	return minCost();
}



/*
 * The deferral queue:
 * Unlike the other Viterbi algorithms, above, this one doesn't put
 * arbitrary values into the decoder input vector, and use unknown for
 * both a match and mismatch.  That created extra noise in the decoder output.
 * Instead, this one simply flushes the last deferral values
 * from the last best candidate, using the confidence from that candidate.
 *
 * Computing soft output values:
 * Get average cost by dividing by how many costs got added.
 * Confidence = 1 - average cost.
 * Digital output @ confidence -> soft output:
 * 0 @ 0% -> 0.5
 * 0 @ 100% -> 0.0
 * 1 @ 0% -> 0.5
 * 1 @ 100% -> 1.0
 * interpolate linearly
 */

void SoftVector::decode(ViterbiTurbo &decoder, SoftVector& target) const
{
	int K = target.size();
	assert(size() == (unsigned)(2 * K));
	const unsigned deferral = decoder.deferral();
	decoder.initializeStates();
	const unsigned step = decoder.iRate();
	// input pointer
	const float *ip = mStart + step - 1;
	// output pointer
	float *op = target.begin();
	// fill up the deferral queue
	for (unsigned  i = 0; i < deferral; i++, ip += step) {
		decoder.step(ip);
	}
	const ViterbiTurbo::vCand *minCost = 0;
	float confidence = 0;
	// the normal stuff
	int avgCount = (deferral + 1) * 2;
	for (unsigned i = 0; i < K-deferral; i++, ip += step, avgCount += 2) {
		minCost = decoder.step(ip);
		confidence = 1.0 - (minCost->cost / (float)avgCount);
		//cout << "i: " << i << " cost: " << minCost->cost << " conf: " << confidence << endl;
		*op++ = ((minCost->iState >> deferral) & 1) ?
			(confidence + 1.0) / 2.0 :
			(1.0 - confidence) / 2.0 ;
	}
	// flush the deferral queue
	for (int i = deferral-1; i >= 0; i--) {
		*op++ = ((minCost->iState >> i) & 1) ?
			(confidence + 1.0) / 2.0 :
			(1.0 - confidence) / 2.0 ;
	}
}



void SoftVector::decode(ViterbiTurbo &decoder, BitVector& target, TurboInterleaver& wInterleaver) const
{
	assert(size() == target.size() * 3 + 12);
	int K = (size() - 12) / 3;
	SoftVector xkzk(2 * K);
	// extract xk and zk
	for (int i = 0; i < K; i++) {
		xkzk[i*2] = mStart[i*3]; // xk
		xkzk[i*2+1] = mStart[i*3+1]; // zk
	}
	// first pass decode
	SoftVector t(K);
	xkzk.decode(decoder, t);
	// interleave
	SoftVector ti(K);
	wInterleaver.interleave(t, ti);
	// interleave ti with z'k
	SoftVector tizpk(2 * K);
	for (int i = 0; i < K; i++) {
		tizpk[i*2] = ti[i];
		tizpk[i*2+1] = mStart[i*3+2];
	}
	// second pass decode
	SoftVector tt(K);
	tizpk.decode(decoder, tt);
	// de-interleave
	wInterleaver.unInterleave(tt, target);
	//target = t.sliced();
}

int haltDecode = 0;

void SoftVector::decode(ViterbiTurbo &decoder, SoftVector& target, TurboInterleaver& wInterleaver) const
{
	std::cout << size() << " " << target.size() << endl;
	assert(size() == target.size() * 3 + 12);
	int K = (size() - 12) / 3;
	SoftVector xkzk(2 * K);
	SoftVector guh(K);
	// extract xk and zk
	for (int i = 0; i < K; i++) {
		xkzk[i*2] = guh[i] = mStart[i*3]; // xk
		xkzk[i*2+1] = mStart[i*3+1]; // zk
	}
	// first pass decode
	SoftVector t(K);
	xkzk.decode(decoder, t);
	std::cout << "t: " << t << endl;
	// interleave
	SoftVector ti(K);
	for (int i = 0; i < K; i++) {
		guh[i] = 0.5*(0*guh[i] + 2*t[i]);
	} 
	wInterleaver.interleave(guh/*t*/, ti);
	// interleave ti with z'k
	SoftVector tizpk(2 * K);
	for (int i = 0; i < K; i++) {
		tizpk[i*2] = ti[i];
		tizpk[i*2+1] = mStart[i*3+2];
	}
	// second pass decode
	SoftVector tt(K);
	tizpk.decode(decoder, tt);
	// de-interleave
	SoftVector tempTarget(target.size());
	wInterleaver.unInterleave(tt, tempTarget);
	SoftVector tempOutput = *this;
        for (int i = 0; i < K; i++) {
                tempOutput[3*i] = 0.5*(tempTarget[i]+tempOutput[3*i]);
        }
	haltDecode++;
	if (haltDecode < 1) {
		tempOutput.decode(decoder,target,wInterleaver);
	}
	else {
		haltDecode = 0;
		target = tempTarget;
	}

	//target = t;
}



// UMTS FEC turbo coder internal interleaver, 25.212, 4.2.3.2.3
// The interleaving is so stunningly complicated that a permutation
// vector needs to be initialized when the channel is created,
// then interleaving works off the permutation vector.
TurboInterleaver::TurboInterleaver(int K)
{
	static const int pv[] = {
		7, 3,
		11, 2,
		13, 2,
		17, 3,
		19, 2,
		23, 5,
		29, 2,
		31, 3,
		37, 2,
		41, 6,
		43, 3,
		47, 5,
		53, 2,
		59, 2,
		61, 2,
		67, 2,
		71, 7,
		73, 5,
		79, 3,
		83, 2,
		89, 3,
		97, 5,
		101, 2,
		103, 5,
		107, 2,
		109, 6,
		113, 3,
		127, 3,
		131, 2,
		137, 3,
		139, 2,
		149, 2,
		151, 6,
		157, 5,
		163, 2,
		167, 5,
		173, 2,
		179, 2,
		181, 2,
		191, 19,
		193, 5,
		197, 2,
		199, 3,
		211, 2,
		223, 3,
		227, 2,
		229, 6,
		233, 3,
		239, 7,
		241, 7,
		251, 6,
		257, 3,
		0, 0
	};
	static const int irpp5[] = {4, 3, 2, 1, 0};
	static const int irpp10[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
	static const int irpp20[] = {19, 9, 14, 4, 0, 2, 5, 7, 12, 18, 16, 13, 17, 15, 3, 1, 6, 11, 8, 10};
	static const int irpp20a[] = {19, 9, 14, 4, 0, 2, 5, 7, 12, 18, 10, 8, 13, 17, 3, 1, 16, 6, 15, 11};

	if (K == 0) {
		mPermutation.resize(0);
		return;
	}

	assert(K >= 40 && K <= 5114);
	mPermutation.resize(K);

	// 4.2.3.2.3.1 step 1
	int R;
	if (K <= 159) {
		R = 5;
	} else if (K <= 200) {
		R = 10;
	} else if (K <= 480) {
		R = 20;
	} else if (K <= 530) {
		R = 10;
	} else {
		R = 20;
	}

	// 4.2.3.2.3.1 step 2
	int C, p;
	int v = 0; // init to shut up compiler warning
	if (K >= 481 && K <= 530) {
		p = 53;
		C = p;
	} else {
		for (int pvptr = 0; ; pvptr += 2) {
			p = pv[pvptr];
			v = pv[pvptr+1];
			assert(p != 0);
			if (K <= R * (p+1)) break;
		}
		if (K <= R * (p-1)) {
			C = p-1;
		} else if (R * (p-1) < K && K <= R*p) {
			C = p;
		} else if (R * p < K) {
			C = p+1;
		} else {
			assert(0);
		}
	}

	// 4.2.3.2.3.1 step 3
	vector<int> matrix(R*C);
	for (int i = 0; i < K; i++) {
		matrix[i] = i;
	}
	for (int i = K; i < R * C; i++) {
		matrix[i] = -1;
	}

	// 4.2.3.2.3.2 step 1
	// already have v

	// 4.2.3.2.3.2 step 2
	vector<int> s(p-1);
	s[0] = 1;
	for (int j = 1; j <= p-2; j++) {
		s[j] = (v * s[j-1]) % p;
	}

	// 4.2.3.2.3.2 step 3
	// resuse the prime number list in table 2
	vector<int> q(R);
	q[0] = 1;
	int pvptr = 0;
	for (int i = 1; i <= R-1; i++) {
		while (1) {
			q[i] = pv[pvptr];
			pvptr += 2;
			assert(q[i] != 0);
			assert(q[i] > 6);
			assert(q[i] > q[i-1]);
			if (gcd(q[i], p-1) == 1) break;
		}
	}

	// 4.2.3.2.3.2 step 4
	vector<int> r(R);
	const int *T;
	if (K <= 159) {
		T = irpp5;
		assert(R == 5);
	} else if (K <= 200) {
		T = irpp10;
		assert(R == 10);
	} else if (K <= 480) {
		T = irpp20a;
		assert(R == 20);
	} else if (K <= 530) {
		T = irpp10;
		assert(R == 10);
	} else if (K <= 2280) {
		T = irpp20a;
		assert(R == 20);
	} else if (K <= 2480) {
		T = irpp20;
		assert(R == 20);
	} else if (K <= 3160) {
		T = irpp20a;
		assert(R == 20);
	} else if (K <= 3210) {
		T = irpp20;
		assert(R == 20);
	} else {
		T = irpp20a;
		assert(R == 20);
	}
	for (int i = 0; i <= R-1; i++) {
		r[T[i]] = q[i];
	}

	// 4.2.3.2.3.2 step 5
	for (int i = 0; i < R; i++) {
		vector<int> U(C);
		for (int j = 0; j <= p-2; j++) {
			U[j] = s[(j * r[i]) % (p-1)];
		}
		if (C == p) {
			U[p-1] = 0;
		} else if (C == p+1) {
			U[p-1] = 0;
			U[p] = p;
			if (K == R*C && i == R-1) {
				int t = U[p];
				U[p] = U[0];
				U[0] = t;
			}
		} else if (C == p-1) {
			for (int j = 0; j <= p-2; j++) {
				U[j] = U[j] - 1;
			}
		} else {
			assert(0);
		}
		// now permute row i of matrix based on U
		vector<int> t(C);
		for (int j = 0; j < C; j++) {
			t[j] = matrix[i*C + U[j]];
		}
		for (int j = 0; j < C; j++) {
			matrix[i*C + j] = t[j];
		}
	}

	// 4.2.3.2.3.2 step 6 and 4.2.3.2.3.3
	// combine the inter-row permutation with "reading out the bits"
	// where reading out the bits is actually setting mPermutation
	int outptr = 0;
	// read out the columns in order
	// read out the rows in the order of T
	for (int col = 0; col < C; col++) {
		//std::cout << endl;
		for (int row = 0; row < R; row++) {
			int v = matrix[col + C*T[row]];
			if (v < 0) continue; // ignore padding
			//std::cout << v << " ";
			mPermutation[outptr++] = v;
		}
	}
	//std::cout << endl;
	assert(outptr == K);
}


int TurboInterleaver::gcd(int x, int y)
{
	if (x > y) {
		return x % y == 0 ? y : gcd(y, x % y);
	} else {
		return y % x == 0 ? x : gcd(x, y % x);
	}
}


void TurboInterleaver::interleave(SoftVector &in, SoftVector &out)
{   
	assert(in.size() == mPermutation.size());
	assert(out.size() == mPermutation.size());
	for (unsigned i = 0; i < in.size(); i++) {
		out[i] = in[mPermutation[i]];
	}              
}   

void TurboInterleaver::unInterleave(SoftVector &in, BitVector &out)
{   
	assert(in.size() == mPermutation.size());
	assert(out.size() == mPermutation.size());
	for (unsigned i = 0; i < in.size(); i++) {
		out[mPermutation[i]] = in[i] > 0.5 ? 1 : 0;
	}              
}   

void TurboInterleaver::unInterleave(SoftVector &in, SoftVector &out)
{   
	assert(in.size() == mPermutation.size());
	assert(out.size() == mPermutation.size());
	for (unsigned i = 0; i < in.size(); i++) {
		out[mPermutation[i]] = in[i];
	}              
}   



