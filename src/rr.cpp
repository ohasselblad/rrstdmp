/*
 *   This program calculates the recurrence rate of the vectors x[] and y[],
 *     dependent upon the window size w, overlap n, and recurrence threshold e.
 *     Overlap values are stored in rrcntr to reduce redundant computations for
 *     adjacent recurrence plot computations.
 *
 *   Output: recurrence rate (sparsity of the recurrence plot) of the 2-d map (x,y)
 *
 *
 *   Limitation: if n is larger than w/2, the rrmean is incorrect because the 
 *     overlap overlaps with more than one window -- so a different scheme
 *     for capturing the information from other windows needs to be devised.
 *     The program works perfectly well for n <= w/2+1, though.
 */

#include "rr.h"
#include <iostream>

double rr(double *__restrict__ x, double *__restrict__ y, \
		  int &__restrict__ rrcntr)
{
	extern const int globalWindow, globalOverlap;
	extern double ge;
	double dx = 0.0, dy = 0.0, dmax = 0.0, minusge = 1.0 - ge;
	int RR = rrcntr;	// Transfer previous overlap into current count
	rrcntr = 0;			// Reset overlap for this window
	static int diff = globalWindow - globalOverlap;
	int i, j;
	static double wsquared = static_cast<double> (globalWindow*globalWindow);

//#pragma omp parallel for private(i,j,dx,dy,dmax) \
                         shared(x,y,ge,minusge,diff,RR,rrcntr) \
                         schedule(guided) \
                         reduction(+:RR)

// Region 1 -- Copied from previous region (copy rrcntr into RR?) -- see definitions above


// Regions 2
    for (i = 0; i < 50; i++)
	{
		for (j = 50; j < 100; j++)
		{
            // Can these if statements be amenable to #pragma omp sections?
            //** Are you confident this is sufficient?  (should i and j be swapped?)
            //** Perhaps it is because we're only considering 1/2 the triangle?
            //** Perhaps it's not because we're leaving out x[i] < ge && x[j] > (1.0 - ge)?

			// Calculate recurrences; check if they are on opposite edges
			if ( x[i] > minusge && x[j] < ge )
				dx = fabs(1.0 - x[i] + x[j]);
			else
				dx = fabs(x[i] - x[j]);

			if ( y[i] > minusge && y[j] < ge )
				dy = fabs(1.0 - y[i] + y[j]);
			else
				dy = fabs(y[i] - y[j]);

			// Maximum Norm
			dx > dy ? (dmax = dx) : (dmax = dy);

			// Apply Threshold
			if (dmax < ge)
				RR++;
		}
	}

// Region 3
    for (i = 50; i < 99; i++)
    {
        for (j = i+1; j < 100; j++)
        {
            if ( x[i] > minusge && x[j] < ge )
				dx = fabs(1.0 - x[i] + x[j]);
			else
				dx = fabs(x[i] - x[j]);

			if ( y[i] > minusge && y[j] < ge )
				dy = fabs(1.0 - y[i] + y[j]);
			else
				dy = fabs(y[i] - y[j]);

            // Maximum Norm
            dx > dy ? (dmax = dx) : (dmax = dy);

            // Apply Threshold
            if (dmax < ge)
                rrcntr++;
        }
    }

    RR += rrcntr;

	return ( static_cast<double>(2 * RR + globalWindow) / wsquared );
    /* many calculations could be avoided if threshold is redefined as renormed 
     value = (thrsOld * wsquared - globalWindow) / 2.0
     If 10^6 runs through rr are given, then 4x as many calcs can be avoided
     (including the double() cast) and rr() could return an int...
     While this may speed up the program some, it would also be harder to under-
     stand.
     */
}


double rrInit(double *__restrict__ x, double *__restrict__ y, \
			  int &__restrict__ rrcntr) 
{
	extern const int globalWindow, globalOverlap;
	extern double ge;
	double dx = 0.0, dy = 0.0, dmax = 0.0;
	int RR = 0;
	rrcntr = 0;

	for (int i = 0; i < 99; i++)
	{
		for (int j = i+1; j < 100; j++)
		{
			if ( x[i] > (1.0 - ge) && x[j] < ge )
				dx = fabs(1.0 - x[i] + x[j]);
			else
				dx = fabs(x[i] - x[j]);

			if ( y[i] > (1.0 - ge) && y[j] < ge )
				dy = fabs(1.0 - y[i] + y[j]);
			else
				dy = fabs(y[i] - y[j]);

			//maximum norm
			dx > dy ? (dmax = dx) : (dmax = dy);

			// apply threshold
			if (dmax < ge)
			{
				RR++;
				if ( (i >= 50) && (j > 50) )
					rrcntr++;
			}
		}
	}

	return (static_cast<double>(2 * RR + globalWindow) / static_cast<double>(globalWindow*globalWindow));
}
