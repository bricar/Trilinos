#ifndef BELOS_MUELU_ADAPTER_HPP
#define BELOS_MUELU_ADAPTER_HPP

#include "BelosOperator.hpp"
#include "BelosMultiVec.hpp"
#include "MueLu_Hierarchy.hpp"

// Here is some documentation about the adapter interface of Belos:
// Belos uses the traits techniques for its adapters. Traits OP and MV must be implemented for your operator and multivector classes.
// What is somehow confusing is that Belos also provides an interface Belos::Operator and Belos::MultiVec.
// Internally, Belos only use the traits, not the interface. But Traits for Belos::Operator and Belos::MultiVec are provided, so you can either:
// - implements directly the traits Belos::OperatorTraits and Belos::MultiVecTraits
// - implements the interface Belos::Operator and Belos::MultiVec
//
// Here are the adapaters provided by Belos:
// Traits are implemented for three couples of <MV,OP>, and Belos works out of the box with the following combinaison of <MV,OP>:
// - MV=Belos::MultiVec<...>, OP=Belos::Operator<...>
// - MV=Epetra_MultiVector, OP=Epetra_Operator
// - MV=Tpetra_MultiVector<...>, MV=Tpetra_MultiVector<...>
//
// In addition, wrappers around Epetra_MultiVector and Epetra_Operator are provided to turn these Epetra objects to Belos::MultiVec and Belos::Operator and use MV=Belos::MultiVec<...>, OP=Belos::Operator<...> with Epetra.
// The wrappers are the classes Belos::EpetraMultiVec and Belos::EpetraOp 
// So when using Epetra, you have the choice to:
// - use Belos::LinearProblem<double, Belos::Operator<double>, Belos::MultiVec<double>>
// - or Belos::LinearProblem<double, Epetra_Operator<double>, Epetra_MultiVec<double>>
//
// If you use Epetra, you have to be carreful with the meaning for Epetra_Operator::Apply:
// For instance, Ifpack smoother implements the Epetra_Operator interface but to apply the preconditionner, you have to use ApplyInverse() instead of Apply()!
// To swap the method Apply() and ApplyInverse() of an Epetra_Operator, you can use the class Belos::EpetraPrecOp. Belos::EpetraPrecOp can be used with both OP=Belos::Operator<...> and OP=Epetra_Operator.
//
// Belos file description:
// - src/BelosMultiVecTraits.hpp src/BelosOperatorTraits.hpp : Traits used internally by Belos
// - tpetra/src/BelosTpetraAdapter.*                         : Specialization of the Traits for Tpetra
// - epetra/src/BelosEpetraAdapter.*                         : Specialization of the Traits for Epetra + Implementation of the interface Belos::MultiVec and Belos::Operator for Epetra + Implementation of Belos::EpetraPrecOp
// - src/BelosMultiVec.hpp src/BelosOperator.hpp             : Definition of the interfaces Belos::MultiVec and Belos::Operator + Specialization of the Traits for these interfaces

namespace Belos { 
  // TODO: Should this file be moved to Belos ?
  // TODO: The relation between Belos and MueLu is: Belos uses MueLu as a Preconditionner. So it makes more sense to me.

  // Turns a MueLu::Hierarchy object to a Belos::Operator.
  // This allows to use MueLu as a preconditionner for a Belos::LinearProblem
  // where ScalarType=, MV=Belos::MultiVec<ScalarType> and OP=Belos::Operator<ScalarType>
  //
  // Note: this adapter is implemented only for Epetra (and ScalarType=double), because the interface Belos::Operator and Belos::MultiVec is only implemented for Epetra in Belos.
  // For Tpetra, you can use directly the adapter provided for Belos::LinearProblem where OP=Tpetra::Operator<...> or OP=Cthulhu::Operator<...>
  //
  // TODO: This adapter may also be used for OP=Epetra_Operator if we implements the interface Epetra_Operator by adding inheritence to public virtual Epetra_Operator and a bunch of methods
  // (see also the class Belos::EpetraPrecOp in package/belos/epetra/src/)
  class MueLuEpetraPrecOp : public Belos::Operator<double> { 
    
    typedef MueLu::Hierarchy<double,int, int, Kokkos::DefaultNode::DefaultNodeType, Kokkos::DefaultKernels<double,int,Kokkos::DefaultNode::DefaultNodeType>::SparseOps> Hierarchy; // TODO: remove template parameters of Hierarchy
    typedef Cthulhu::MultiVector<double, int, int> MultiVector;

  public:
    
    //! @name Constructor/Destructor
    //@{ 
    
    //! Default constructor
    MueLuEpetraPrecOp(const Teuchos::RCP<Hierarchy> & H) : Hierarchy_(H) {}
    
    //! Destructor.
    virtual ~MueLuEpetraPrecOp() {};
    //@}
    
    //! @name Operator application method
    //@{ 
    
    /*! \brief This routine takes the Epetra_MultiVector \c x and applies the operator
      to it resulting in the Epetra_MultiVector \c y, which is returned.
      \note It is expected that any problem with applying this operator to \c x will be
      indicated by an std::exception being thrown.
    */
    void Apply ( const Epetra_MultiVector& x, Epetra_MultiVector& y, ETrans trans=NOTRANS ) const {

      TEST_FOR_EXCEPTION(trans!=NOTRANS, EpetraOpFailure, 
                         "Belos::MueLuEpetraPrecOp::Apply, transpose mode != NOTRANS not supported by MueLu preconditionners."); 

      Epetra_MultiVector & temp_x = const_cast<Epetra_MultiVector &>(x);

      const Cthulhu::EpetraMultiVector eX(Teuchos::rcpFromRef(temp_x));
      Cthulhu::EpetraMultiVector eY(Teuchos::rcpFromRef(y));

      //FIXME InitialGuessIsZero currently does nothing in MueLu::Hierarchy.Iterate().
      eY.putScalar(0.0);

      Hierarchy_->Iterate( eX, 1, eY , true);
      
    }

    /*! \brief This routine takes the Belos::MultiVec \c x and applies the operator
      to it resulting in the Belos::MultiVec \c y, which is returned.
      \note It is expected that any problem with applying this operator to \c x will be
      indicated by an std::exception being thrown.
    */
    void Apply ( const Belos::MultiVec<double>& x, Belos::MultiVec<double>& y, ETrans trans=NOTRANS ) const {
      const Epetra_MultiVector* vec_x = dynamic_cast<const Epetra_MultiVector*>(&x);
      Epetra_MultiVector*       vec_y = dynamic_cast<Epetra_MultiVector*>(&y);

      TEST_FOR_EXCEPTION( vec_x==NULL || vec_y==NULL, EpetraOpFailure, 
                          "Belos::MueLuEpetraPrecOp::Apply, x and/or y cannot be dynamic cast to an Epetra_MultiVector.");

      Apply(*vec_x, *vec_y, trans);
    }
  
  private:
  
    Teuchos::RCP<Hierarchy> Hierarchy_;
  };


} // namespace Belos

#endif // BELOS_MUELU_ADAPTER_HPP
