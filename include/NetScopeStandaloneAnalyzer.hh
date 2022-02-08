#ifndef NetScopeStandaloneAnalyzer_HH
#define NetScopeStandaloneAnalyzer_HH
 
#include "DatAnalyzer.hh"
#include <assert.h>
#include "TLeaf.h"
#include "TObjArray.h"

// This is the class that should be used for parsing and analyzing
// NetScope data files in .root format produced by the python script.

class NetScopeStandaloneAnalyzer : public DatAnalyzer {
  public:
	  struct FTBFPixelEvent {
	      double xSlope;
	      double ySlope;
	      double xIntercept;
	      double yIntercept;
              double chi2;
              double xResidBack;
	      double yResidBack;
	      int trigger;
	      int runNumber;
	      int nPlanes;
              int numPixels;
              int numBackPlanes;
	      Long64_t timestamp;
	      Long64_t bco;
	  };

    //Scope Tektronix DPO7254 ADC already in account in the binary conversion
    NetScopeStandaloneAnalyzer() : DatAnalyzer(999, 999, 999, 1, 1., 0) {}

    void GetCommandLineArgs(int argc, char **argv);

    std::string split(const std::string& half, const std::string& s, const std::string& h) const;

    void GetDim(TTree* const tree, const std::string& var, unsigned int& f, unsigned int& s);

    void InitLoop();

    int GetChannelsMeasurement(int i_aux);

    unsigned int GetTimeIndex(unsigned int n_ch) { return 0; }

    void Analyze();
    
  protected:
    vector<int> active_ch;
    // Set by command line arguments or default
    TString pixel_input_file_path;
    bool skip_tracks;
    // Pixel events variables
    FTBFPixelEvent* pixel_event= nullptr;
    TFile *pixel_file = nullptr;
    TTree *pixel_tree = nullptr;

    unsigned long int idx_px_tree = 0;
    unsigned long int entries_px_tree = 0;

    float xIntercept=0;
    float yIntercept=0;
    float xSlope=0;
    float ySlope=0;
    vector<float> x_DUT;
    vector<float> y_DUT;
    float chi2=0;
    float xResidBack=0;
    float yResidBack=0;
    int ntracks=0;
    int nplanes=0;
    int npix=0;
    int nback=0;
};

#endif
