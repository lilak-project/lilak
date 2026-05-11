#include "LKParameterContainer.h"

void run_tree_draw(TString parName="")
{
    if (parName.IsNull()) {
        std::cout << "Usage: lilak draw [parameter.mac]" << std::endl;
        return;
    }

    auto par = new LKParameterContainer(parName);
    par->Draw();
}
