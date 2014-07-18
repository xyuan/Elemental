/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"
#include "El-C.h"
using namespace El;

extern "C" {

#define C_PROTO_BASE(SIG,T) \
  /* Circulant */ \
  ElError ElCirculant_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt aSize, CREFLECT(T)* aBuf ) \
  { try { \
      std::vector<T> a( Reinterpret(aBuf), Reinterpret(aBuf)+aSize ); \
      Circulant( *Reinterpret(A), a ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElCirculantDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt aSize, CREFLECT(T)* aBuf ) \
  { try { \
      std::vector<T> a( Reinterpret(aBuf), Reinterpret(aBuf)+aSize ); \
      Circulant( *Reinterpret(A), a ); \
    } EL_CATCH; return EL_SUCCESS; } \
  /* Diagonal */ \
  ElError ElDiagonal_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt dSize, CREFLECT(T)* dBuf ) \
  { try { \
      std::vector<T> d( Reinterpret(dBuf), Reinterpret(dBuf)+dSize ); \
      Diagonal( *Reinterpret(A), d ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElDiagonalDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt dSize, CREFLECT(T)* dBuf ) \
  { try { \
      std::vector<T> d( Reinterpret(dBuf), Reinterpret(dBuf)+dSize ); \
      Diagonal( *Reinterpret(A), d ); \
    } EL_CATCH; return EL_SUCCESS; } \
  /* Forsythe */ \
  ElError ElForsythe_ ## SIG \
  ( ElMatrix_ ## SIG J, ElInt n, CREFLECT(T) alpha, CREFLECT(T) lambda ) \
  { EL_TRY( \
      Forsythe( \
        *Reinterpret(J), n, Reinterpret(alpha), Reinterpret(lambda) ) ) } \
  ElError ElForsytheDist_ ## SIG \
  ( ElDistMatrix_ ## SIG J, ElInt n, CREFLECT(T) alpha, CREFLECT(T) lambda ) \
  { EL_TRY( \
      Forsythe( \
        *Reinterpret(J), n, Reinterpret(alpha), Reinterpret(lambda) ) ) } \
  /* Ones */ \
  ElError ElOnes_ ## SIG ( ElMatrix_ ## SIG A, ElInt m, ElInt n ) \
  { EL_TRY( Ones( *Reinterpret(A), m, n ) ) } \
  ElError ElOnesDist_ ## SIG ( ElDistMatrix_ ## SIG A, ElInt m, ElInt n ) \
  { EL_TRY( Ones( *Reinterpret(A), m, n ) ) } \
  /* Uniform */ \
  ElError ElUniform_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt m, ElInt n, \
    CREFLECT(T) center, Base<T> radius ) \
  { EL_TRY( Uniform( *Reinterpret(A), m, n, Reinterpret(center), radius ) ) } \
  ElError ElUniformDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt m, ElInt n, \
    CREFLECT(T) center, Base<T> radius ) \
  { EL_TRY( Uniform( *Reinterpret(A), m, n, Reinterpret(center), radius ) ) }

#define C_PROTO_NOINT(SIG,T) \
  /* Cauchy */ \
  ElError ElCauchy_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt xSize, CREFLECT(T)* xBuf, \
                        ElInt ySize, CREFLECT(T)* yBuf ) \
  { try { \
      std::vector<T> x( Reinterpret(xBuf), Reinterpret(xBuf)+xSize ); \
      std::vector<T> y( Reinterpret(yBuf), Reinterpret(yBuf)+ySize ); \
      Cauchy( *Reinterpret(A), x, y ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElCauchyDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt xSize, CREFLECT(T)* xBuf, \
                            ElInt ySize, CREFLECT(T)* yBuf ) \
  { try { \
      std::vector<T> x( Reinterpret(xBuf), Reinterpret(xBuf)+xSize ); \
      std::vector<T> y( Reinterpret(yBuf), Reinterpret(yBuf)+ySize ); \
      Cauchy( *Reinterpret(A), x, y ); \
    } EL_CATCH; return EL_SUCCESS; } \
  /* Cauchy-like */ \
  ElError ElCauchyLike_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt rSize, CREFLECT(T)* rBuf, \
                        ElInt sSize, CREFLECT(T)* sBuf, \
                        ElInt xSize, CREFLECT(T)* xBuf, \
                        ElInt ySize, CREFLECT(T)* yBuf ) \
  { try { \
      std::vector<T> r( Reinterpret(rBuf), Reinterpret(rBuf)+rSize ); \
      std::vector<T> s( Reinterpret(sBuf), Reinterpret(sBuf)+sSize ); \
      std::vector<T> x( Reinterpret(xBuf), Reinterpret(xBuf)+xSize ); \
      std::vector<T> y( Reinterpret(yBuf), Reinterpret(yBuf)+ySize ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElCauchyLikeDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt rSize, CREFLECT(T)* rBuf, \
                            ElInt sSize, CREFLECT(T)* sBuf, \
                            ElInt xSize, CREFLECT(T)* xBuf, \
                            ElInt ySize, CREFLECT(T)* yBuf ) \
  { try { \
      std::vector<T> r( Reinterpret(rBuf), Reinterpret(rBuf)+rSize ); \
      std::vector<T> s( Reinterpret(sBuf), Reinterpret(sBuf)+sSize ); \
      std::vector<T> x( Reinterpret(xBuf), Reinterpret(xBuf)+xSize ); \
      std::vector<T> y( Reinterpret(yBuf), Reinterpret(yBuf)+ySize ); \
      CauchyLike( *Reinterpret(A), r, s, x, y ); \
    } EL_CATCH; return EL_SUCCESS; } \
  /* Demmel */ \
  ElError ElDemmel_ ## SIG ( ElMatrix_ ## SIG A, ElInt n ) \
  { EL_TRY( Demmel( *Reinterpret(A), n ) ) } \
  ElError ElDemmelDist_ ## SIG ( ElDistMatrix_ ## SIG A, ElInt n ) \
  { EL_TRY( Demmel( *Reinterpret(A), n ) ) } \
  /* Ehrenfest */ \
  ElError ElEhrenfest_ ## SIG ( ElMatrix_ ## SIG P, ElInt n ) \
  { EL_TRY( Ehrenfest( *Reinterpret(P), n ) ) } \
  ElError ElEhrenfestDist_ ## SIG ( ElDistMatrix_ ## SIG P, ElInt n ) \
  { EL_TRY( Ehrenfest( *Reinterpret(P), n ) ) } \
  ElError ElEhrenfestStationary_ ## SIG \
  ( ElMatrix_ ## SIG PInf, ElInt n ) \
  { EL_TRY( EhrenfestStationary( *Reinterpret(PInf), n ) ) } \
  ElError ElEhrenfestStationaryDist_ ## SIG \
  ( ElDistMatrix_ ## SIG PInf, ElInt n ) \
  { EL_TRY( EhrenfestStationary( *Reinterpret(PInf), n ) ) } \
  ElError ElEhrenfestDecay_ ## SIG ( ElMatrix_ ## SIG A, ElInt n ) \
  { EL_TRY( EhrenfestDecay( *Reinterpret(A), n ) ) } \
  /* TODO: Distributed EhrenfestDecay */ \
  /* ExtendedKahan */ \
  ElError ElExtendedKahan_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt k, Base<T> phi, Base<T> mu ) \
  { EL_TRY( ExtendedKahan( *Reinterpret(A), k, phi, mu ) ) } \
  /* TODO: Distributed ExtendedKahan */ \
  /* Fiedler */ \
  ElError ElFiedler_ ## SIG \
  ( ElMatrix_ ## SIG A, ElInt cSize, CREFLECT(T)* cBuf ) \
  { try { \
      std::vector<T> c( Reinterpret(cBuf), Reinterpret(cBuf)+cSize ); \
      Fiedler( *Reinterpret(A), c ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElFiedlerDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, ElInt cSize, CREFLECT(T)* cBuf ) \
  { try { \
      std::vector<T> c( Reinterpret(cBuf), Reinterpret(cBuf)+cSize ); \
      Fiedler( *Reinterpret(A), c ); \
    } EL_CATCH; return EL_SUCCESS; }

#define C_PROTO_INT(SIG,T) \
  C_PROTO_BASE(SIG,T)

#define C_PROTO_REAL(SIG,T) \
  C_PROTO_BASE(SIG,T) \
  C_PROTO_NOINT(SIG,T)

#define C_PROTO_COMPLEX(SIG,SIGBASE,T) \
  C_PROTO_BASE(SIG,T) \
  C_PROTO_NOINT(SIG,T) \
  /* Bull's head */ \
  ElError ElBullsHead_ ## SIG ( ElMatrix_ ## SIG A, ElInt n ) \
  { EL_TRY( BullsHead( *Reinterpret(A), n ) ) } \
  ElError ElBullsHeadDist_ ## SIG ( ElDistMatrix_ ## SIG A, ElInt n ) \
  { EL_TRY( BullsHead( *Reinterpret(A), n ) ) } \
  /* Egorov */ \
  ElError ElEgorov_ ## SIG \
  ( ElMatrix_ ## SIG A, Base<T> (*phase)(ElInt,ElInt), ElInt n ) \
  { try { \
      std::function<Base<T>(Int,Int)> phaseFunc(phase); \
      Egorov( *Reinterpret(A), phaseFunc, n ); \
    } EL_CATCH; return EL_SUCCESS; } \
  ElError ElEgorovDist_ ## SIG \
  ( ElDistMatrix_ ## SIG A, Base<T> (*phase)(ElInt,ElInt), ElInt n ) \
  { try { \
      std::function<Base<T>(Int,Int)> phaseFunc(phase); \
      Egorov( *Reinterpret(A), phaseFunc, n ); \
    } EL_CATCH; return EL_SUCCESS; }

#include "El/macros/CInstantiate.h"

} // extern "C"