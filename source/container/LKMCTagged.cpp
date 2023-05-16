#include "LKMCTagged.hpp"

LKMCTagged::LKMCTagged()
{
}

LKMCTagged::~LKMCTagged()
{
}

void LKMCTagged::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    fMCID = -1;
    fMCPurity = -1;
}

void LKMCTagged::Copy(TObject &obj) const
{
    LKContainer::Copy(obj);
    auto container = (LKMCTagged &) obj;

    container.SetMCTag(fMCID, fMCPurity);
}

void LKMCTagged::SetMCID(Int_t id) { fMCID = id; }
void LKMCTagged::SetMCPurity(Double_t purity) { fMCPurity = purity; }

void LKMCTagged::SetMCTag(Int_t id, Double_t purity)
{
    fMCID = id;
    fMCPurity = purity;
}
