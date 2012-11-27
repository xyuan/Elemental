/*
   Copyright (c) 2009-2012, Jack Poulson
   All rights reserved.

   This file is part of Elemental.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    - Neither the name of the owner nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/
#include "elemental.hpp"
using namespace std;
using namespace elem;

template<typename T> 
void TestHer2k
( bool print, UpperOrLower uplo, Orientation orientation,
  int m, int k, T alpha, T beta, const Grid& g )
{
    DistMatrix<T> A(g), B(g), C(g);

    if( orientation == NORMAL )
    {
        Uniform( m, k, A );
        Uniform( m, k, B );
    }
    else
    {
        Uniform( k, m, A );
        Uniform( k, m, B );
    }
    HermitianUniformSpectrum( m, C, 1, 10 );
    if( print )
    {
        A.Print("A");
        B.Print("B");
        C.Print("C");
    }

    if( g.Rank() == 0 )
    {
        cout << "  Starting Her2k...";
        cout.flush();
    }
    mpi::Barrier( g.Comm() );
    const double startTime = mpi::Time();
    Her2k( uplo, orientation, alpha, A, B, beta, C );
    mpi::Barrier( g.Comm() );
    const double runTime = mpi::Time() - startTime;
    const double realGFlops = 2.*double(m)*double(m)*double(k)/(1.e9*runTime);
    const double gFlops = ( IsComplex<T>::val ? 4*realGFlops : realGFlops );
    if( g.Rank() == 0 )
    {
        cout << "DONE. " << endl
             << "  Time = " << runTime << " seconds. GFlops = " 
             << gFlops << endl;
    }
    if( print )
    {
        ostringstream msg;
        if( orientation == NORMAL )
            msg << "C := " << alpha << " A B' + B A'" << beta << " C";
        else
            msg << "C := " << alpha << " A' B + B' A" << beta << " C";
        C.Print( msg.str() );
    }
}

int 
main( int argc, char* argv[] )
{
    Initialize( argc, argv );
    mpi::Comm comm = mpi::COMM_WORLD;
    const int commRank = mpi::CommRank( comm );
    const int commSize = mpi::CommSize( comm );
    
    try
    {
        MpiArgs args( argc, argv, comm );
        int r = args.Optional("--r",0,"height of process grid");
        const char uploChar = args.Optional
            ("--uplo",'L',"upper/lower storage: L/U");
        const char transChar = args.Optional
            ("--trans",'N',"orientation: N/T/C");
        const int m = args.Optional("--m",100,"height of result");
        const int k = args.Optional("--k",100,"inner dimension");
        const int nb = args.Optional("--nb",96,"algorithmic blocksize");
        const int nbLocal = args.Optional("--nbLocal",32,"local blocksize");
        const bool print = args.Optional("--print",false,"print matrices?");
        args.Process();

        if( r == 0 )
            r = Grid::FindFactor( commSize );
        if( commSize % r != 0 )
            throw std::logic_error("Invalid process grid height");
        const int c = commSize / r;
        const Grid g( comm, r, c );
        const UpperOrLower uplo = CharToUpperOrLower( uploChar );
        const Orientation orientation = CharToOrientation( transChar );
        SetBlocksize( nb );
        SetLocalTrr2kBlocksize<double>( nbLocal );
        SetLocalTrr2kBlocksize<Complex<double> >( nbLocal );

#ifndef RELEASE
        if( rank == 0 )
        {
            cout << "==========================================\n"
                 << " In debug mode! Performance will be poor! \n"
                 << "==========================================" << endl;
        }
#endif
        if( commRank == 0 )
            cout << "Will test Her2k" << uploChar << transChar << endl;

        if( commRank == 0 )
        {
            cout << "--------------------------------------\n"
                 << "Testing with doubles:                 \n"
                 << "--------------------------------------" << endl;
        }
        TestHer2k<double>
        ( print, uplo, orientation, m, k, (double)3, (double)4, g );

        if( commRank == 0 )
        {
            cout << "--------------------------------------\n"
                 << "Testing with double-precision complex:\n"
                 << "--------------------------------------" << endl;
        }
        TestHer2k<Complex<double> >
        ( print, uplo, orientation, m, k, 
          Complex<double>(3), Complex<double>(4), g );
    }
    catch( ArgException& e ) { }
    catch( exception& e )
    {
        ostringstream os;
        os << "Process " << commRank << " caught error message:" << endl 
           << e.what() << endl;
        cerr << os.str();
#ifndef RELEASE
        DumpCallStack();
#endif
    }
    Finalize();
    return 0;
}
