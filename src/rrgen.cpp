
#include "rr.h"
#include "stdmp.h"
#include "usage.h"
#include "rrmean.h"
#include "summaries.h"
#include "misc.h"

#include <fstream>
#include <cmath>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <vector>

#include <gmpxx.h>
#include <gmp.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_histogram.h>
#include <gsl/gsl_fit.h>

using std::cout;
using std::endl;
using std::cerr;
using std::ios;
using std::ofstream;
using std::ifstream;

int main(int argc, char *argv[])
{
	const char* inFilename = "input.txt";
	const char* outFilename = "rr_histogram.txt";
	double* x,* y;								// Will be dynamically declared matrices of length w
	const double TWOPI = 2.0*M_PI;
	// Parameter values
	double e = 0.05, k = 0.97163540631/TWOPI;	// Redefine k to save future computation
	unsigned long int w = 10, n = 1, diff;		// Window size and overlap
	ofstream fid;							// Output file streams

	// Program control variables
	bool silent = false, isSticky = false;
	bool doFit = false, publish = true;

	// Recurrence rate variables
	double thr = 0.0, rrLast, rrCurr, rrMean;	// Sticking Threshold, recurrence rate placeholders
	int rrcntr = 0;								// Counter for redundant rp recurrences
	mpz_t winNumMax, currWin, rrEnter, tau;		// RR time variables
	double tauDoub;

	// Initialize multiprecision variables
	mpz_t l;
	mpz_init_set_ui(currWin,1);
	mpz_init(winNumMax);
	mpz_init_set_ui(l,1000);
	mpz_init_set_ui(rrEnter,1);
	mpz_init_set_ui(tau,1);

	// Initialize runtime information
	time_t t1 = time(NULL), t2, rawTime;
	struct tm * timeinfo;
	time( &rawTime );
	timeinfo = localtime( &rawTime );

	// Initialize histogram basics
	int nBins = 20;
	 
	/**********************************************************/
	// Read commandline input
	const char* optstring(":i:o:e:w:n:l:k:r:b:spfch?");
	if ( argc==1 )
	{ 
		usage_rrstdmp(e,k,l,n,w,thr,nBins); 
		exit(0);
	}

    while (optind < argc)
	{
		optarg = NULL;
		int c;
		while ( (c = getopt(argc, argv, optstring)) != -1 )
		{
			switch(c)
			{
				case 'i': //initial values input
					inFilename = argv[optind-1];
					break;
				case 'o':
					outFilename = argv[optind-1];
					break;
				case 'e':
					e = atof(optarg);
					break;
				case 'w':
					w = atoi(optarg);
					if ( (n >= w) && (n != 1) )
					{
						cerr << "\aError[1]: window is ";
						cerr << ( n > w ?"smaller than":"equal to" );
						cerr << " overlap size." << endl;
						exit(0);
					}
					break;
				case 'n':
					n = atoi(optarg);
					if (n<1)
					{
						cerr << "\aError: overlap must be greater than or \
						equal to 1." << endl;
						exit(0);
					} else if ( (n >= w) && (w != 10) )
					{
						cerr << "\aError[2]: overlap is ";
						cerr << (n>w?"greater than":"equal to");
						cerr << " window size." << endl;
						exit(0);
					}
					break;
				case 'l': 
					mpz_set_str(l,optarg,10);
					break;
				case 'k':
					k = atof(optarg)/TWOPI;
					break;
				case 'r':
					thr = atof(optarg);
					if ( thr > 1.0 || thr < 0.0 ) {
						cerr << "\aError: threshold (r) must be between 0.0 \
						and 1.0" << endl;
						exit(0);
					}
					break;
				case 'b':
					nBins = atoi(optarg);
					if (nBins < 1)
					{
						cerr << "\aError: number of bins must be larger \
						than 0." << endl;
						exit(0);
					}
					break;
				case 's':
					silent = true;
					break;
				case 'p':
					publish = false;
					break;
				case 'f':
					doFit = true;
					break;
				case 'c':
					changeLog();
					exit(0);
				case 'h':
				case '?':
				default:
					usage_rrstdmp(e,k,l,n,w,thr,nBins);
					exit(0);
			}
		}
    }
	
	/**********************************************************/	
	//Initialize variables based on commandline
	x = new double[w];
	y = new double[w];
	diff = w - n;
	mpz_cdiv_q_ui(winNumMax,l,diff);
	if(thr==0.0) thr = rrmean(k,w,n,e,rrcntr);

	// Initialize histogram
	gsl_histogram * h = gsl_histogram_alloc(nBins);
	double range[nBins+1];
	logspace(range,nBins+1,1.99,9.0);
	gsl_histogram_set_ranges(h,range,nBins+1);
	
	// open initial values file and extract initial values
	ifstream input(inFilename,ifstream::in);
	if ( !input.is_open() )
	{
		cerr << "ERROR: could not open initial value input file " << inFilename << "." << endl;
		cerr << "       A sample input file called sample_input.txt was created.\a" << endl;
		ofstream new_input("sample_input.txt",ios::out);
		new_input << 0.5 << '\t' << 0.601 << endl;
		new_input.close();
		exit(0);
	}
	input >> x[0];
	input >> y[0];
	input.close();

	double xInit = x[0];
	double yInit = y[0];

	/**********************************************************/
	// Print summary of input values
	if (!silent)
	{
		cout << endl;
		cout << argv[0] << " summary of inputs: \n" << endl;
		cout << "\tkicking strength (k) = \t " << k*TWOPI << endl;
		cout << "\trp threshold (e) = \t " << e << endl;
		cout << "\twindow size (w) = \t " << w << endl;
		cout << "\twindow overlap (n) = \t " << n << endl;
		cout << "\ttrajectory length (l) =  " << l << endl;
		cout << "\tnumber of rr values = \t " << winNumMax << endl;
		cout << "\tsticking threshold (r) = " << thr << endl;
		cout << "\thistogram bins (b) = \t " << nBins << endl;
		cout << "\tcalculate linear fit: \t " << (doFit?"Yes":"No") << endl;
		cout << "\n\t" << outFilename << " will contain the final data." << endl;
		cout << "\n\t" << inFilename << " contained the following values: " << endl;
		cout << "\tx[0] = " << x[0] << "\ty[0] = " << y[0] << endl;
		cout << "\nDate and time rrstdmp was initialized: " << asctime(timeinfo) << endl;	
	}

	/**********************************************************/
	// Kernel of program
	
	// Initial data (without overlap)
	stdmp(k,x,y,w);
	rrCurr = rrInit(e,x,y,w,n,rrcntr);

	// If begin in sticky event, exit it
	while ( (rrCurr) > thr && mpz_cmp(winNumMax, currWin) )
	{
		stdmp(k,x,y,w,n);
		rrCurr = rr(e,x,y,w,n,rrcntr);
		mpz_add_ui(currWin,currWin,1);
	}

	if (!mpz_cmp(winNumMax,currWin))
	{
		cerr << "\n\aError:\tThreshold (r) larger than largest recurrence" 
		"rate value.  No data output.\n";
		cerr << "\tConsider a smaller rr threshold (r) or larger data set (l)." << endl;
		publish = false;
		exit(0);
	}

	for (currWin; mpz_cmp(winNumMax, currWin); mpz_add_ui(currWin,currWin,1))
	{
		stdmp(k,x,y,w,n);
		rrLast = rrCurr;
		rrCurr = rr(e,x,y,w,n,rrcntr);
		
		if (rrCurr > thr && rrLast < thr)
		{
			mpz_set(rrEnter,currWin);
			isSticky = true;	
		} 
		else if (rrCurr < thr && rrLast > thr)
		{
			mpz_sub(tau,currWin,rrEnter);
			tauDoub = static_cast<double> (diff*mpz_get_ui(tau) + n);
			gsl_histogram_increment(h,tauDoub);
			isSticky = false;
		}
	}

	delete [] x;
	delete [] y;
	x = NULL;
	y = NULL;
	/**********************************************************/
	// Construct CDF

	double histSum = gsl_histogram_sum(h);
	std::vector<int> binTimes;
	
	// Find non-zero bins and store their indices in the vector binTimes[]
	for ( int i = 0; i < nBins; i++ )
		if ( gsl_histogram_get(h,i) )
			binTimes.push_back(i);
	
	// Create vectors to store the x and y histogram values that are
	//   modified and copied from the histogram information
	x = new double[binTimes.size()];
	y = new double[binTimes.size()];
	
	for ( int j = binTimes.size()-1; j >= 0; j-- )
	{
		y[j] = gsl_histogram_get(h,binTimes[j]);
		x[j] = ( range[binTimes[j]] + range[binTimes[j]+1] ) / 2.0;
	}	
	
	// Accumulate y component for CDF
	cumSumNorm(y,binTimes.size(),histSum);
	
	// Print to file
	fid.open(outFilename,ios::out);
	if (fid.is_open())
	{
		for (int i = 0; i<binTimes.size(); i++)
			fid << log10(x[i]) << '\t' << log10(y[i]) << endl;
		fid.close();
	} else
	{
		cerr << "Error: Could not open " << outFilename << endl;
		cout << "\tPrinting to screen..." << endl;
		for (int i = 0; i<binTimes.size(); i++)
			cout << log10(x[i]) << '\t' << log10(y[i]) << endl;
	}
	
	/**********************************************************/
	// Perform least square linear fit if chosen
	if (doFit)
	{
		for (int i = 0; i < binTimes.size(); i++)
		{
			x[i] = log10(x[i]);
			y[i] = log10(y[i]);
		}

		double b, m, cov00, cov01, cov11, sumsq;
		gsl_fit_linear(x,1,y,1,binTimes.size(),&b,&m, \
					   &cov00,&cov01,&cov11,&sumsq);
		double m2 = mlefit(x,binTimes.size());
		cout << "Ordinary log-log fit:" << endl;
		cout << '\t' << m << "x + " << b << endl;
		cout << "MLE fit:" << endl;
		cout << '\t' << m2 << endl; 
	}
	

	/**********************************************************/
	// End of program

	delete[] x;
	delete[] y;
	
	t2 = time(NULL) - t1;
	t1 = t2;
	if (!silent) timesummary(t1,t2);

	//Summarize results into log
	if (publish)
	{
		ofstream resultsLog("rrstdmp_results.log",ios::app);
		if (resultsLog.is_open())
		{
			resultsLog << k*TWOPI << "," << e << "," << mpz_get_d(l) << "," << w << "," << n << ",";
			resultsLog << xInit << "," << yInit << "," << t1 << "," << asctime(timeinfo);
/*			if (doFit)
				resultsLog << m << "," << m2 << ",";
 */
			resultsLog.close();
			if (!silent) cout << "Input summary written to rrstdmp_results.log" << endl;
		} else
			cerr << "Error: Could not open rrstdmp_results.log" << endl;
	}

	gsl_histogram_free(h);
	mpz_clear(l);
	
	return 0;
}
