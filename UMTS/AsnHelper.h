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

#ifndef ASNHELPER_H
#define ASNHELPER_H
#include "ByteVector.h"

#ifndef RN_CALLOC
#define RN_CALLOC(type) ((type*)calloc(1,sizeof(type)))
#endif

#include "asn_system.h"	// Dont let other includes land in namespace ASN.
#include <ctype.h>
extern "C" {
extern int rn_asn_debug;
};

namespace ASN {
#include "ENUMERATED.h"
#include "BIT_STRING.h"
};

namespace UMTS {
extern void setAsnBIT_STRING(ASN::BIT_STRING_t *result,uint8_t *buf, unsigned numBits);
extern ASN::BIT_STRING_t allocAsnBIT_STRING(unsigned numBits);
extern void setASN1SeqOfDigits(void *seq, const char* digit);
extern bool uperEncodeToBV(ASN::asn_TYPE_descriptor_t *td,void *sptr, ByteVector &result, const std::string descr="");
void *uperDecodeFromByteV(ASN::asn_TYPE_descriptor_t *asnType,ByteVector &bv);
void *uperDecodeFromBitV(ASN::asn_TYPE_descriptor_t *asnType,BitVector &in);
std::string asn2string(ASN::asn_TYPE_descriptor_t *asnp, const void *struct_ptr);
class UEInfo;
void asnLogMsg(unsigned rbid, ASN::asn_TYPE_descriptor_t *asnp, const void *struct_ptr, const char *comment="",UEInfo *uep=NULL, uint32_t urnti=0);

// Some trivial wrappers for ENUMERATED_t and INTEGER_t:
ASN::ENUMERATED_t toAsnEnumerated(unsigned value);
long asnEnum2long(ASN::ENUMERATED_t &thing);

// Figures out the reverse enum values from the asn description.
// These do not exist in the asn description, but the names of the enum values
// almost always include the value that is encoded by that enum constant.
// For example: foo45 might be the enum for value 45.
// Used extensively in URRCRB.h.
class AsnEnumMap
{
	unsigned mNumValues;
	long *mActualValues;
	void asnLoadEnumeratedValues(ASN::asn_TYPE_descriptor_t &asnp, unsigned maxEnumValue);

	public:
	AsnEnumMap(ASN::asn_TYPE_descriptor_t &asnp, unsigned maxEnumValue) {
		asnLoadEnumeratedValues(asnp, maxEnumValue);
	}

	void dump();

	// Return the asn enum value for an actual value.
	// This is slow, but only happens at setup.
	// Return "close" value if none match.
	int findEnum(long actual);

	// Various ways to get the data out:
	void cvtAsn(ASN::ENUMERATED_t *pasnval,long actual) {
		asn_long2INTEGER(pasnval,findEnum(actual));
	}
	void cvtAsn(ASN::ENUMERATED_t &asnval,long actual) { cvtAsn(&asnval,actual); }

	ASN::ENUMERATED_t toAsn(long actual) {
		return toAsnEnumerated(findEnum(actual));
	}

	ASN::ENUMERATED_t *allocAsn(long actual) {
		ASN::ENUMERATED_t *result = RN_CALLOC(ASN::ENUMERATED_t);
		cvtAsn(result,actual);
		return result;
	}
};

//class AsnEnum {
//	AsnEnumMap &mMap;
//	long mActual;
//	public:
//	AsnEnum(AsnEnumMap &wMap, long wActual) : mMap(wMap), mActual(wActual) { }
//	void toAsn(ASN::ENUMERATED_t &result) {
//		asn_long2INTEGER(&result,mMap.findEnum(mActual));
//	}
//};

// ================== classes to turn ASN things into ByteVectors ====================


// Convert an ASN BIT_STRING_t into a ByteVector by copying the memory.
struct AsnBitString2BV : public ByteVector {
	void finish(ASN::BIT_STRING_t *ptr) {
		if (ptr->bits_unused) setSizeBits(ptr->size*8 - ptr->bits_unused);
	}
	AsnBitString2BV(ASN::BIT_STRING_t *ptr) : ByteVector(ptr->buf,ptr->size) { finish(ptr); }
	AsnBitString2BV(ASN::BIT_STRING_t &ref) : ByteVector(ref.buf,ref.size) { finish(&ref); }
};

// Same as above, but dont copy the memory.  This is only an efficiency issue.
// This function is used to do bit-manipulations defined in ByteVector on the bytes making up the bitstring.
// For example:
// 		BITSTRING_t something;	// in asn somewhere.
// Get it:
// 		uint32_t result = AsnBitString2BV(something).getField(0,32);
// Or set it:
//		setAsnBIT_STRING(&something,(uint8_t*)calloc(1,4),32);	// allocate it first
// 		AsnBitString2BVTemp(something).setField(0,somevalue,32);		// then set it
// WARNING!! As of 12-17 The ByteVectorTemp will corrupt the underlying string if you accidently
// dup it (which is easy) so ONLY use this to immediately call setField, as it was intended.
struct AsnBitString2BVTemp : public ByteVectorTemp {
	void finish(ASN::BIT_STRING_t *ptr) {
		if (ptr->bits_unused) setSizeBits(ptr->size*8 - ptr->bits_unused);
	}
	AsnBitString2BVTemp(ASN::BIT_STRING_t *ptr) : ByteVectorTemp(ptr->buf,ptr->size) { finish(ptr); }
	AsnBitString2BVTemp(ASN::BIT_STRING_t &ref) : ByteVectorTemp(ref.buf,ref.size) { finish(&ref); }
};

// Convert an ASN OCTET_STRING into a ByteVector by copying the memory.
struct AsnOctetString2BV : public ByteVector {
	AsnOctetString2BV(ASN::OCTET_STRING_t *ptr) : ByteVector(ptr->buf,ptr->size) {}
	AsnOctetString2BV(ASN::OCTET_STRING_t &ref) : ByteVector(ref.buf,ref.size) {}
};

// Convert an ASN A_SEQUENCE_OF(Digit_t), or generically, any A_SEQUENCE_OF(long)
// where the long values are limited to bytes, into a ByteVector by copying the memory.
struct AsnSeqOfDigit2BV : public ByteVector {
	AsnSeqOfDigit2BV(void*list); // The argument must be ASN::A_SEQUENCE_OF(Digit_t) or equivalent.
};

}; // namespace UMTS

#endif
