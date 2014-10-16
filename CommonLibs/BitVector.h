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


#ifndef BITVECTOR_H
#define BITVECTOR_H

#include "MemoryLeak.h"
#include "Vector.h"
#include <stdint.h>
#include <vector>


class BitVector;
class SoftVector;
class ViterbiTurbo;
class TurboInterleaver;



/** Shift-register (LFSR) generator, 64 bits, for parity checking in GSM/GPRS/UMTS. */
class ParityGenerator64 {

	private:

	uint64_t mCoeff;	///< polynomial coefficients. LSB is zero exponent.
	uint64_t mState;	///< shift register state. LSB is most recent.
	uint64_t mMask;		///< mask for reading state
	unsigned mLen;		///< number of bits used in shift register
	unsigned mLen_1;	///< mLen - 1

	public:

	ParityGenerator64(uint64_t wCoeff, unsigned wLen)
		:mCoeff(wCoeff),mState(0),
		mMask((1ULL<<wLen)-1),
		mLen(wLen),mLen_1(wLen-1)
	{ assert(wLen<64); }

	void clear() { mState=0; }

	/**@name Accessors */
	//@{
	uint64_t state() const { return mState & mMask; }
	unsigned size() const { return mLen; }
	//@}

	/**
		Calculate one bit of a syndrome.
		This is in the .h for inlining.
	*/
	void syndromeShift(unsigned inBit)
	{
		const unsigned fb = (mState>>(mLen_1)) & 0x01;
		mState = (mState<<1) ^ (inBit & 0x01);
		if (fb) mState ^= mCoeff;
	}

	/**
		Update the generator state by one cycle.
		This is in the .h for inlining.
	*/
	void encoderShift(unsigned inBit)
	{
		const unsigned fb = ((mState>>(mLen_1)) ^ inBit) & 0x01;
		mState <<= 1;
		if (fb) mState ^= mCoeff;
	}


};



/** Shift-register (LFSR) generator, 32 bits for seqeunce generation for UMTS. */
class SequenceGenerator32 {

	private:

	uint32_t mCoeff;	///< polynomial coefficients. LSB is zero exponent.
	uint32_t mState;	///< shift register state. LSB is oldest.
	uint32_t mMask;		///< state mask
	unsigned mLen;		///< number of bits used in shift register
	unsigned mLen_1;	///< mLen - 1

	/** Generate even/odd parity for a 32-bit word. */
	uint32_t parity(uint32_t v) const
	{
		v = (v ^ (v>>16));
		v = (v ^ (v>>8));
		v = (v ^ (v>>4));
		v = (v ^ (v>>2));
		v = (v ^ (v>>1));
		return v & 0x01;
	}

	public:

	SequenceGenerator32(uint32_t wCoeff, unsigned wLen)
		:mCoeff(wCoeff),mState(0),
		mMask((1ULL<<wLen)-1),
		mLen(wLen),mLen_1(wLen-1)
	{ assert(wLen<32); }

	void clear() { mState=0; }

	/**@name Accessors */
	//@{
	uint32_t state() const { return mState & mMask; }
	unsigned size() const { return mLen; }
	void state(uint32_t wState) { mState = wState & mMask; }
	//@}

	/** Step the generator forward one cycle. */
	void step()
	{
		uint32_t fb = parity(mState & mCoeff);
		mState = (mState >> 1) | (fb << mLen_1);
	}

	public:

	uint32_t LSB() const { return mState & 0x01; }

	uint32_t read(uint32_t mask) { return parity(mState & mask); }

};




/** Parity (CRC-type) generator and checker based on a Generator. */
class Parity : public ParityGenerator64 {

	protected:

	unsigned mCodewordSize;

	public:

	Parity(uint64_t wCoefficients, unsigned wParitySize, unsigned wCodewordSize)
		:ParityGenerator64(wCoefficients, wParitySize),
		mCodewordSize(wCodewordSize)
	{ }

	/** Compute the parity word and write it into the target segment.  */
	void writeParityWord(const BitVector& data, BitVector& parityWordTarget, bool invert=true);

	/** Compute the syndrome of a received sequence. */
	uint64_t syndrome(const BitVector& receivedCodeword);
};



/*
	A comment on deferal delays for the Viterbi algorithm:

	To determine how long the storage time d should be, we note that the probability
	that an error at time k will not have been detected before time k+d is [1-(1/m)]`^d,
	The probability that z consecutive input symbols are not equal to m - 1 or 0,
	as the case may be. For m=4 and d=20, [1-(1/m)]^d = 0.003;

	(From Maximum-Likelihood Seauence Estimation of Digital Sequences in the Presence of
	Intersymbol Interference, G. DAVID FORNEY, JR., IEEE TRANSACTIONS ON INFORMATION THEORY,
	VOL. IT-18, NO. 3, MAY 1972)

	So...

	If the target FER is e:

	d log(1-(1/m)) = log(e)
	d = log(e) / log(1-(1/m))

	If e=0.01, log(e) = -2.

	For m=4, 1-(1/m)=0.75 and log(0.75)=-0.125.
	So 2/0.125 = 16.

	For m=9, 1-(1/m)=0.8889 and log(0.8889)=-0.0512.
	So 2/0.0512 = 39.

*/



/**
	Class to represent convolutional coders/decoders of rate 1/2, memory length 4.
	This is the "workhorse" coder for most GSM channels.
*/
class ViterbiR2O4 {

	private:
		/**name Lots of precomputed elements so the compiler can optimize like hell. */
		//@{
		/**@name Core values. */
		//@{
		static const unsigned mIRate = 2;	///< reciprocal of rate
		static const unsigned mOrder = 4;	///< memory length of generators
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
			float cost;			///< cost (metric value), float to support soft inputs
		} vCand;

		/** Clear a structure. */
		void clear(vCand& v)
		{
			v.iState=0;
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
		

		ViterbiR2O4();

		/** Set all cost metrics to zero. */
		void initializeStates();

		/**
			Full cycle of the Viterbi algorithm: branch, metrics, prune, select.
			@return reference to minimum-cost candidate.
		*/
		const vCand& step(uint32_t inSample, const float *probs, const float *iprobs);

	private:

		/** Branch survivors into new candidates. */
		void branchCandidates();

		/** Compute cost metrics for soft-inputs. */
		void getSoftCostMetrics(uint32_t inSample, const float *probs, const float *iprobs);

		/** Select survivors from the candidate set. */
		void pruneCandidates();

		/** Find the minimum cost survivor. */
		const vCand& minCost() const;

		/**
			Precompute the state tables.
			@param g Generator index 0..((1/rate)-1)
		*/
		void computeStateTables(unsigned g);

		/**
			Precompute the generator outputs.
			mCoeffs must be defined first.
		*/
		void computeGeneratorTable();

};

/**
	Class to represent convolutional coders/decoders of rate 1/2, memory length 9.
	This is for UMTS.
*/
class ViterbiR2O9 {

	private:
		/**name Lots of precomputed elements so the compiler can optimize like hell. */
		// (pat) Wouldn't optimization come from heaven?
		//@{
		/**@name Core values. */
		//@{
		static const unsigned mIRate = 2;	///< reciprocal of rate
		static const unsigned mOrder = 9;	///< memory length of generators
		// hack
		//@}
		/**@name Derived values. */
		//@{
		static const unsigned mIStates = 0x01 << mOrder;	///< number of states, number of survivors
		static const uint64_t mSMask = mIStates-1;			///< survivor mask
		static const uint64_t mCMask = (mSMask<<1) | 0x01;	///< candidate mask
		static const uint64_t mOMask = (0x01<<mIRate)-1;	///< ouput mask, all iRate low bits set
		static const unsigned mNumCands = mIStates*2;		///< number of candidates to generate during branching
		static const unsigned mDeferral = 39;			///< deferral to be used
		// hack
		//@}
		//@}

		/**@name Precomputed tables. */
		//@{
		uint64_t mCoeffs[mIRate];					///< polynomial for each generator
		uint64_t mStateTable[mIRate][2*mIStates];	///< precomputed generator output tables
		uint64_t mGeneratorTable[2*mIStates];		///< precomputed coder output table
		//@}

		/**@name T-algorithm */
		//@{
		unsigned mPopulation;
		float mDeltaT;
		//@}
	
	public:

		/**
		  A candidate sequence in a Viterbi decoder.
		  The 64-bit state register can support a deferral of 6 with a 9th-order coder.
		 */
		typedef struct candStruct {
			uint64_t iState;	///< encoder input associated with this candidate
			uint64_t oState;	///< encoder output associated with this candidate
			float cost;			///< cost (metric value), float to support soft inputs
			struct candStruct* next;
		} vCand;

		/** Clear a structure. */
		void clear(vCand& v)
		{
			v.iState=0;
			v.oState=0;
			v.cost=0;
			v.next=NULL;
		}
		

	private:

		/**@name Survivors and candidates. */
		//@{
		vCand* mSurvivors;
		vCand* mCandidates;
		vCand* mWinnersTable[mIStates];
		//@}



		/**@name vCand memory allocation */
		//@{

		vCand* mAllocPool;

		/** Pop a vCand from a list. */
		vCand* pop(vCand*& list);

		/** Push a vCand into a list. */
		void push(vCand* item, vCand*& list);

		/** Allocate a new vCand structure from heap or mAllocPool. */
		vCand* alloc();

		/** Release a vCand structure back to mAllocPool. */
		void release(vCand*);

		//@}

	public:

		unsigned iRate() const { return mIRate; }
		uint64_t cMask() const { return mCMask; }
		uint64_t stateTable(unsigned g, unsigned i) const { return mStateTable[g][i]; }
		unsigned deferral() const { return mDeferral; }
		

		ViterbiR2O9(float wDeltaT = 9.0);

		~ViterbiR2O9();

		/** Set the delta-T parameter. */
		void deltaT(float wDeltaT) { mDeltaT = wDeltaT; }

		/** Set all cost metrics to zero. */
		void initializeStates();

		/**
			Full cycle of the Viterbi algorithm: branch, metrics, prune, select.
			@return reference to minimum-cost candidate.
		*/
		const vCand* step(uint64_t inSample, const float *probs, const float *iprobs);

	private:


		/** Branch survivors into new candidates. */
		void branchCandidates();

		/** Compute cost metrics for soft-inputs. */
		void getSoftCostMetrics(uint64_t inSample, const float *probs, const float *iprobs);

		/** Select survivors from the candidate set. */
		void pruneCandidates();

		/** Find the minimum cost survivor. */
		const vCand* minCost();

		/**
			Precompute the state tables.
			@param g Generator index 0..((1/rate)-1)
		*/
		void computeStateTables(unsigned g);

		/**
			Precompute the generator outputs.
			mCoeffs must be defined first.
		*/
		void computeGeneratorTable();

};

DEFINE_MEMORY_LEAK_DETECTOR_CLASS(BitVector,MemCheckBitVector)


class BitVector : public Vector<char>, MemCheckBitVector {


	public:

	/**@name Constructors. */
	//@{

	/**@name Casts of Vector constructors. */
	//@{
	BitVector(char* wData, char* wStart, char* wEnd)
		:Vector<char>(wData,wStart,wEnd)
	{ BVDEBUG("bvc1\n"); }
	BitVector(size_t len=0):Vector<char>(len) { BVDEBUG("bfc2\n"); }
	BitVector(const Vector<char>& source):Vector<char>(source) { BVDEBUG("bvc3\n"); }
	BitVector(Vector<char>& source):Vector<char>(source) { BVDEBUG("bvc4\n"); }
	BitVector(BitVector&src): Vector<char>(src) { BVDEBUG("bvc5\n"); }
	// (pat) There MUST be non-inherited copy constructors in every non-trivial class
	// or you get improperly constructed objects.
	BitVector(const BitVector&src):Vector<char>(src) { BVDEBUG("bvc6\n"); }
	BitVector(const Vector<char>& source1, const Vector<char> source2):Vector<char>(source1,source2) { BVDEBUG("bvc7\n"); }
	//@}

	/** Construct from a string of "0" and "1". */
	BitVector(const char* valString);
	//@}

	/** Index a single bit. */
	bool bit(size_t index) const
	{
		// We put this code in .h for fast inlining.
		const char *dp = mStart+index;
		assert(dp<mEnd);
		return (*dp) & 0x01;
	}

	/**@name Casts and overrides of Vector operators. */
	//@{
	BitVector segment(size_t start, size_t span)
	{
		char* wStart = mStart + start;
		char* wEnd = wStart + span;
		assert(wEnd<=mEnd);
		return BitVector(NULL,wStart,wEnd);
	}

	BitVector alias()
		{ return segment(0,size()); }

	const BitVector alias() const
		{ return segment(0,size()); }

	const BitVector segment(size_t start, size_t span) const
		{ return (BitVector)(Vector<char>::segment(start,span)); }

	BitVector head(size_t span) { return segment(0,span); }
	const BitVector head(size_t span) const { return segment(0,span); }
	BitVector tail(size_t start) { return segment(start,size()-start); }
	const BitVector tail(size_t start) const { return segment(start,size()-start); }
	//@}


	void zero() { fill(0); }

	/**@name FEC operations. */
	//@{
	/** Calculate the syndrome of the vector with the given Generator. */
	uint64_t syndrome(ParityGenerator64& gen) const;
	/** Calculate the parity word for the vector with the given Generator. */
	uint64_t parity(ParityGenerator64& gen) const;
	/** Encode the signal with the GSM rate 1/2 convolutional encoder. */
	void encode(const ViterbiR2O4& encoder, BitVector& target);
//#if RN_UMTS
	void encode(const ViterbiR2O9& encoder, BitVector& target);
	void encode(const ViterbiTurbo& encoder, BitVector& target, TurboInterleaver& wInterleaver);
//#endif
	//@}


	/** Invert 0<->1. */
	void invert();

	/**@name Byte-wise operations. */
	//@{
	/** Reverse an 8-bit vector. */
	void reverse8();
	/** Reverse entire vector. */
	void reverse();
	/** Reverse groups of 8 within the vector (byte reversal). */
	void LSB8MSB();
	//@}

	/**@name Serialization and deserialization. */
	//@{
	uint64_t peekField(size_t readIndex, unsigned length) const;
	uint64_t peekFieldReversed(size_t readIndex, unsigned length) const;
	uint64_t readField(size_t& readIndex, unsigned length) const;
	uint64_t readFieldReversed(size_t& readIndex, unsigned length) const;
	void fillField(size_t writeIndex, uint64_t value, unsigned length);
	void fillFieldReversed(size_t writeIndex, uint64_t value, unsigned length);
	void writeField(size_t& writeIndex, uint64_t value, unsigned length);
	void writeFieldReversed(size_t& writeIndex, uint64_t value, unsigned length);
	void write0(size_t& writeIndex) { writeField(writeIndex,0,1); }
	void write1(size_t& writeIndex) { writeField(writeIndex,1,1); }
	//@}

	/** Sum of bits. */
	unsigned sum() const;

	/** Reorder bits, dest[i] = this[map[i]]. */
	void map(const unsigned *map, size_t mapSize, BitVector& dest) const;

	/** Reorder bits, dest[map[i]] = this[i]. */
	void unmap(const unsigned *map, size_t mapSize, BitVector& dest) const;

	/** Pack into a char array. */
	void pack(unsigned char*) const;

	/** Unpack from a char array. */
	void unpack(const unsigned char*);

	/** Make a hexdump string. */
	void hex(std::ostream&) const;
	std::string hexstr() const;

	/** Unpack from a hexdump string.
	*  @returns true on success, false on error. */
	bool unhex(const char*);
	std::ostream& textBitVector(std::ostream&os) const;
	std::string str() const;

	// (pat) set() is required when the other is not a variable, for example, a function return.
	// In C++, if you pass a function return as a reference then C++ silently creates a const temp variable,
	// which in this class causes a constructor in the Vector class to allocate a new copy of the other
	// as a possibly surprising side effect.
	// If you use set() instead, you get a reference to other, instead of an allocated copy of other.
	void set(BitVector other)	// That's right.  No ampersand.
	{
		clear();
		mData=other.mData;
		mStart=other.mStart;
		mEnd=other.mEnd;
		other.mData=NULL;
	}

	bool operator==(const BitVector &other) const;
}; // class BitVector



std::ostream& operator<<(std::ostream&, const BitVector&);


DEFINE_MEMORY_LEAK_DETECTOR_CLASS(SoftVector,MemCheckSoftVector)


/**
  The SoftVector class is used to represent a soft-decision signal.
  Values 0..1 represent probabilities that a bit is "true".
 */
class SoftVector: public Vector<float>, public MemCheckSoftVector {

	public:

	/** Build a SoftVector of a given length. */
	SoftVector(size_t wSize=0):Vector<float>(wSize) {}

	/** Construct a SoftVector from a C string of "0", "1", and "X". */
	SoftVector(const char* valString);

	/** Construct a SoftVector from a BitVector. */
	SoftVector(const BitVector& source);

	/** Construct a SoftVector from two SoftVectors. */
	SoftVector(SoftVector &a, SoftVector &b) : Vector<float>(a, b) {};

	/**
		Wrap a SoftVector around a block of floats.
		The block will be delete[]ed upon desctuction.
	*/
	SoftVector(float *wData, unsigned length)
		:Vector<float>(wData,length)
	{}

	SoftVector(float* wData, float* wStart, float* wEnd)
		:Vector<float>(wData,wStart,wEnd)
	{ }

	/**
		Casting from a Vector<float>.
		Note that this is NOT pass-by-reference.
	*/
	SoftVector(Vector<float> source)
		:Vector<float>(source)
	{}

	// (pat) There MUST be non-inherited copy constructors in every non-trivial class
	// or you get improperly constructed objects.
	SoftVector(SoftVector &src) : Vector<float>(src) {}
	SoftVector(const SoftVector &src) : Vector<float>(src) {}


	/**@name Casts and overrides of Vector operators. */
	//@{
	SoftVector segment(size_t start, size_t span)
	{
		float* wStart = mStart + start;
		float* wEnd = wStart + span;
		assert(wEnd<=mEnd);
		return SoftVector(NULL,wStart,wEnd);
	}

	SoftVector alias()
		{ return segment(0,size()); }

	const SoftVector alias() const
		{ return segment(0,size()); }

	const SoftVector segment(size_t start, size_t span) const
		{ return (SoftVector)(Vector<float>::segment(start,span)); }

	SoftVector head(size_t span) { return segment(0,span); }
	const SoftVector head(size_t span) const { return segment(0,span); }
	SoftVector tail(size_t start) { return segment(start,size()-start); }
	const SoftVector tail(size_t start) const { return segment(start,size()-start); }
	//@}

	/** Decode soft symbols with the GSM rate-1/2 Viterbi decoder. */
	void decode(ViterbiR2O4 &decoder, BitVector& target) const;
//#if RN_UMTS
	void decode(ViterbiR2O9 &decoder, BitVector& target) const;
	void decode(ViterbiTurbo &decoder, SoftVector& target) const;
	void decode(ViterbiTurbo &decoder, BitVector& target, TurboInterleaver& wInterleaver) const;
	void decode(ViterbiTurbo &decoder, SoftVector& target, TurboInterleaver& wInterleaver) const;
//#endif

	// (pat) How good is the SoftVector in the sense of the bits being solid?
	// Result of 1 is perfect and 0 means all the bits were 0.5
	// If plow is non-NULL, also return the lowest energy bit.
	float getEnergy(float *low=0) const;

	/** Fill with "unknown" values. */
	void unknown() { fill(0.5F); }

	/** Return a hard bit value from a given index by slicing. */
	bool bit(size_t index) const
	{
		const float *dp = mStart+index;
		assert(dp<mEnd);
		return (*dp)>0.5F;
	}

	/** Slice the whole signal into bits. */
	BitVector sliced() const;
	void sliced(BitVector &result) const;

	std::string str() const;
};



std::ostream& operator<<(std::ostream&, const SoftVector&);




#endif
// vim: ts=4 sw=4
