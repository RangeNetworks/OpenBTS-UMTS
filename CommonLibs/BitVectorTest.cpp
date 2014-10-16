/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
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
#include <math.h>
 
using namespace std;

// We must have a gConfig now to include BitVector.
#include "Configuration.h"
ConfigurationTable gConfig;


bool veq(BitVector v1, BitVector v2)
{
	for (unsigned i = 0; i < v1.size(); i++) {
		if (v1.bit(i) != v2.bit(i)) return false;
	}
	return true;
}

BitVector randomBitVector(int n)
{
	BitVector t(n);
	for (int i = 0; i < n; i++) t[i] = random()%2;
	return t;
}

bool permutationCheck(vector<int>& pv)
{
	BitVector bv(pv.size());
	for (unsigned i = 0; i < pv.size(); i++) bv[i] = 0;
	for (unsigned i = 0; i < pv.size(); i++) {
		unsigned p = pv[i];
		if (p < 0 || p >= pv.size()) return false;
		if (bv[p]) return false;
		bv[p] = 1;
	}
	return true;
}

void test2O4()
{
	BitVector v1("0000111100111100101011110000");
	cout << v1 << endl;
	v1.LSB8MSB();
	cout << v1 << endl;
	ViterbiR2O4 vCoder;
	BitVector v2(v1.size()*2);
	v1.encode(vCoder,v2);
	cout << v2 << endl;
	SoftVector sv2(v2);
	cout << sv2 << endl;
	for (unsigned i=0; i<sv2.size()/4; i++) sv2[random()%sv2.size()]=0.5;
	cout << sv2 << endl;
	BitVector v3(v1.size());
	sv2.decode(vCoder,v3);
	cout << v3 << endl;
	cout << "R2O4 decode " << (veq(v1,v3) ? "ok" : "fail") << endl;

	cout << v3.segment(3,4) << endl;

	BitVector v4(v3.segment(0,4),v3.segment(8,4));
	cout << v4 << endl;

	BitVector v5("000011110000");
	int r1 = v5.peekField(0,8);
	int r2 = v5.peekField(4,4);
	int r3 = v5.peekField(4,8);
	cout << r1 <<  ' ' << r2 << ' ' << r3 << endl;
	cout << v5 << endl;
	v5.fillField(0,0xa,4);
	int r4 = v5.peekField(0,8);
	cout << v5 << endl;
	cout << r4 << endl;

	v5.reverse8();
	cout << v5 << endl;

	BitVector mC = "000000000000111100000000000001110000011100001101000011000000000000000111000011110000100100001010000010100000101000001010000010100000010000000000000000000000000000000000000000000000001100001111000000000000000000000000000000000000000000000000000010010000101000001010000010100000101000001010000001000000000000000000000000110000111100000000000001110000101000001100000001000000000000";
	SoftVector mCS(mC);
	BitVector mU(mC.size()/2);
	mCS.decode(vCoder,mU);
	cout << "c=" << mCS << endl;
	cout << "u=" << mU << endl;
	cout << "c.str=" << mCS.str() << endl;
	// (pat) Try putting in some -
	mCS[0] = 0.4;
	cout << "start with -: c.str=" << mCS.str() << endl;
	mCS[0] = 1;
	mCS[1] = 0.4;
	cout << "start with -: c.str=" << mCS.str() << endl;


	unsigned char ts[9] = "abcdefgh";
	BitVector tp(70);
	cout << "ts=" << ts << endl;
	tp.unpack(ts);
	cout << "tp=" << tp << endl;
	tp.pack(ts);
	cout << "ts=" << ts << endl;
}

void test2O9()
{
	BitVector v1("0000111100111100101011110000");
	cout << v1 << endl;
	v1.LSB8MSB();
	cout << v1 << endl;
	ViterbiR2O9 vCoder;
	BitVector v2(v1.size()*2);
	v1.encode(vCoder,v2);
	cout << v2 << endl;
	SoftVector sv2(v2);
	cout << sv2 << endl;
	for (unsigned i=0; i<sv2.size()/4; i++) sv2[random()%sv2.size()]=0.5;
	cout << sv2 << endl;
	BitVector v3(v1.size());
	sv2.decode(vCoder,v3);
	cout << v3 << endl;
	cout << "R2O9 decode " << (veq(v1,v3) ? "ok" : "fail") << endl;
}

const int inter1Columns[] = { 1, 2, 4, 8 };

const char inter1Perm[4][8] = {
	{0},
	{0, 1},
	{0, 2, 1, 3},
	{0, 4, 2, 6, 1, 5, 3, 7}
};

const char inter2Perm[] = {
	0, 20, 10, 5, 15, 25, 3, 13, 23, 8, 18, 28, 1, 11, 21,
	6, 16, 26, 4, 14, 24, 19, 9, 29, 12, 2, 7, 22, 27, 17
};

void testInterleavings()
{
	int lth1 = 48;
	int C2 = 30;
	for (int i = 0; i < 4; i++) {
		BitVector v1 = randomBitVector(lth1);
		BitVector v2(lth1);
		BitVector v3(lth1);
		v1.interleavingNP(inter1Columns[i], inter1Perm[i], v2);
		v2.deInterleavingNP(inter1Columns[i], inter1Perm[i], v3);
		cout << "first " << i << " " << (veq(v1, v3) ? "ok" : "fail") << endl;
	}
	for (int lth2 = 90; lth2 < 120; lth2++) {
		BitVector v1 = randomBitVector(lth2);
		BitVector v2(lth2);
		BitVector v3(lth2);
		v1.interleavingWP(C2, inter2Perm, v2);
		v2.deInterleavingWP(C2, inter2Perm, v3);
		cout << "second " << lth2 << " " << (veq(v1, v3) ? "ok" : "fail") << endl;
	}
	for (int lth = 48; lth <= 4800; lth *= 10) {
		TurboInterleaver er(lth);
		cout << "Turbo Interleaver permutation(" << lth << ") " << (permutationCheck(er.permutation()) ? "ok" : "fail") << endl;
		SoftVector er1 = randomBitVector(lth);
		SoftVector er2(lth);
		er.interleave(er1, er2);
		BitVector er3(lth);
		er.unInterleave(er2, er3);
		cout << "Turbo Interleaver(" << lth << ") " << (veq(er1.sliced(), er3) ? "ok" : "fail") << endl;
	}
}

float noise(SoftVector v, SoftVector sv)
{
	assert(v.size() == sv.size());
	float noise = 0;
	for (unsigned i = 0; i < v.size(); i++) {
		noise += fabs(v[i] - sv[i]);
	}
	return noise / (float)v.size();
}

void testTurbo()
{
	int K = 40;
	ViterbiTurbo vCoder;
	TurboInterleaver interleaver(K);
	float noisein = 0;
	float noiseout = 0;
	int n = 0;
	for (int k = 1; k < 5; k++) {
		int ok = 0;
		for (int j = 0; j < 20; j++) {
			BitVector v1 = randomBitVector(K);
			BitVector v2(K * 3 + 12);
			v1.encode(vCoder, v2, interleaver);
			SoftVector sv2(v2);
			for (unsigned i=0; i<sv2.size()/(k*4); i++) sv2[random()%sv2.size()]=0.5;
			BitVector v3(K);
			sv2.decode(vCoder,v3, interleaver);
			if (veq(v1,v3)) ok++;
			// check SNR
			SoftVector sv3(K);
			sv2.decode(vCoder, sv3, interleaver);
			noisein += noise(v2, sv2);
			noiseout += noise(v1, sv3);
			n++;
		}
		cout << 20-ok << " fail, and " << ok << " ok" << endl;
		cout << "avg noise in (" << noisein/n << "), / avg noise out (" << noiseout/n << ") = " << noisein/noiseout << endl;
	}
}

int main()
{
	test2O4();
	test2O9();
	testInterleavings();
	testTurbo();
}
