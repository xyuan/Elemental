/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"
#include "./util.hpp"

namespace El {
namespace lp {
namespace affine {

// The following solves a pair of linear programs in "affine" conic form:
//
//   min c^T x
//   s.t. A x = b, G x + s = h, s >= 0,
//
//   max -b^T y - h^T z
//   s.t. A^T y + G^T z + c = 0, z >= 0,
//
// as opposed to the more specific "direct" conic form:
//
//   min c^T x
//   s.t. A x = b, x >= 0,
//
//   max -b^T y
//   s.t. A^T y - z + c = 0, z >= 0,  
//
// which corresponds to G = -I and h = 0, using a simple Infeasible Path 
// Following (IPF) scheme. 
//
// NOTE: This routine should only be used for academic purposes, as the 
//       Mehrotra alternative typically requires an order of magnitude fewer 
//       iterations.

template<typename Real>
void IPF
( const Matrix<Real>& APre,
  const Matrix<Real>& GPre,
  const Matrix<Real>& bPre,
  const Matrix<Real>& cPre,
  const Matrix<Real>& hPre,
        Matrix<Real>& x,
        Matrix<Real>& y, 
        Matrix<Real>& z,
        Matrix<Real>& s,
  const IPFCtrl<Real>& ctrl )
{
    DEBUG_ONLY(CSE cse("lp::affine::IPF"))    

    // TODO: Move these into the control structure
    const bool checkResiduals = true;
    const bool standardShift = true;

    // Equilibrate the LP by diagonally scaling [A;G]
    auto A = APre;
    auto G = GPre;
    auto b = bPre;
    auto c = cPre;
    auto h = hPre;
    const Int m = A.Height();
    const Int k = G.Height();
    const Int n = A.Width();
    Matrix<Real> dRowA, dRowG, dCol;
    if( ctrl.outerEquil )
    {
        StackedRuizEquil( A, G, dRowA, dRowG, dCol, ctrl.print );

        DiagonalSolve( LEFT, NORMAL, dRowA, b );
        DiagonalSolve( LEFT, NORMAL, dRowG, h );
        DiagonalSolve( LEFT, NORMAL, dCol,  c );
        if( ctrl.primalInit )
        {
            DiagonalScale( LEFT, NORMAL, dCol,  x );
            DiagonalSolve( LEFT, NORMAL, dRowG, s );
        }
        if( ctrl.dualInit )
        {
            DiagonalScale( LEFT, NORMAL, dRowA, y );
            DiagonalScale( LEFT, NORMAL, dRowG, z );
        }
    }
    else
    {
        Ones( dRowA, m, 1 );
        Ones( dRowG, k, 1 );
        Ones( dCol,  n, 1 );
    }

    const Real bNrm2 = Nrm2( b );
    const Real cNrm2 = Nrm2( c );
    const Real hNrm2 = Nrm2( h );

    Initialize
    ( A, G, b, c, h, x, y, z, s, 
      ctrl.primalInit, ctrl.dualInit, standardShift );

    Real relError = 1;
    Matrix<Real> J, d,
                 rmu, rc, rb, rh,
                 dx, dy, dz, ds;
    Matrix<Real> dxError, dyError, dzError;
    const Int indent = PushIndent();
    for( Int numIts=0; numIts<=ctrl.maxIts; ++numIts )
    {
        // Ensure that s and z are in the cone
        // ===================================
        const Int sNumNonPos = NumNonPositive( s );
        const Int zNumNonPos = NumNonPositive( z );
        if( sNumNonPos > 0 || zNumNonPos > 0 )
            LogicError
            (sNumNonPos," entries of s were nonpositive and ",
             zNumNonPos," entries of z were nonpositive");

        // Compute the duality measure
        // ===========================
        const Real mu = Dot(s,z) / k;

        // Check for convergence
        // =====================
        // |c^T x - (-b^T y - h^T z)| / (1 + |c^T x|) <= tol ?
        // ---------------------------------------------------
        const Real primObj = Dot(c,x);
        const Real dualObj = -Dot(b,y) - Dot(h,z);
        const Real objConv = Abs(primObj-dualObj) / (1+Abs(primObj));
        // || r_b ||_2 / (1 + || b ||_2) <= tol ?
        // --------------------------------------
        rb = b;
        rb *= -1;
        Gemv( NORMAL, Real(1), A, x, Real(1), rb );
        const Real rbNrm2 = Nrm2( rb );
        const Real rbConv = rbNrm2 / (1+bNrm2);
        // || r_c ||_2 / (1 + || c ||_2) <= tol ?
        // --------------------------------------
        rc = c;
        Gemv( TRANSPOSE, Real(1), A, y, Real(1), rc );
        Gemv( TRANSPOSE, Real(1), G, z, Real(1), rc );
        const Real rcNrm2 = Nrm2( rc );
        const Real rcConv = rcNrm2 / (1+cNrm2);
        // || r_h ||_2 / (1 + || h ||_2) <= tol
        // ------------------------------------
        rh = h;
        rh *= -1;
        Gemv( NORMAL, Real(1), G, x, Real(1), rh );
        rh += s;
        const Real rhNrm2 = Nrm2( rh );
        const Real rhConv = rhNrm2 / (1+hNrm2);
        // Now check the pieces
        // --------------------
        relError = Max(Max(Max(objConv,rbConv),rcConv),rhConv);
        if( ctrl.print )
        {
            const Real xNrm2 = Nrm2( x );
            const Real yNrm2 = Nrm2( y );
            const Real zNrm2 = Nrm2( z );
            const Real sNrm2 = Nrm2( s );
            Output
            ("iter ",numIts,":\n",Indent(),
             "  ||  x  ||_2 = ",xNrm2,"\n",Indent(),
             "  ||  y  ||_2 = ",yNrm2,"\n",Indent(),
             "  ||  z  ||_2 = ",zNrm2,"\n",Indent(),
             "  ||  s  ||_2 = ",sNrm2,"\n",Indent(),
             "  || r_b ||_2 = ",rbNrm2,"\n",Indent(),
             "  || r_c ||_2 = ",rcNrm2,"\n",Indent(),
             "  || r_h ||_2 = ",rhNrm2,"\n",Indent(),
             "  || r_b ||_2 / (1 + || b ||_2) = ",rbConv,"\n",Indent(),
             "  || r_c ||_2 / (1 + || c ||_2) = ",rcConv,"\n",Indent(),
             "  || r_h ||_2 / (1 + || h ||_2) = ",rhConv,"\n",Indent(),
             "  primal = ",primObj,"\n",Indent(),
             "  dual   = ",dualObj,"\n",Indent(),
             "  |primal - dual| / (1 + |primal|) = ",objConv);
        }
        if( relError <= ctrl.targetTol )
            break;
        if( numIts == ctrl.maxIts && relError > ctrl.minTol )
            RuntimeError
            ("Maximum number of iterations (",ctrl.maxIts,") exceeded without ",
             "achieving minTol=",ctrl.minTol);

        // Compute the search direction
        // ============================
        // r_mu := s o z - sigma*mu*e
        // --------------------------
        rmu = z;
        DiagonalScale( LEFT, NORMAL, s, rmu );
        Shift( rmu, -ctrl.centering*mu );
        // Construct the KKT system
        // ------------------------
        KKT( A, G, s, z, J );
        KKTRHS( rc, rb, rh, rmu, z, d );
        // Solve for the direction
        // -----------------------
        try { symm_solve::Overwrite( LOWER, NORMAL, J, d ); }
        catch(...)
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
        ExpandSolution( m, n, d, rmu, s, z, dx, dy, dz, ds );

        if( checkResiduals && ctrl.print )
        {
            dxError = rb;
            Gemv( NORMAL, Real(1), A, dx, Real(1), dxError );
            const Real dxErrorNrm2 = Nrm2( dxError );

            dyError = rc;
            Gemv( TRANSPOSE, Real(1), A, dy, Real(1), dyError );
            Gemv( TRANSPOSE, Real(1), G, dz, Real(1), dyError );
            const Real dyErrorNrm2 = Nrm2( dyError );

            dzError = rh;
            Gemv( NORMAL, Real(1), G, dx, Real(1), dzError );
            dzError += ds;
            const Real dzErrorNrm2 = Nrm2( dzError );

            Output
            ("|| dxError ||_2 / (1 + || r_b ||_2) = ",
             dxErrorNrm2/(1+rbNrm2),"\n",Indent(),
             "|| dyError ||_2 / (1 + || r_c ||_2) = ",
             dyErrorNrm2/(1+rcNrm2),"\n",Indent(),
             "|| dzError ||_2 / (1 + || r_h ||_2) = ",
             dzErrorNrm2/(1+rhNrm2));
        }

        // Take a step in the computed direction
        // =====================================
        const Real alphaPrimal = MaxStepInPositiveCone( s, ds, Real(1) );
        const Real alphaDual = MaxStepInPositiveCone( z, dz, Real(1) );
        const Real alphaMax = Min(alphaPrimal,alphaDual);
        if( ctrl.print )
            Output("alphaMax = ",alphaMax);
        const Real alpha =
          IPFLineSearch
          ( A, G, b, c, h, x, y, z, s, dx, dy, dz, ds,
            Real(0.99)*alphaMax,
            ctrl.targetTol*(1+bNrm2), 
            ctrl.targetTol*(1+cNrm2), 
            ctrl.targetTol*(1+hNrm2),
            ctrl.lineSearchCtrl );
        if( ctrl.print )
            Output("alpha = ",alpha);
        Axpy( alpha, dx, x );
        Axpy( alpha, dy, y );
        Axpy( alpha, dz, z );
        Axpy( alpha, ds, s );
        if( alpha == Real(0) )
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
    }
    SetIndent( indent );

    if( ctrl.outerEquil )
    {
        DiagonalSolve( LEFT, NORMAL, dCol,  x );
        DiagonalSolve( LEFT, NORMAL, dRowA, y );
        DiagonalSolve( LEFT, NORMAL, dRowG, z );
        DiagonalScale( LEFT, NORMAL, dRowG, s );
    }
}

template<typename Real>
void IPF
( const AbstractDistMatrix<Real>& APre,
  const AbstractDistMatrix<Real>& GPre,
  const AbstractDistMatrix<Real>& bPre,
  const AbstractDistMatrix<Real>& cPre,
  const AbstractDistMatrix<Real>& hPre,
        AbstractDistMatrix<Real>& xPre,
        AbstractDistMatrix<Real>& yPre, 
        AbstractDistMatrix<Real>& zPre,
        AbstractDistMatrix<Real>& sPre,
  const IPFCtrl<Real>& ctrl )
{
    DEBUG_ONLY(CSE cse("lp::affine::IPF"))    

    // TODO: Move these into the control structure
    const bool checkResiduals = true;
    const bool standardShift = true;

    const Grid& grid = APre.Grid();
    const int commRank = grid.Rank();

    // Ensure that the inputs have the appropriate read/write properties
    DistMatrix<Real> A(grid), G(grid), b(grid), c(grid), h(grid);
    A.Align(0,0);
    G.Align(0,0);
    b.Align(0,0);
    c.Align(0,0);
    A = APre;
    G = GPre;
    b = bPre;
    c = cPre;
    h = hPre;
    ProxyCtrl control;
    control.colConstrain = true;
    control.rowConstrain = true;
    control.colAlign = 0;
    control.rowAlign = 0;
    // NOTE: {x,s} do not need to be read proxies when !ctrl.primalInit
    auto xPtr = ReadWriteProxy<Real,MC,MR>(&xPre,control); auto& x = *xPtr;
    auto sPtr = ReadWriteProxy<Real,MC,MR>(&sPre,control); auto& s = *sPtr;
    // NOTE: {y,z} do not need to be read proxies when !ctrl.dualInit
    auto yPtr = ReadWriteProxy<Real,MC,MR>(&yPre,control); auto& y = *yPtr;
    auto zPtr = ReadWriteProxy<Real,MC,MR>(&zPre,control); auto& z = *zPtr;

    // Equilibrate the LP by diagonally scaling [A;G]
    const Int m = A.Height();
    const Int k = G.Height();
    const Int n = A.Width();
    DistMatrix<Real,MC,STAR> dRowA(grid),
                             dRowG(grid);
    DistMatrix<Real,MR,STAR> dCol(grid);
    if( ctrl.outerEquil )
    {
        StackedRuizEquil( A, G, dRowA, dRowG, dCol, ctrl.print );

        DiagonalSolve( LEFT, NORMAL, dRowA, b );
        DiagonalSolve( LEFT, NORMAL, dRowG, h );
        DiagonalSolve( LEFT, NORMAL, dCol,  c );
        if( ctrl.primalInit )
        {
            DiagonalScale( LEFT, NORMAL, dCol,  x );
            DiagonalSolve( LEFT, NORMAL, dRowG, s );
        }
        if( ctrl.dualInit )
        {
            DiagonalScale( LEFT, NORMAL, dRowA, y );
            DiagonalScale( LEFT, NORMAL, dRowG, z );
        }
    }
    else
    {
        Ones( dRowA, m, 1 );
        Ones( dRowG, k, 1 );
        Ones( dCol,  n, 1 );
    }

    const Real bNrm2 = Nrm2( b );
    const Real cNrm2 = Nrm2( c );
    const Real hNrm2 = Nrm2( h );

    Initialize
    ( A, G, b, c, h, x, y, z, s, 
      ctrl.primalInit, ctrl.dualInit, standardShift );

    Real relError = 1;
    DistMatrix<Real> J(grid), d(grid), 
                     rc(grid), rb(grid), rh(grid), rmu(grid),
                     dx(grid), dy(grid), dz(grid), ds(grid);
    ds.AlignWith( s );
    dz.AlignWith( s );
    rmu.AlignWith( s );
    DistMatrix<Real> dxError(grid), dyError(grid), dzError(grid);
    dzError.AlignWith( s );
    const Int indent = PushIndent();
    for( Int numIts=0; numIts<=ctrl.maxIts; ++numIts )
    {
        // Ensure that s and z are in the cone
        // ===================================
        const Int sNumNonPos = NumNonPositive( s );
        const Int zNumNonPos = NumNonPositive( z );
        if( sNumNonPos > 0 || zNumNonPos > 0 )
            LogicError
            (sNumNonPos," entries of s were nonpositive and ",
             zNumNonPos," entries of z were nonpositive");

        // Compute the duality measure
        // ===========================
        const Real mu = Dot(s,z) / k;

        // Check for convergence
        // =====================
        // |c^T x - (-b^T y - h^T z)| / (1 + |c^T x|) <= tol ?
        // ---------------------------------------------------
        const Real primObj = Dot(c,x);
        const Real dualObj = -Dot(b,y) - Dot(h,z);
        const Real objConv = Abs(primObj-dualObj) / (1+Abs(primObj));
        // || r_b ||_2 / (1 + || b ||_2) <= tol ?
        // --------------------------------------
        rb = b;
        rb *= -1;
        Gemv( NORMAL, Real(1), A, x, Real(1), rb );
        const Real rbNrm2 = Nrm2( rb );
        const Real rbConv = rbNrm2 / (1+bNrm2);
        // || r_c ||_2 / (1 + || c ||_2) <= tol ?
        // --------------------------------------
        rc = c;
        Gemv( TRANSPOSE, Real(1), A, y, Real(1), rc );
        Gemv( TRANSPOSE, Real(1), G, z, Real(1), rc );
        const Real rcNrm2 = Nrm2( rc );
        const Real rcConv = rcNrm2 / (1+cNrm2);
        // || r_h ||_2 / (1 + || h ||_2) <= tol
        // ------------------------------------
        rh = h;
        rh *= -1;
        Gemv( NORMAL, Real(1), G, x, Real(1), rh );
        rh += s;
        const Real rhNrm2 = Nrm2( rh );
        const Real rhConv = rhNrm2 / (1+hNrm2);
        // Now check the pieces
        // --------------------
        relError = Max(Max(Max(objConv,rbConv),rcConv),rhConv);
        if( ctrl.print )
        {
            const Real xNrm2 = Nrm2( x );
            const Real yNrm2 = Nrm2( y );
            const Real zNrm2 = Nrm2( z );
            const Real sNrm2 = Nrm2( s );
            if( commRank == 0 )
                Output
                ("iter ",numIts,":\n",Indent(),
                 "  ||  x  ||_2 = ",xNrm2,"\n",Indent(),
                 "  ||  y  ||_2 = ",yNrm2,"\n",Indent(),
                 "  ||  z  ||_2 = ",zNrm2,"\n",Indent(),
                 "  ||  s  ||_2 = ",sNrm2,"\n",Indent(),
                 "  || r_b ||_2 = ",rbNrm2,"\n",Indent(),
                 "  || r_c ||_2 = ",rcNrm2,"\n",Indent(),
                 "  || r_h ||_2 = ",rhNrm2,"\n",Indent(),
                 "  || r_b ||_2 / (1 + || b ||_2) = ",rbConv,"\n",Indent(),
                 "  || r_c ||_2 / (1 + || c ||_2) = ",rcConv,"\n",Indent(),
                 "  || r_h ||_2 / (1 + || h ||_2) = ",rhConv,"\n",Indent(),
                 "  primal = ",primObj,"\n",Indent(),
                 "  dual   = ",dualObj,"\n",Indent(),
                 "  |primal - dual| / (1 + |primal|) = ",objConv);
        }
        if( relError <= ctrl.targetTol )
            break;
        if( numIts == ctrl.maxIts && relError > ctrl.minTol )
            RuntimeError
            ("Maximum number of iterations (",ctrl.maxIts,") exceeded without ",
             "achieving minTol=",ctrl.minTol);

        // Compute the search direction
        // ============================

        // r_mu := s o z - sigma*mu*e
        // --------------------------
        rmu = z;
        DiagonalScale( LEFT, NORMAL, s, rmu );
        Shift( rmu, -ctrl.centering*mu );

        // Construct the KKT system
        // ------------------------
        KKT( A, G, s, z, J );
        KKTRHS( rc, rb, rh, rmu, z, d );

        // Solve for the direction
        // -----------------------
        try { symm_solve::Overwrite( LOWER, NORMAL, J, d ); }
        catch(...)
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
        ExpandSolution( m, n, d, rmu, s, z, dx, dy, dz, ds );

        if( checkResiduals && ctrl.print )
        {
            dxError = rb;
            Gemv( NORMAL, Real(1), A, dx, Real(1), dxError );
            const Real dxErrorNrm2 = Nrm2( dxError );

            dyError = rc;
            Gemv( TRANSPOSE, Real(1), A, dy, Real(1), dyError );
            Gemv( TRANSPOSE, Real(1), G, dz, Real(1), dyError );
            const Real dyErrorNrm2 = Nrm2( dyError );

            dzError = rh;
            Gemv( NORMAL, Real(1), G, dx, Real(1), dzError );
            dzError += ds;
            const Real dzErrorNrm2 = Nrm2( dzError );

            // TODO: dmuError

            if( commRank == 0 )
                Output
                ("|| dxError ||_2 / (1 + || r_b ||_2) = ",
                 dxErrorNrm2/(1+rbNrm2),"\n",Indent(),
                 "|| dyError ||_2 / (1 + || r_c ||_2) = ",
                 dyErrorNrm2/(1+rcNrm2),"\n",Indent(),
                 "|| dzError ||_2 / (1 + || r_h ||_2) = ",
                 dzErrorNrm2/(1+rhNrm2));
        }

        // Take a step in the computed direction
        // =====================================
        const Real alphaPrimal = MaxStepInPositiveCone( s, ds, Real(1) );
        const Real alphaDual = MaxStepInPositiveCone( z, dz, Real(1) );
        const Real alphaMax = Min(alphaPrimal,alphaDual);
        if( ctrl.print && commRank == 0 )
            Output("alphaMax = ",alphaMax);
        const Real alpha =
          IPFLineSearch
          ( A, G, b, c, h, x, y, z, s, dx, dy, dz, ds,
            Real(0.99)*alphaMax,
            ctrl.targetTol*(1+bNrm2), 
            ctrl.targetTol*(1+cNrm2), 
            ctrl.targetTol*(1+hNrm2),
            ctrl.lineSearchCtrl );
        if( ctrl.print && commRank == 0 )
            Output("alpha = ",alpha);
        Axpy( alpha, dx, x );
        Axpy( alpha, dy, y );
        Axpy( alpha, dz, z );
        Axpy( alpha, ds, s );
        if( alpha == Real(0) )
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
    }
    SetIndent( indent );

    if( ctrl.outerEquil )
    {
        DiagonalSolve( LEFT, NORMAL, dCol,  x );
        DiagonalSolve( LEFT, NORMAL, dRowA, y );
        DiagonalSolve( LEFT, NORMAL, dRowG, z );
        DiagonalScale( LEFT, NORMAL, dRowG, s );
    }
}

template<typename Real>
void IPF
( const SparseMatrix<Real>& APre,
  const SparseMatrix<Real>& GPre,
  const Matrix<Real>& bPre,
  const Matrix<Real>& cPre,
  const Matrix<Real>& hPre,
        Matrix<Real>& x,
        Matrix<Real>& y, 
        Matrix<Real>& z,
        Matrix<Real>& s,
  const IPFCtrl<Real>& ctrl )
{
    DEBUG_ONLY(CSE cse("lp::affine::IPF"))    
    const Real eps = Epsilon<Real>();

    // TODO: Move these into the control structure
    const bool checkResiduals = true;
    const bool standardShift = true;
    // Sizes of || w ||_max which force levels of equilibration
    const Real diagEquilTol = Pow(eps,Real(-0.15));
    const Real ruizEquilTol = Pow(eps,Real(-0.25));

    // Equilibrate the LP by diagonally scaling [A;G]
    auto A = APre;
    auto G = GPre;
    auto b = bPre;
    auto c = cPre;
    auto h = hPre;
    const Int m = A.Height();
    const Int k = G.Height();
    const Int n = A.Width();
    Matrix<Real> dRowA, dRowG, dCol;
    if( ctrl.outerEquil )
    {
        StackedRuizEquil( A, G, dRowA, dRowG, dCol, ctrl.print );

        DiagonalSolve( LEFT, NORMAL, dRowA, b );
        DiagonalSolve( LEFT, NORMAL, dRowG, h );
        DiagonalSolve( LEFT, NORMAL, dCol,  c );
        if( ctrl.primalInit )
        {
            DiagonalScale( LEFT, NORMAL, dCol,  x );
            DiagonalSolve( LEFT, NORMAL, dRowG, s );
        }
        if( ctrl.dualInit )
        {
            DiagonalScale( LEFT, NORMAL, dRowA, y );
            DiagonalScale( LEFT, NORMAL, dRowG, z );
        }
    }
    else
    {
        Ones( dRowA, m, 1 );
        Ones( dRowG, k, 1 );
        Ones( dCol,  n, 1 );
    }

    const Real bNrm2 = Nrm2( b );
    const Real cNrm2 = Nrm2( c );
    const Real hNrm2 = Nrm2( h );
    const Real twoNormEstA = TwoNormEstimate( A, ctrl.basisSize );
    const Real twoNormEstG = TwoNormEstimate( G, ctrl.basisSize );
    const Real origTwoNormEst = twoNormEstA + twoNormEstG + 1;
    if( ctrl.print )
        Output
        ("|| A ||_2 estimate: ",twoNormEstA,"\n",Indent(),
         "|| G ||_2 estimate: ",twoNormEstG,"\n",Indent());

    vector<Int> map, invMap;
    ldl::NodeInfo info;
    ldl::Separator rootSep;
    Initialize
    ( A, G, b, c, h, x, y, z, s, map, invMap, rootSep, info, 
      ctrl.primalInit, ctrl.dualInit, standardShift, ctrl.qsdCtrl );

    Matrix<Real> regTmp, regPerm;
    regTmp.Resize( m+n+k, 1 );
    regPerm.Resize( m+n+k, 1 );
    for( Int i=0; i<m+n+k; ++i )
    {
        if( i < n )
        {
            regTmp.Set( i, 0, ctrl.qsdCtrl.regPrimal );
            regPerm.Set( i, 0, 10*eps );
        }
        else
        {
            regTmp.Set( i, 0, -ctrl.qsdCtrl.regDual );
            regPerm.Set( i, 0, -10*eps );
        }
    }
    Scale( origTwoNormEst, regTmp );
    Scale( origTwoNormEst, regPerm );

    // Construct the static portion of the KKT system
    // ==============================================
    SparseMatrix<Real> JStatic;
    StaticKKT( A, G, regPerm, JStatic, false );
    JStatic.FreezeSparsity();
    if( ctrl.primalInit && ctrl.dualInit )
    {
        NestedDissection( JStatic.LockedGraph(), map, rootSep, info );
        InvertMap( map, invMap );
    }

    SparseMatrix<Real> J, JOrig;
    ldl::Front<Real> JFront;
    Matrix<Real> d,
                 w,
                 rc, rb, rh, rmu,
                 dx, dy, dz, ds;

    Real relError = 1;
    Matrix<Real> dInner;
    Matrix<Real> dxError, dyError, dzError;
    const Int indent = PushIndent();
    for( Int numIts=0; numIts<=ctrl.maxIts; ++numIts )
    {
        // Ensure that s and z are in the cone
        // ===================================
        const Int sNumNonPos = NumNonPositive( s );
        const Int zNumNonPos = NumNonPositive( z );
        if( sNumNonPos > 0 || zNumNonPos > 0 )
            LogicError
            (sNumNonPos," entries of s were nonpositive and ",
             zNumNonPos," entries of z were nonpositive");

        // Compute the duality measure and scaling point
        // =============================================
        const Real mu = Dot(s,z) / k;
        PositiveNesterovTodd( s, z, w );
        const Real wMaxNorm = MaxNorm( w );

        // Check for convergence
        // =====================
        // |c^T x - (-b^T y - h^T z)| / (1 + |c^T x|) <= tol ?
        // ---------------------------------------------------
        const Real primObj = Dot(c,x);
        const Real dualObj = -Dot(b,y) - Dot(h,z);
        const Real objConv = Abs(primObj-dualObj) / (1+Abs(primObj));
        // || r_b ||_2 / (1 + || b ||_2) <= tol ?
        // --------------------------------------
        rb = b;
        rb *= -1;
        Multiply( NORMAL, Real(1), A, x, Real(1), rb );
        const Real rbNrm2 = Nrm2( rb );
        const Real rbConv = rbNrm2 / (1+bNrm2);
        // || r_c ||_2 / (1 + || c ||_2) <= tol ?
        // --------------------------------------
        rc = c;
        Multiply( TRANSPOSE, Real(1), A, y, Real(1), rc );
        Multiply( TRANSPOSE, Real(1), G, z, Real(1), rc );
        const Real rcNrm2 = Nrm2( rc );
        const Real rcConv = rcNrm2 / (1+cNrm2);
        // || r_h ||_2 / (1 + || h ||_2) <= tol
        // ------------------------------------
        rh = h;
        rh *= -1;
        Multiply( NORMAL, Real(1), G, x, Real(1), rh );
        rh += s;
        const Real rhNrm2 = Nrm2( rh );
        const Real rhConv = rhNrm2 / (1+hNrm2);
        // Now check the pieces
        // --------------------
        relError = Max(Max(Max(objConv,rbConv),rcConv),rhConv);
        if( ctrl.print )
        {
            const Real xNrm2 = Nrm2( x );
            const Real yNrm2 = Nrm2( y );
            const Real zNrm2 = Nrm2( z );
            const Real sNrm2 = Nrm2( s );
            Output
            ("iter ",numIts,":\n",Indent(),
             "  ||  x  ||_2 = ",xNrm2,"\n",Indent(),
             "  ||  y  ||_2 = ",yNrm2,"\n",Indent(),
             "  ||  z  ||_2 = ",zNrm2,"\n",Indent(),
             "  ||  s  ||_2 = ",sNrm2,"\n",Indent(),
             "  || r_b ||_2 = ",rbNrm2,"\n",Indent(),
             "  || r_c ||_2 = ",rcNrm2,"\n",Indent(),
             "  || r_h ||_2 = ",rhNrm2,"\n",Indent(),
             "  || r_b ||_2 / (1 + || b ||_2) = ",rbConv,"\n",Indent(),
             "  || r_c ||_2 / (1 + || c ||_2) = ",rcConv,"\n",Indent(),
             "  || r_h ||_2 / (1 + || h ||_2) = ",rhConv,"\n",Indent(),
             "  primal = ",primObj,"\n",Indent(),
             "  dual   = ",dualObj,"\n",Indent(),
             "  |primal - dual| / (1 + |primal|) = ",objConv);
        }
        if( relError <= ctrl.targetTol )
            break;
        if( numIts == ctrl.maxIts && relError > ctrl.minTol )
            RuntimeError
            ("Maximum number of iterations (",ctrl.maxIts,") exceeded without ",
             "achieving minTol=",ctrl.minTol);

        // Compute the search direction
        // ============================

        // r_mu := s o z - sigma*mu*e
        // --------------------------
        rmu = z;
        DiagonalScale( LEFT, NORMAL, s, rmu );
        Shift( rmu, -ctrl.centering*mu );

        // Construct the KKT system
        // ------------------------
        JOrig = JStatic;
        JOrig.FreezeSparsity();
        FinishKKT( m, n, s, z, JOrig );
        J = JOrig;
        J.FreezeSparsity();
        UpdateRealPartOfDiagonal( J, Real(1), regTmp );

        if( wMaxNorm >= ruizEquilTol )
            SymmetricRuizEquil( J, dInner, ctrl.print );
        else if( wMaxNorm >= diagEquilTol )
            SymmetricDiagonalEquil( J, dInner, ctrl.print );
        else
            Ones( dInner, J.Height(), 1 );

        JFront.Pull( J, map, info );
        KKTRHS( rc, rb, rh, rmu, z, d );

        // Solve for the direction
        // -----------------------
        try
        {
            LDL( info, JFront, LDL_2D );
            reg_qsd_ldl::SolveAfter
            ( JOrig, regTmp, dInner, invMap, info, JFront, d, ctrl.qsdCtrl );
        }
        catch(...)
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
        ExpandSolution( m, n, d, rmu, s, z, dx, dy, dz, ds );

        if( checkResiduals && ctrl.print )
        {
            dxError = rb;
            Multiply( NORMAL, Real(1), A, dx, Real(1), dxError );
            const Real dxErrorNrm2 = Nrm2( dxError );

            dyError = rc;
            Multiply( TRANSPOSE, Real(1), A, dy, Real(1), dyError );
            Multiply( TRANSPOSE, Real(1), G, dz, Real(1), dyError );
            const Real dyErrorNrm2 = Nrm2( dyError );

            dzError = rh;
            Multiply( NORMAL, Real(1), G, dx, Real(1), dzError );
            dzError += ds;
            const Real dzErrorNrm2 = Nrm2( dzError );

            // TODO: dmuError
            // TODO: Also compute and print the residuals with regularization

            Output
            ("|| dxError ||_2 / (1 + || r_b ||_2) = ",
             dxErrorNrm2/(1+rbNrm2),"\n",Indent(),
             "|| dyError ||_2 / (1 + || r_c ||_2) = ",
             dyErrorNrm2/(1+rcNrm2),"\n",Indent(),
             "|| dzError ||_2 / (1 + || r_h ||_2) = ",
             dzErrorNrm2/(1+rhNrm2));
        }

        // Take a step in the computed direction
        // =====================================
        const Real alphaPrimal = MaxStepInPositiveCone( s, ds, Real(1) );
        const Real alphaDual = MaxStepInPositiveCone( z, dz, Real(1) );
        const Real alphaMax = Min(alphaPrimal,alphaDual);
        if( ctrl.print )
            Output("alphaMax = ",alphaMax);
        const Real alpha =
          IPFLineSearch
          ( A, G, b, c, h, x, y, z, s, dx, dy, dz, ds,
            Real(0.99)*alphaMax,
            ctrl.targetTol*(1+bNrm2), 
            ctrl.targetTol*(1+cNrm2), 
            ctrl.targetTol*(1+hNrm2),
            ctrl.lineSearchCtrl );
        if( ctrl.print )
            Output("alpha = ",alpha);
        Axpy( alpha, dx, x );
        Axpy( alpha, dy, y );
        Axpy( alpha, dz, z );
        Axpy( alpha, ds, s );
        if( alpha == Real(0) )
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
    }
    SetIndent( indent );

    if( ctrl.outerEquil )
    {
        DiagonalSolve( LEFT, NORMAL, dCol,  x );
        DiagonalSolve( LEFT, NORMAL, dRowA, y );
        DiagonalSolve( LEFT, NORMAL, dRowG, z );
        DiagonalScale( LEFT, NORMAL, dRowG, s );
    }
}

template<typename Real>
void IPF
( const DistSparseMatrix<Real>& APre,
  const DistSparseMatrix<Real>& GPre,
  const DistMultiVec<Real>& bPre,
  const DistMultiVec<Real>& cPre,
  const DistMultiVec<Real>& hPre,
        DistMultiVec<Real>& x,
        DistMultiVec<Real>& y, 
        DistMultiVec<Real>& z,
        DistMultiVec<Real>& s,
  const IPFCtrl<Real>& ctrl )
{
    DEBUG_ONLY(CSE cse("lp::affine::IPF"))    
    const Real eps = Epsilon<Real>();

    // TODO: Move these into the control structure
    const bool checkResiduals = true;
    const bool standardShift = true;
    // Sizes of || w ||_max which force levels of equilibration
    const Real diagEquilTol = Pow(eps,Real(-0.15));
    const Real ruizEquilTol = Pow(eps,Real(-0.25));

    mpi::Comm comm = APre.Comm();
    const int commRank = mpi::Rank(comm);

    // Equilibrate the LP by diagonally scaling [A;G]
    auto A = APre;
    auto G = GPre;
    auto b = bPre;
    auto h = hPre;
    auto c = cPre;
    const Int m = A.Height();
    const Int k = G.Height();
    const Int n = A.Width();
    DistMultiVec<Real> dRowA(comm), dRowG(comm), dCol(comm);
    if( ctrl.outerEquil ) 
    {
        StackedRuizEquil( A, G, dRowA, dRowG, dCol, ctrl.print );

        DiagonalSolve( LEFT, NORMAL, dRowA, b );
        DiagonalSolve( LEFT, NORMAL, dRowG, h );
        DiagonalSolve( LEFT, NORMAL, dCol,  c );
        if( ctrl.primalInit )
        {
            DiagonalScale( LEFT, NORMAL, dCol,  x );
            DiagonalSolve( LEFT, NORMAL, dRowG, s );
        }
        if( ctrl.dualInit )
        {
            DiagonalScale( LEFT, NORMAL, dRowA, y );
            DiagonalScale( LEFT, NORMAL, dRowG, z );
        }
    }
    else
    {
        Ones( dRowA, m, 1 );
        Ones( dRowG, k, 1 );
        Ones( dCol,  n, 1 );
    }

    const Real bNrm2 = Nrm2( b );
    const Real cNrm2 = Nrm2( c );
    const Real hNrm2 = Nrm2( h );
    const Real twoNormEstA = TwoNormEstimate( A, ctrl.basisSize );
    const Real twoNormEstG = TwoNormEstimate( G, ctrl.basisSize );
    const Real origTwoNormEst = twoNormEstA + twoNormEstG + 1;
    if( ctrl.print && commRank == 0 )
        Output
        ("|| A ||_2 estimate: ",twoNormEstA,"\n",Indent(),
         "|| G ||_2 estimate: ",twoNormEstG);

    DistMap map, invMap;
    ldl::DistNodeInfo info;
    ldl::DistSeparator rootSep;
    Initialize
    ( A, G, b, c, h, x, y, z, s, map, invMap, rootSep, info, 
      ctrl.primalInit, ctrl.dualInit, standardShift, ctrl.qsdCtrl );

    DistMultiVec<Real> regTmp(comm), regPerm(comm);
    regTmp.Resize( m+n+k, 1 );
    regPerm.Resize( m+n+k, 1 );
    for( Int iLoc=0; iLoc<regTmp.LocalHeight(); ++iLoc )
    {
        const Int i = regTmp.GlobalRow(iLoc);
        if( i < n )
        {
            regTmp.SetLocal( iLoc, 0, ctrl.qsdCtrl.regPrimal );
            regPerm.SetLocal( iLoc, 0, 10*eps );
        }
        else
        {
            regTmp.SetLocal( iLoc, 0, -ctrl.qsdCtrl.regDual );
            regPerm.SetLocal( iLoc, 0, -10*eps );
        }
    }
    Scale( origTwoNormEst, regTmp );
    Scale( origTwoNormEst, regPerm );

    // Construct the static portion of the KKT system
    // ==============================================
    DistSparseMatrix<Real> JStatic(comm);
    StaticKKT( A, G, regPerm, JStatic, false );
    JStatic.FreezeSparsity();
    if( ctrl.primalInit && ctrl.dualInit ) 
    {
        NestedDissection( JStatic.LockedDistGraph(), map, rootSep, info );
        InvertMap( map, invMap );
    }
    auto meta = JStatic.InitializeMultMeta();

    DistSparseMatrix<Real> J(comm), JOrig(comm);
    ldl::DistFront<Real> JFront;
    DistMultiVec<Real> d(comm),
                       w(comm),
                       rc(comm), rb(comm), rh(comm), rmu(comm),
                       dx(comm), dy(comm), dz(comm), ds(comm);

    Real relError = 1;
    DistMultiVec<Real> dInner(comm);
    DistMultiVec<Real> dxError(comm), dyError(comm), dzError(comm);
    const Int indent = PushIndent();
    for( Int numIts=0; numIts<=ctrl.maxIts; ++numIts )
    {
        // Ensure that s and z are in the cone
        // ===================================
        const Int sNumNonPos = NumNonPositive( s );
        const Int zNumNonPos = NumNonPositive( z );
        if( sNumNonPos > 0 || zNumNonPos > 0 )
            LogicError
            (sNumNonPos," entries of s were nonpositive and ",
             zNumNonPos," entries of z were nonpositive");

        // Compute the duality measure and scaling point
        // =============================================
        const Real mu = Dot(s,z) / k;
        PositiveNesterovTodd( s, z, w );
        const Real wMaxNorm = MaxNorm( w );

        // Check for convergence
        // =====================
        // |c^T x - (-b^T y - h^T z)| / (1 + |c^T x|) <= tol ?
        // ---------------------------------------------------
        const Real primObj = Dot(c,x);
        const Real dualObj = -Dot(b,y) - Dot(h,z);
        const Real objConv = Abs(primObj-dualObj) / (1+Abs(primObj));
        // || r_b ||_2 / (1 + || b ||_2) <= tol ?
        // --------------------------------------
        rb = b;
        rb *= -1;
        Multiply( NORMAL, Real(1), A, x, Real(1), rb );
        const Real rbNrm2 = Nrm2( rb );
        const Real rbConv = rbNrm2 / (1+bNrm2);
        // || r_c ||_2 / (1 + || c ||_2) <= tol ?
        // --------------------------------------
        rc = c;
        Multiply( TRANSPOSE, Real(1), A, y, Real(1), rc );
        Multiply( TRANSPOSE, Real(1), G, z, Real(1), rc );
        const Real rcNrm2 = Nrm2( rc );
        const Real rcConv = rcNrm2 / (1+cNrm2);
        // || r_h ||_2 / (1 + || h ||_2) <= tol
        // ------------------------------------
        rh = h;
        rh *= -1;
        Multiply( NORMAL, Real(1), G, x, Real(1), rh );
        rh += s;
        const Real rhNrm2 = Nrm2( rh );
        const Real rhConv = rhNrm2 / (1+hNrm2);
        // Now check the pieces
        // --------------------
        relError = Max(Max(Max(objConv,rbConv),rcConv),rhConv);
        if( ctrl.print )
        {
            const Real xNrm2 = Nrm2( x );
            const Real yNrm2 = Nrm2( y );
            const Real zNrm2 = Nrm2( z );
            const Real sNrm2 = Nrm2( s );
            if( commRank == 0 )
                Output
                ("iter ",numIts,":\n",Indent(),
                 "  ||  x  ||_2 = ",xNrm2,"\n",Indent(),
                 "  ||  y  ||_2 = ",yNrm2,"\n",Indent(),
                 "  ||  z  ||_2 = ",zNrm2,"\n",Indent(),
                 "  ||  s  ||_2 = ",sNrm2,"\n",Indent(),
                 "  || r_b ||_2 = ",rbNrm2,"\n",Indent(),
                 "  || r_c ||_2 = ",rcNrm2,"\n",Indent(),
                 "  || r_h ||_2 = ",rhNrm2,"\n",Indent(),
                 "  || r_b ||_2 / (1 + || b ||_2) = ",rbConv,"\n",Indent(),
                 "  || r_c ||_2 / (1 + || c ||_2) = ",rcConv,"\n",Indent(),
                 "  || r_h ||_2 / (1 + || h ||_2) = ",rhConv,"\n",Indent(),
                 "  primal = ",primObj,"\n",Indent(),
                 "  dual   = ",dualObj,"\n",Indent(),
                 "  |primal - dual| / (1 + |primal|) = ",objConv);
        }
        if( relError <= ctrl.targetTol )
            break;
        if( numIts == ctrl.maxIts && relError > ctrl.minTol )
            RuntimeError
            ("Maximum number of iterations (",ctrl.maxIts,") exceeded without ",
             "achieving minTol=",ctrl.minTol);

        // Compute the search direction
        // ============================

        // r_mu := s o z - sigma*mu*e
        // --------------------------
        rmu = z;
        DiagonalScale( LEFT, NORMAL, s, rmu );
        Shift( rmu, -ctrl.centering*mu );

        // Construct the KKT system
        // ------------------------
        JOrig = JStatic;
        JOrig.FreezeSparsity();
        FinishKKT( m, n, s, z, JOrig );
        JOrig.multMeta = meta;
        J = JOrig;
        J.FreezeSparsity();
        J.multMeta = meta;
        UpdateRealPartOfDiagonal( J, Real(1), regTmp );

        if( wMaxNorm >= ruizEquilTol )
            SymmetricRuizEquil( J, dInner, ctrl.print );
        else if( wMaxNorm >= diagEquilTol )
            SymmetricDiagonalEquil( J, dInner, ctrl.print );
        else
            Ones( dInner, J.Height(), 1 );

        JFront.Pull( J, map, rootSep, info );
        KKTRHS( rc, rb, rh, rmu, z, d );

        // Compute the search direction
        // ----------------------------
        try
        {
            LDL( info, JFront, LDL_2D );
            reg_qsd_ldl::SolveAfter
            ( JOrig, regTmp, dInner, invMap, info, JFront, d, ctrl.qsdCtrl );
        }
        catch(...)
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance ",ctrl.minTol);
        }
        ExpandSolution( m, n, d, rmu, s, z, dx, dy, dz, ds );

        if( checkResiduals && ctrl.print )
        {
            dxError = rb;
            Multiply( NORMAL, Real(1), A, dx, Real(1), dxError );
            const Real dxErrorNrm2 = Nrm2( dxError );

            dyError = rc;
            Multiply( TRANSPOSE, Real(1), A, dy, Real(1), dyError );
            Multiply( TRANSPOSE, Real(1), G, dz, Real(1), dyError );
            const Real dyErrorNrm2 = Nrm2( dyError );

            dzError = rh;
            Multiply( NORMAL, Real(1), G, dx, Real(1), dzError );
            dzError += ds;
            const Real dzErrorNrm2 = Nrm2( dzError );

            // TODO: dmuError
            // TODO: Also compute and print the residuals with regularization

            if( commRank == 0 )
                Output
                ("|| dxError ||_2 / (1 + || r_b ||_2) = ",
                 dxErrorNrm2/(1+rbNrm2),"\n",Indent(),
                 "|| dyError ||_2 / (1 + || r_c ||_2) = ",
                 dyErrorNrm2/(1+rcNrm2),"\n",Indent(),
                 "|| dzError ||_2 / (1 + || r_h ||_2) = ",
                 dzErrorNrm2/(1+rhNrm2));
        }

        // Take a step in the computed direction
        // =====================================
        const Real alphaPrimal = MaxStepInPositiveCone( s, ds, Real(1) );
        const Real alphaDual = MaxStepInPositiveCone( z, dz, Real(1) );
        const Real alphaMax = Min(alphaPrimal,alphaDual);
        if( ctrl.print && commRank == 0 )
            Output("alphaMax = ",alphaMax);
        const Real alpha =
          IPFLineSearch
          ( A, G, b, c, h, x, y, z, s, dx, dy, dz, ds,
            Real(0.99)*alphaMax,
            ctrl.targetTol*(1+bNrm2), 
            ctrl.targetTol*(1+cNrm2), 
            ctrl.targetTol*(1+hNrm2),
            ctrl.lineSearchCtrl );
        if( ctrl.print && commRank == 0 )
            Output("alpha = ",alpha);
        Axpy( alpha, dx, x );
        Axpy( alpha, dy, y );
        Axpy( alpha, dz, z );
        Axpy( alpha, ds, s );
        if( alpha == Real(0) )
        {
            if( relError <= ctrl.minTol )
                break;
            else
                RuntimeError
                ("Could not achieve minimum tolerance of ",ctrl.minTol);
        }
    }
    SetIndent( indent );

    if( ctrl.outerEquil )
    {
        DiagonalSolve( LEFT, NORMAL, dCol,  x );
        DiagonalSolve( LEFT, NORMAL, dRowA, y );
        DiagonalSolve( LEFT, NORMAL, dRowG, z );
        DiagonalScale( LEFT, NORMAL, dRowG, s );
    }
}

#define PROTO(Real) \
  template void IPF \
  ( const Matrix<Real>& A, \
    const Matrix<Real>& G, \
    const Matrix<Real>& b, \
    const Matrix<Real>& c, \
    const Matrix<Real>& h, \
          Matrix<Real>& x, \
          Matrix<Real>& y, \
          Matrix<Real>& z, \
          Matrix<Real>& s, \
    const IPFCtrl<Real>& ctrl ); \
  template void IPF \
  ( const AbstractDistMatrix<Real>& A, \
    const AbstractDistMatrix<Real>& G, \
    const AbstractDistMatrix<Real>& b, \
    const AbstractDistMatrix<Real>& c, \
    const AbstractDistMatrix<Real>& h, \
          AbstractDistMatrix<Real>& x, \
          AbstractDistMatrix<Real>& y, \
          AbstractDistMatrix<Real>& z, \
          AbstractDistMatrix<Real>& s, \
    const IPFCtrl<Real>& ctrl ); \
  template void IPF \
  ( const SparseMatrix<Real>& A, \
    const SparseMatrix<Real>& G, \
    const Matrix<Real>& b, \
    const Matrix<Real>& c, \
    const Matrix<Real>& h, \
          Matrix<Real>& x, \
          Matrix<Real>& y, \
          Matrix<Real>& z, \
          Matrix<Real>& s, \
    const IPFCtrl<Real>& ctrl ); \
  template void IPF \
  ( const DistSparseMatrix<Real>& A, \
    const DistSparseMatrix<Real>& G, \
    const DistMultiVec<Real>& b, \
    const DistMultiVec<Real>& c, \
    const DistMultiVec<Real>& h, \
          DistMultiVec<Real>& x, \
          DistMultiVec<Real>& y, \
          DistMultiVec<Real>& z, \
          DistMultiVec<Real>& s, \
    const IPFCtrl<Real>& ctrl );

#define EL_NO_INT_PROTO
#define EL_NO_COMPLEX_PROTO
#include "El/macros/Instantiate.h"

} // namespace affine
} // namespace lp
} // namespace El
