/*
  Copyright 2012-2017 Joao Hespanha

  This file is part of Tencalc.

  TensCalc is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  TensCalc is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with TensCalc.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <time.h>
#include "math.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <unistd.h>
#elif __linux__
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

/******************************************************************************/
/* ipmPD_CSsolver() - Basic iteration of an interior point method                   */
/******************************************************************************/

/* Functions to do the basic computations, typically generated by ipmPDeq_CS.m  */
extern void initPrimalDual__();
extern void initDualEq__();
extern void initDualIneq__();
extern void updatePrimalDual__();
extern void setAlpha__(double *alpha);
extern void setMu__(double *mu);
extern void getfg__(double *f,double *g);
extern void getNorminf_G__(double *norminf_eq);
extern void getNorminf_Grad__(double *norminf_grad);
extern void getGapMinFMinLambda__(double *gap,double *ineq,double *dual);
extern void getAlphas_a__(double *primalAlpha,double *dualAlpha);
extern void getAlphas_s__(double *primalAlpha,double *dualAlpha);
extern void getMinF_a__(double *ineq);
extern void getMinF_s__(double *ineq);
extern void getRho__(double *sigma);
extern void saveWW__(char *filename);
extern void savedx_s__(char *filename);
extern void saveb_s__(char *filename);

#if debugConvergence==1
extern void getZNL__(double *z,double *nu,double *lambda);
extern void getFG__(double *F,double *G);
extern void getDnu_s__(double *dNu_s);
extern void getDlambda_s__(double *dLambda_s);
#define printf mexPrintf
#endif

/* commented out */
extern void getGap__(double *gap);

#ifdef DYNAMIC_LIBRARY
#ifdef __APPLE__
#define EXPORT __attribute__((visibility("default")))
#elif __linux__
#define EXPORT __attribute__((visibility("default")))
#elif _WIN32
#define EXPORT __declspec(dllexport)
#endif
#endif

//#define DEBUG

/*****************/
/* Main function */
/*****************/

#if verboseLevel>=2
#define printf2(...) mexPrintf(__VA_ARGS__)
#else
#define printf2(...) 
#endif

#if verboseLevel>=3
#define printf3(...) mexPrintf(__VA_ARGS__)
#else
#define printf3(...) 
#endif

#define MIN(A,B) (((A)<(B))?(A):(B))
#define MAX(A,B) (((A)>(B))?(A):(B))

void printMatrix(const char *name,double *mat,int m, int n)
{
  int i,j,nnz=0;
  if (0) {
    mexPrintf("%s[%d,%d] =\n",name,m,n);
    for (i=0;i<m;i++) {
      mexPrintf("%2d: ",i);
      for (j=0;j<n;j++) {
	mexPrintf("%10g ",mat[i+m*j]);
	if (fabs(mat[i+m*j])>1e-7) nnz++;
      }
      mexPrintf("\n"); }
  } else {
    mexPrintf("\n%s =[\n",name);
    for (i=0;i<m;i++) {
      for (j=0;j<n;j++) {
	mexPrintf("%g,",mat[i+m*j]);
	if (fabs(mat[i+m*j])>1e-7) nnz++;
      }
      mexPrintf(";"); }
    mexPrintf("]; %% (nnz=%d)\n",nnz);
  }
}

EXPORT void ipmPDeq_CSsolver(
	       /* inputs */
	       double  *mu0,
	       int32_t *maxIter,
	       int32_t *saveIter,
	       /* outputs */
	       int32_t *status,
	       int32_t *iter,
	       double  *time,
               double  *z,
               double  *nu,
               double  *lambda,
               double  *dZ_s,
               double  *dNu_s,
               double  *dLambda_s,
               double  *G,
               double  *F,
	       double  *primalAlpha_s,
	       double  *dualAlpha_s,
	       double  *finalAlpha
               )
{
  *iter=0; // iteration number

  double norminf_grad,alphaMax_=alphaMax;

#if nF>0
  double mu=*mu0,muMin=desiredDualityGap/nF/2,alpha=0,gap,ineq,ineq1,dual,primalAlpha,dualAlpha;
#if skipAffine != 1
  double sigma;
#endif
#endif 

#if nG>0
  double norminf_eq;
#endif

#if verboseLevel>=2
  double f,g;
#endif

  printf2("%s.c (skipAffine=%d,delta=%g,allowSave=%d): %d primal variables (%d+%d+%d), %d eq. constr., %d ineq. constr.\n",__FUNCTION__,skipAffine,(double)delta,allowSave,nZ,nU,nD,nX,nG,nF);
  printf3("Iter   cost1      cost2      |grad|     |eq|    inequal     dual      gap       mu      alphaA     sigma    alphaS   time [us]\n");
#if nF>0
  printf3("%3d: <-maxIter       tol->%10.2e%10.2e                    %10.2e%10.2e\n",*maxIter,gradTolerance,equalTolerance,desiredDualityGap,muMin);
#else
  printf3("%3d: <-maxIter       tol->%10.2e\n",*maxIter,gradTolerance,equalTolerance);
#endif
  
#if verboseLevel>=1
  clock_t dt0=clock();
#endif
#if verboseLevel>=3
  clock_t dt1;
#endif

  //initPrimalDual__();
  initPrimal__();

#if nF>0
  //getGap__(&gap);
  //if (mu < gap/nF/10) mu = gap/nF/10;
  setMu__(&mu);
#endif

#if nG>0
  initDualEq__();
#endif
#if nF>0
  initDualIneq__();
#endif

  while (1) {
#if verboseLevel>=3
    dt1=clock();
#endif

    (*iter)++;
    printf3("%3d:",*iter);

    if ((*iter) > (*maxIter)) {
      printf3("maximum # iterations (%d) reached.\n",*maxIter);
      (*status) = 8;
      break; }

#if debugConvergence==1
    // get "old" state
    getZNL__(z,nu,lambda);
    z+=nZ;
    nu+=nNu;
    lambda+=nF;

    getFG__(F,G);
    G+=nG;
    F+=nF;
#endif

    /*************************/
    /* Check exit conditions */
    /*************************/
#if verboseLevel>=3
    getfg__(&f,&g);
    printf3("%11.3e%11.3e",f,g);
#endif
    getNorminf_Grad__(&norminf_grad);
    printf3("%10.2e",norminf_grad);

    if (isnan(norminf_grad)) {
	printf3("  -> failed to invert hessian\n");
	(*status) = 4;
#if allowSave==1
	mexPrintf("Saving \"" saveNamePrefix "_WW.values\" due to status = 4\n");
	saveWW__(saveNamePrefix "_WW.values");
	mexPrintf("Saving \"" saveNamePrefix "_dx_s.values\" due to status = 4\n");
	savedx_s__(saveNamePrefix "_dx_s.values");
	mexPrintf("Saving \"" saveNamePrefix "_B_s.values\" due to status = 4\n");
	saveb_s__(saveNamePrefix "_B_s.values");
#endif
	break;
      }

#if nG>0
    getNorminf_G__(&norminf_eq);
#if verboseLevel>=3
    printf3("%10.2e",norminf_eq);
#endif
#else
    printf3("    -eq-  ");
#endif
#if nF>0
    getGapMinFMinLambda__(&gap,&ineq,&dual);
    printf3("%10.2e%10.2e%10.2e",ineq,dual,gap);
    if (ineq<=0) {
        printf3("  -> (primal) variables violate constraints\n");
        (*status) = 1;
        break;
    }
    if (dual<=0) {
        printf3("  -> negative value for dual variables\n");
        (*status) = 2;
        break;
    }
#else
    printf3("   -ineq-    -dual-    -gap-  ");
#endif

    if (norminf_grad<=gradTolerance
#if nF>0
        && gap<=desiredDualityGap
#endif
#if nG>0
        && norminf_eq<=equalTolerance
#endif
         ) {
               printf3("  -> clean exit\n");
               (*status) = 0;
               break;
    }

#if nF>0
    printf3("%10.2e",mu);
#else
    printf3("   -mu-   ");
#endif

#if nF==0
    /****************************************/
    /******  NO INEQUALITY CONSTRAINTS ******/
    /****************************************/
    setAlpha__(&alphaMax_);
    printf3("  -alphaA-  -sigma- ");
    printf3("%10.2e",alphaMax_);

#if allowSave==1
    if ((*iter)==(*saveIter)) {
      mexPrintf("Saving \"" saveNamePrefix "_WW.values\" due to iter = saveIter\n");
      saveWW__(saveNamePrefix "_WW.values");
      mexPrintf("Saving \"" saveNamePrefix "_dx_s.values\" due to iter = saveIter\n");
      savedx_s__(saveNamePrefix "_dx_s.values");
      mexPrintf("Saving \"" saveNamePrefix "_b_s.values\" due to iter = saveIter\n");
      saveb_s__(saveNamePrefix "_B_s.values");
    }
#endif

    updatePrimalDual__();
#else

    /******************************************/
    /******  WITH INEQUALITY CONSTRAINTS ******/
    /******************************************/

#if skipAffine==1
    printf3(" -alphaA-  -sigma-");
#else
    /*******************************************************************/
    /** Affine search direction                                       **/
    /*******************************************************************/

    getAlphas_a__(&primalAlpha,&dualAlpha);

    alpha = MIN(primalAlpha,dualAlpha);
    alphaMax_ = (alpha<alphaMax)?alpha:alphaMax;
    //mexPrintf("\nAlphaPrimal_a = %10.3e, AlphaDual_a = %10.3e\n",primalAlpha,dualAlpha);

    if (alphaMax_ >= alphaMin) {
      // try max
      alpha=alphaMax_;
      setAlpha__(&alpha);getMinF_a__(&ineq);
      if (ineq<0) {
	// try min
        alpha=alphaMin;
	setAlpha__(&alpha);getMinF_a__(&ineq);
	if (ineq>0) {
	  // try between min and max
	  for (alpha = alphaMax_*.95 ; alpha >= alphaMin ; alpha /= 2) {
	    setAlpha__(&alpha);getMinF_a__(&ineq);
	    if (ineq>=0) {
	      break; }
	  }
	  if (alpha < alphaMin) {
	    alpha = 0;
	    setAlpha__(&alpha);
	  }
	} else {
	  alpha = 0;
	  setAlpha__(&alpha);
	}
      }
    } else {
      alpha = 0;
      setAlpha__(&alpha);
    }
    printf3("%10.2e",alpha);

    // update mu based on sigma, but this only seems to be safe for:
    // 1) "long" newton steps in the affine direction
    // 2) equality constraints fairly well satisfied (perhaps not very important)
    if (alpha> alphaMax/2
#if nG>0
	&& norminf_eq<100*equalTolerance
#endif
        ) {
      getRho__(&sigma);
      if (sigma>1) sigma=1;
      if (sigma<0) sigma=0;
#if delta==2
      sigma=sigma*sigma;
#else
      sigma=sigma*sigma*sigma;
#endif
      printf3("%10.2e",sigma);
      mu = sigma*gap/nF;
      if (mu < muMin) mu = muMin;
      setMu__(&mu); 
    } else {
      printf3("  -sigma- ");
    }
#endif  // skipAffine==1

    /*******************************************************************/
    /** Combined search direction                                     **/
    /*******************************************************************/

#if debugConvergence==1
    // get combined search direction
    getDz_s__(dZ_s);
    dZ_s+=nZ;
    getDnu_s__(dNu_s);
    dNu_s+=nNu;
    getDlambda_s__(dLambda_s);
    dLambda_s+=nF;
#endif
    
    getAlphas_s__(&primalAlpha,&dualAlpha);

#if debugConvergence==1
    *(primalAlpha_s++)=primalAlpha;
    *(dualAlpha_s++)=dualAlpha;
#endif
    
#if allowSave==1
    if ((*iter)==(*saveIter)) {
      mexPrintf("Saving \"" saveNamePrefix "_WW.values\" due to iter = saveIter\n");
      saveWW__(saveNamePrefix "_WW.values");
      mexPrintf("Saving \"" saveNamePrefix "_dx_s.values\" due to iter = saveIter\n");
      savedx_s__(saveNamePrefix "_dx_s.values");
      mexPrintf("Saving \"" saveNamePrefix "_B_s.values\" due to iter = saveIter\n");
      saveb_s__(saveNamePrefix "_B_s.values");
    }
#endif

#define STEPBACK .99
    
    alpha = STEPBACK*MIN(primalAlpha,dualAlpha);
    alphaMax_ = (alpha<alphaMax)?alpha:alphaMax;
    //mexPrintf("\n\tAlphaPrimal_s=%10.3e, AlphaDual_s=%10.3e, alpha=%10.3e ",primalAlpha,dualAlpha,alphaMax_);

    if (alphaMax_ >= alphaMin) {
      // try max
      alpha=alphaMax_/STEPBACK;
      setAlpha__(&alpha);getMinF_s__(&ineq);
      //mexPrintf(" minF(maxAlpha=%10.3e)=%10.3e ",alpha,ineq);
      if (isnan(ineq)) {
	  printf3("  -> failed to invert hessian\n");
	  (*status) = 4;
#if allowSave==1
	  mexPrintf("Saving \"" saveNamePrefix "_WW.values\" due to status = 4\n");
	  saveWW__(saveNamePrefix "_WW.values");
	  mexPrintf("Saving \"" saveNamePrefix "_dx_s.values\" due to status = 4\n");
	  savedx_s__(saveNamePrefix "_dx_s.values");
	  mexPrintf("Saving \"" saveNamePrefix "_B_s.values\" due to status = 4\n");
	  saveb_s__(saveNamePrefix "_B_s.values");
#endif
	  break;
	}
      if (ineq>0) {
        alpha *= STEPBACK;
	// recheck just to be safe in case not convex
	setAlpha__(&alpha);getMinF_s__(&ineq1);
	//mexPrintf(" minF(final? alpha=%g)=%10.3e ",alpha,ineq1);
	if (ineq1>ineq/10)
	  updatePrimalDual__();
      }
      if (ineq<=0 || ineq1<=ineq/10) {
        // try min
	alpha=alphaMin/STEPBACK;
	setAlpha__(&alpha);getMinF_s__(&ineq);
	//mexPrintf(" minF(minAlpha=%10.3e)=%10.3e ",alpha,ineq);
	if (ineq>0) {
          // try between min and max
	  for (alpha = alphaMax_*.95;alpha >= alphaMin;alpha /= 2) {
            setAlpha__(&alpha);getMinF_s__(&ineq);
	    //mexPrintf(" minF(%g)=%10.3e ",alpha,ineq);
	    if (ineq>0) {
	      // backtrace just a little
              alpha *= STEPBACK;
	      // recheck just to be safe in case not convex
	      setAlpha__(&alpha);getMinF_s__(&ineq1);
	      //mexPrintf(" minF(final? alpha=%g)=%10.3e ",alpha,ineq1);
	      if (ineq1>ineq/10) {
		updatePrimalDual__();
		break; }
	    }
	  } 
	  if (alpha < alphaMin) {
	    alpha = 0;
	    setAlpha__(&alpha);
	  }
	} else {
	  alpha = 0;
	  setAlpha__(&alpha);
	}
      }
    } else {
      alpha = 0;
      setAlpha__(&alpha);
    }
#if debugConvergence==1
    *(finalAlpha++)=alpha;
#endif

    printf3("%10.2e",alpha);

#if skipAffine==1
    // More aggressive if
    // 1) "long" newton steps in the affine direction
    // 2) small gradient
    // 3) equality constraints fairly well satisfied
    // (2+3 mean close to the central path)
    int th_grad=norminf_grad<MAX(1e-1,1e2*gradTolerance);
#if nG>0
    int th_eq=norminf_eq<MAX(1e-3,1e2*equalTolerance);
#endif
    if (alpha>.5 && th_grad
#if nG>0
	&& th_eq
#endif
	) {
      mu *= muFactorAggressive;
      if (mu < muMin) mu = muMin;
      setMu__(&mu); 
      printf3(" *");
    } else {
      mu *= muFactorConservative;
      if (mu < muMin) mu = muMin;
      setMu__(&mu);
      if (alpha<0*.001) {
	mu = MIN(1e2,1.25*mu);
	setMu__(&mu);
	initDualIneq__();
      }
#if verboseLevel>=3
      if (th_grad)
	printf3("g");
      else
	printf(" ");
#if nG>0
      if (th_eq)
	printf3("e");
      else
#endif
	printf3(" ");
#endif
    }
#endif
    
    // if no motion, slowly increase mu
    if (alpha<alphaMin) {
      mu /= (muFactorConservative*muFactorConservative); // square to compensate for previous decrease
      if (mu < muMin) mu = muMin;
      setMu__(&mu); }

#endif
#if verboseLevel>=3
    dt1=clock()-dt1;
#endif
    printf3("%8.1lfus\n",dt1/(double)CLOCKS_PER_SEC*1e6);

  } // while(1)

#if allowSave==1
  if ((*saveIter)==0 && (*status)==0) {
      mexPrintf("  Saving \"" saveNamePrefix "_WW.values\" due to saveIter = 0\n");
      saveWW__(saveNamePrefix "_WW.values");
      mexPrintf("Saving \"" saveNamePrefix "_dx_s.values\" due to saveIter = 0\n");
      savedx_s__(saveNamePrefix "_dx_s.values");
      mexPrintf("Saving \"" saveNamePrefix "_B_s.values\" due to saveIter = 0\n");
      saveb_s__(saveNamePrefix "_B_s.values");
    }
#endif

#if debugConvergence==1
  // get final state 
  getZNL__(z,nu,lambda);
  z+=nZ;
  nu+=nNu;
  lambda+=nF;
  
  //getFG__(F,G);
  //G+=nG;
  //F+=nF;
#endif

  if ((*status)==8) {
    getNorminf_Grad__(&norminf_grad);
    if (norminf_grad>gradTolerance) {
      (*status) |= 16;
    }
#if nG>0
    getNorminf_G__(&norminf_eq);
    if (norminf_eq>equalTolerance) {
      (*status) |= 32;
    }
#endif
#if nF>0
    getGapMinFMinLambda__(&gap,&ineq,&dual);
    if (gap>desiredDualityGap) {
      (*status) |= 64;
    }
    if (mu>muMin) {
      (*status) |= 128;
    }
    if (alpha<=alphaMin) 
      (*status) |= 1792; // (256|512|1024);
    else if (alpha<=.1)
      (*status) |= 1536; // (512|1024);
    else if (alpha<=.5)
      (*status) |= 1024;
#endif
  }
  
#if verboseLevel>=1
  (*time)=(clock()-dt0)/(double)CLOCKS_PER_SEC;
#endif
    
#if verboseLevel>=2
  getfg__(&f,&g);
  if ((*status)<8) {
    getNorminf_Grad__(&norminf_grad);
#if nG>0
    getNorminf_G__(&norminf_eq);
#endif
#if nF>0
    getGapMinFMinLambda__(&gap,&ineq,&dual);
#endif
  }
  
  printf2("%3d:status=0x%X, ",(*iter),(*status));
  printf2("cost=%13.5e,%13.5e",f,g);
#if nG>0
  printf2(", |eq|=%10.2e",norminf_eq);
#endif
#if nF>0
  printf2(", ineq=%10.2e,\n              dual=%10.2e, gap=%10.2e, last alpha=%10.2e",ineq,dual,gap,alpha);
#endif
  printf2(", |grad|=%10.2e",norminf_grad);
  printf2(" (%.1lfus,%.2lfus/iter)\n",(*time)*1e6,(*time)/(double)(*iter)*1e6);
#endif
}

