#ifndef __CINT__
#include "RooGlobalFunc.h"
#endif
#include <memory>
#include <fstream>
#include <ostream>
#include <iostream>
#include "TH1.h"
#include "TFile.h"
#include "TTree.h"
#include "TMath.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TLatex.h"
#include "TAxis.h"
#include "TArray.h"
#include "TMatrixD.h"
#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"
#include "RooAbsReal.h"
#include "RooAbsPdf.h"
#include "RooArgList.h"
#include "RooDataSet.h"
#include "RooFormulaVar.h"
#include "RooEffProd.h"
#include "RooGenericPdf.h"
#include "RooExponential.h"
#include "RooBernstein.h"
#include "RooChebychev.h"
#include "RooNovosibirsk.h"
#include "RooCBShape.h"
#include "RooBukinPdf.h"
#include "RooProdPdf.h"
#include "RooRealVar.h"
#include "RooDataSet.h"
#include "RooDataHist.h"
#include "TCanvas.h"
#include "RooPlot.h"
#include "RooCurve.h"
#include "RooHist.h"
#include "RooWorkspace.h"
#include "RooFitResult.h"
#include "RooList.h"
#include "RooAddPdf.h"

using namespace std;
using namespace RooFit;

const double obs = 17865.;

TString outdir_;
TString outdirplots_;

TH1* QCD_HistSyst(bool up, int iPar, double init_5[5], double para_a_array[5], double para_b_array[5], double dpara_b_array[5], TMatrixD para_b, TMatrixD eigenVa)
{
    TString outname; 
    TMatrixD para_a_sys(5,1,init_5); // final parameters used for systematics
    TMatrixD para_b_sys(5,1,init_5);

    para_b.GetMatrix2Array(para_b_array);

    if(up)
        para_b_array[iPar] += 2.*abs(dpara_b_array[iPar]);
    else
        para_b_array[iPar] -= 2.*abs(dpara_b_array[iPar]);

    para_b_sys.SetMatrixArray(para_b_array);
    para_b_sys.Print("v");
    para_a_sys.Mult(eigenVa,para_b_sys);	// a+/-da = T(b+/-db)
    para_a_sys.Print("v");
    para_a_sys.GetMatrix2Array(para_a_array);	// overwritten
    
    outname = Form("PAR%i_13TeV%s", iPar, up ? "Up" : "Down");

    cout << "outname : " << outname << endl;
    cout << "para_a_array[0] = " << para_a_array[0] << endl;
    cout << "para_a_array[1] = " << para_a_array[1] << endl;
    cout << "para_a_array[2] = " << para_a_array[2] << endl;
    cout << "para_a_array[3] = " << para_a_array[3] << endl;
    cout << "para_a_array[4] = " << para_a_array[4] << endl;

    RooRealVar mbb("mbb","mbb", 240.0, 1700.0);
    RooRealVar peak("peak", "peak", para_a_array[0], 50.0, 500.0, "GeV");
    RooRealVar slope("slope", "slope", para_a_array[1], 0.0, 0.1);
    RooRealVar tail("tail", "tail", para_a_array[2], -1.0, 1.0);
    RooRealVar turnon("turnon", "turnon", para_a_array[3], 240., 750.);
    RooRealVar width("width", "width", para_a_array[4], 5.0, 875.0, "GeV");


    mbb.setBins(73);
    // Build PS*Novosibirsk p.d.f in terms of mbb,peak,width and tail

    RooNovosibirsk novo("novosibirsk","novosibirsk PDF", mbb, peak, width, tail);
    RooFormulaVar eff("novosibirskeff", "0.5*(TMath::Erf(slope*(mbb-turnon)) + 1)", RooArgSet(mbb, slope, turnon));
    RooEffProd novoeffprod("novoeffprod","novoeffprod PDF", novo, eff);

    TH1* h1 = novoeffprod.createHistogram("QCD", mbb, Binning(73,240.,1700.));
    h1->Scale(obs/h1->Integral());
    h1->SetTitle("QCD Templates");
    h1->SetName("QCD_Mbb_CMS_"+outname);
    cout << "h1->Integral() : " << h1->Integral() << endl;

    // F i t   m o d e l   t o   d a t a ,   p l o t   m o d e l 
    // ---------------------------------------------------------
    // Make a second plot frame in x and draw both the 
    // data and the p.d.f in the frame
    RooDataSet* data = novoeffprod.generate(mbb,obs);
    RooPlot* xframe = mbb.frame(Title("Turn-on x Novosibirsk PDF "+outname));
    data->plotOn(xframe,RooFit::Name("data_curve"));
    novoeffprod.plotOn(xframe,RooFit::Name("fit_curve"));


    // F i t   m o d e l   t o   d a t a
    // -----------------------------
    novoeffprod.fitTo(*data);

    double chi2 = xframe->chiSquare("fit_curve", "data_curve", 3);
    cout << "Chi^2 = " << chi2 << endl;

    // Draw all frames on a canvas
    TCanvas* c = new TCanvas("fit result"+outname,"fit result"+outname,600,400);
    gPad->SetLeftMargin(0.15); xframe->GetYaxis()->SetTitleOffset(1.6); xframe->Draw();
    c->SaveAs(outdirplots_+"Fitresult_"+outname+"_Novo.pdf");

    return h1;
}

void drawcanvas(TH1* h_novo, TH1* h_syst)
{
    //gStyle->SetOptTitle(0);
    gStyle->SetOptStat("");
    gStyle->SetLegendBorderSize(0);

    TH1* h_central = (TH1*)h_novo->Clone("h_central");
    TH1* h_ratio = (TH1*)h_novo->Clone("h_ratio");

    bool logy = 1;
    TCanvas canvas;
    canvas.SetCanvasSize(500,500);

    TPad* pad1;
    pad1 = new TPad("pad1","",0,0.1,1,1);
    pad1->SetBottomMargin(0.2);
    pad1->SetRightMargin(0.05); // The ratio plot below inherits the right and left margins settings here!
    pad1->SetLeftMargin(0.16); 
    pad1->Draw();
    pad1->cd();
    if (logy)
    {
        pad1->SetLogy();
        h_central->SetMaximum(h_central->GetMaximum()*10);
        h_central->SetMinimum(h_central->GetMinimum()/10);
    }
    else
    {
        h_central->SetMaximum(h_central->GetMaximum()+200.);
        h_central->SetMinimum(0.);
    }
    h_central->SetTitle(h_syst->GetName());
    h_central->GetXaxis()->SetTitleOffset(999); //Effectively turn off x axis title on main plot
    h_central->GetXaxis()->SetLabelOffset(999); //Effectively turn off x axis label on main plot
    h_central->GetYaxis()->SetTitleSize(0.041);
    h_central->GetYaxis()->SetTitleOffset(1.20);
    h_central->GetYaxis()->SetLabelSize(0.04);
    h_central->Draw("hist");
    h_syst->Draw("same hist");	

    canvas.cd();
    //bottom ratio
    TPad *pad2 = new TPad("pad2","",0,0.0,1,0.25);
    pad2->SetTopMargin(1);
    pad2->SetBottomMargin(0.33);
    pad2->SetLeftMargin(pad1->GetLeftMargin());
    pad2->SetRightMargin(pad1->GetRightMargin());
    pad2->SetGridy();
    pad2->Draw();
    pad2->cd();

    h_ratio->SetTitle("");
    h_ratio->Divide(h_syst);
    h_ratio->SetMarkerStyle(8);
    h_ratio->SetMarkerSize(0.5);	
    h_ratio->GetXaxis()->SetTitleSize(0.15);
    h_ratio->GetXaxis()->SetTitleOffset(0.85);
    h_ratio->GetXaxis()->SetLabelSize(0.12);
    h_ratio->GetXaxis()->SetLabelOffset(0.008);
    h_ratio->SetYTitle("Ratio");
    h_ratio->GetYaxis()->CenterTitle(kTRUE);
    h_ratio->GetYaxis()->SetTitleSize(0.15);
    h_ratio->GetYaxis()->SetTitleOffset(0.3);
    h_ratio->GetYaxis()->SetNdivisions(3,5,0);
    h_ratio->GetYaxis()->SetLabelSize(0.12);
    h_ratio->GetYaxis()->SetLabelOffset(0.011);

    h_ratio->Draw("p");
    h_ratio->SetMaximum(1.5);
    h_ratio->SetMinimum(0.5);

    TString pdfname;
    pdfname = h_syst->GetName();
    canvas.SaveAs(outdirplots_+pdfname+"_Novo.pdf");

}


//int main(int argc, char* argv[])
void BackgroundTemplatesNovoeff( TString directory = "novoeffprod_240to1700_20GeV") 
{
    //TH1::SetDefaultSumw2();
    // R e a d   w o r k s p a c e   f r o m   f i l e
    // -----------------------------------------------
    // Open input file with workspace
    outdir_ = directory;
    outdirplots_ = outdir_+"/plots/";
    TString filename = outdir_+"/workspace/FitContainer_workspace.root";
    TFile *f = new TFile(filename);
    TTree *t = (TTree*)f->Get("fit_b");

    double covMatrix[100];
    double eigenVector[100];
    t->SetBranchAddress("covMatrix", covMatrix);
    t->SetBranchAddress("eigenVector", eigenVector);
    t->GetEntry(0);

    // Retrieve workspace from file
    RooWorkspace* w = (RooWorkspace*) f->Get("workspace");

    w->Print("v");
    // R e t r i e v e   p d f ,   d a t a   f r o m   w o r k s p a c e
    // -----------------------------------------------------------------

    // Retrieve x,model and data from workspace
    RooRealVar* mbb = w->var("mbb");
    RooRealVar* peak = w->var("peak_novoeff");
    RooRealVar* tail = w->var("tail_novoeff");
    RooRealVar* width = w->var("width_novoeff");
    RooRealVar* slope = w->var("slope_novoeff");
    RooRealVar* turnon = w->var("turnon_novoeff");
    RooAbsPdf* novoeffprod = w->pdf("background");
    //int nPars = novopsprod->getParameters().getSize();

    peak->Print();
    tail->Print();
    width->Print();
    slope->Print();
    turnon->Print();

    double peak_ = peak->getVal();
    double tail_ = tail->getVal();
    double width_ = width->getVal();
    double slope_ = slope->getVal();
    double turnon_ = turnon->getVal();
    double dpeak_ = peak->getError();
    double dtail_ = tail->getError();
    double dwidth_ = width->getError();
    double dslope_ = slope->getError();
    double dturnon_ = turnon->getError();

    // Create Pseu-do Data from function
    TH1* h_central = novoeffprod->createHistogram("QCD", *mbb, Binning(73,240.,1700.));
    h_central->Scale(obs/h_central->Integral());
    h_central->SetTitle("QCD Templates");
    h_central->SetName("data_obs");
    h_central->SetStats(kFALSE);
    h_central->SetLineColor(kBlack);
    h_central->SetLineWidth(2);
    //h_central->Draw("hist");

    // F i t   m o d e l   t o   d a t a
    // ---------------------------------
    //RooFitResult* r = novopsprod->fitTo(*data,Save());
    RooDataSet* data = novoeffprod->generate(*mbb, obs);
    novoeffprod->fitTo(*data);

    // F i t   m o d e l   t o   d a t a ,   p l o t   m o d e l 
    // ---------------------------------------------------------
    // Make a second plot frame in x and draw both the 
    // data and the p.d.f in the frame
    RooPlot* xframe = mbb->frame(Title("Turn-on x Novosibirsk PDF with Pseudo-Data in SR"));
    data->plotOn(xframe,RooFit::Name("data_curve"));
    novoeffprod->plotOn(xframe,RooFit::Name("fit_curve")); //,VisualizeError(*r,1));

    double chi2 = xframe->chiSquare("fit_curve", "data_curve", 3);
    cout << "Chi^2 = " << chi2 << endl;

    // Draw all frames on a canvas
    TCanvas* c = new TCanvas("fit result","fit result",650,500);
    gPad->SetLeftMargin(0.15); xframe->GetYaxis()->SetTitleOffset(1.6); xframe->Draw();
    c->SaveAs(outdirplots_+"Fitresult_Central_Novo.pdf");

    // Central Histogram for central templates without errors
    TH1* h_novo = (TH1*)h_central->Clone("h_novo");
    //h_novo->Scale(obs/h_novo->Integral());
    h_novo->SetTitle("QCD Templates");
    h_novo->SetName("QCD_Mbb");
    h_novo->SetStats(kFALSE);
    h_novo->SetLineColor(kBlack);
    h_novo->SetLineWidth(2);

    // Output file to store all QCD Templates
    TFile* outputFile = new TFile(outdir_+"/BackgroundTemplates.root","RECREATE");
    outputFile->cd();
    h_central->Write();
    h_novo->Write();

    // Diagonalization of Correlation Matrix
    // -------------------------------------

    double init_25[5*5];
    fill(init_25, init_25 + 5*5, 1.);
    double unit_25[5*5] = {1.,0.,0.,0.,0.,
                           0.,1.,0.,0.,0.,
                           0.,0.,1.,0.,0.,
                           0.,0.,0.,1.,0.,
                           0.,0.,0.,0.,1.};
    double init_5[5] = {1.,1.,1.,1.,1.};
    // The order of the paramters in these arrays must be like this in order to work with the covMatrix.
    double para_a_array[5] = {peak_,slope_,tail_,turnon_,width_};
    double dpara_a_array[5] = {dpeak_,dslope_,dtail_,dturnon_,dwidth_};

    // Matrix a //
    // -----------
    TMatrixD corMatrixa(5,5,covMatrix); //cm_elements);
    TMatrixD eigenVa(5,5,eigenVector); //eigen_elements);
    TMatrixD eigenVa_inverse(5,5,eigenVector); //eigen_elements);
    TMatrixD eigenVa_trans(5,5,eigenVector); //eigen_elements);
    TMatrixD unit(5,5,unit_25);
    eigenVa_inverse.Invert();
    eigenVa_trans.MultT(unit,eigenVa);
    cout << "\nEigenvectors of Covariance Matrix a " << endl;
    eigenVa.Print("v");
    cout << "\nInverse Eigenvectors of Covariance Matrix a " << endl;
    eigenVa_inverse.Print("v");
    cout << "\nTranspose Eigenvectors of Covariance Matrix a " << endl;
    eigenVa_trans.Print("v");

    //TMatrixD unit(3,3,init_9);
    //unit.Mult(eigenVa,eigenVa_inverse);
    //unit.Print("v");
    TMatrixD para_a(5,1,para_a_array);
    TMatrixD dpara_a(5,1,dpara_a_array);
    cout << "\nParameters Matrix a " << endl;
    para_a.Print("v");
    cout << "\nError of Parameters (symmetric) Matrix a " << endl;
    dpara_a.Print("v");

    // Matrix b (diagonal of a) //
    // ---------------------------
    TMatrixD corMatrixb(5,5,init_25);
    TMatrixD eigenVb(5,5,init_25);
    eigenVb.Mult(eigenVa_trans,corMatrixa);
    corMatrixb.Mult(eigenVb,eigenVa);
    cout << "\nCovariance Matrix b (after diagonalization) " << endl;
    corMatrixb.Print("v");

    double eigen_values[5*5] = {0.};
    corMatrixb.GetMatrix2Array(eigen_values);
    TMatrixD para_b(5,1,init_5);
    TMatrixD dpara_b(5,1,init_5);
    para_b.Mult(eigenVa_trans,para_a);  // use inverse!!!
    //dpara_b.Mult(eigenVa,dpara_a);
    cout << "\nParameters Matrix b " << endl;
    para_b.Print("v");  
    //std::cout << "\nError of Parameters (symmetric) Matrix b " << std::endl;
    //dpara_b.Print("v");

    double para_b_array[5] = {1.,1.,1.,1.,1.};
    double dpara_b_array[5] = {TMath::Sqrt(eigen_values[0]),
                               TMath::Sqrt(eigen_values[6]),
                               TMath::Sqrt(eigen_values[12]),
                               TMath::Sqrt(eigen_values[18]),
                               TMath::Sqrt(eigen_values[24])};  

    // Get Matrix to Array for varying up and down
    // -------------------------------------------
    dpara_b.SetMatrixArray(dpara_b_array);
    cout << "\nError of Parameters (symmetric) Matrix b " << endl;
    dpara_b.Print("v");

    TCanvas *canvas = new TCanvas("QCD Templates","QCD Templates",800,800);
    canvas->Divide(2,2);
    canvas->cd(1);
    h_central->Draw("hist");

    TH1* h_syst_up;
    TH1* h_syst_down;

    for (int i = 0; i < 5; ++i)
    {
        h_syst_up = QCD_HistSyst(true,i,init_5,para_a_array,para_b_array,dpara_b_array,para_b,eigenVa);
        h_syst_up->SetLineColor(kRed);
        h_syst_up->SetLineWidth(2);
        h_syst_up->SetStats(kFALSE);
        drawcanvas(h_novo,h_syst_up);

        h_syst_down = QCD_HistSyst(false,i,init_5,para_a_array,para_b_array,dpara_b_array,para_b,eigenVa);
        h_syst_down->SetLineColor(kBlue);
        h_syst_down->SetLineWidth(2);
        h_syst_down->SetStats(kFALSE);
        drawcanvas(h_novo,h_syst_down);

        canvas->cd(i+2);
        gPad->SetLogy();
        h_novo->Draw("hist");
        h_syst_up->Draw("same hist");
        h_syst_down->Draw("same hist");
        h_syst_up->Write();
        h_syst_down->Write();
    }

    canvas->SaveAs(outdirplots_+"QCD_Templates_Novo.pdf");

    outputFile->Close();
    cout << "End of code :)" << endl;

}


