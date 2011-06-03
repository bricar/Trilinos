// @HEADER
//
// ***********************************************************************
//
//           Amesos2: Templated Direct Sparse Solver Package
//                  Copyright 2010 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ***********************************************************************
//
// @HEADER

#include <string>

#include <Teuchos_UnitTestHarness.hpp>
#include <Teuchos_VerboseObject.hpp>
#include <Teuchos_FancyOStream.hpp>
#include <Teuchos_Array.hpp>
#include <Teuchos_Tuple.hpp>
#include <Teuchos_as.hpp>
#include <Teuchos_OrdinalTraits.hpp>
#include <Teuchos_ScalarTraits.hpp>

#include <Tpetra_DefaultPlatform.hpp>

#include "Amesos2_TpetraCrsMatrixAdapter.hpp"
#include "Amesos2_Util_is_same.hpp"

namespace {

  using std::cout;
  using std::endl;
  using std::string;

  using Teuchos::RCP;
  using Teuchos::ArrayRCP;
  using Teuchos::rcp;
  using Teuchos::rcpFromRef;
  using Teuchos::Comm;
  using Teuchos::Array;
  using Teuchos::ArrayView;
  using Teuchos::tuple;
  using Teuchos::ScalarTraits;
  using Teuchos::OrdinalTraits;
  using Teuchos::FancyOStream;
  using Teuchos::VerboseObjectBase;

  using Tpetra::global_size_t;
  using Tpetra::CrsMatrix;
  using Tpetra::Map;
  using Tpetra::createUniformContigMap;

  using Amesos::MatrixAdapter;

  using Amesos::Util::is_same;


  typedef Tpetra::DefaultPlatform::DefaultPlatformType::NodeType Node;

  bool testMpi = true;

  // Where to look for input files
  string filedir;

  TEUCHOS_STATIC_SETUP()
  {
    Teuchos::CommandLineProcessor &clp = Teuchos::UnitTestRepository::getCLP();
    clp.setOption("filedir",&filedir,"Directory of matrix files.");
    clp.addOutputSetupOptions(true);
    clp.setOption("test-mpi", "test-serial", &testMpi,
		  "Test Serial by default (for now) or force MPI test.  In a serial build,"
		  " this option is ignored and a serial comm is always used." );
  }

  RCP<const Comm<int> > getDefaultComm()
  {
    RCP<const Comm<int> > ret;
    if( testMpi ){
      ret = Tpetra::DefaultPlatform::getDefaultPlatform().getComm();
    } else {
      ret = rcp(new Teuchos::SerialComm<int>());
    }
    return ret;
  }

  RCP<FancyOStream> getDefaultOStream()
  {
    return( VerboseObjectBase::getDefaultOStream() );
  }

  template<typename T1, typename T2>
  const RCP<Array<std::pair<T1,T2> > >
  zip(const ArrayView<T1>& a, const ArrayView<T2>& b)
  {
    typedef std::pair<T1,T2> result_type;
    size_t size = std::min(a.size(), b.size());
    RCP<Array<result_type > > r = rcp( new Array<result_type>(size) );
    for( size_t i = 0; i < size; ++i){
      (*r)[i] = std::make_pair(a[i], b[i]);
    }
    return(r);
  }
  
  template<typename T>
  bool contains(const ArrayView<T> a, T t)
  {
    typedef typename ArrayView<T>::iterator it;
    it first = a.begin();
    it last  = a.end();
    return( std::find(first, last, t) != last );
  }


  /*
   * UNIT TESTS
   */

  TEUCHOS_UNIT_TEST_TEMPLATE_3_DECL( CrsMatrixAdapter, Initialization, Scalar, LO, GO )
  {
    /* Test correct initialization of the MatrixAdapter
     *
     * - All Constructors
     * - Correct initialization of class members
     * - Correct typedefs ( using Amesos::is_same<> )
     */
    typedef ScalarTraits<Scalar> ST;
    typedef CrsMatrix<Scalar,LO,GO,Node> MAT;
    typedef MatrixAdapter<MAT> ADAPT;

    const global_size_t INVALID = OrdinalTraits<global_size_t>::invalid();
    RCP<const Comm<int> > comm = getDefaultComm();
    //const size_t numprocs = comm->getSize();
    const size_t rank     = comm->getRank();
    // create a Map
    const size_t numLocal = 10;
    RCP<Map<LO,GO,Node> > map = rcp( new Map<LO,GO,Node>(INVALID,numLocal,0,comm) );
    RCP<MAT> eye = rcp( new MAT(map,1) );
    GO base = numLocal*rank;
    for( size_t i = 0; i < numLocal; ++i ){
      eye->insertGlobalValues(base+i,tuple<GO>(base+i),tuple<Scalar>(ST::one()));
    }
    eye->fillComplete();
    // Constructor from RCP
    RCP<ADAPT> adapter  = rcp(new MatrixAdapter<MAT>(eye));
    // Copy constructor
    RCP<ADAPT> adapter2 = rcp(new MatrixAdapter<MAT>(*adapter));

    // The following should all pass at compile time
    TEST_ASSERT( (is_same<Scalar,typename ADAPT::scalar_type>::value) );
    TEST_ASSERT( (is_same<LO,typename ADAPT::local_ordinal_type>::value) );
    TEST_ASSERT( (is_same<GO,typename ADAPT::global_ordinal_type>::value) );
    TEST_ASSERT( (is_same<Node,typename ADAPT::node_type>::value) );
    TEST_ASSERT( (is_same<global_size_t,typename ADAPT::global_size_type>::value) );
    TEST_ASSERT( (is_same<MAT,typename ADAPT::matrix_type>::value) );
  
  }

  TEUCHOS_UNIT_TEST_TEMPLATE_3_DECL( CrsMatrixAdapter, Dimensions, Scalar, LO, GO )
  {
    // Test that the dimensions reported by the adapter match those as reported
    // by the Tpetra::CrsMatrix
    // Check equality of mapped method calls
    typedef ScalarTraits<Scalar> ST;
    typedef CrsMatrix<Scalar,LO,GO,Node> MAT;
    const global_size_t INVALID = OrdinalTraits<global_size_t>::invalid();
    RCP<const Comm<int> > comm = getDefaultComm();
    //const size_t numprocs = comm->getSize();
    const size_t rank     = comm->getRank();
    // create a Map
    const size_t numLocal = 10;
    RCP<Map<LO,GO,Node> > map = rcp( new Map<LO,GO,Node>(INVALID,numLocal,0,comm) );
    RCP<MAT> eye = rcp( new MAT(map,1) );
    GO base = numLocal*rank;
    for( size_t i = 0; i < numLocal; ++i ){
      eye->insertGlobalValues(base+i,tuple<GO>(base+i),tuple<Scalar>(ST::one()));
    }
    eye->fillComplete();

    // Constructor from RCP
    RCP<MatrixAdapter<MAT> > adapter  = rcp(new MatrixAdapter<MAT>(eye));

    TEST_EQUALITY(eye->getGlobalNumEntries(), adapter->getGlobalNNZ());
    TEST_EQUALITY(eye->getNodeNumEntries(), adapter->getLocalNNZ());
    TEST_EQUALITY(eye->getGlobalNumRows(), adapter->getGlobalNumRows());
    TEST_EQUALITY(eye->getNodeNumRows(), adapter->getLocalNumRows());
    TEST_EQUALITY(eye->getNodeNumCols(), adapter->getLocalNumCols());
    TEST_EQUALITY(eye->getGlobalNumCols(), adapter->getGlobalNumCols());
    TEST_EQUALITY(eye->getGlobalMaxNumRowEntries(), adapter->getMaxNNZ());
  }

  TEUCHOS_UNIT_TEST_TEMPLATE_3_DECL( CrsMatrixAdapter, CRS, Scalar, LO, GO )
  {
    /* Test the getCrs() method of MatrixAdapter.  We check against a simple
     * test matrix that we construct on the fly.
     */
    typedef ScalarTraits<Scalar> ST;
    typedef CrsMatrix<Scalar,LO,GO,Node> MAT;
    typedef MatrixAdapter<MAT> ADAPT;
    typedef std::pair<Scalar,GO> my_pair_t;
    RCP<const Comm<int> > comm = getDefaultComm();
    //const size_t numprocs = comm->getSize();
    const size_t rank     = comm->getRank();
    // create a Map for our matrix
    global_size_t nrows = 6;
    RCP<const Map<LO,GO,Node> > map = createUniformContigMap<LO,GO>(nrows,comm);
    RCP<MAT> mat = rcp( new MAT(map,3) ); // max of three entries in a row

    /* We will be using the following matrix for this test:
     * 
     * [ [ 7,  0,  -3, 0,  -1, 0 ]
     *   [ 2,  8,  0,  0,  0,  0 ]
     *   [ 0,  0,  1,  0,  0,  0 ]
     *   [ -3, 0,  0,  5,  0,  0 ]
     *   [ 0,  -1, 0,  0,  4,  0 ]
     *   [ 0,  0,  0,  -2, 0,  6 ] ]
     */
    // Construct matrix
    if( rank == 0 ){
      mat->insertGlobalValues(0,tuple<GO>(0,2,4),tuple<Scalar>(7,-3,-1));
      mat->insertGlobalValues(1,tuple<GO>(0,1),tuple<Scalar>(2,8));
      mat->insertGlobalValues(2,tuple<GO>(2),tuple<Scalar>(1));
      mat->insertGlobalValues(3,tuple<GO>(0,3),tuple<Scalar>(-3,5));
      mat->insertGlobalValues(4,tuple<GO>(1,4),tuple<Scalar>(-1,4));
      mat->insertGlobalValues(5,tuple<GO>(3,5),tuple<Scalar>(-2,6));
    }
    mat->fillComplete();

    RCP<ADAPT> adapter = rcp(new ADAPT(mat));

    Array<Scalar> nzvals_test(tuple<Scalar>(7,-3,-1,2,8,1,-3,5,-1,4,-2,6));
    Array<GO> colind_test(tuple<GO>(0,2,4,0,1,2,0,3,1,4,3,5));
    Array<global_size_t> rowptr_test(tuple<global_size_t>(0,3,5,6,8,10,12));

    Array<Scalar> nzvals(adapter->getGlobalNNZ());
    Array<GO> colind(adapter->getGlobalNNZ());
    Array<global_size_t> rowptr(adapter->getGlobalNumRows() + 1);
    size_t nnz;

    adapter->getCrs(nzvals,colind,rowptr,nnz);

    if ( rank == 0 ){
      // getCrs() does not guarantee a permutation of the non-zero
      // values and the column indices, we just know that they need to
      // match up with what is expected.
      GO maxRow = map->getMaxAllGlobalIndex();
      for ( GO row = map->getMinAllGlobalIndex(); row <= maxRow; ++row ){
	global_size_t rp  = rowptr[row];
	global_size_t nrp = rowptr[row+1];
	global_size_t row_nnz = nrp - rp;
	TEST_ASSERT( rp < nzvals.size() );
	TEST_ASSERT( rp < colind.size() );
	const RCP<Array<my_pair_t> > expected_pairs
	  = zip(nzvals_test.view(rp,row_nnz), colind_test.view(rp,row_nnz));
	const RCP<Array<my_pair_t> > got_pairs
	  = zip(nzvals.view(rp,row_nnz), colind.view(rp,row_nnz));
	for ( global_size_t i = 0; i < row_nnz; ++i ){
	  TEST_ASSERT( contains((*got_pairs)(), (*expected_pairs)[i]) );
	}
      }
      TEST_COMPARE_ARRAYS(rowptr,rowptr_test);
      TEST_EQUALITY(nnz,12);
    }
  }

  TEUCHOS_UNIT_TEST_TEMPLATE_3_DECL( CrsMatrixAdapter, CCS, Scalar, LO, GO )
  {
    /* Test the getCcs() method of MatrixAdapter.  Again, check against a known
     * matrix
     */
    typedef ScalarTraits<Scalar> ST;
    typedef CrsMatrix<Scalar,LO,GO,Node> MAT;
    typedef MatrixAdapter<MAT> ADAPT;

    RCP<const Comm<int> > comm = getDefaultComm();
    //const size_t numprocs = comm->getSize();
    const size_t rank     = comm->getRank();
    // create a Map for our matrix
    global_size_t nrows = 6;
    RCP<const Map<LO,GO,Node> > map = createUniformContigMap<LO,GO>(nrows,comm);
    RCP<MAT> mat = rcp( new MAT(map,3) ); // max of three entries in a row

    /* We will be using the following matrix for this test:
     * 
     * [ [ 7,  0,  -3, 0,  -1, 0 ]
     *   [ 2,  8,  0,  0,  0,  0 ]
     *   [ 0,  0,  1,  0,  0,  0 ]
     *   [ -3, 0,  0,  5,  0,  0 ]
     *   [ 0,  -1, 0,  0,  4,  0 ]
     *   [ 0,  0,  0,  -2, 0,  6 ] ]
     */
    // Construct matrix
    if( rank == 0 ){
      mat->insertGlobalValues(0,tuple<GO>(0,2,4),tuple<Scalar>(7,-3,-1));
      mat->insertGlobalValues(1,tuple<GO>(0,1),tuple<Scalar>(2,8));
      mat->insertGlobalValues(2,tuple<GO>(2),tuple<Scalar>(1));
      mat->insertGlobalValues(3,tuple<GO>(0,3),tuple<Scalar>(-3,5));
      mat->insertGlobalValues(4,tuple<GO>(1,4),tuple<Scalar>(-1,4));
      mat->insertGlobalValues(5,tuple<GO>(3,5),tuple<Scalar>(-2,6));
    }
    mat->fillComplete();

    // Print for sanity sake
    // RCP<FancyOStream> os = getDefaultOStream();
    // mat->describe(*os,Teuchos::VERB_EXTREME);

    RCP<ADAPT> adapter = rcp(new ADAPT(mat));

    Array<Scalar> nzvals_test(tuple<Scalar>(7,2,-3,8,-1,-3,1,5,-2,-1,4,6));
    Array<GO> rowind_test(tuple<GO>(0,1,3,1,4,0,2,3,5,0,4,5));
    Array<global_size_t> colptr_test(tuple<global_size_t>(0,3,5,7,9,11,12));

    Array<Scalar> nzvals(adapter->getGlobalNNZ());
    Array<GO> rowind(adapter->getGlobalNNZ());
    Array<global_size_t> colptr(adapter->getGlobalNumRows() + 1);
    size_t nnz;

    adapter->getCcs(nzvals,rowind,colptr,nnz);

    // Only rank 0 gets the CRS representation
    if( rank == 0 ){
      // getCCS() does guarantee an increasing row permutation for
      // rowind, so we can just compare the expected and received
      // straight-up
      TEST_COMPARE_ARRAYS(nzvals,nzvals_test);
      TEST_COMPARE_ARRAYS(rowind,rowind_test);
      TEST_COMPARE_ARRAYS(colptr,colptr_test);
      TEST_EQUALITY(nnz,12);
    }
  }

  /* Also need to test the updateValues[Crs,Ccs]() methods, to make sure that
   * those changes are persistent in the underlying matrices (once implemented).
   */

  /*
   * Instantiations
   */

#ifdef HAVE_TEUCHOS_COMPLEX
#  define UNIT_TEST_GROUP_ORDINAL_COMPLEX_FLOAT(LO, GO)	\
  typedef std::complex<float> ComplexFloat;		\
  UNIT_TEST_GROUP_ORDINAL_SCALAR(LO, GO, ComplexFloat)
#  define UNIT_TEST_GROUP_ORDINAL_COMPLEX_DOUBLE(LO, GO)	\
  typedef std::complex<double> ComplexDouble;			\
  UNIT_TEST_GROUP_ORDINAL_SCALAR(LO, GO, ComplexDouble) 
#else
#  define UNIT_TEST_GROUP_ORDINAL_COMPLEX_FLOAT(LO, GO)
#  define UNIT_TEST_GROUP_ORDINAL_COMPLEX_DOUBLE(LO, GO)
#endif

  // Uncomment this for really fast development cycles but make sure to comment
  // it back again before checking in so that we can test all the types.
  // #define FAST_DEVELOPMENT_UNIT_TEST_BUILD

#define UNIT_TEST_GROUP_ORDINAL_SCALAR( LO, GO, SCALAR )		\
  TEUCHOS_UNIT_TEST_TEMPLATE_3_INSTANT( CrsMatrixAdapter, Initialization, SCALAR, LO, GO ) \
  TEUCHOS_UNIT_TEST_TEMPLATE_3_INSTANT( CrsMatrixAdapter, Dimensions, SCALAR, LO, GO ) \
  TEUCHOS_UNIT_TEST_TEMPLATE_3_INSTANT( CrsMatrixAdapter, CRS, SCALAR, LO, GO ) \
  TEUCHOS_UNIT_TEST_TEMPLATE_3_INSTANT( CrsMatrixAdapter, CCS, SCALAR, LO, GO ) 
  
#define UNIT_TEST_GROUP_ORDINAL( ORDINAL )		\
  UNIT_TEST_GROUP_ORDINAL_ORDINAL( ORDINAL, ORDINAL )

# ifdef FAST_DEVELOPMENT_UNIT_TEST_BUILD
#    define UNIT_TEST_GROUP_ORDINAL_ORDINAL( LO, GO )	\
  UNIT_TEST_GROUP_ORDINAL_SCALAR( LO, GO, double)	\
  UNIT_TEST_GROUP_ORDINAL_COMPLEX_FLOAT( LO, GO )
  UNIT_TEST_GROUP_ORDINAL(int)

# else // not FAST_DEVELOPMENT_UNIT_TEST_BUILD

#    define UNIT_TEST_GROUP_ORDINAL_ORDINAL( LO, GO )	\
  UNIT_TEST_GROUP_ORDINAL_SCALAR(LO, GO, float)		\
  UNIT_TEST_GROUP_ORDINAL_SCALAR(LO, GO, double)	\
  UNIT_TEST_GROUP_ORDINAL_COMPLEX_FLOAT(LO, GO)		\
  UNIT_TEST_GROUP_ORDINAL_COMPLEX_DOUBLE(LO,GO)

  UNIT_TEST_GROUP_ORDINAL(int)

  typedef long int LongInt;
  UNIT_TEST_GROUP_ORDINAL_ORDINAL( int, LongInt )
#    ifdef HAVE_TEUCHOS_LONG_LONG_INT
  typedef long long int LongLongInt;
  UNIT_TEST_GROUP_ORDINAL_ORDINAL( int, LongLongInt )
#    endif

# endif // FAST_DEVELOPMENT_UNIT_TEST_BUILD

} // end anonymous namespace
