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

#include "AsnHelper.h"
//#include "ByteVector.h"	included from AsnHelper.h
#include "SgsnBase.h"		// For the layer 3 logging facility.
#include <ctype.h>
#include <Configuration.h>
#include "URRC.h"
extern ConfigurationTable gConfig;	// Where is this thing?

// (pat) This variable controls asn debug statements.
// It is used in the ASN/makefile to over-ride the default ASN debugger.
extern "C" {
int rn_asn_debug = 1;
};

namespace ASN {

//#include "ENUMERATED.h"	included from AsnHelper.h
//#include "BIT_STRING.h"	included from AsnHelper.h
#include "Digit.h"
#include "asn_SEQUENCE_OF.h"
};

namespace UMTS {

// Return true on success, false on failure.
// Make the ByteVector large enough to hold the expected encoded message,
// and the ByteVector size will be shrink wrapped around the result before return.
// If the descr is specified, an error message is printed on failure before return.
bool uperEncodeToBV(ASN::asn_TYPE_descriptor_t *td,void *sptr, ByteVector &result, const std::string descr)
{
	rn_asn_debug = gConfig.getNum("UMTS.Debug.ASN");
	ASN::asn_enc_rval_t rval = uper_encode_to_buffer(td,sptr,result.begin(),result.allocSize());

	if (rval.encoded < 0) {
		LOG(ALERT) << "ASN encoder failed encoding '"<<descr<<"' into buf bytesize="<<result.size();
		return false;
	}
	// (pat) rval.encoded is number of bits, despite documentation
	// at asn_enc_rval_t that claims it is in bytes.
	result.setSizeBits(rval.encoded);
	return true;
}

// Decode an Asn message and return whatever kind of message pops out.
void *uperDecodeFromByteV(ASN::asn_TYPE_descriptor_t *asnType,ByteVector &bv)
{
	void *result = NULL;
	ASN::asn_dec_rval_s rval = uper_decode_complete(NULL,  // optional stack size
		asnType, &result,
		bv.begin(), bv.size()	// per buffer size is in bytes.
		);
		//0,	// No skip bits
		//0);	// No unused bits. (There may be but we dont tell per.)

	if (rval.code != ASN::RC_OK) {
		// What else should we say about this?
		LOG(ERR) << "incoming message could not be decoded.";
		return 0;
	}
	if (rval.consumed != bv.size()) {
		LOG(INFO) << "incoming message consumed only" << rval.consumed << " of " << bv.size() << " bytes.";
	}
	return result;
}

// Same as uperDecodeFromByteV but work on a BitVector
void *uperDecodeFromBitV(ASN::asn_TYPE_descriptor_t *asnType,BitVector &in)
{
	ByteVector bv(in);
	return uperDecodeFromByteV(asnType,bv);
}

// Set the ASN BIT_STRING_t to an allocated buffer of the proper size.
// User must determine the proper size for the ASN message being used.
void setAsnBIT_STRING(ASN::BIT_STRING_t *result,uint8_t *buf, unsigned numBits)
{
	result->buf = buf;
	result->size = (numBits+7)/8;
	result->bits_unused = (numBits%8) ? (8-(numBits%8)) : 0;
}

ASN::BIT_STRING_t allocAsnBIT_STRING(unsigned numBits)
{
	ASN::BIT_STRING_t result;
	setAsnBIT_STRING(&result,(uint8_t*)calloc(1,(7+numBits)/8),numBits);
	return result;
}

/** Copy a string of ASCII digits into an ASN.1 SEQUENCE OF DIGIT. */
void setASN1SeqOfDigits(void *seq, const char* digit)
{
	ASN::asn_sequence_empty(seq);
	while (*digit != '\0') {
		ASN::Digit_t* d = (ASN::Digit_t*)calloc(1,sizeof(ASN::Digit_t));
		assert(d);
		*d = *digit++ - '0';
		int ret = ASN::ASN_SEQUENCE_ADD(seq,d);
		assert(ret==0);
	}
}

// Convert an integral value to an ASN ENUMERATED struct.
// Note: if you use this in an assignment, it will not free the previous value, if any.
// Example:
//		ENUMERATED_t someAsnEnumeratedValue;			// In asn somewhere.
//		someAsnEnumeratedValue = toAsnEnumerated(3);	// In our code.
ASN::ENUMERATED_t toAsnEnumerated(unsigned value)
{
	ASN::ENUMERATED_t result;
	memset(&result,0,sizeof(result));
	asn_long2INTEGER(&result,value);
	return result;
}

long asnEnum2long(ASN::ENUMERATED_t &thing)
{
	long result;
	if (asn_INTEGER2long(&thing,&result)) {
		// We should not get this; it indicates a drastic encoding error in the UE.
		// If we do get it, will have to add arguments to figure out where it happened.
		LOG(ERR) << "failure converting asn ENUMERATED type to long";
	}
	return result;
}


// The argument must be A_SEQUENCE_OF(Digit_t) or equivalent.
// Digit_t is typedefed to long.
typedef A_SEQUENCE_OF(long) asn_sequence_of_long;
AsnSeqOfDigit2BV::AsnSeqOfDigit2BV(void*arg)
		: ByteVector(((asn_sequence_of_long*)arg)->count)
{
	asn_sequence_of_long *list = (asn_sequence_of_long*)arg;
	int cnt = size();
	uint8_t *bytes = begin();
	for (int i = 0; i < cnt; i++) {
		// The Digit_t is typedefed to long, and the longs are allocated.
		// (It couldnt be any more wasteful; pretty amusing that this is to compress the structures.)
		bytes[i] = (uint8_t) *(list->array[i]);
	}
}

//=============== Functions for AsnEnumMap ====================================

// Call back for ASN print function.
static int mycb(const void *buf, size_t size, void *application_specific_key)
{
	// Format of the value is "enumvalue (actualvalue)", eg: "5 (dat20)"
	const char *cp = (const char*)buf;
	cp = strchr(cp,'(');
	while (*cp != 0 && !isdigit(*cp)) { cp++; }
	long *presult = (long*) application_specific_key;
	*presult = atoi(cp);
	//printf("mycp buffer=%s %ld\n",(const char*)buf,*presult);
	return 0;
}

void AsnEnumMap::asnLoadEnumeratedValues(ASN::asn_TYPE_descriptor_t &asnp, unsigned maxEnumValue)
{
	mNumValues = (1+maxEnumValue);
	mActualValues = (long*) malloc(mNumValues*sizeof(long));
	// Note: n is a long because that is what is expected.
	for (long n = 0; n <= (long)maxEnumValue; n++) {
		ASN::ENUMERATED_t foo;
		memset(&foo,0,sizeof(foo));
		ASN::asn_long2INTEGER(&foo, n);
		// In the original asn description, the actual values are just strings,
		// not the actual integral values that we need, but the actual
		// value is (almost) always included in the string.
		// The asn compiler does not provide any native way to get the enum strings out,
		// but we can use the print_struct which does a call-back to us with the info.
		asnp.print_struct(&asnp,(const void*) &foo,0,mycb,(void*)&mActualValues[n]);
	}
}

void AsnEnumMap::dump()
{
	for (unsigned enumValue = 0; enumValue < mNumValues; enumValue++) {
		printf("%u=>%ld ",enumValue,mActualValues[enumValue]);
		if (enumValue && enumValue % 8 == 0) printf("\n");
	}
	printf("\n");
}

// Return the asn enum value for an actual value.
// This is slow, but only happens at setup.
// Return "close" value if none match.
int AsnEnumMap::findEnum(long actual)
{
	unsigned closeindex = 0;
	long closediff = 0x7fffffff;
	for (unsigned enumValue = 0; enumValue < mNumValues; enumValue++) {
		if (actual == mActualValues[enumValue]) return enumValue;
		int diff = mActualValues[enumValue] - actual;
		if (diff < 0) diff = - diff;
		if (diff < closediff) { closediff = diff; closeindex = enumValue; }
	}
	//std::cout << "warning: enum value " << actual << " does not match, using: " << mActualValues[closeindex] <<"\n";
	return closeindex;
}

// Call back for ASN print function.
static int mycb2(const void *buf, size_t size, void *application_specific_key)
{
	const char *cp = (const char*)buf;	// Gotta love c++.
	// Format of the value is "enumvalue (actualvalue)", eg: "5 (dat20)"
	std::ostringstream *ssp = (std::ostringstream*)application_specific_key;
	ssp->write(cp,size);
	return 0;
}

// Like asn_fprint but return the result in a C++ string.
std::string asn2string(ASN::asn_TYPE_descriptor_t *asnp, const void *struct_ptr)
{
	std::ostringstream ss;
	asnp->print_struct(asnp,struct_ptr,0,mycb2,(void*)&ss);
	return ss.str();
}

void asnLogMsg(unsigned rbid, ASN::asn_TYPE_descriptor_t *asnp, const void *struct_ptr,
	const char *comment,
	UEInfo *uep,		// Or NULL if none.
	uint32_t urnti)		// If uep is NULL, put this in the log instead.
{
	int debug = gConfig.getNum("UMTS.Debug.Messages");
	if (debug || IS_LOG_LEVEL(INFO)) {
		// This C++ IO paradigm is so crappy.
		std::string readable = asn2string(asnp,struct_ptr);
		std::string id = uep ? uep->ueid() : format(" urnti=0x%x",urnti);
		_LOG(INFO) << (comment?comment:"") <<id<<LOGVAR(rbid) <<" "<< readable.c_str();
		if (debug && comment) {
			MGLOG(comment <<id<<LOGVAR(rbid));
			LOGWATCH(comment <<id<<LOGVAR(rbid));
		}
	}
}

//void AsnBitString::finish(ASN::BIT_STRING_t *ptr)
//{
//	if (ptr->bits_unused) setSizeBits(ptr->size*8 - ptr->bits_unused);
//}
//
//AsnBitString::AsnBitString(ASN::BIT_STRING_t *ptr) :
//	ByteVector(ptr->buf,ptr->size)
//{
//	finish(ptr);
//}
//
//AsnBitString::AsnBitString(ASN::BIT_STRING_t &ref) :
//	ByteVector(ref.buf,ref.size)
//{
//	finish(ptr);
//}

};	// namespace UMTS
