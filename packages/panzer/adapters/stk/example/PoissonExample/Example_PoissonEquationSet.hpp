#ifndef __PoissonExample_EquationSet_Energy_hpp__
#define __PoissonExample_EquationSet_Energy_hpp__

#include <vector>
#include <string>

#include "Teuchos_RCP.hpp"
#include "Panzer_EquationSet_DefaultImpl.hpp"
#include "Panzer_Traits.hpp"
#include "Phalanx_FieldManager.hpp"

namespace Example {

/** The equation set serves two roles. The first is to let the panzer library
  * know which fields this equation set defines and their names. It registers
  * the evaluators required for a particular equation set. The level of the
  * granularity is largely up to a user. For instance this could be the momentum
  * or continuity equation in Navier-Stokes, or it could simply be the Navier-Stokes
  * equations. 
  *
  * Generally, this inherits from the panzer::EquationSet_DefaultImpl which takes
  * care of adding the gather (extract basis coefficients from solution vector) and 
  * scatter (using element matrices and vectors distribute and sum their values
  * to a global linear system) evaluators. These use data members that must be set by
  * the user.
  */
template <typename EvalT>
class PoissonEquationSet : public panzer::EquationSet_DefaultImpl<EvalT> {
public:    

   /** In the constructor you set all the fields provided by this
     * equation set. 
     */
   PoissonEquationSet(const panzer::InputEquationSet& ies,
                      const panzer::CellData& cell_data,
		      const Teuchos::RCP<panzer::GlobalData>& global_data,
                      const bool build_transient_support);
    
   /** The specific evaluators are registered with the field manager argument.
     */
   void buildAndRegisterEquationSetEvaluators(PHX::FieldManager<panzer::Traits>& fm,
                                              const std::vector<std::pair<std::string,Teuchos::RCP<panzer::BasisIRLayout> > > & dofs,
                                              const Teuchos::ParameterList& user_data) const;

protected:

   std::string m_do_convection;
};

}

#include "Example_PoissonEquationSet_impl.hpp"

#endif
