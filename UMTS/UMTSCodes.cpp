/**@file Objects for generating UMTS channelization, scrambling and sync codes, from 3GPP 25.213 Sections 4 & 5. */

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

#include "MemoryLeak.h"
#include "UMTSCodes.h"

using namespace UMTS;


/// The global OVSF tree
OVSFTree UMTS::gOVSFTree;

void OVSFTree::branch(unsigned SFI)
{
       // 3GPP 25.213 4.3.1.

        if (SFI==0) return seed();

        // If this set is already generated, just return.
        if (mCodeSets[SFI]) return;

        // If the parent set doesn't exist yet, generate it first.
        if (!mCodeSets[SFI-1]) branch(SFI-1);

        // Generate SFI from SFI-1 (the parent set).
        // Here, "p" means "parent".
        // code lengths and counts at this level and the parent level
        unsigned N = 1<<SFI;
        unsigned Np = N/2;
        // the new code set
        mCodeSets[SFI] = new int8_t*[N];
        int8_t **cn = mCodeSets[SFI];

        // the parent code set
        int8_t **p = mCodeSets[SFI-1];
        // iterate over each code in the set at this SF
        for (unsigned ip=0; ip<N; ip+=2) {
                cn[ip] = new int8_t[N];
                cn[ip+1] = new int8_t[N];
                // iterate over the chips
                for (unsigned cix=0; cix<Np; cix++) {
                        const int8_t vn = p[ip/2][cix];
                        const int8_t vi = -vn;
                        // normal codes
                        cn[ip][cix] = vn;
                        cn[ip][cix+Np] = vn;
                        cn[ip+1][cix] = vn;
                        cn[ip+1][cix+Np] = vi;
                }
        }
}


OVSFTree::OVSFTree()
{
	for (int i = 0; i < 10; i++) {
	  mCodeSets[i] = NULL;
        }
	seed();

}



void OVSFTree::seed()
{
	if (mCodeSets[0]) return;

	mCodeSets[0] = new int8_t*[1];
	mCodeSets[0][0] = new int8_t[1];
	mCodeSets[0][0][0] = 1;
}


const int8_t* OVSFTree::code(unsigned SFI, unsigned index)
{
	if (!mCodeSets[SFI]) branch(SFI);
	return mCodeSets[SFI][index];
}

/// The global H8 tree
Hadamard8 UMTS::gHadamard8;

void Hadamard8::branch(unsigned dim)
{
       // 3GPP 25.213 4.3.1.

        if (dim==0) return seed();

        // If this set is already generated, just return.
        if (mCodeSets[dim]) return;

        // If the parent set doesn't exist yet, generate it first.
        if (!mCodeSets[dim-1]) branch(dim-1);

        // Generate H_dim from H_(dim-1) (the parent set).
        // Here, "p" means "parent".
        // code lengths and counts at this level and the parent level
        unsigned N = 1<<dim;
        unsigned Np = N/2;
        // the new code set and inverted version
        mCodeSets[dim] = new int8_t*[N];
        int8_t **cn = mCodeSets[dim];
        // the parent code set
        int8_t **p = mCodeSets[dim-1];
        // iterate over each code in the set at this SF
        for (unsigned ip=0; ip<Np; ip++) {
                cn[ip] = new int8_t[N];
                cn[ip+Np] = new int8_t[N];
                // iterate over the chips
                for (unsigned cix=0; cix<Np; cix++) {
                        const int8_t vn = p[ip][cix];
                        const int8_t vi = -vn;
                        // normal codes
                        cn[ip][cix] = vn;
                        cn[ip][cix+Np] = vn;
                        cn[ip+Np][cix] = vn;
                        cn[ip+Np][cix+Np] = vi;
                }
        }
}


Hadamard8::Hadamard8()
{
	for (int i = 0; i < 9; i++) {
	  mCodeSets[i] = NULL;
        }
	seed();

}



void Hadamard8::seed()
{
	if (mCodeSets[0]) return;

	mCodeSets[0] = new int8_t*[1];
	mCodeSets[0][0] = new int8_t[1];
	mCodeSets[0][0][0] = 1;

}


const int8_t* Hadamard8::code(unsigned index)
{
	if (!mCodeSets[8]) branch(8);
	return mCodeSets[8][index];
}


ScramblingCode::ScramblingCode(unsigned xCoeff, unsigned yCoeff, unsigned order, unsigned len)
	:mXGenerator(xCoeff,order), mYGenerator(yCoeff,order)
{
	RN_MEMCHKNEW(ScramblingCode);
	// Allocate space for the X & Y subcodes.
	mXFBCode = new unsigned char[len];
	mXFFCode = new unsigned char[len];
	mYFBCode = new unsigned char[len];
	mYFFCode = new unsigned char[len];
}

ScramblingCode::~ScramblingCode()
{
	RN_MEMCHKDEL(ScramblingCode);
	delete mXFBCode;
	delete mXFFCode;
	delete mYFBCode;
	delete mYFFCode;
	delete mICode;
	delete mQCode;
}


void ScramblingCode::generateXYSubcodes(SequenceGenerator32& gen, unsigned readMask, unsigned char* codeFB, unsigned char* codeFF, unsigned len)
{
	for (unsigned i=0; i<len; i++) {
		codeFB[i] = gen.LSB();
		codeFF[i] = gen.read(readMask);
		gen.step();
	}
}


void ScramblingCode::sumCodes(const unsigned char* codeX, const unsigned char* codeY, int8_t* codeC, unsigned len)
{
	for (unsigned i=0; i<len; i++) {
		codeC[i] = ((codeX[i] ^ codeY[i]) ? -1 : 1);
	}
}







DownlinkScramblingCode::DownlinkScramblingCode(unsigned N)
	:ScramblingCode(0x081,0x04a1,18)
{
	mICode = new int8_t[gFrameLen];
	mQCode = new int8_t[gFrameLen];

	// Init the generators.
	mXGenerator.state(0x01);
	mYGenerator.state(0x03ffff);

	// Run the X generator forward.
	for (unsigned i=0; i<N; i++) mXGenerator.step();

	// Generate the X & Y subcodes.
	generateXYSubcodes(mXGenerator, 0x08050, mXFBCode, mXFFCode);
	generateXYSubcodes(mYGenerator, 0x0ff60, mYFBCode, mYFFCode);

	// Generate I&Q codes.
	sumCodes(mXFBCode,mYFBCode,mICode);
	sumCodes(mXFFCode,mYFFCode,mQCode);
}






UplinkScramblingCode::UplinkScramblingCode(unsigned N)
	:ScramblingCode(0x09,0x0f,25,gFrameLen+4096)
{
	// Allocate space for the C codes.
	int8_t mC1Code[gFrameLen+4096];
	int8_t mC2Code[gFrameLen+4096];

	// Init the generators.
	mXGenerator.state((1<<24)+N);
	mYGenerator.state(0x01ffffff);

	// Generate the X & Y subcodes.
	generateXYSubcodes(mXGenerator, 0x040090, mXFBCode, mXFFCode,gFrameLen+4096);
	generateXYSubcodes(mYGenerator, 0x020050, mYFBCode, mYFFCode,gFrameLen+4096);

	// Generate C1 and C2 codes.
	sumCodes(mXFBCode,mYFBCode,mC1Code,gFrameLen+4096);
	sumCodes(mXFFCode,mYFFCode,mC2Code,gFrameLen+4096);

	// Generate a full complex code.
        mICode = new int8_t[gFrameLen+4096];
        mQCode = new int8_t[gFrameLen+4096];
	for (unsigned i=0; i<gFrameLen+4096; i+=2) {
		mICode[i] = mC1Code[i];
		mICode[i+1] = mC1Code[i+1];
		mQCode[i] = mC1Code[i]*mC2Code[i];
		mQCode[i+1] = -(mC1Code[i+1]*mC2Code[i]);
	}

}




UplinkScramblingCode::~UplinkScramblingCode()
{
}




PrimarySyncCode::PrimarySyncCode()
{
	// 3GPP 25.213 5.2.3

	unsigned char a[16] = {0,0,0,0,0,0,1,1,0,1,0,1,0,1,1,0};
	unsigned char rep[16] = {0,0,0,1,1,0,1,1,0,0,0,1,0,1,0,0};

	for (int i=0; i<16; i++) {
		for (int j=0; j<16; j++) {
			mCode[i*16+j] = ( (a[j] ^ rep[i]) ? -1 : 1);
		}
	}
}



SecondarySyncCode::SecondarySyncCode(unsigned N)
{
        unsigned char b[16] = {0,0,0,0,0,0,1,1,1,0,1,0,1,0,0,1};
	unsigned char rep[16] = {0,0,0,1,0,0,1,1,0,1,0,1,1,1,1,1};
	unsigned char z[256];

	for (int i=0; i<16; i++) {
		for (int j=0; j<16; j++) {
			z[i*16+j] = b[j] ^ rep[i];
		}
	}

	// The H8 matrix is in the OVSF tree already.
	const int8_t* h = gHadamard8.code(N);

	for (int i=0; i<256; i++) {
		mCode[i] = (h[i] * (z[i] ? -1 : 1));
	}
}




