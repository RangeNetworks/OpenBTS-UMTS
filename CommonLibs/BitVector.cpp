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
#include <stdio.h>
#include <sstream>

using namespace std;

int gVectorDebug = 0;

/**
  Apply a Galois polymonial to a binary seqeunce.
  @param val The input sequence.
  @param poly The polynomial.
  @param order The order of the polynomial.
  @return Single-bit result.
*/
static unsigned applyPoly(uint64_t val, uint64_t poly)
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






BitVector::BitVector(const char *valString)
	:Vector<char>(strlen(valString))
{
	uint32_t accum = 0;
	for (size_t i=0; i<size(); i++) {
		accum <<= 1;
		if (valString[i]=='1') accum |= 0x01;
		mStart[i] = accum;
	}
}





uint64_t BitVector::peekField(size_t readIndex, unsigned length) const
{
	uint64_t accum = 0;
	char *dp = mStart + readIndex;
	assert(dp+length <= mEnd);
	for (unsigned i=0; i<length; i++) {
		accum = (accum<<1) | ((*dp++) & 0x01);
	}
	return accum;
}




uint64_t BitVector::peekFieldReversed(size_t readIndex, unsigned length) const
{
	uint64_t accum = 0;
	char *dp = mStart + readIndex + length - 1;
	assert(dp<mEnd);
	for (int i=(length-1); i>=0; i--) {
		accum = (accum<<1) | ((*dp--) & 0x01);
	}
	return accum;
}




uint64_t BitVector::readField(size_t& readIndex, unsigned length) const
{
	const uint64_t retVal = peekField(readIndex,length);
	readIndex += length;
	return retVal;
}


uint64_t BitVector::readFieldReversed(size_t& readIndex, unsigned length) const
{
	const uint64_t retVal = peekFieldReversed(readIndex,length);
	readIndex += length;
	return retVal;
}





void BitVector::fillField(size_t writeIndex, uint64_t value, unsigned length)
{
	char *dpBase = mStart + writeIndex;
	char *dp = dpBase + length - 1;
	assert(dp < mEnd);
	while (dp>=dpBase) {
		*dp-- = value & 0x01;
		value >>= 1;
	}
}


void BitVector::fillFieldReversed(size_t writeIndex, uint64_t value, unsigned length)
{
	char *dp = mStart + writeIndex;
	char *dpEnd = dp + length - 1;
	assert(dpEnd < mEnd);
	while (dp<=dpEnd) {
		*dp++ = value & 0x01;
		value >>= 1;
	}
}




void BitVector::writeField(size_t& writeIndex, uint64_t value, unsigned length)
{
	fillField(writeIndex,value,length);
	writeIndex += length;
}


void BitVector::writeFieldReversed(size_t& writeIndex, uint64_t value, unsigned length)
{
	fillFieldReversed(writeIndex,value,length);
	writeIndex += length;
}


void BitVector::invert()
{
	for (size_t i=0; i<size(); i++) {
		// (pat) 3-27-2012: This used ~ which left the data non-0 or 1.
		mStart[i] = !mStart[i];
	}
}

void BitVector::reverse()
{
	for (size_t i = 0; i < size()/2; i++) {
		char tmp = mStart[i];
		mStart[i] = mStart[size()-1-i];
		mStart[size()-1-i] = tmp;
	}
}		


void BitVector::reverse8()
{
	assert(size()>=8);

	char tmp0 = mStart[0];
	mStart[0] = mStart[7];
	mStart[7] = tmp0;

	char tmp1 = mStart[1];
	mStart[1] = mStart[6];
	mStart[6] = tmp1;

	char tmp2 = mStart[2];
	mStart[2] = mStart[5];
	mStart[5] = tmp2;

	char tmp3 = mStart[3];
	mStart[3] = mStart[4];
	mStart[4] = tmp3;
}



void BitVector::LSB8MSB()
{
	if (size()<8) return;
	size_t size8 = 8*(size()/8);
	size_t iTop = size8 - 8;
	for (size_t i=0; i<=iTop; i+=8) segment(i,8).reverse8();
}



uint64_t BitVector::syndrome(ParityGenerator64& gen) const
{
	gen.clear();
	const char *dp = mStart;
	while (dp<mEnd) gen.syndromeShift(*dp++);
	return gen.state();
}


uint64_t BitVector::parity(ParityGenerator64& gen) const
{
	gen.clear();
	const char *dp = mStart;
	while (dp<mEnd) gen.encoderShift(*dp++);
	return gen.state();
}


void BitVector::encode(const ViterbiR2O4& coder, BitVector& target)
{
	size_t sz = size();
	assert(sz*coder.iRate() == target.size());

	// Build a "history" array where each element contains the full history.
	uint32_t history[sz];
	uint32_t accum = 0;
	for (size_t i=0; i<sz; i++) {
		accum = (accum<<1) | bit(i);
		history[i] = accum;
	}

	// Look up histories in the pre-generated state table.
	char *op = target.begin();
	for (size_t i=0; i<sz; i++) {
		unsigned index = coder.cMask() & history[i];
		for (unsigned g=0; g<coder.iRate(); g++) {
			*op++ = coder.stateTable(g,index);
		}
	}
}


void BitVector::encode(const ViterbiR2O9& coder, BitVector& target)
{
	size_t sz = size();
	assert(sz*coder.iRate() == target.size());

	// Build a "history" array where each element contains the full history.
	uint64_t history[sz];
	uint64_t accum = 0;
	for (size_t i=0; i<sz; i++) {
		accum = (accum<<1) | bit(i);
		history[i] = accum;
	}

	// Look up histories in the pre-generated state table.
	char *op = target.begin();
	for (size_t i=0; i<sz; i++) {
		unsigned index = coder.cMask() & history[i];
		for (unsigned g=0; g<coder.iRate(); g++) {
			*op++ = coder.stateTable(g,index);
		}
	}
}




unsigned BitVector::sum() const
{
	unsigned sum = 0;
	for (size_t i=0; i<size(); i++) sum += mStart[i] & 0x01;
	return sum;
}




void BitVector::map(const unsigned *map, size_t mapSize, BitVector& dest) const
{
	for (unsigned i=0; i<mapSize; i++) {
		dest.mStart[i] = mStart[map[i]];
	}
}




void BitVector::unmap(const unsigned *map, size_t mapSize, BitVector& dest) const
{
	for (unsigned i=0; i<mapSize; i++) {
		dest.mStart[map[i]] = mStart[i];
	}
}





ostream& operator<<(ostream& os, const BitVector& hv)
{
	for (size_t i=0; i<hv.size(); i++) {
		if (hv.bit(i)) os << '1';
		else os << '0';
	}
	return os;
}




ViterbiR2O4::ViterbiR2O4()
{
	assert(mDeferral < 32);
	mCoeffs[0] = 0x019;
	mCoeffs[1] = 0x01b;
	computeStateTables(0);
	computeStateTables(1);
	computeGeneratorTable();
}




void ViterbiR2O4::initializeStates()
{
	for (unsigned i=0; i<mIStates; i++) clear(mSurvivors[i]);
	for (unsigned i=0; i<mNumCands; i++) clear(mCandidates[i]);
}



void ViterbiR2O4::computeStateTables(unsigned g)
{
	assert(g<mIRate);
	for (unsigned state=0; state<mIStates; state++) {
		// 0 input
		uint32_t inputVal = state<<1;
		mStateTable[g][inputVal] = applyPoly(inputVal, mCoeffs[g]);
		// 1 input
		inputVal |= 1;
		mStateTable[g][inputVal] = applyPoly(inputVal, mCoeffs[g]);
	}
}

void ViterbiR2O4::computeGeneratorTable()
{
	for (unsigned index=0; index<mIStates*2; index++) {
		mGeneratorTable[index] = (mStateTable[0][index]<<1) | mStateTable[1][index];
	}
}






void ViterbiR2O4::branchCandidates()
{
	// Branch to generate new input states.
	const vCand *sp = mSurvivors;
	for (unsigned i=0; i<mNumCands; i+=2) {
		// extend and suffix
		const uint32_t iState0 = (sp->iState) << 1;				// input state for 0
		const uint32_t iState1 = iState0 | 0x01;				// input state for 1
		const uint32_t oStateShifted = (sp->oState) << mIRate;	// shifted output
		const float cost = sp->cost;
		sp++;
		// 0 input extension
		mCandidates[i].cost = cost;
		mCandidates[i].oState = oStateShifted | mGeneratorTable[iState0 & mCMask];
		mCandidates[i].iState = iState0;
		// 1 input extension
		mCandidates[i+1].cost = cost;
		mCandidates[i+1].oState = oStateShifted | mGeneratorTable[iState1 & mCMask];
		mCandidates[i+1].iState = iState1;
	}
}


void ViterbiR2O4::getSoftCostMetrics(const uint32_t inSample, const float *matchCost, const float *mismatchCost)
{
	const float *cTab[2] = {matchCost,mismatchCost};
	for (unsigned i=0; i<mNumCands; i++) {
		vCand& thisCand = mCandidates[i];
		// We examine input bits 2 at a time for a rate 1/2 coder.
		const unsigned mismatched = inSample ^ (thisCand.oState);
		thisCand.cost += cTab[mismatched&0x01][1] + cTab[(mismatched>>1)&0x01][0];
	}
}


void ViterbiR2O4::pruneCandidates()
{
	const vCand* c1 = mCandidates;					// 0-prefix
	const vCand* c2 = mCandidates + mIStates;		// 1-prefix
	for (unsigned i=0; i<mIStates; i++) {
		if (c1[i].cost < c2[i].cost) mSurvivors[i] = c1[i];
		else mSurvivors[i] = c2[i];
	}
}


const ViterbiR2O4::vCand& ViterbiR2O4::minCost() const
{
	int minIndex = 0;
	float minCost = mSurvivors[0].cost;
	for (unsigned i=1; i<mIStates; i++) {
		const float thisCost = mSurvivors[i].cost;
		if (thisCost>=minCost) continue;
		minCost = thisCost;
		minIndex=i;
	}
	return mSurvivors[minIndex];
}


const ViterbiR2O4::vCand& ViterbiR2O4::step(uint32_t inSample, const float *probs, const float *iprobs)
{
	branchCandidates();
	getSoftCostMetrics(inSample,probs,iprobs);
	pruneCandidates();
	return minCost();
}






ViterbiR2O9::ViterbiR2O9(float wDeltaT)
{
	assert(mDeferral < 64);
	mCoeffs[0] = 0x11d; // the octal polynomials in 25.212 4.2.3.1 is backwards.
	mCoeffs[1] = 0x1af;
	computeStateTables(0);
	computeStateTables(1);
	computeGeneratorTable();

	mAllocPool=NULL;
	mSurvivors=NULL;
	mCandidates=NULL;

	mDeltaT = wDeltaT;
}




ViterbiR2O9::~ViterbiR2O9()
{
	while (mAllocPool) delete pop(mAllocPool);
	while (mCandidates) delete pop(mCandidates);
	while (mSurvivors) delete pop(mSurvivors);
}




ViterbiR2O9::vCand* ViterbiR2O9::pop(ViterbiR2O9::vCand*& list)
{
	vCand* ret = list;
	if (ret) list = ret->next;
	return ret;
}

void ViterbiR2O9::push(ViterbiR2O9::vCand* item, ViterbiR2O9::vCand*& list)
{
	item->next = list;
	list = item;
}


ViterbiR2O9::vCand* ViterbiR2O9::alloc()
{
	vCand* ret = pop(mAllocPool);
	if (!ret) ret = new vCand;
	return ret;
}

void ViterbiR2O9::release(ViterbiR2O9::vCand* v)
{
	push(v,mAllocPool);
}



void ViterbiR2O9::initializeStates()
{
	vCand *seed = alloc();
	clear(*seed);
	push(seed,mSurvivors);
	mPopulation=1;
}



void ViterbiR2O9::computeStateTables(unsigned g)
{
	assert(g<mIRate);
	for (unsigned state=0; state<mIStates; state++) {
		// 0 input
		uint64_t inputVal = state<<1;
		mStateTable[g][inputVal] = applyPoly(inputVal, mCoeffs[g]);
		// 1 input
		inputVal |= 1;
		mStateTable[g][inputVal] = applyPoly(inputVal, mCoeffs[g]);
	}
}

void ViterbiR2O9::computeGeneratorTable()
{
	for (unsigned index=0; index<mIStates*2; index++) {
		mGeneratorTable[index] = (mStateTable[0][index]<<1) | mStateTable[1][index];
	}
}






void ViterbiR2O9::branchCandidates()
{
	while (mSurvivors) {
		// extend and suffix
		vCand *sp = pop(mSurvivors);
		const uint64_t iState0 = (sp->iState) << 1;				// input state for 0
		const uint64_t iState1 = iState0 | 0x01;				// input state for 1
		const uint64_t oStateShifted = (sp->oState) << mIRate;	// shifted output
		const float cost = sp->cost;
		release(sp);
		// 0 input extension
		vCand *cp = alloc();
		cp->cost = cost;
		cp->oState = oStateShifted | mGeneratorTable[iState0 & mCMask];
		cp->iState = iState0;
		push(cp,mCandidates);
		// 1 input extension
		cp = alloc();
		cp->cost = cost;
		cp->oState = oStateShifted | mGeneratorTable[iState1 & mCMask];
		cp->iState = iState1;
		push(cp,mCandidates);
	}
}


void ViterbiR2O9::getSoftCostMetrics(const uint64_t inSample, const float *matchCost, const float *mismatchCost)
{
	const float *cTab[2] = {matchCost,mismatchCost};
	vCand *cp = mCandidates;
	while (cp) {
		// Estimate costs based on oState.
		const unsigned mismatched = inSample ^ (cp->oState);
		cp->cost += cTab[mismatched&0x01][1] + cTab[(mismatched>>1)&0x01][0];
		cp = cp->next;
	}
}


void ViterbiR2O9::pruneCandidates()
{
	for (unsigned i=0; i<mIStates; i++) mWinnersTable[i]=NULL;

	while (mCandidates) {
		// Compare candidates based on iState.
		vCand *cp = pop(mCandidates);
		unsigned suffix = cp->iState & mSMask;
		vCand *wt = mWinnersTable[suffix];
		if (!wt) {
			mWinnersTable[suffix] = cp;
			continue;
		}
		if (cp->cost >= wt->cost) {
			release(cp);
			continue;
		}
		release(wt);
		mWinnersTable[suffix]=cp;
	}

}


const ViterbiR2O9::vCand* ViterbiR2O9::minCost()
{
	// Find the minimum cost survivor.
	float cMin = 0;
	vCand* sMin = NULL;
	mPopulation=0;
	float cSum = 0.0;
	for (unsigned i=0; i<mIStates; i++) {
		vCand* s = mWinnersTable[i];
		if (!s) continue;
		const float c = s->cost;
		mPopulation++;
		cSum += c;
		if (!sMin) {
			sMin=s;
			cMin=c;
			continue;
		}
		if (c<cMin) {
			sMin=s;
			cMin=c;
		}
	}

	// Set the threshold.
	float T = cMin + mDeltaT;

	// Did the distribution got truely flat?
	// If so, we might as well toss it.
#if 0
	float cAvg = cSum/mPopulation;
	if (cAvg==cMin) {
		for (unsigned i=0; i<mIStates; i++) {
			vCand* s = mWinnersTable[i];
			if (!s) continue;
			if (s==sMin) continue;
			release(s);
		}
		push(sMin,mSurvivors);
		return sMin;
	}
#endif

	// Apply the T-algorithm.
	for (unsigned i=0; i<mIStates; i++) {
		vCand* s = mWinnersTable[i];
		if (!s) continue;
		if (s->cost < T) push(s,mSurvivors);
		else release(s);
	}

	// cout << "min=" << cMin << " num=" << mPopulation << " avg=" << cAvg << "\n"; //HACK
	return sMin;
}


const ViterbiR2O9::vCand* ViterbiR2O9::step(uint64_t inSample, const float *probs, const float *iprobs)
{
	branchCandidates();
	getSoftCostMetrics(inSample,probs,iprobs);
	pruneCandidates();
	const vCand* min = minCost();
	return min;
}




uint64_t Parity::syndrome(const BitVector& receivedCodeword)
{
	return receivedCodeword.syndrome(*this);
}


void Parity::writeParityWord(const BitVector& data, BitVector& parityTarget, bool invert)
{
	uint64_t pWord = data.parity(*this);
	if (invert) pWord = ~pWord; 
	parityTarget.fillField(0,pWord,size());
}









SoftVector::SoftVector(const BitVector& source)
{
	resize(source.size());
	for (size_t i=0; i<size(); i++) {
		if (source.bit(i)) mStart[i]=1.0F;
		else mStart[i]=0.0F;
	}
}



SoftVector::SoftVector(const char *valString)
{
	resize(strlen(valString));
	for (size_t i=0; i<size(); i++) {
		if (valString[i]=='0') mStart[i]=0.0F;
		else if (valString[i]=='1') mStart[i]=1.0F;
		else  mStart[i]=0.5F;
	}
}


// Leave out until someone needs it.
//void SoftVector::sliced(BitVector &result) const
//{
//	size_t sz = size();
//	assert(result.size() >= sz);
//	char *rp = result.begin();
//	for (size_t i=0; i<sz; i++) {
//		*rp++ = !!(mStart[i]>0.5F);
//	}
//}

BitVector SoftVector::sliced() const
{
	// TODO: Base this on sliced(BitVector&)
	size_t sz = size();
	BitVector newSig(sz);
	for (size_t i=0; i<sz; i++) {
		if (mStart[i]>0.5F) newSig[i]=1;
		else newSig[i] = 0;
	}
	return newSig;
}



void SoftVector::decode(ViterbiR2O4 &decoder, BitVector& target) const
{
	const size_t sz = size();
	const unsigned deferral = decoder.deferral();
	const size_t ctsz = sz + deferral*decoder.iRate();
	assert(sz <= decoder.iRate()*target.size());

	// Build a "history" array where each element contains the full history.
	uint32_t history[ctsz];
	{
		BitVector bits = sliced();
		uint32_t accum = 0;
		for (size_t i=0; i<sz; i++) {
			accum = (accum<<1) | bits.bit(i);
			history[i] = accum;
		}
		// Repeat last bit at the end.
		for (size_t i=sz; i<ctsz; i++) {
			accum = (accum<<1) | (accum & 0x01);
			history[i] = accum;
		}
	}

	// Precompute metric tables.
	float matchCostTable[ctsz];
	float mismatchCostTable[ctsz];
	{
		const float *dp = mStart;
		for (size_t i=0; i<sz; i++) {
			// pVal is the probability that a bit is correct.
			// ipVal is the probability that a bit is incorrect.
			float pVal = dp[i];
			if (pVal>0.5F) pVal = 1.0F-pVal;
			float ipVal = 1.0F-pVal;
			// This is a cheap approximation to an ideal cost function.
			if (pVal<0.01F) pVal = 0.01;
			if (ipVal<0.01F) ipVal = 0.01;
			matchCostTable[i] = 0.25F/ipVal;
			mismatchCostTable[i] = 0.25F/pVal;
		}
	
		// pad end of table with unknowns
		for (size_t i=sz; i<ctsz; i++) {
			matchCostTable[i] = 0.5F;
			mismatchCostTable[i] = 0.5F;
		}
	}

	{
		decoder.initializeStates();
		// Each sample of history[] carries its history.
		// So we only have to process every iRate-th sample.
		const unsigned step = decoder.iRate();
		// input pointer
		const uint32_t *ip = history + step - 1;
		// output pointers
		char *op = target.begin();
		const char *const opt = target.end();
		// table pointers
		const float* match = matchCostTable;
		const float* mismatch = mismatchCostTable;
		size_t oCount = 0;
		while (op<opt) {
			// Viterbi algorithm
			assert(match-matchCostTable<(int)(sizeof(matchCostTable)/sizeof(matchCostTable[0])-1));
			assert(mismatch-mismatchCostTable<(int)(sizeof(mismatchCostTable)/sizeof(mismatchCostTable[0])-1));
			const ViterbiR2O4::vCand &minCost = decoder.step(*ip, match, mismatch);
			ip += step;
			match += step;
			mismatch += step;
			// output
			if (oCount>=deferral) *op++ = (minCost.iState >> deferral)&0x01;
			oCount++;
		}
	}
}



void SoftVector::decode(ViterbiR2O9 &decoder, BitVector& target) const
{
	const size_t sz = size();
	const unsigned deferral = decoder.deferral();
	const size_t ctsz = sz + deferral*decoder.iRate();
	assert(sz <= decoder.iRate()*target.size());

	// Build a "history" array where each element contains the full history.
	uint32_t history[ctsz];
	{
		BitVector bits = sliced();
		uint32_t accum = 0;
		for (size_t i=0; i<sz; i++) {
			accum = (accum<<1) | bits.bit(i);
			history[i] = accum;
		}
		// Repeat last bit at the end.
		for (size_t i=sz; i<ctsz; i++) {
			accum = (accum<<1) | (accum & 0x01);
			history[i] = accum;
		}
	}

	// Precompute metric tables.
	float matchCostTable[ctsz];
	float mismatchCostTable[ctsz];
	{
		const float *dp = mStart;
		for (size_t i=0; i<sz; i++) {
			// pVal is the probability that a bit is correct.
			// ipVal is the probability that a bit is incorrect.
			float pVal = dp[i];
			if (pVal>0.5F) pVal = 1.0F-pVal;
			float ipVal = 1.0F-pVal;
			// This is a cheap approximation to an ideal cost function.
			if (pVal<0.01F) pVal = 0.01;
			if (ipVal<0.01F) ipVal = 0.01;
			matchCostTable[i] = 0.25F/ipVal;
			mismatchCostTable[i] = 0.25F/pVal;
		}
	
		// pad end of table with unknowns
		for (size_t i=sz; i<ctsz; i++) {
			matchCostTable[i] = 0.5F;
			mismatchCostTable[i] = 0.5F;
		}
	}

	{
		decoder.initializeStates();
		// Each sample of history[] carries its history.
		// So we only have to process every iRate-th sample.
		const unsigned step = decoder.iRate();
		// input pointer
		const uint32_t *ip = history + step - 1;
		// output pointers
		char *op = target.begin();
		const char *const opt = target.end();
		// table pointers
		const float* match = matchCostTable;
		const float* mismatch = mismatchCostTable;
		size_t oCount = 0;
		while (op<opt) {
			// Viterbi algorithm
			assert(match-matchCostTable<(int)(sizeof(matchCostTable)/sizeof(matchCostTable[0])-1));
			assert(mismatch-mismatchCostTable<(int)(sizeof(mismatchCostTable)/sizeof(mismatchCostTable[0])-1));
			const ViterbiR2O9::vCand *minCost = decoder.step(*ip, match, mismatch);
			ip += step;
			match += step;
			mismatch += step;
			// output
			if (oCount>=deferral) *op++ = (minCost->iState >> deferral)&0x01;
			oCount++;
			// cout << oCount << " " << std::hex << minCost->iState << "\n" << std::dec; //HACK
		}
	}
}



// (pat) Added 6-22-2012
float SoftVector::getEnergy(float *plow) const
{
	const SoftVector &vec = *this;
	int len = vec.size();
	float avg = 0; float low = 1;
	for (int i = 0; i < len; i++) {
		float bit = vec[i];
		float energy = 2*((bit < 0.5) ? (0.5-bit) : (bit-0.5));
		if (energy < low) low = energy;
		avg += energy/len;
	}
	if (plow) { *plow = low; }
	return avg;
}

ostream& operator<<(ostream& os, const SoftVector& sv)
{
	for (size_t i=0; i<sv.size(); i++) {
		if (sv[i]<0.25) os << "0";
		else if (sv[i]>0.75) os << "1";
		else os << "-";
	}
	return os;
}

std::string SoftVector::str() const
{
	std::ostringstream ss;	// This is a dopey way to do this when we know the expected size, but we are using C++ so oh well.
	ss <<"SoftVector(size=" <<size() <<" data=";
	int accum = 0;
	bool valid = true;
	unsigned i = 0;
	float energy = 0.0;		// energy in data as a fraction from 0 to 1.0.
	bool outofbounds = false;
	while (i<size()) {
		float val = (*this)[i];	// gotta love this syntax.
		if (val < -0.000001 || val > 1.00001) { outofbounds = true; break; }
		if (val < 0.5) {
			energy += 2*(0.5-val); 
			accum = (accum<<1);
			if (val > 0.25) { valid = false; }
		} else {
			energy += 2*(val-0.5); 
			accum = (accum<<1) + 1; 
			if (val < 0.75) { valid = false; }
		}
		i++;
		if (i % 4 == 0 || i == size()) {	// The i == size() test catches the final non-full nibble, if any.
			if (valid) { ss << std::hex << accum << std::dec; } else { ss << "-"; }
			valid = true;
			accum = 0;
		}
	}

	if (outofbounds) {
		// This SoftVector is invalid. Switch to alternate format to print its full contents:
		ss.seekp(0);
		ss <<"SoftVector(size=" <<size() <<" data=";
		for (i = 0; i < size(); i++) {
			ss << " " << (*this)[i];
		}
		ss << ")";
		return ss.str();
	} else {
		ss << format(" %.1f%%)",(100.0 * energy / size()));
	}
	return ss.str();
}



void BitVector::pack(unsigned char* targ) const
{
	// Assumes MSB-first packing.
	unsigned bytes = size()/8;
	for (unsigned i=0; i<bytes; i++) {
		targ[i] = peekField(i*8,8);
	}
	unsigned whole = bytes*8;
	unsigned rem = size() - whole;
	if (rem==0) return;
	targ[bytes] = peekField(whole,rem) << (8-rem);
}


void BitVector::unpack(const unsigned char* src)
{
	// Assumes MSB-first packing.
	unsigned bytes = size()/8;
	for (unsigned i=0; i<bytes; i++) {
		fillField(i*8,src[i],8);
	}
	unsigned whole = bytes*8;
	unsigned rem = size() - whole;
	if (rem==0) return;
	fillField(whole,src[bytes] >> (8-rem),rem);
}

void BitVector::hex(ostream& os) const
{
	os << std::hex;
	unsigned digits = size()/4;
	size_t wp=0;
	for (unsigned i=0; i<digits; i++) {
		os << readField(wp,4);
	}
	// (pat 9-8-2012) Previously this did not print any remaining bits in the final nibble.
	unsigned rem = size() - 4*digits;
	if (rem) { os << readField(wp,rem); }
	os << std::dec;		// C++ I/O is so foobar.  It may not have been in dec mode when we started.
}

std::ostream& BitVector::textBitVector(ostream&os) const
{
	os <<"BitVector(size=" <<size() <<" data=";
	hex(os);
	os <<")";
	return os;
}

std::string BitVector::str() const
{
	std::ostringstream ss;	// This is a dopey way to do this when we know the expected size, but we are using C++ so oh well.
	textBitVector(ss);
	return ss.str();
}

std::string BitVector::hexstr() const
{
	std::ostringstream ss;
	hex(ss);
	return ss.str();
}


bool BitVector::unhex(const char* src)
{
	// Assumes MSB-first packing.
	unsigned int val;
	unsigned digits = size()/4;
	for (unsigned i=0; i<digits; i++) {
		if (sscanf(src+i, "%1x", &val) < 1) {
			return false;
		}
		fillField(i*4,val,4);
	}
	unsigned whole = digits*4;
	unsigned rem = size() - whole;
	if (rem>0) {
		if (sscanf(src+digits, "%1x", &val) < 1) {
			return false;
		}
		fillField(whole,val,rem);
	}
	return true;
}

bool BitVector::operator==(const BitVector &other) const
{
	unsigned l = size();
	return l == other.size() && 0==memcmp(begin(),other.begin(),l);
}

// vim: ts=4 sw=4
