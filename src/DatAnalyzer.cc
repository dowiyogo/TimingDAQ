#define DEFAULT_FOR_EMPTY_CH 0

#include "DatAnalyzer.hh"

using namespace std;

DatAnalyzer::DatAnalyzer(int numChannels, int numTimes, int numSamples, int res, float scale) :
        NUM_CHANNELS(numChannels), NUM_TIMES(numTimes), NUM_SAMPLES(numSamples),
        DAC_RESOLUTION(res), DAC_SCALE(scale),
        file(0), tree(0) {
    time = new float*[numTimes];
    channel = new float*[numChannels];

    for(unsigned int i=0; i<numChannels; i++) {
      channel[i] = new float[numSamples];
      if(i<numTimes) time[i] = new float[numSamples];
    }
    cout << "NUM_CHANNELS: " << NUM_CHANNELS << flush;
    cout << "NUM_TIMES: " << NUM_TIMES << endl;
    cout << "NUM_SAMPLES: " << NUM_SAMPLES << endl;
}

DatAnalyzer::~DatAnalyzer() {
    cout << "In DatAnalyzer destructor" << endl;
    if (file) {
        file->Close();
    }
}

TString DatAnalyzer::ParseCommandLine( int argc, char* argv[], TString opt )
{
  TString out = "";
  for (int i = 1; i < argc && out==""; i++ ) {
    TString tmp( argv[i] );
    if ( tmp.Contains("--"+opt) ) {
      if(tmp.Contains("=")) {
        out = tmp(tmp.First("=")+1, tmp.Length());
      }
      else {
        out = "true";
      }
    }
  }
  return out;
}

void DatAnalyzer::GetCommandLineArgs(int argc, char **argv){

  input_file_path = ParseCommandLine( argc, argv, "input_file" );
  ifstream in_file(input_file_path.Data());
  if (!in_file || input_file_path == "" || !input_file_path.EndsWith(".dat")){
    cerr << "[ERROR]: please provide a valid input file. Use: --input_file=<your_input_file_path>.dat " << endl;
    exit(0);
  }
  else
  {
    cout << "Input file: " << input_file_path.Data() << endl;
  }

  output_file_path = ParseCommandLine( argc, argv, "output_file" );
  if ( output_file_path == "" ){
    output_file_path = input_file_path;
    output_file_path.ReplaceAll(".dat", ".root");
  }
  else if (!output_file_path.EndsWith(".root")) output_file_path += ".root";
  cout << "Output file: " << output_file_path.Data() << endl;

  // -------- Non compulsory command line arguments
  TString aux;

  aux = ParseCommandLine( argc, argv, "N_evts" );
  N_evts = aux.Atoi();
  cout << "Number of events: " << flush;
  if(N_evts == 0){ cout << "Not specified." << endl;}
  else{ cout << N_evts << endl;}

  aux = ParseCommandLine( argc, argv, "start_evt" );
  if(aux != "") {
    start_evt = aux.Atoi();
    cout << "[INFO]: Starting from event: " << start_evt << endl;
   }

  aux = ParseCommandLine( argc, argv, "config" );
  if(aux == ""){
    aux = "config/15may2017_new.config";
  }
  cout << "Config file: " << aux.Data() << endl;
  config = new Config(aux.Data());
  if ( !config->hasChannels() || !config->isValid() ) {
    cerr << "\nFailed to load channel information from config " << aux.Data() << endl;
    exit(0);
  }

  aux = ParseCommandLine( argc, argv, "save_raw" );
  aux.ToLower();
  if(aux == "true") save_raw =  true;

  aux = ParseCommandLine( argc, argv, "save_meas" );
  aux.ToLower();
  if(aux == "true") save_meas =  true;

  aux = ParseCommandLine( argc, argv, "draw_debug_pulses" );
  aux.ToLower();
  if(aux == "true") {
    cout << "[INFO]: Showing debug pulses" << endl;
    draw_debug_pulses =  true;
  }
}

void DatAnalyzer::InitLoop() {
    cout << "Initializing infut file reader and output tree" << endl;
    file = new TFile(output_file_path.Data(), "RECREATE");
    tree = new TTree("pulse", "Digitized waveforms");

    tree->Branch("i_evt", &i_evt, "i_evt/i");

    if(save_meas){
      tree->Branch("channel", &(channel[0][0]), Form("channel[%d][%d]/F", NUM_CHANNELS, NUM_SAMPLES));
      tree->Branch("time", &(time[0][0]), Form("time[%d][%d]/F", NUM_TIMES, NUM_SAMPLES));
    }

    for(auto n : var_names){
      var[n] = new float[NUM_CHANNELS];
      tree->Branch(n, &(var[n]), n+Form("[%d]/F", NUM_CHANNELS));
    }

    // Initialize the input file stream
    bin_file = fopen( input_file_path.Data(), "r" );
}

void DatAnalyzer::ResetVar(unsigned int n_ch) {
  for(auto n: var_names) {
    var[n][n_ch] = DEFAULT_FOR_EMPTY_CH;
  }
}

void DatAnalyzer::ResetAnalysisVariables() {
  for(unsigned int i=0; i<NUM_CHANNELS; i++) {
    for(unsigned int j=0; j<NUM_SAMPLES; j++) {
      channel[i][j] = 0;
      if(i < NUM_TIMES) time[i][j] = 0;
    }
  }
}

float DatAnalyzer::GetPulseIntegral(float *a, float *t, unsigned int i_st, unsigned int i_stop) //returns charge in pC asssuming 50 Ohm termination
{
  //Simpson's Rule for equaled space with Cartwright correction for unequaled space
  float integral = 0.;
  for (unsigned int i=i_st; i < i_stop-2 ; i+=2) {
    float aux = ( 2-(t[i+2]-t[i+1])/(t[i+1]-t[i]) ) * a[i];
    aux += (t[i+2]-t[i])*(t[i+2]-t[i])/((t[i+2]-t[i+1])*(t[i+1]-t[i])) * a[i+1];
    aux += ( 2-(t[i+1]-t[i])/(t[i+2]-t[i+1]) ) * a[i+2];

    integral += ( (t[i+2]-t[i]) / 6.0 ) * aux;
  }

  integral *= 1e-9 * 1e-3 * (1.0/50.0) * 1e12; //in units of pC, for 50Ohm termination
  return integral;
}

unsigned int DatAnalyzer::GetIdxClosest(float value, float* v, unsigned int i_st, int direction) {
  unsigned int idx_end = direction>0 ? NUM_SAMPLES-1 : 0;

  unsigned int i = i_st;
  unsigned int i_min = i_st;
  float min_distance = fabs(value - v[i]);

  while( i != idx_end ) {
    i += direction;
    float d = fabs(value - v[i]);
    if( d < min_distance ) {
      min_distance = d;
      i_min = i;
    }
  }

  return i_min;
}

unsigned int DatAnalyzer::GetIdxFirstCross(float value, float* v, unsigned int i_st, int direction) {
  unsigned int idx_end = direction>0 ? NUM_SAMPLES-1 : 0;

  bool rising = value > v[i_st]? true : false;

  unsigned int i = i_st;

  while( i != idx_end ) {
    if(rising && v[i] > value) break;
    else if( !rising && v[i] < value) break;
    i += direction;
  }

  return i;
}

void DatAnalyzer::AnalyticalPolinomialSolver(unsigned int Np, float* in_x, float* in_y, unsigned int deg, float* &out_coeff, float* err) {
  if(deg <= 0 || deg>3) { cout << "[ERROR]: You don't need AnalyticalPolinomialSolver for this" << endl; exit(0);}

  TVectorF x, x2, x3;
  x.Use(Np, in_x);

  TVectorF y;
  y.Use(Np, in_y);

  TVectorF e;
  if (err != 0) e.Use(Np, err);

  TMatrixF A(Np, deg+1);
  TMatrixFColumn(A, 0) = 1.;
  TMatrixFColumn(A, 1) = x;


  float *in_x2, *in_x3;
  if( deg >= 2 ) {
    in_x2 = new float[Np];
    for(unsigned int i = 0; i < Np; i++) in_x2[i] = in_x[i]*in_x[i];
    x2.Use(Np, in_x2);
    TMatrixFColumn(A, 2) = x2;
  }
  if ( deg >= 3 ) {
    in_x3 = new float[Np];
    for(unsigned int i = 0; i < Np; i++) in_x3[i] = in_x2[i] * in_x[i];
    x3.Use(Np, in_x3);
    TMatrixFColumn(A, 3) = x3;
  }

  TMatrixF Aw = A;
  TVectorF yw = y;
  if( err != 0 ) {
    for (Int_t irow = 0; irow < A.GetNrows(); irow++) {
      TMatrixFRow(Aw,irow) *= 1/e(irow);
      yw(irow) /= e(irow);
    }
  }

  TDecompSVD svd(Aw);
  Bool_t ok;
  const TVectorF c_svd = svd.Solve(yw,ok);

  out_coeff = new float[deg+1];
  for(unsigned int i = 0; i<= deg; i++) {
    out_coeff[i] = c_svd[i];
  }

  delete [] in_x2;
  delete [] in_x3;
  return;
}

void DatAnalyzer::Analyze(){
  for(unsigned int i=0; i<NUM_CHANNELS; i++) {
    ResetVar(i);
    if ( !config->hasChannel(i) ) {
      continue;
    }

    TString name = Form("pulse_event%d_ch%d", i_evt, i);

    // Get the attenuation/amplification scale factor and convert ADC counts to mV
    float scale_factor = (1000 * DAC_SCALE / (float)DAC_RESOLUTION) * config->getChannelMultiplicationFactor(i);


    // ------- Get baseline ------
    float baseline = 0;
    unsigned int bl_st_idx = 5;
    unsigned int bl_lenght = 150;
    for(unsigned int j=bl_st_idx; j<(bl_st_idx+bl_lenght); j++) {
      baseline += channel[i][j];
    }
    baseline /= (float) bl_lenght;


    // ------------- Get minimum position, max amplitude and scale the signal
    unsigned int idx_min = 0;
    float amp = 0;
    for(unsigned int j=0; j<NUM_SAMPLES; j++) {
      channel[i][j] = scale_factor * (channel[i][j] - baseline);
      if(( j>0 && j<NUM_SAMPLES-1 && channel[i][j] < amp) || j == 1) {
        idx_min = j;
        amp = channel[i][j];
      }
    }
    var["baseline"][i] = scale_factor * baseline;
    var["V_peak"][i] = amp;
    var["t_peak"][i] = time[GetTimeIndex(i)][idx_min];

    float baseline_RMS = 0;
    for(unsigned int j=bl_st_idx; j<=(bl_st_idx+bl_lenght); j++) {
      baseline_RMS += channel[i][j]*channel[i][j];
    }
    baseline_RMS = sqrt(baseline_RMS/bl_lenght);
    var["baseline_RMS"][i] = baseline_RMS;

    // --------------- Find closest 10% and 90% index
    unsigned int integral_l_bound = idx_min;
    unsigned int integral_r_bound = idx_min;
    unsigned int j_90_pre, j_10_pre;
    unsigned int j_90_post, j_10_post;

    unsigned int deg = 3;
    float * coeff_poly_fit = 0;
    if( fabs(amp) > 3*baseline_RMS && fabs(channel[i][idx_min+1]) > 2*baseline_RMS && fabs(channel[i][idx_min-1]) > 2*baseline_RMS) {
      j_10_pre = GetIdxFirstCross(amp*0.1, channel[i], idx_min, -1);
      j_10_post = GetIdxFirstCross(amp*0.1, channel[i], idx_min, +1);

      // -------------- Integrate the pulse
      integral_l_bound = j_10_pre;
      integral_r_bound = j_10_post;
      var["integral"][i] = GetPulseIntegral(channel[i], time[GetTimeIndex(i)], integral_l_bound, integral_r_bound);
      var["intfull"][i] = GetPulseIntegral(channel[i], time[GetTimeIndex(i)], 5, NUM_SAMPLES-5);

      // -------------- Compute rise and falling time
      j_90_pre = GetIdxFirstCross(amp*0.9, channel[i], j_10_pre, +1);
      var["risetime"][i] = time[GetTimeIndex(i)][j_90_pre] - time[GetTimeIndex(i)][j_10_pre];
      j_90_post = GetIdxFirstCross(amp*0.9, channel[i], j_10_post, -1);
      var["fallingtime"][i] = time[GetTimeIndex(i)][j_10_post] - time[GetTimeIndex(i)][j_10_post];

      AnalyticalPolinomialSolver( j_90_pre-j_10_pre , &(time[GetTimeIndex(i)][j_10_pre]), &(channel[i][j_10_pre]), deg, coeff_poly_fit);
    }



    // Draw plot of the pulse
    if(draw_debug_pulses) {
      float * yerr = new float[NUM_SAMPLES];
      for(unsigned j = 0; j < NUM_SAMPLES; j++) yerr[j] = 0 * var["baseline_RMS"][i];
      TGraphErrors* pulse = new TGraphErrors(NUM_SAMPLES, time[GetTimeIndex(i)], channel[i], 0, yerr);
      pulse->SetNameTitle("g_"+name, "g_"+name);

      TCanvas* c =  new TCanvas("c_"+name, "c_"+name, 1200, 600);
      TLine* line = new TLine();
      // Draw pulse
      pulse->SetMarkerStyle(4);
      pulse->SetMarkerSize(0.5);
      pulse->GetYaxis()->SetTitle("Amplitude [mV]");
      pulse->GetXaxis()->SetTitle("Time [ns]");
      pulse->Draw("APE1");
      // Draw baseline
      line->SetLineWidth(1);
      line->SetLineColor(46);
      line->SetLineStyle(7);
      line->DrawLine(time[GetTimeIndex(i)][0], 0, time[GetTimeIndex(i)][NUM_SAMPLES-1], 0);
      line->SetLineStyle(1);
      line->DrawLine(time[GetTimeIndex(i)][bl_st_idx], 0, time[GetTimeIndex(i)][bl_st_idx+bl_lenght], 0);
      line->SetLineColor(47);
      line->DrawLine(time[GetTimeIndex(i)][0], var["baseline_RMS"][i], time[GetTimeIndex(i)][NUM_SAMPLES-1], var["baseline_RMS"][i]);
      line->DrawLine(time[GetTimeIndex(i)][0], -var["baseline_RMS"][i], time[GetTimeIndex(i)][NUM_SAMPLES-1], -var["baseline_RMS"][i]);

      // Draw peak
      line->SetLineColor(8);
      line->SetLineStyle(4);
      line->DrawLine(time[GetTimeIndex(i)][0], var["V_peak"][i], var["t_peak"][i], var["V_peak"][i]);
      line->DrawLine(var["t_peak"][i], 0, var["t_peak"][i], var["V_peak"][i]);

      // Draw 10% and 90% lines;
      line->SetLineColor(4);
      line->DrawLine(time[GetTimeIndex(i)][0], 0.1*var["V_peak"][i], var["t_peak"][NUM_SAMPLES], 0.1*var["V_peak"][i]);
      line->DrawLine(time[GetTimeIndex(i)][0], 0.9*var["V_peak"][i], var["t_peak"][NUM_SAMPLES], 0.9*var["V_peak"][i]);

      // Draw integral area
      int N_tot_integral = integral_r_bound-integral_l_bound;
      if(N_tot_integral > 0) {
        vector<float> aux_time = {time[GetTimeIndex(i)][integral_l_bound]};
        vector<float> aux_volt = {0};
        for(unsigned int j = integral_l_bound; j <= integral_r_bound; j++) {
          aux_time.push_back(time[GetTimeIndex(i)][j]);
          aux_volt.push_back(channel[i][j]);
        }
        aux_time.push_back(time[GetTimeIndex(i)][integral_r_bound]);
        aux_volt.push_back(0);
        TGraph * integral_pulse = new TGraph(aux_time.size(), &(aux_time[0]), &(aux_volt[0]));
        integral_pulse->SetFillColor(40);
        integral_pulse->SetFillStyle(3144);
        integral_pulse->Draw("FC");
        TText* t_int = new TText(var["t_peak"][i], var["V_peak"][i], Form("Int = %1.2e (%1.2e) pC", var["integral"][i], var["intfull"][i]));
        t_int->SetTextAlign(kHAlignLeft+kVAlignBottom);
        t_int->Draw();

        // Draw 90% and 10% pre and post points
        TGraph* gr_pre_post = new TGraph(4);
        gr_pre_post->SetPoint(0, time[GetTimeIndex(i)][j_10_pre], channel[i][j_10_pre]);
        gr_pre_post->SetPoint(1, time[GetTimeIndex(i)][j_90_pre], channel[i][j_90_pre]);
        gr_pre_post->SetPoint(2, time[GetTimeIndex(i)][j_10_post], channel[i][j_10_post]);
        gr_pre_post->SetPoint(3, time[GetTimeIndex(i)][j_90_post], channel[i][j_90_post]);
        gr_pre_post->SetMarkerColor(4);
        gr_pre_post->Draw("P*");

        vector<float> polyval;
        for(unsigned int j = j_10_pre; j <= j_90_pre; j++) {
          float aux = 0;
          for( int n = 0; n <= deg; n++) {
            aux += coeff_poly_fit[n] * pow(time[GetTimeIndex(i)][j] , n);
          }
          polyval.push_back(aux);
        }
        TGraph* g_poly = new TGraph(polyval.size(), &(time[GetTimeIndex(i)][j_10_pre]), &(polyval[0]) );
        g_poly->SetLineColor(2);
        g_poly->SetLineWidth(2);
        g_poly->SetLineStyle(7);
        g_poly->Draw("C");
      }

      c->SetGrid();
      c->SaveAs("~/Desktop/debug/"+name+".png");
    }

    if(coeff_poly_fit != 0) delete coeff_poly_fit;
  }
}

void DatAnalyzer::RunEventsLoop() {
    InitLoop();

    cout << "Events loop started" << endl;
    unsigned int N_written_evts = 0;
    for( i_evt = 0; !feof(bin_file) && (N_evts==0 || i_evt<N_evts); i_evt++){
        int out = GetChannelsMeasurement();
        if(out == -1) break;

        if( i_evt >= start_evt ) {
          Analyze();

          N_written_evts++;
          tree->Fill();
        }
    }

    fclose(bin_file);
    cout << "\nProcessed total of " << N_written_evts << " events\n";

    file->Write();
    file->Close();
}
