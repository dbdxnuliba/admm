#ifndef _KUKA_DEFAULT_GETTER_INERTIA_PARAMETERS_
#define _KUKA_DEFAULT_GETTER_INERTIA_PARAMETERS_

#include "dynamics_parameters.h"

namespace iit {
namespace Kuka {
namespace dyn {

class DefaultParamsGetter : public RuntimeParamsGetter
{
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
        DefaultParamsGetter() {
            resetDefaults();
        }
        ~DefaultParamsGetter() {};

    public:
        void resetDefaults() {
        }

    private:
        RuntimeInertiaParams values;
};

}
}
}
#endif
