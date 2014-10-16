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

#ifndef TURBOCODER_H
#define TURBOCODER_H


/**
	Class to represent one pass of the UMTS turbo decoder.
	One pass is rate 1/2, memory length 4.
*/
class ViterbiTurbo
{

	private:
		/**name Lots of precomputed elements so the compiler can optimize like hell. */
		//@{
		/**@name Core values. */
		//@{
		static const unsigned mIRate = 2;	///< reciprocal of rate
		static const unsigned mOrder = 3;	///< memory length of generators
		//@}
		/**@name Derived values. */
		//@{
		static const unsigned mIStates = 0x01 << mOrder;	///< number of states, number of survivors
		static const uint32_t mSMask = mIStates-1;			///< survivor mask
		static const uint32_t mCMask = (mSMask<<1) | 0x01;	///< candidate mask
		static const uint32_t mOMask = (0x01<<mIRate)-1;	///< ouput mask, all iRate low bits set
		static const unsigned mNumCands = mIStates*2;		///< number of candidates to generate during branching
		static const unsigned mDeferral = 6*mOrder;			///< deferral to be used
		//@}
		//@}

		/** Precomputed tables. */
		//@{
		uint32_t mCoeffs[mIRate];					///< polynomial for each generator
		uint32_t mStateTable[mIRate][2*mIStates];	///< precomputed generator output tables
		uint32_t mGeneratorTable[2*mIStates];		///< precomputed coder output table
		//@}
	
	public:

		/**
		  A candidate sequence in a Viterbi decoder.
		  The 32-bit state register can support a deferral of 6 with a 4th-order coder.
		 */
		typedef struct candStruct {
			uint32_t iState;	///< encoder input associated with this candidate
			uint32_t oState;	///< encoder output associated with this candidate
			char rState;		///< real state of encoder associated with this candidate
			float cost;			///< cost (metric value), float to support soft inputs
		} vCand;

		/** Clear a structure. */
		void clear(vCand& v)
		{
			v.iState=0;
			v.rState=0;
			v.oState=0;
			v.cost=0;
		}
		

	private:

		/**@name Survivors and candidates. */
		//@{
		vCand mSurvivors[mIStates];			///< current survivor pool
		vCand mCandidates[2*mIStates];		///< current candidate pool
		//@}

	public:

		unsigned iRate() const { return mIRate; }
		uint32_t cMask() const { return mCMask; }
		uint32_t stateTable(unsigned g, unsigned i) const { return mStateTable[g][i]; }
		unsigned deferral() const { return mDeferral; }
		

		ViterbiTurbo();

		/** Set all cost metrics to zero. */
		void initializeStates();

		/**
			Full cycle of the Viterbi algorithm: branch, metrics, prune, select.
			@return reference to minimum-cost candidate.
		*/
		const vCand* step(const float *inSample);

	private:

		/** Branch survivors into new candidates. */
		void branchCandidates();

		/** Compute cost metrics for soft-inputs. */
		void getSoftCostMetrics(const float *inSample);

		/** Select survivors from the candidate set. */
		void pruneCandidates();

		/** Find the minimum cost survivor. */
		const vCand* minCost() const;

		/**
			Precompute the state tables.
		*/
		void computeStateTables(unsigned g);

		/**
			Precompute the generator outputs.
			mCoeffs must be defined first.
		*/
		void computeGeneratorTable();

};


class TurboInterleaver {

	private:

	std::vector<int> mPermutation;

	public:

	// UMTS FEC turbo coder internal interleaver
	TurboInterleaver(int K);	// (pat) K is number of input bits.

	void interleave(SoftVector &in, SoftVector &out);

	void unInterleave(SoftVector &in, BitVector &out);

	void unInterleave(SoftVector &in, SoftVector &out);

	int gcd(int x, int y);

	std::vector<int> &permutation() {
		return mPermutation;
	}

};
#endif
// vim: ts=4 sw=4
