/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef __SIGNALVECTOR_H__
#define __SIGNALVECTOR_H__

#include "MemoryLeak.h"
#include "Vector.h"
#include "Complex.h"

/** Indicated signalVector symmetry */
enum Symmetry {
  NONE = 0,
  ABSSYM = 1
};

DEFINE_MEMORY_LEAK_DETECTOR_CLASS(signalVector,MemChecksignalVector)

/** the core data structure of the Transceiver */
class signalVector: public Vector<complex>, public MemChecksignalVector
{

 private:
  
  Symmetry symmetry;   ///< the symmetry of the vector
  bool realOnly;       ///< true if vector is real-valued, not complex-valued
  
 public:
  
  /** Constructors */
  signalVector(int dSize=0, Symmetry wSymmetry = NONE):
    Vector<complex>(dSize),
    realOnly(false)
    { 
      symmetry = wSymmetry; 
    };
    
  signalVector(complex* wData, size_t start, 
	       size_t span, Symmetry wSymmetry = NONE, bool wRealOnly = false):
    Vector<complex>(NULL,wData+start,wData+start+span),
    realOnly(wRealOnly)
    { 
      symmetry = wSymmetry; 
    };
      
  signalVector(const signalVector &vec1, const signalVector &vec2):
    Vector<complex>(vec1,vec2),
    realOnly(false)
    { 
      symmetry = vec1.symmetry; 
    };
	
  signalVector(const signalVector &wVector):
    Vector<complex>(wVector.size()),
    realOnly(false)
    {
      wVector.copyTo(*this); 
      symmetry = wVector.getSymmetry();
    };

  /** symmetry operators */
  Symmetry getSymmetry() const { return symmetry;};
  void setSymmetry(Symmetry wSymmetry) { symmetry = wSymmetry;}; 

  /** real-valued operators */
  bool isRealOnly() const { return realOnly;};
  void isRealOnly(bool wOnly) { realOnly = wOnly;};

  signalVector segment(size_t start, size_t span)
    {
       return signalVector(this->begin(),start,span,
			   this->getSymmetry(),
			   this->isRealOnly());
    };


};

#endif
