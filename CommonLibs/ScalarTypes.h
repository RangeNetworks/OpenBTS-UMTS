/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011-2014 Range Networks, Inc.
 * Patched by FlUxIuS @ Penthertz SAS
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef SCALARTYPES_H
#define SCALARTYPES_H
#include <iostream>	// For size_t
#include <stdint.h>
//#include "GSMCommon.h"	// Was included for Z100Timer

// We dont bother to define *= /= etc.; you'll have to convert: a*=b; to: a=a*b;
#define _INITIALIZED_SCALAR_FUNCS(Classname,Basetype,Init) \
	Classname() : value(Init) {} \
	Classname(Basetype wvalue) { value = wvalue; } /* Can set from basetype. */ \
	operator Basetype(void) const { return value; }		/* Converts from basetype. */ \
	Basetype operator++() { return ++value; } \
	Basetype operator++(int) { return value++; } \
	Basetype operator=(Basetype wvalue) { return value = wvalue; } \
	Basetype operator+=(Basetype wvalue) { return value = value + wvalue; } \
	Basetype operator-=(Basetype wvalue) { return value = value - wvalue; } \
	Basetype* operator&() { return &value; } \


#define _INITIALIZED_SCALAR_FUNCSBOOL(Classname,Basetype,Init) \
        Classname() : value(Init) {} \
        Classname(Basetype wvalue) { value = wvalue; } /* Can set from basetype. */ \
        operator Basetype(void) const { return value; }         /* Converts from basetype. */ \
        Basetype operator++() { return true; } \
        Basetype operator++(int) { return true; } \
        Basetype operator=(Basetype wvalue) { return value = wvalue; } \
        Basetype operator+=(Basetype wvalue) { return value = value + wvalue; } \
        Basetype operator-=(Basetype wvalue) { return value = value - wvalue; } \
        Basetype* operator&() { return &value; } \


#define _DECLARE_SCALAR_TYPE(Classname_i,Classname_z,Basetype) \
	template <Basetype Init> \
	struct Classname_i { \
		Basetype value; \
		_INITIALIZED_SCALAR_FUNCS(Classname_i,Basetype,Init) \
	}; \
	typedef Classname_i<0> Classname_z;

#define _DECLARE_SCALAR_TYPEBOOL(Classname_i,Classname_z,Basetype) \
        template <Basetype Init> \
        struct Classname_i { \
                Basetype value; \
                _INITIALIZED_SCALAR_FUNCSBOOL(Classname_i,Basetype,Init) \
        }; \
        typedef Classname_i<0> Classname_z;


// Usage:
// Where 'classname' is one of the types listed below, then:
// 		classname_z specifies a zero initialized type;
// 		classname_i<value> initializes the type to the specified value.
// We also define Float_z.
_DECLARE_SCALAR_TYPE(Int_i, 	Int_z,  	int)
_DECLARE_SCALAR_TYPE(Char_i,	Char_z, 	signed char)
_DECLARE_SCALAR_TYPE(Int16_i,	Int16_z,	int16_t)
_DECLARE_SCALAR_TYPE(Int32_i,	Int32_z,	int32_t)
_DECLARE_SCALAR_TYPE(UInt_i,  	UInt_z,  	unsigned)
_DECLARE_SCALAR_TYPE(UChar_i,  	UChar_z,  	unsigned char)
_DECLARE_SCALAR_TYPE(UInt8_i,  	UInt8_z,  	uint8_t)
_DECLARE_SCALAR_TYPE(UInt16_i,	UInt16_z,	uint16_t)
_DECLARE_SCALAR_TYPE(UInt32_i,	UInt32_z,	uint32_t)
_DECLARE_SCALAR_TYPE(Size_t_i,	Size_t_z,	size_t)
_DECLARE_SCALAR_TYPEBOOL(Bool_i,  	Bool_z, 	bool)

// float is special, because C++ does not permit the template initalization:
struct Float_z {
	float value;
	_INITIALIZED_SCALAR_FUNCS(Float_z,float,0)
};
struct Double_z {
	double value;
	_INITIALIZED_SCALAR_FUNCS(Double_z,double,0)
};


class ItemWithValueAndWidth {
	public:
	virtual unsigned getValue() const = 0;
	virtual unsigned getWidth() const = 0;
};

// A Range Networks Field with a specified width.
// See RLCMessages.h for examples.
template <int Width=32, unsigned Init=0>
class Field_i : public ItemWithValueAndWidth
{
	public:
	unsigned value;
	_INITIALIZED_SCALAR_FUNCS(Field_i,unsigned,Init)
	unsigned getWidth() const { return Width; }
	unsigned getValue() const { return value; }
};

// Synonym for Field_i, but no way to do it.
template <int Width, unsigned Init=0>
class Field_z : public ItemWithValueAndWidth
{
	public:
	unsigned value;
	_INITIALIZED_SCALAR_FUNCS(Field_z,unsigned,Init)
	unsigned getWidth() const { return Width; }
	unsigned getValue() const { return value; }
};

// This is an uninitialized field.
template <int Width=32, unsigned Init=0>
class Field : public ItemWithValueAndWidth
{
	public:
	unsigned value;
	_INITIALIZED_SCALAR_FUNCS(Field,unsigned,Init)
	unsigned getWidth() const { return Width; }
	unsigned getValue() const { return value; }
};


// A Z100Timer with an initial value specified.
//template <int Init>
//class Z100Timer_i : public GSM::Z100Timer {
//	public:
//	Z100Timer_i() : GSM::Z100Timer(Init) {}
//};

#endif
