//#include "LKDrawing.cpp"
//#include "LKDrawingGroup.cpp"
//#include "LKDrawingCluster.cpp"
//#include "LKDataViewer.cpp"

struct function_data { TString formula; int npar; };
vector<function_data> functions;
void make_functions();

LKDrawing* make_drawing(int id=-1)
{
    int n = 200;
    double x2 = 10;
    int iRandom = id;
    make_functions();
    int maxRandom = functions.size();

    ///////////////////////////////////////////////////////////

    if (id<0) {
        gRandom -> SetSeed(time(0));
        id = gRandom -> Uniform(0,1000000);
    }
    if (id>=maxRandom)
        iRandom = gRandom -> Uniform(0,1000000);
    gRandom -> SetSeed(time(0)+id);
    int nFill = gRandom->Uniform(5000,100000);

    TString formula = functions[iRandom%maxRandom].formula;
    int npar = functions[iRandom%maxRandom].npar;
    formula = Form("abs(%s)",formula.Data());

    ///////////////////////////////////////////////////////////

    auto drawing = new LKDrawing();

    auto hist = new TH1D(Form("hist%d",id), "", n, 0, x2);
    TString f1Name = Form("f%d",id);
    TString title = Form("[%d]  %s",id,formula.Data());

    auto f1 = new TF1(f1Name,formula.Data(),0,x2);
    f1 -> SetNpx(n);
    if (npar>0) title += "  {";
    for (auto iPar=0; iPar<npar; ++iPar) {
        int par = gRandom -> Uniform(1,x2);
        f1 -> SetParameter(iPar,par);
        if (iPar!=0) title += ", ";
        title += Form("%d",par);
    }
    title += "}";
    hist -> SetTitle(title);
    for (auto iFill=0; iFill<nFill; ++iFill) hist -> Fill(f1->GetRandom(0,x2));
    hist -> Fit(f1,"RQN0");

    drawing -> Add(hist);
    drawing -> Add(f1);
    return drawing;
}

void make_functions()
{
    functions.push_back(function_data{"pol1", 1});
    functions.push_back(function_data{"pol2", 2});
    functions.push_back(function_data{"pol3", 3});
    functions.push_back(function_data{"pol4", 4});
    functions.push_back(function_data{"gaus(0)", 3});
    functions.push_back(function_data{"[0]/(x+1)",1});
    functions.push_back(function_data{"[0]*exp(x)", 1});
    functions.push_back(function_data{"[0]*log(x)", 1});
    functions.push_back(function_data{"[0]*sqrt(x)", 1});
    functions.push_back(function_data{"[0]*sinh(x)", 1});
    functions.push_back(function_data{"[0]*cosh(x)", 1});
    functions.push_back(function_data{"[0]*atan(x)", 1});
    functions.push_back(function_data{"[0]/(1.0+exp(-x))",1});
    functions.push_back(function_data{"[0]*(x-floor(x+0.5))",1});
    functions.push_back(function_data{"[0]*exp(-[1]*x)*sin(x)",2});
    functions.push_back(function_data{"[0]*sin(x)+[1]*exp(-[2]*x)",2});
    functions.push_back(function_data{"[0]*(exp(x)+fabs(sin(x)))", 1});
    functions.push_back(function_data{"[0]*TMath::Poisson([1],[2])", 3});
    functions.push_back(function_data{"[0]*(pow(x-[1],2)+pow([2],2))",3});
    functions.push_back(function_data{"[0]*TMath::Landau(x)*sin(x)", 1});
    functions.push_back(function_data{"[0]*TMath::BesselI0(x)", 1});
    functions.push_back(function_data{"[0]*exp(-[1]/([2]*x))", 3});
    functions.push_back(function_data{"[0]*2*pi*sqrt(x/[1])", 2});
    functions.push_back(function_data{"[0]*sin(x)/x", 1});
}
