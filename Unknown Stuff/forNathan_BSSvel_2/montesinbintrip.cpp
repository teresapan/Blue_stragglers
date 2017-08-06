//this version used Nathan's mass function for the cores of GCs and also his fit to the core binary frequencies in GCs

//to run: ./montesinbintrip -o <output file name> -m <Mcluster[MSun]> -r <half-mass radius[pc]> -n <number of runs> -e <1 = 1+2, 2 = 2+2>
// e.g.,  ./montesinbintrip -m 1e3 -r 3 -n 1000 -o test.txt -e 1
// also option to set random seed with -s, and [Fe/H] value with -f, and field binary fraction with -b, and age [Myr] with -t
// also option to draw eccentricities from normal distribution with -c -2 and to draw period from uniform distribution with -p -21
// any positive -p value sets every period (in days) equal (will be overridden if -a is set)
// any positive -c value sets every eccentricity equal
// also option to set all masses equal with -z, and all radii equal with -y
// also option to set a constant scaling on the impact parameter with -x
// also option to set individual masses
// h = m0, i = m1, j = m2, k = m3 
// also option to set individual radii
// q = r0, u = r1, v = r2, w = r3
// NOTE: for 1+2, 0 = single, 1 = primary, 2 = secondary ; for 2+2, 0 = primary1, 1 = secondary1, 2 = primary2, 3 = secondary2
// usemass (-z) takes presidence over hijk, and dito for useradius (-y) over quvw

// also option to set the star IDs for the minimum values with -d and -g

// also set all semi-major axes (in AU) equal with -a (overrides -p>2) 

// set tidal tolerance in binsingle/binbin with -X

//the user inputs a total cluster mass and scale radius then this code defines a Plummer model to calculate the central velocity dispersion and the escape velocity
//the user also inputs the number of runs, whether it's binbin vs. binsingle, and the output file name

//This program runs n_bs binaries through scattering experiments
//if you you set ecnum = 1 (below), the code runs binary-single experiments
//if you you set ecnum = 2 (below), the code runs binary-binary experiments

//orbital parameters are either chosen from field distribution or set manually by the user in randparamb
//angles are chosen randomly
//velocities are either chosen from a Maxwellian, using the sigm and vesc values defined below, or defined explicitly by the user in main
//impact parameters are either chosen randomly from a uniform distribution between 0 and 1 times a1(+a2) or defined by the user in main

//----------------
//NOTE: optimization (-O3) is causing errors in the code--segmentation faults, and e>1 warnings.  So no optimization for now.
//g++ -o montesinbintrip montesinbintrip.cpp
//for debugging in gdb :
//g++ -g -o montesinbintrip montesinbintrip.cpp 
//NOTE: within gdb use "set logging file FILE" (to save output to FILE--but is this working?)

//--------------
//readcol,'test.txt',id,x,m0,m1,m2,m3,r0,r1,r2,r3,a1,a2,e1,e2,b,v,vel,p1,p2,x,mp1,ms1,mp2,ms2,e1f,e2f,a1f,a2f,p1f,p2f,vb1f,vb2f,vs1f,vs2f,tf,rm,te,tp,format='F,A,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,A,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,F,I,F'

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

using namespace std;
/////////////////////////////
// All of these can be set at the command line
////////////////////////////


//just for safety.  the Pmax value is replaced by a check for the hard-soft boundary, and the Pmin value is replaced by a check of the Roche limit of the primary
double Pmin = -5.; //log(P [days]) minimum period cutoff
double Pmax = 15.; //log(P [days]) maximum period cutoff

//can be reset from input
double Mcl = 100; //total cluster mass (MSun)
double Nstars = -1.; //set the total number of stars (not recommended, but would avoid the lengthy calculation of the number of stars)
double Rhm = 3.; //half-mass radius
double fb_field = 0.5; //field binary fraction (will be used to calculate cluster binary fraction from resulting period distribution)
int n_bs = 1; //number of encounters
int ecnum = 1; // 1 == binary-single, 2 ==  binary-binary
string ofile = "monte.bencounters.txt"; //output file name
double age = 10000.; //cluster age in Myr

float ecdist = -1; //-1=flat eccentricity distribution, -2=normal distribution fit to M35 binaries, >0 is an eccentricity
float pedist = -1; //-1=log-normal period distribution (from Raghavan 2010), -2=uniform in log(period), >0 is a period for all binaries

int doplim = 1; //1 = limit the period according the the hard-soft boundary and Roche limit, otherwise take the Pmin and Pmax above as limits

double usesma = -1.; //will be set below if user defines a semi-major axis for all binaries
double usesma2 = -1.; //will be set below if user defines an outer semi-major axis for all triples
double usesma11 = -1.; //will be set below if user defines an inner semi-major axis for triple/binary 1
double usesma12 = -1.; //will be set below if user defines an outer semi-major axis triple 1
double usesma21 = -1.; //will be set below if user defines an inner semi-major axis for triple/binary 2
double usesma22 = -1.; //will be set below if user defines an outer semi-major axis for triple 2
double usep2 = -1.; //will be set below if user defines an outer period for all triples
double usee2 = -1.; //will be set below if user defines an outer eccentricity for all triples

double usemass = -1.; // will be set below if the user defines a mass for all stars
double useradius = -1.; // will be set below if the user defines a radius for all stars

double usemass0 = -1.; // will be set below if the user defines a mass for star 0
double useradius0 = -1.; // will be set below if the user defines a radius for star 0
double usemass1 = -1.; // will be set below if the user defines a mass for star 1
double useradius1 = -1.; // will be set below if the user defines a radius for star 1
double usemass2 = -1.; // will be set below if the user defines a mass for star 2
double useradius2 = -1.; // will be set below if the user defines a radius for star 2
double usemass3 = -1.; // will be set below if the user defines a mass for star 3
double useradius3 = -1.; // will be set below if the user defines a radius for star 3
double usemass4 = -1.; // will be set below if the user defines a mass for star 4
double useradius4 = -1.; // will be set below if the user defines a radius for star 4
double usemass5 = -1.; // will be set below if the user defines a mass for star 5
double useradius5 = -1.; // will be set below if the user defines a radius for star 5

int Rmin_i = 1; //star ID1 for which we will print the minimum approach from FEWBODY
int Rmin_j = 2; //star ID2 for which we will print the minimum approach from FEWBODY

double bscale = 1.; // can be used to scale the impact parameter
double bmin = 0.; //the minimum impact parameter
double bplummer = -1.;//if !=-1 then draw impact parameters from a Plummer density distribution out to bplummer [AU], if <-1 then take the average interparticle spacing as bplummer

double useKTG = -1; //set to any value other than -1 to use the KTG IMF (Leigh is default)
double usefieldfb = -1; //set to any value other than -1 to use the Raghavan field binary period distribution to get binary frequency

double usemeanm = -1; //set to any value other than -1 to set the mean mass of the cluster (and disregard the true mean mass from the MF)

double vinf_min = -1.; // can be used to set the vinf value (given as v/vcrit)
double vinf_sc = 0.; // can be used to set the range of a uniform distribution to draw the vinf values from (if vinf_min ~=-1)
double vinf = -1.; //if vinf_min != -1, this will contain the velocity at infinity drawn from a uniform distribution
double rseed = -1; //will be reset below or on input
//double ZZ = 0.02; //metallicity (for calculating the stellar radii and luminosities)
double FeH = 0.0; //metallicity (for calculating the stellar radii and luminosities)

int docluster = 1; // =1 to run as if the stars are in a cluster, =0 to run as if the stars are in the field
double maxm = 100.; //MSun maximum mass used in choosing stars from the IMF -->will be re-defined below based on cluster age (as MSTO)
double minm = 0.1; //MSun minimum mass used in choosing stars from the IMF 
double sigma0 = -1.; //km/s velocity dispersion for the Maxwellian, but reset to values of a Plummer model given cluster mass and half-mass radius if docluster = 1
double vesc = -1.; //km/s escape velocity for the Maxwellian, but reset to values of a Plummer model given cluster mass and half-mass radius if docluster = 1

double tidaltol = 1e-05; // tidal tolerance [1e-05] for binsingle and binbin


int FB_KS = 0; //0/1=don't/do use K-S regularization in FEWBODY
double FB_CPUSTOP = 3600.; //(s) to ensure that the encounter isn't stopped by the CPU time limit (this is actually the default in FEWBODY)
double FB_XFAC = 1.; //expansion factor for collisions in FEWBODY (default is 3)
double FB_TSTOP = 1.e6; //stopping time in fewbody (in unites of dynamical times)
double FB_RELACC = 1.e-09; //relative accuracy of the integrator in fewbody

////////////////////////////////
//items below here should not be modified
///////////////

#define Zsun 0.02 //solar metallicity (as in SSE)
#define Xsun 0.7 //solar metallicity (as in SSE)
#define PI 3.14159265358979323846
#define tiny 1.0e-10
#define aursun (1.0/(695500*6.68459e-9)) //convert from AU to Rsun
#define Msun 1.989e33 //solar mass in g
#define G (6.67259e-8*Msun/(1.e5*1.e5*1.e5)) //km^3/(M_sun*s^2)  
#define pctkm 3.086e13 //conversion from pc to km (a(pc)*pctkm=a(km))
#define pctAU 206264.806 //conversion from pc to AU (a(pc)*pctAU=a(AU))
#define AUtkm 149597871. //conversion from AU to km (a(AU)*AUtkm=a(km))

//NL: Changed to 100000 from 10000 to make sure we don't stop encounters artificially. CHECK OTHER SIMILAR CRITERION RELATED TO STOPPING ENCOUNTERS EARLY!!!
int storylen=100000; //maximum character length of the story line

////////////////////

//structure for the relevant cluster values
struct cluster_t {
  double Mcl,Rhm,rpl,sigma0,vesc,fb_field,fb_hard,meanphs,meanm,rho0,Z,FeH,age,MSTO;
  long Nstars;
} cluster[1];

//structure to hold initial parameters for the encounter
struct param_bb_t {
  int id,id2;
  double m,n,o,p,l,M,r,g,i,j,u,L,a,Q,q,O,e,F,f,P,b,v,vel,period1,period2,period3,period4,phs,vpar,vperp;
} parambb[1];

//structure to hold final parameters for the encounter
struct fin_bb_t {
  int type,id1,id2,id3,id4,id5,id6;
  double a1,a2,a3,a4,e1,e2,e3,e4,P1,P2,P3,P4,mp1,mp2,mp3,ms1,ms2,ms3,mt1,mt2,vb1,vb2,vb3,vs1,vs2,vt1,vt2,Pf1,Pf2,Pf3,Pf4,af1,af2,af3,af4,t_fin,vbx,vby,vbz,vsx,vsy,vsz,Rmean,Rmin,tau_enc;
  string story;
} finbb[1];


//initialize the cluster
void initialcluster(cluster_t cluster[], int ic)//INITIAL EVERYTHING TO 0
{
  cluster[ic].age = 0.;
  cluster[ic].Mcl = 0.;
  cluster[ic].Rhm = 0.;
  cluster[ic].rpl = 0.;
  cluster[ic].sigma0 = 0.;
  cluster[ic].vesc = 0.;
  cluster[ic].fb_field = 0.;
  cluster[ic].fb_hard = 0.;
  cluster[ic].rho0 = 0.;
  cluster[ic].Z = 0.;
  cluster[ic].FeH = 0.;
  cluster[ic].meanm = 0.;
  cluster[ic].meanphs = 0.;
  cluster[ic].MSTO = 0.;
  cluster[ic].Nstars = 0;
}

//initialize the initial encounter structure
void initialparambb (param_bb_t parambb[], int i) //INITIAL EVERYTHING TO 0
{
  parambb[i].id=-1;
  parambb[i].id2=0;
  parambb[i].m=0.;
  parambb[i].n=0.;
  parambb[i].o=0.;
  parambb[i].p=0.;
  parambb[i].l=0.;
  parambb[i].M=0.;
  parambb[i].r=0.;
  parambb[i].g=0.;
  parambb[i].i=0.;
  parambb[i].j=0.;
  parambb[i].u=0.;
  parambb[i].L=0.;
  parambb[i].a=1.0e15;
  parambb[i].Q=0.;
  parambb[i].q=0.;
  parambb[i].O=0.;
  parambb[i].e=0.;
  parambb[i].F=0.;
  parambb[i].f=0.;
  parambb[i].P=0.;
  parambb[i].b=0.;
  parambb[i].v=0.;
  parambb[i].vel=0.;
  parambb[i].period1=0.;
  parambb[i].period2=0.;
  parambb[i].period3=0.;
  parambb[i].period4=0.;
  parambb[i].phs=0.;
  parambb[i].vpar=0.;
  parambb[i].vperp=0.;

}

//initialize the final encounter structure
void initialfinbb (fin_bb_t finbb[],int i) //INITIAL EVERYTHING TO 0.
{
  finbb[i].a1=0.;
  finbb[i].a2=0.;
  finbb[i].a3=0.;
  finbb[i].a4=0.;
  finbb[i].e1=0.;
  finbb[i].e2=0.;
  finbb[i].e3=0.;
  finbb[i].e4=0.;
  finbb[i].P1=0.;
  finbb[i].P2=0.;
  finbb[i].P3=0.;
  finbb[i].P4=0.;
  finbb[i].mp1=0.;
  finbb[i].mp2=0.;
  finbb[i].mp3=0.;
  finbb[i].ms1=0.;
  finbb[i].ms2=0.;
  finbb[i].ms3=0.;
  finbb[i].mt1=0.;
  finbb[i].mt2=0.;
  finbb[i].vb1=0.;
  finbb[i].vb2=0.;
  finbb[i].vb3=0.;
  finbb[i].vs1=0.;
  finbb[i].vs2=0.;
  finbb[i].vt1=0.;
  finbb[i].vt2=0.;
  finbb[i].Pf1=0.;
  finbb[i].Pf2=0.;
  finbb[i].Pf3=0.;
  finbb[i].Pf4=0.;
  finbb[i].af1=0.;
  finbb[i].af2=0.;
  finbb[i].af3=0.;
  finbb[i].af4=0.;
  finbb[i].t_fin=0.;
  finbb[i].type=0;
  finbb[i].story="";
  finbb[i].id1=-1;
  finbb[i].id2=-1;
  finbb[i].id3=-1;
  finbb[i].id4=-1;
  finbb[i].id5=-1;
  finbb[i].id6=-1;
  finbb[i].Rmean=0.;
  finbb[i].Rmin=0.;
  finbb[i].tau_enc=0.;
}



//calculate the cluster values, assuming a Plummer model
//see Heggie & Hut book (Table 1) and also Kroupa's initial conditions Chapter in N-body book

double checkstab(double q, double eout, double imu, double ain){
  //Mardling & Aarseth 2001 stability
  //also from Fabrycky & Tremaine 2007 eq. 37
  //qout=m3/(m1+m2)

  double val;
  val=2.8*pow((1.+q),(2./5.))*pow(1.+eout,2./5.)*pow(1.-eout,-6./5.)*(1.-0.3*imu/180.)*ain;

  //  fprintf(stdout,"q=%le eout=%le imu=%le\n",q,eout,imu);

 return val;
}

//draw a mass from a KTP IMF
double getKTGmass(double minm, double maxm)
{
  //  cout << "getKTGmass" << endl;

  //Kroupa, Tout & Gilmore (1993), MNRAS 262, 545 IMF generating function
  //returns a mass in Msun
  double gama1,gama2,gama3,gama4,x1,mass;
  
  //for alpha1 = 1.3 in Table 10
  gama1=0.19;
  gama2=1.55;
  gama3=0.05;
  gama4=0.6;
  mass=-999.;
  while (mass < minm || mass > maxm) { //Note: only defined properly for minm >= 0.08
	 x1=(rand() % 1000000)/1000000.;
	 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
	 mass=0.08+(gama1*pow(x1,gama2)+gama3*pow(x1,gama4))/(pow((1.-x1),0.58));
  }

  return mass;
}

void getLEIGHad(double m, double gd[2])
{
  double g,d;
  //I performed a fit to Nathan's slopes and intercepts, and I beleive this should give us what we need
  //see /Users/geller/WORK/subsubgiants/collision/checktkh.pro (might change, but this should be in the original version)
  //I will use this if the mass is outside of the limits from his Table 4

  g = 1.25609 - 0.641412*m;
  d = 1.88951 + 1.06979*m;

  /*
  if (m >= 0.65 && m < 0.75) {
	 g = 0.81;
	 d = 2.64;
  } 
  if (m >= 0.55 && m < 0.65) {
	 g = 0.91;
	 d = 2.50;
  }
  if (m >= 0.45 && m < 0.55) {
	 g = 0.93;
	 d = 2.43;
  }
  if (m >= 0.35 && m < 0.45) {
	 g = 0.99;
	 d = 2.33;
  }
  if (m >= 0.25 && m < 0.35) {
	 g = 1.12;
	 d = 2.14;
  }
  */

  gd[0] = g;
  gd[1] = d;
	 	 
}
//draw a mass from Nathan's mass function, using rejection sampling
//Leigh et al. (2012),  MNRAS, 422, 1592
double getLEIGHMFmass(double Ntot, double mmin, double mmax) 
{

  double gd[2],lfv,fv,lmfv,mfv,x1,x2;

  //Nathan's MF increases always either towards the highest or lowest mass
  getLEIGHad(mmax,gd);
  lmfv = gd[0]*log10(Ntot/1000.) + gd[1];
  mfv = pow(10.,lmfv);
  getLEIGHad(mmin,gd);
  lmfv = gd[0]*log10(Ntot/1000.) + gd[1];
  if (pow(10.,lmfv) > mfv) mfv = pow(10.,lmfv);

  //get the mass
  x1 = (rand() % 1000000)/1000000.*(mmax - mmin) + mmin;
  x2 = (rand() % 1000000)/1000000.*mfv;
  getLEIGHad(x1,gd);
  lfv = gd[0]*log10(Ntot/1000.) + gd[1];
  fv = pow(10.,lfv);
  while (x2 > fv) {
	 x1 = (rand() % 1000000)/1000000.*(mmax - mmin) + mmin;
	 x2 = (rand() % 1000000)/1000000.*mfv;
	 getLEIGHad(x1,gd);
	 lfv = gd[0]*log10(Ntot/1000.) + gd[1];
	 fv = pow(10.,lfv);
	 if (x2 <= fv) break;
  }
  
  return x1;
}

//draw a radius from a Plummer model (doesn't quite work yet)
double getPlummer_r(cluster_t cluster[], int ic)
{
  //from Kroupa chapter in N-body book
  
  double x,rdraw;
  x = (rand() % 1000000)/1000000.;
  rdraw = pow((pow(x,-2./3.) - 1.), -1./2.);

  return rdraw*(cluster[ic].rpl*pctAU);

}

//estimate the turnoff mass
double getMSTO(double age)
{
  //use formula A3 from Tout et al. 1997, MNRAS, 732, 748 for t_MSTO to invert for MSTO
  double ttol = 0.01; //Myr tolerance in age difference
  double tdiff = 1.e5;
  double mguess = 100.;
  double dm = 50.;
  double tguess = 0.;

  //must be some very efficient algorithm that is someone thought of the do this type of thing, but this should work
  while (tdiff > ttol) {
	 tguess = (2500. + 670.*pow(mguess,2.5) + pow(mguess,4.5)) / (0.033*pow(mguess,1.5) + 0.35*pow(mguess,4.5)); //in Myr
	 tdiff = abs(age - tguess);
	 if (tdiff > ttol){
		if (tguess > age){
		  mguess = mguess + dm;
		} else {
		  mguess = mguess - dm;
		}
		dm = dm/2.;
	 }
	 //	 cout << age << " " << tguess << " " << tdiff << " " << mguess << endl;
  }

  return mguess;
}

//estimate the number of stars given the total mass by drawing from Nathan's MF
double getNstars(double Mcluster)
{

  cout << "estimating the number of stars in the cluster... " << endl;

  double mguess,mguess0,mdiff,Nguess,dN,Nstars,Nstars0,mdiff0,Nave,eM,stM,tM,eMlim,Nmave,dN2;
  int i,j,Niter;

  //take some initial guess over a coarse grid, and then step through more carefully
  Niter = 5;

  Nstars0 = round(Mcluster);
  dN2 = 0;
  for (j=1; j<=Niter; j++){
	 //Number of trials to average over
	 Nave = 100*round(1.e3/Mcluster)*j;
	 if (Nave < 1) Nave = 1;

	 //pick some low mass as a starting estimate
	 Nguess = Nstars0 - dN2;
	 Nstars = Nguess;

	 //steps in N
	 dN = round(0.01*Mcluster/j);
	 
	 mdiff0 = 1e10;
	 mguess0 = 0.;
	 while (mguess0 < Mcluster) { //some large N limit
		mguess = 0.;
		if (useKTG == -1) {
		  for (i=0; i<(Nguess*Nave); i++) mguess += getLEIGHMFmass(Nguess, minm, maxm);
		} else {
		  for (i=0; i<(Nguess*Nave); i++) mguess += getKTGmass(minm, maxm);
		}		  
		mguess0 = mguess/Nave;
		mdiff = Mcluster - mguess0;
		if (abs(mdiff) < mdiff0 && mguess0 < Mcluster) {
		  mdiff0 = abs(mdiff);
		  Nstars = Nguess;
		}
		if (mguess0 > Mcluster) break;
		//		cout << Nguess << " " << mguess0 << " " << mdiff << endl;
		Nguess += dN;
	 }
	 Nstars0 = Nstars;

	 dN2 = dN;
	 if (dN == 1) break;
  }


  /*
  eMlim = minm;

  //pick some low mass as a starting estimate
  dN2 = 2;
  Nguess = Nstars0 - dN2;
  Nstars = Nguess;

  //steps in Nxs
  dN = 1;

  mdiff0 = 1e10;

  while (Nguess < (Nstars0+dN2)) { 
	 eM = 10*eMlim;
	 Nmave = 0;
	 tM = 0.;
	 stM = 0.;
	 while (eM > eMlim) {
		mguess = 0.;
		for (i=0; i<(Nguess*Nave); i++) mguess += getLEIGHMFmass(Nguess, minm, maxm);
		mguess = mguess/Nave;
		Nmave ++;
		tM += mguess;
		stM += mguess*mguess;
		if (Nmave > 5) eM = pow((stM/Nmave - tM/Nmave*tM/Nmave),0.5) * pow((Nmave/(Nmave-1.)),0.5) / pow(Nmave,0.5);
		//		cout << Nguess << " " << Nmave << " " << tM/Nmave << " " << eM << endl;
	 }
	 mguess = tM/Nmave;
	 mdiff = Mcluster - mguess;

	 if (abs(mdiff) < mdiff0 && mguess < Mcluster) {
		mdiff0 = abs(mdiff);
		Nstars = Nguess;
	 }
	 if (mguess > Mcluster) break;
	 cout << Nguess << " " << mguess << " " << mdiff << endl;
	 Nguess += dN;
  }
  */

  cout << "Nstars = " << Nstars << endl;
  return Nstars;

}

//estimate the number of stars given the total mass by drawing from the IMF
double getNstars_old(double Mcluster)
{

  cout << "estimating the number of stars ... ";

  double mdiff,mguess,mstar,eNstars,Ntol,tN,etN,Nave,stN;
  long Nstars;
  Nstars = 0;
  Nave = 0.;
  eNstars = 0.;
  etN = Mcluster;
  Ntol = 0.01*Mcluster;
//  Ntol = Mcluster;
  if (Ntol < 1) Ntol = 1.;
  while (etN > Ntol) {
	 mdiff = Mcluster;
	 mguess = 0.0;
	 mstar = 10.*maxm;
	 mstar = getKTGmass(minm, maxm);
	 tN = 0.0;
	 while (mdiff > mstar) {
		mguess += mstar;
		tN ++;
		mdiff = Mcluster - mguess;
		if (mdiff < 0) break;
		mstar = getKTGmass(minm, maxm);
	 }
	 Nave ++;
	 Nstars += tN;
	 eNstars += tN*tN;

	 stN = tN;
	 tN = Nstars/Nave;
	 if (Nave > 5) etN = pow((eNstars/Nave - tN*tN),0.5) * pow((Nave/(Nave-1.)),0.5) / pow(Nave,0.5);

	 //	 cout << Nave << " " << stN << " " << Nstars/Nave << " " << mguess << " " << Mcluster << " " << mdiff << " " << etN << " " << Ntol << endl;
  }
  cout << Nstars/Nave << endl;

  return Nstars/Nave;

}


//get the main-sequence radius, given a mass
double getradius(double m, double ZZ) 
{
  //  cout << "getradius" << endl;

  //taken from Tout et al. (1996) MNRAS, 281, 257
  //also used in SSE 
  //given a mass in units of Msun, this function returns a radius in units of Rsun

  double t,i,k,g,u,v,e,o,p;
  double radn,radd,lgz;
  double rad=1.0;

  lgz = log10(ZZ/Zsun);

  t =  1.71535900 + 0.62246212*lgz -  0.92557761*lgz*lgz -  1.16996966*lgz*lgz*lgz - 0.30631491*lgz*lgz*lgz*lgz;
  i =  6.59778800 - 0.42450044*lgz - 12.13339427*lgz*lgz - 10.73509484*lgz*lgz*lgz - 2.51487077*lgz*lgz*lgz*lgz;
  k = 10.08855000 - 7.11727086*lgz - 31.67119479*lgz*lgz - 24.24848322*lgz*lgz*lgz - 5.33608972*lgz*lgz*lgz*lgz;
  g =  1.01249500 + 0.32699690*lgz -  0.00923418*lgz*lgz -  0.03876858*lgz*lgz*lgz - 0.00412750*lgz*lgz*lgz*lgz;
  u =  0.07490166 + 0.02410413*lgz +  0.07233664*lgz*lgz +  0.03040467*lgz*lgz*lgz + 0.00197741*lgz*lgz*lgz*lgz;
  v =  0.01077422;
  e =  3.08223400 + 0.94472050*lgz -  2.15200882*lgz*lgz -  2.49219496*lgz*lgz*lgz - 0.63848738*lgz*lgz*lgz*lgz;
  o = 17.84778000 - 7.45345690*lgz - 48.96066856*lgz*lgz - 40.05386135*lgz*lgz*lgz - 9.09331816*lgz*lgz*lgz*lgz;
  p =  0.00022582 - 0.00186899*lgz +  0.00388783*lgz*lgz +  0.00142402*lgz*lgz*lgz - 0.00007671*lgz*lgz*lgz*lgz;

  radn = t*pow(m,2.5) + i*pow(m,6.5) + k*pow(m,11) + g*pow(m,19) + u*pow(m,19.5);
  radd = v +e*m*m + o*pow(m,8.5) + pow(m,18.5) + p*pow(m,19.5);
  if (radd != 0 && radd > 0 && radn > 0) rad=radn/radd;

  return rad;
}

//get the main-sequence luminosity, given a mass
double getluminosity(double m, double ZZ)
{
  //  cout << "getluminosity" << endl;

  //taken from Tout et al. (1996) MNRAS, 281, 257
  //also used in SSE 
  //given a mass in units of Msun, this function returns a luminsosity in units of Lsun

  double a,b,g,d,e,f,n;
  double lumn,lumd,lgz;
  double lum=1.0;

  lgz = log10(ZZ/Zsun);

  a = 0.39704170 -  0.32913574*lgz +  0.34776688*lgz*lgz +  0.37470851*lgz*lgz*lgz + 0.09011915*lgz*lgz*lgz*lgz;
  b = 8.52762600 - 24.41225973*lgz + 56.43597107*lgz*lgz + 37.06152575*lgz*lgz*lgz + 5.45624060*lgz*lgz*lgz*lgz;
  g = 0.00025546 -  0.00123461*lgz -  0.00023246*lgz*lgz +  0.00045519*lgz*lgz*lgz + 0.00016176*lgz*lgz*lgz*lgz;
  d = 5.43288900 -  8.62157806*lgz + 13.44202049*lgz*lgz + 14.51584135*lgz*lgz*lgz + 3.39793084*lgz*lgz*lgz*lgz;
  e = 5.56357900 - 10.32345225*lgz + 19.44322980*lgz*lgz + 18.97351347*lgz*lgz*lgz + 4.16903097*lgz*lgz*lgz*lgz;
  f = 0.78866060 -  2.90870942*lgz +  6.54713531*lgz*lgz +  4.05606657*lgz*lgz*lgz + 0.53287322*lgz*lgz*lgz*lgz;
  n = 0.00586685 -  0.01704237*lgz +  0.03872348*lgz*lgz +  0.02570041*lgz*lgz*lgz + 0.00383376*lgz*lgz*lgz*lgz;

  lumn=a*pow(m,5.5) + b*pow(m,11.);
  lumd=g + pow(m,3.) + d*pow(m,5.) + e*pow(m,7.) + f*pow(m,8.) + n*pow(m,9.5);

  if (lumd !=0 && lumd > 0 && lumn > 0) lum=lumn/lumd;

  return lum;
}

//draw a velocity from a lowered maxwellian using rejection sampling
// can derive this from a Maxwell-Boltzman distribution
// Normal Maxwell-Boltzman :
// f(v) = C * j^3. * v^2. * exp(-1.* j^2. * v^2.) , where C is a constant 
// j = sqrt(m / (2. * m0 * sigma0^2.)), where m0 is mean mass, and sigma0 is 1D velocity dispersion
// for lowered Maxwellian, golling Spitzer's book
// f(v) = C* j^3. * v^2. * (exp(-1. * j^2. * v^2.) - exp(-1. * j^2. * vesc^2.))
// note that both sigma0 and vesc are 1D velocities
// since here we only need the shape of the distribution, we can ignore the C*j^3 prefactor
double getlmvel(cluster_t cluster[], int ic, double mass) 
{

  //first find the maximum of the lowered maxwellian distribution (mfv) numerically
  int Nval=5000;
  double xx,fv,mfv,x1,x2,j2;
  j2 = mass / (2. * cluster[ic].meanm * cluster[ic].sigma0 * cluster[ic].sigma0);

  mfv=0.;
  for (int ii=0; ii<Nval; ii++) {
	 xx=double(ii)/double(Nval)*cluster[ic].vesc;
	 //previous versions used this, which implicitly assumes that m = 3.*m0 (probably not too bad if we are considering a few-body encounter)
	 //fv=xx*xx/(cluster[ic].sigma0*cluster[ic].sigma0)*( exp(-1.5*xx*xx/(cluster[ic].sigma0*cluster[ic].sigma0)) - exp(-1.5*cluster[ic].vesc*cluster[ic].vesc/(cluster[ic].sigma0*cluster[ic].sigma0)));
	 fv = xx*xx * ( exp(-1. * j2 * xx*xx) - exp(-1.* j2 * cluster[ic].vesc*cluster[ic].vesc));
	 if (fv > mfv) mfv=fv;
  }
  
  //get the velocity
  x1=(rand() % 1000000)/1000000.*cluster[ic].vesc;
  x2=(rand() % 1000000)/1000000.*mfv;
  //fv=x1*x1/(cluster[ic].sigma0*cluster[ic].sigma0)*( exp(-1.5*x1*x1/(cluster[ic].sigma0*cluster[ic].sigma0)) - exp(-1.5*cluster[ic].vesc*cluster[ic].vesc/(cluster[ic].sigma0*cluster[ic].sigma0)));
  fv = x1*x1 * ( exp(-1. * j2 * x1*x1) - exp(-1.* j2 * cluster[ic].vesc*cluster[ic].vesc));
  while (x2 > fv) {
	 x1=(rand() % 1000000)/1000000.*cluster[ic].vesc; 
	 x2=(rand() % 1000000)/1000000.*mfv;
	 //fv=x1*x1/(cluster[ic].sigma0*cluster[ic].sigma0)*( exp(-1.5*x1*x1/(cluster[ic].sigma0*cluster[ic].sigma0)) - exp(-1.5*cluster[ic].vesc*cluster[ic].vesc/(cluster[ic].sigma0*cluster[ic].sigma0)));
	 fv = x1*x1 * ( exp(-1. * j2 * x1*x1) - exp(-1.* j2 * cluster[ic].vesc*cluster[ic].vesc));
	 if (x2 <= fv) break;
  }
  
  return x1;
}

double getvinf(cluster_t cluster[], int ic, param_bb_t parambb[], int ip, double mass1, double mass2) 
{
  double v1mag, v1[3], v2mag, v2[3], vrel[3], vpar1[3], vpar2[3], vpar[3], vpar_mag, vperp1[3], vperp2[3], vperp[3], vperp_mag;
  double xx, theta, tv, vpar_fac;

  //choose two (3D) velocity magnitudes from the lowered Maxwellian
  v1mag = getlmvel(cluster, ic, mass1);
  v2mag = getlmvel(cluster, ic, mass2);

  //now choose their vectors isotropically
  //following Kroup's chapter on N-body initial conditions
  xx = (rand() % 1000000)/1000000.;
  theta = (rand() % 1000000)/1000000.*2.*PI;
  v1[2] =  (2.*xx - 1.)*v1mag;
  tv = pow( (v1mag*v1mag - v1[2]*v1[2]), 0.5);
  v1[0] = tv*cos(theta);
  v1[1] = tv*sin(theta);

  xx = (rand() % 1000000)/1000000.;
  theta = (rand() % 1000000)/1000000.*2.*PI;
  v2[2] =  (2.*xx - 1.)*v2mag;
  tv = pow( (v2mag*v2mag - v2[2]*v2[2]), 0.5);
  v2[0] = tv*cos(theta);
  v2[1] = tv*sin(theta);

  //calculate the relative velocity vector
  for (int i=0; i<3; i++) vrel[i] = v1[i] - v2[i];

  //find the parallel project of v1 and v2 onto vrel (see, e.g., http://www.maths.usyd.edu.au/u/MOW/vectors/vectors-10/v-10-2.html)
  vpar_fac = (v1[0]*vrel[0] + v1[1]*vrel[1] + v1[2]*vrel[2])/(vrel[0]*vrel[0] + vrel[1]*vrel[1] + vrel[2]*vrel[2]);
  for (int i=0; i<3; i++) vpar1[i] = vpar_fac*vrel[i];

  vpar_fac = (v2[0]*vrel[0] + v2[1]*vrel[1] + v2[2]*vrel[2])/(vrel[0]*vrel[0] + vrel[1]*vrel[1] + vrel[2]*vrel[2]);
  for (int i=0; i<3; i++) vpar2[i] = vpar_fac*vrel[i];

  //add these together for vpar
  //assume that we can rotate our coordinate system such that vpar points along a new x direction.
  //the magnitud of vpar = vpar_mag can then be fed into FEWBODY as vinf
  for (int i=0; i<3; i++) vpar[i] = vpar1[i] + vpar2[i];
  vpar_mag = pow(vpar[0]*vpar[0] + vpar[1]*vpar[1] + vpar[2]*vpar[2], 0.5);
  parambb[ip].vpar = vpar_mag; //this is vinf for FEWBODY

  //find the perpendicular project of v1 and v2 onto vrel (see, e.g., http://www.maths.usyd.edu.au/u/MOW/vectors/vectors-10/v-10-2.html)
  for (int i=0; i<3; i++) vperp1[i] = v1[i] - vpar1[i];
  for (int i=0; i<3; i++) vperp2[i] = v2[i] - vpar2[i];

  //add these together for vperp
  //following our previous rotation, take the magnitude of vperp = vperp_mag to point along the y axis
  //(this may formally be the z axis, but we don't care about this because FEWBODY will randomly orient a binary)
  //we will then save this perpendicular magnitude as add it to the y component of whatever velocity FEWBODY spits out before printin a final velocity to the output files
  for (int i=0; i<3; i++) vperp[i] = vperp1[i] + vperp2[i];
  vperp_mag = pow(vperp[0]*vperp[0] + vperp[1]*vperp[1] + vperp[2]*vperp[2], 0.5);
  xx = (rand() % 1000000)/1000000.;
  if (xx > 0.5){
	 parambb[ip].vperp = vperp_mag;
  } else {
	 parambb[ip].vperp = -1.*vperp_mag;
  }

  //cout << v1mag << " " << v2mag << " " << v1[0] << " " << v1[1] << " " << v1[2] << " " << v2[0] << " " << v2[1] << " " << v2[2] << " " <<  parambb[ip].vpar << " " <<  parambb[ip].vperp << endl;

  return parambb[ip].vpar;


}
void getcluster(cluster_t cluster[], int ic)
{
  double phi0,meanm,mtmp,mstar;
  double Nmean = 1e6; //for mean mass

  //Plummer scale radius [pc]
  cluster[ic].rpl = cluster[ic].Rhm * pow( (pow(2.,(2./3.)) - 1.), 0.5);
  //mass density at r=0
  //  cluster[ic].rho0 = 3.*cluster[ic].Mcl/(4.*PI*pow(cluster[ic].rpl,3.));
  //number density at r=0
  cluster[ic].rho0 = 3.*cluster[ic].Nstars/(4.*PI*pow(cluster[ic].rpl,3.));
  //specific potential at r=0
  phi0 = -1.*G*cluster[ic].Mcl/(cluster[ic].rpl*pctkm);
  //1D velocity dispersion at r=0 [km/s]
  cluster[ic].sigma0 = pow( (-1./6.*phi0 ), 0.5);
  //escape velocity at r=0 [km/s]
  cluster[ic].vesc = pow( (-2.*phi0), 0.5);

  //mean mass
  meanm = 0.;
  for (int j=0; j<Nmean; j++){
	 //	 mstar = 10.*maxm;
	 //	 while (mstar > maxm || mstar < minm) mstar = getKTGmass();
	 if (useKTG == -1){
		mstar = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm);
	 } else {
		mstar = getKTGmass(minm, maxm);
	 }
	 meanm += mstar;
  }
  cluster[ic].meanm = meanm/Nmean;

}

//use Nathan's fit to the ACS data in Leigh et al. (2013), MNRAS, 428, 897
//to estimate the core binary fraction
void getLEIGHfbc(cluster_t cluster[], param_bb_t parambb[], int ic)
{

  //first get the mass inside rc from a Plummer model (see Heggie's book, Table 8.1, page 73)
  //  rpl = Rhm * sqrt(2.^(2./3.) - 1.)
  //  rc = rpl / sqrt(2.)
  //  Mrc = Mcl*(1. + (rpl/rc)^2.)^(-3./2.)
  double Mrc = cluster[ic].Mcl*pow(3.,(-3./2.));

  cluster[ic].fb_hard = 2.8*pow(Mrc, -0.37); //just guessing at the constant in front (but Nathan says OK)

  //also do this just so that I can print it out
  //hard binary frequency
  //take the mean hard-soft boundary period
  double meanphs = 0.;
  for (int j=0; j<n_bs; j++){
	 meanphs += parambb[j].phs;
  }
  meanphs = meanphs/n_bs;
  cluster[ic].meanphs = meanphs;

}

void getclusterfb(cluster_t cluster[], param_bb_t parambb[], int ic)
{

  //hard binary frequency
  //take the mean hard-soft boundary period
  double meanphs = 0.;
  for (int j=0; j<n_bs; j++){
	 meanphs += parambb[j].phs;
  }
  meanphs = meanphs/n_bs;
  cluster[ic].meanphs = meanphs;

  //now integrate the log-normal distribution to find the frequency of hard binaries
  double dP = 0.001; 
  double mP = 5.03;
  double sP = 2.28;
  double fP = 0.;
  double P = Pmin;
  double norm = 1./(sP * pow((2.*PI), 0.5));
  while (P < meanphs) {
	 fP += exp(-0.5 * pow( ((P - mP)/sP) , 2.));
	 P += dP;
  }


  fP = norm*(fP*dP);
  double Nb = cluster[ic].Nstars*cluster[ic].fb_field;
  double Ns = cluster[ic].Nstars - Nb;
  double Nbnew = Nb*fP;
  double Nsnew = Ns + 2.*(1.-fP)*Nb;
  cluster[ic].fb_hard = fP*cluster[ic].fb_field; //assumes the total number of stars stays the same, but the fraction of binaries changes
  //cluster[ic].fb_hard = Nbnew/(Nbnew + Nsnew);
  cout << "fb " <<  cluster[ic].fb_field << " " << fP*cluster[ic].fb_field << " " << cluster[ic].fb_hard << " " << cluster[ic].Nstars << " " << Nbnew+Nsnew << endl;

}

//calculate the rate of encouter+1 + encounter+2, for disrupting an ongoing encounter
void gettau_enc(cluster_t cluster[], fin_bb_t finbb[], int i)
{
  double tmp,tau_enc;

  double vrms = cluster[0].sigma0*pow(3.,0.5);
  tmp = (1000.0 / cluster[0].rho0) * (vrms/5.0) * (0.5/cluster[0].meanm) * (1.0/ (finbb[i].Rmean/aursun));

  tau_enc = 0.0;
  if (ecnum == 1){
	 double tau_32 = 4.2*pow(10.0,10.0) * (1./cluster[0].fb_hard)       * tmp;
	 double tau_31 = 5.4*pow(10.0,10.0) * (1./(1.0-cluster[0].fb_hard)) * tmp;
	 tau_enc = 1./(1.0/tau_32 + 1.0/tau_31);
  }
  if (ecnum == 2){
	 double tau_42 = 1.8*pow(10.0,10.0) * (1./cluster[0].fb_hard)       * tmp;
	 double tau_41 = 4.2*pow(10.0,10.0) * (1./(1.0-cluster[0].fb_hard)) * tmp;
	 tau_enc = 1./(1.0/tau_42 + 1.0/tau_41);
  }


  finbb[i].tau_enc = tau_enc;
}

//FINDS NUMBER OF STARS FOR ARRAY SIZES, from the FEWBODY output
int getn_in(string file) 
{
  string line,junk;
  int n_in=-1;

  ifstream binout(file.c_str());
  while (! binout.eof()){
	 getline(binout,line);
	 //This first one will always tell me the number of particles
	 if (line.substr(0,4) == "(Par" && n_in == -1){
		binout >> junk >> junk >> n_in;
		getline(binout,line);
		break;
	 }
	 if (binout.eof()) break;
  }
  binout.close();

  return n_in;
}


void getdata(string file, string efile, double t[], double m[], double r[][3],double v[][3], double R_eff[],char story[],double M0[],double V0[],double D0[],double T0[], double t_fin[], int n_in, string cmd, double Rmean[], double Rmin[]) //READS IN THE DATA
{
  //  cout << "getdata" << endl;
  int checki,checkj;
  int i=0;
  int s=0;
  int np=0;
  int Nchar=0;
  int compflag=1;
  string line, junk,statp,stat,endstory;
  string::size_type pos1,pos2,ptest,ptest2;
  ifstream binout(file.c_str());
  ifstream ebinout(efile.c_str());
  Rmin[0] = -1.;
  sprintf(story,"%s","");
  junk="";
  for (int j=0;j<n_in;j++) m[j]=0.0;
  //  while (! binout.eof() and i < n_in){
  while (! ebinout.eof()) {
	 getline(ebinout,line);
	 if (line.substr(0,4) == "UNIT"){
		getline(ebinout,line);
		pos1 = line.find("v_crit=")+7;
		pos2 = line.find("km")-1;
		V0[0]=atof(line.substr(pos1,pos2-pos1).c_str());
		pos1=line.find("l=")+2;
		pos2=line.find("AU")-1;
		D0[0]=atof(line.substr(pos1,pos2-pos1).c_str())*AUtkm; //1.496e8;
		pos1=line.find("t_dyn=")+6;
		pos2=line.find("yr")-1;
		T0[0]=atof(line.substr(pos1,pos2-pos1).c_str());
		getline(ebinout,line);
		pos1=line.find("M=")+2;
		pos2=line.find("M_sun")-1;
		M0[0]=atof(line.substr(pos1,pos2-pos1).c_str());
	 }
	 if (line.substr(0,9) == "  t_final"){
		//		fprintf(stdout,"%s\n",line.c_str());
		pos1 = line.find("(")+1;
		pos2=line.find("yr")-1;
		t_fin[0]=atof(line.substr(pos1,pos2-pos1).c_str());
		//		//		cout << "in getdata : t_fin " << t_fin[0] <<endl;

		//		fprintf(stdout,"%f\n",t_fin[0]);
		//		fprintf(stdout,"%s\n",line.substr(pos1,pos2-pos1).c_str());
	 }
	 if (line.substr(0,7) == "OUTCOME") {
		//check to be sure that this outcome is the same as the one in the ofile
		getline(ebinout,line);
		pos1 = line.find("NOT complete");
		if (pos1 < 150) {
		  fprintf(stdout,"\nWARNING! Encounter NOT complete.\n");
		  fprintf(stdout,"%s\n\n",cmd.c_str());
		  compflag = 0;
		}
		pos1 = line.find("nobj");
		pos2 = line.rfind("(");
		if (pos1 < 150 && pos2 < 150) {
		  stat=line.substr(pos1+9,pos2-pos1-11);
		  ptest = stat.find("OUTCOME"); //this will happen if the encounter doesn't finish in the maximum CPU time
		  ptest2 = stat.find(")Log"); //this will happen if the encounter doesn't finish in the maximum CPU time
		  if (ptest >100 && ptest2 > 100) endstory=stat;
		}
	 }
	 if (line.substr(0,7) == "  Rmean"){
		pos1 = line.find("(")+1;
		pos2=line.find("RSUN")-1;
		Rmean[0]=atof(line.substr(pos1,pos2-pos1).c_str())/2.; //since FEWBODY is giving me a maximum distance, i.e. diameter
	 }
	 if (line.substr(0,6) == "  Rmin"){
		pos1 = line.find("Rmin_i")+7;
		checki = atoi(line.substr(pos1,1).c_str());
		pos1 = line.find("Rmin_j")+7;
		checkj = atoi(line.substr(pos1,1).c_str());
		if ( (checki == Rmin_i && checkj == Rmin_j) || (checki == Rmin_j && checkj == Rmin_i)){
		  pos1 = line.find("(")+1;
		  pos2=line.find("RSUN")-1;
		  Rmin[0]=atof(line.substr(pos1,pos2-pos1).c_str()); 
		}
		//		cout << checki << " " << checkj << " " << Rmin[0] << endl;

	 }

	 if (ebinout.eof()) break; //safety check
  }
  ebinout.close();

  stat="";

  while (! binout.eof()){
	 getline(binout,line);
	 //GET THE TOTAL TIME FOR THE ENCOUNTER

	 if (line.substr(0,4) == "(Par") while (line.substr(0,4) != ")Par"){	
		  getline(binout,line);
		  if (line.substr(0,5) == "  cur") {
			 //GATHER BINARY/EXCHANGE/MERGER INFO FROM STATUS LINES INTO story
			 statp=stat;
			 stat=line;
			 pos1 = stat.find("nobj");
			 pos2 = stat.rfind("(");
			 if (pos1 < 150 && pos2 < 150) {
				stat=stat.substr(pos1+9,pos2-pos1-11);
				ptest = stat.find("OUTCOME"); //this will happen if the encounter doesn't finish in the maximum CPU time
				ptest2 = stat.find(")Log"); //this will happen if the encounter doesn't finish in the maximum CPU time (or maybe if too many lines for fewbody array?)
				if (statp.compare(stat) != 0 && ptest > 100 && ptest2 > 100){
				  Nchar+=stat.length();
				  if (Nchar >= storylen) fprintf(stdout,"\nWARNING! story length is too long, missing : %s\n\n",stat.c_str());
				  if (Nchar < storylen) sprintf(story,"%s %s %s",story,stat.c_str(),"|");
				}
			 }
		  }
		  if (line.substr(0,4) =="(Dyn"){
			 np++;
			 if (np > 1) {
				binout >> junk >> junk >> t[i];
				t[i]=t[i]*T0[0];
				binout >> junk >> junk >> m[i];
				m[i]=m[i]*M0[0];
				binout >> junk >> junk >> r[i][0] >> r[i][1] >> r[i][2];
				for (int j=0; j<3; j++) r[i][j]=r[i][j]*D0[0];
				binout >> junk >> junk >> v[i][0] >> v[i][1] >> v[i][2];
				for (int j=0; j<3; j++) v[i][j]=v[i][j]*V0[0];
				//				binout >> junk >> junk >> R_eff[i];
				//				R_eff[i]=R_eff[i]*D0[0];
				i++;
				if (i > n_in) break; //safety check
			 }
		  }
		}
	 if (binout.eof() || i > n_in) break; //safety check
  }
  binout.close();

  //check that the last story line from ofile matches the outcome in efile
  //identify the last story line
  junk=story;
  pos1=junk.rfind("|");
  junk=junk.substr(0,pos1);
  pos1=junk.rfind("|")+1;
  if (pos1 > junk.length()) pos1=0;
  junk=junk.substr(pos1,junk.size()-pos1);
  pos1=junk.find_first_not_of(" ");
  pos2=junk.find_last_not_of(" ");
  junk=junk.substr(pos1,pos2-pos1+1);
  if (endstory.compare(junk) != 0) {
	 fprintf(stdout,"\nWARNING! last story line in binout.o.txt is not the same as the OUTCOME in binout.e.txt : %s  vs. %s\n\n",junk.c_str(),endstory.c_str());
	 Nchar+=endstory.length();
	 if (Nchar >= storylen) fprintf(stdout,"\nWARNING! story length is too long, missing : %s\n\n",endstory.c_str());
	 if (endstory.length() == 0) fprintf(stdout,"\nWARNING! endstory is empty!! \n\n");
	 if (Nchar < storylen && endstory.length() > 0) sprintf(story,"%s %s %s",story,endstory.c_str(),"|");
  }
	

  //to make a flag if the encounter is not complete
  if (compflag == 0) {
	 sprintf(story,"%s %s",story,"NOT_COMPLETE");
  }

}


//calculate the binary parameters given positions and velocities (after the encounter)
void getbinfinvals(int bin[], double m[],double r[][3], double v[][3], double finval[],int bID, int typ, double vperp) //CALCULATE THE FINAL E,L,a,e,P
{
  //  cout << "getbinfinvals" << endl;

  //CENTER OF MASS COORDINATES
  double vsing=0;
  double vsing1,vsing2;
  double vx1,vy1,vz1,vx2,vy2,vz2,x1,y1,z1,x2,y2,z2,m1,m2;

  
  //  if (bin[2] != -1 && bin[3] == -1) vsing=sqrt(pow(v[bin[2]][0],2)+pow(v[bin[2]][1],2)+pow(v[bin[2]][2],2));
  //  if (bin[2] != -1 && bin[3] != -1) {
  //	 //IF WE HAVE TWO OTHER STARS, THEN TAKE THE CoM VELOCITY
  //	 vsing1=sqrt(pow(v[bin[2]][0],2)+pow(v[bin[2]][1],2)+pow(v[bin[2]][2],2));
  //	 vsing2=sqrt(pow(v[bin[3]][0],2)+pow(v[bin[3]][1],2)+pow(v[bin[3]][2],2));
  //	 vsing=(m[bin[2]]*vsing1+m[bin[3]]*vsing2)/(m[bin[2]]+m[bin[3]]);
  //  }
  //  finval[5]=vsing;

  vsing1=0.0;
  vsing2=0.0;
  //if (bin[2] != -1) vsing1=sqrt(v[bin[2]][0]*v[bin[2]][0]+v[bin[2]][1]*v[bin[2]][1]+v[bin[2]][2]*v[bin[2]][2]);
  //if (bin[3] != -1) vsing2=sqrt(v[bin[3]][0]*v[bin[3]][0]+v[bin[3]][1]*v[bin[3]][1]+v[bin[3]][2]*v[bin[3]][2]);
//add on the perpendicular component of the velocity not originally sent into FEWBODY
  if (bin[2] != -1) vsing1=sqrt(v[bin[2]][0]*v[bin[2]][0] + (v[bin[2]][1] + vperp)*(v[bin[2]][1] + vperp) + v[bin[2]][2]*v[bin[2]][2]);
  if (bin[3] != -1) vsing2=sqrt(v[bin[3]][0]*v[bin[3]][0] + (v[bin[3]][1] + vperp)*(v[bin[3]][1] + vperp) + v[bin[3]][2]*v[bin[3]][2]);

  finval[5]=vsing1;
  finval[7]=vsing2; //awkward order, but safest not to change things any further

  double rcm[3],vcm[3],rmod[2][3],vmod[2][3];
  for (int i=0; i<3; i++){
	 rcm[i]=(1/(m[bin[0]]+m[bin[1]]))*(m[bin[0]]*r[bin[0]][i]+m[bin[1]]*r[bin[1]][i]);
	 rmod[0][i]=r[bin[0]][i]-rcm[i];
	 rmod[1][i]=r[bin[1]][i]-rcm[i];
	 vcm[i]=(1/(m[bin[0]]+m[bin[1]]))*(m[bin[0]]*v[bin[0]][i]+m[bin[1]]*v[bin[1]][i]);
	 vmod[0][i]=v[bin[0]][i]-vcm[i];
	 vmod[1][i]=v[bin[1]][i]-vcm[i];
	 //	 cout << rmod[0][i] << " " << rmod[1][i] << " " << r[bin[0]][0] << " " << r[bin[1]][i] << " " << rcm[i] << endl;

  }


  double sep=sqrt(pow(rmod[0][0]-rmod[1][0],2.)+pow(rmod[0][1]-rmod[1][1],2.)+pow(rmod[0][2]-rmod[1][2],2.));
  //fprintf(stdout,"     %g %g %g %g %g %g\n",rmod[0][0],rmod[1][0],rmod[0][1],rmod[1][1],rmod[0][2],rmod[1][2]);
  //double vbin=sqrt(vcm[0]*vcm[0]+vcm[1]*vcm[1]+vcm[2]*vcm[2]);
//add on the perpendicular component of the velocity not originally sent into FEWBODY
  double vbin=sqrt(vcm[0]*vcm[0] + (vcm[1] + vperp)*(vcm[1] + vperp) + vcm[2]*vcm[2]);

  finval[6]=vbin;
  
  //BINARY ANGULAR MOMENTUM
  double Lx[2],Ly[2],Lz[2],L[2];
  for (int i=0; i<2; i++) Lx[i]=m[bin[i]]*(rmod[i][1]*vmod[i][2]-rmod[i][2]*vmod[i][1]);			
  for (int i=0; i<2; i++) Ly[i]=m[bin[i]]*(rmod[i][0]*vmod[i][2]-rmod[i][2]*vmod[i][0]);
  for (int i=0; i<2; i++) Lz[i]=m[bin[i]]*(rmod[i][0]*vmod[i][1]-rmod[i][1]*vmod[i][0]);
  for (int i=0; i<2; i++) L[i]=sqrt(Lx[i]*Lx[i]+Ly[i]*Ly[i]+Lz[i]*Lz[i]);
  finval[0]=L[0]+L[1];
  double Lbin=finval[0];

  
  //BINARY ENERGY
  double T[2],V;
  for (int i=0; i<2; i++) {
	 T[i]=0.5*m[bin[i]]*(vmod[i][0]*vmod[i][0]+vmod[i][1]*vmod[i][1]+vmod[i][2]*vmod[i][2]);
	 //fprintf(stdout,"   %d %g %g %g %g %g\n",i,m[bin[i]],vmod[i][0],vmod[i][1],vmod[i][2],T[i]);
  }
  V=(-G*m[bin[0]]*m[bin[1]])/sep;
  //fprintf(stdout,"   %g %g %g %g %g\n",m[bin[0]],m[bin[1]],G,sep,V);

  finval[1]=T[0]+T[1]+V;
  double Ebin=finval[1];

  double K=G*(m[bin[0]]*m[bin[1]]);
  double mu=(m[bin[0]]*m[bin[1]])/(m[bin[0]]+m[bin[1]]); //REDUCED MASS
  finval[2]=K/(2.*fabs(Ebin)); //SEMI-MAJOR AXIS IN km
  double a=finval[2];
  if (mu == 0) {
	 fprintf(stdout,"\nWARNING!! ZERO MASS BINARY: for e and p\n");
	 finval[3]=0.;
	 finval[4]=0.;
  } else {
	 //	 cout << 1.+((2.*Ebin*pow(Lbin,2))/(mu*pow(K,2))) << " " << mu << " " << K << " " << Ebin << " " << Lbin << endl;
          if ((1.+(2.*Ebin*Lbin*Lbin)/(mu*K*K)) < 0) {
	   if (abs(1.+(2.*Ebin*Lbin*Lbin)/(mu*K*K)) < tiny) finval[3]=0.0; else finval[3]=-1.0;
	   fprintf(stdout,"\nWARNING! e sqrt(negative number),setting e=%g for ID=%i type=%d Ebin=%g Lbin=%g mu=%g K=%g 1+%20.15e\n\n",finval[3],bID,typ,Ebin,Lbin,mu,K,((2.*Ebin*Lbin*Lbin)/(mu*K*K)));
	   
         } else finval[3]=sqrt(1.+((2.*Ebin*Lbin*Lbin)/(mu*K*K))); //ECCENTRICITY
	 finval[4]=2.*PI*sqrt(mu/K)*sqrt(a*a*a); //PERIOD (seconds)
  }

  if (finval[3] > 0.9999) {
	 fprintf(stdout,"\nWARNING(0)! e > 1, setting e = 0.999 for ID=%i type=%d Ebin=%g velbin=%g e=%g P=%g\n\n",bID,typ,Ebin,vbin,finval[3],finval[4]);
	 finval[3]=0.999;
	 //	 exit(3);
  }

  if (finval[2] <= 0.) {
	 fprintf(stdout,"\nWARNING! a <= 0 after encounter for ID=%i type=%d Ebin=%g velbin=%g e=%g a=%g\n\n",bID,typ,Ebin,vbin,finval[3],finval[2]);
	 //	 exit(3);
  }

}

void gettripfinvals(int bin[], double m[],double r[][3], double v[][3], double fintval[], int run, int tID, int typ, double vperp) //CALCULATE THE FINAL E,L,a,e,P for triples
{
  //  cout << "gettripfinvals" << endl;
  int i;

  if (bin[3] != -1) fintval[11]=sqrt(v[bin[3]][0]*v[bin[3]][0]+v[bin[3]][1]*v[bin[3]][1]+v[bin[3]][2]*v[bin[3]][2]); //velocity of single star

  //INNER BINARY
  double finval1[8];
  double a1,e1,P1,mb1,mb2,Pf1,Tc1,vtrip;

  getbinfinvals(bin,m,r,v,finval1,tID,typ,vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR FIRST BINARY
  //CONVERT VALUES TO DESIRED UNITS AND SAVE IN THE fin[] ARRAY
  a1=finval1[2] / AUtkm; // AU
  e1=finval1[3];
  P1=finval1[4] /8.64e4;  //days
  int prim=-1;
  //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
  if (m[bin[0]] > m[bin[1]]) prim=0; else prim=1;
  mb1=m[bin[prim]];
  mb2=m[bin[abs(1-prim)]];
  //if (Pf1 < 0) Pf1=0.0;  //JUST INCASE (NORMALLY HAPPENS WITH TRIPLE SYSTEMS)
  for (i=0; i<5; i++) fintval[i]=finval1[i];
 

  //OUTER ORBIT
  //CENTER OF MASS COORDINATES
  double rcmt[2][3],vcmt[2][3],mtrip[2],finvalt[7];
  double a2,e2,P2;
  int bint[4];
  for (int i=0; i<3; i++){
	 rcmt[0][i]=(1./(m[bin[0]]+m[bin[1]]))*(m[bin[0]]*r[bin[0]][i]+m[bin[1]]*r[bin[1]][i]);
	 vcmt[0][i]=(1./(m[bin[0]]+m[bin[1]]))*(m[bin[0]]*v[bin[0]][i]+m[bin[1]]*v[bin[1]][i]);
	 rcmt[1][i]=r[bin[2]][i];
	 vcmt[1][i]=v[bin[2]][i];
  }
  mtrip[0]=m[bin[0]]+m[bin[1]]; //inner binary
  mtrip[1]=m[bin[2]];  //tertiary
  bint[0]=0;
  bint[1]=1;
  bint[2]=-1.;
  bint[3]=-1.;
  getbinfinvals(bint,mtrip,rcmt,vcmt,finvalt,tID,typ,vperp);
  a2=finvalt[2] / AUtkm; // AU
  e2=finvalt[3];
  P2=finvalt[4] /8.64e4;  //days
  for (i=0; i<5; i++) fintval[i+5]=finvalt[i];

  //find the velocity of the triple
  vtrip=0.0;

  //for (int i=0; i<3; i++) vtrip+=pow((1./(m[bin[0]]+m[bin[1]]+m[bin[2]]))*(m[bin[0]]*v[bin[0]][i]+m[bin[1]]*v[bin[1]][i]+m[bin[2]]*v[bin[2]][i]),2.);
//add on the perpendicular component of the velocity not originally sent into FEWBODY
  for (int i=0; i<3; i++) {
	 if (i == 1){
		vtrip+=pow( (1./(m[bin[0]]+m[bin[1]]+m[bin[2]]))*(m[bin[0]]*v[bin[0]][i]+m[bin[1]]*v[bin[1]][i]+m[bin[2]]*v[bin[2]][i]) + vperp , 2.);
	 } else {
		vtrip+=pow((1./(m[bin[0]]+m[bin[1]]+m[bin[2]]))*(m[bin[0]]*v[bin[0]][i]+m[bin[1]]*v[bin[1]][i]+m[bin[2]]*v[bin[2]][i]),2.);
	 }
  }
  vtrip=sqrt(vtrip);
  fintval[10]=vtrip; //is fintval[10] used anywhere else??

  //print the results to the triple file
  //  if (noprint != 1) fprintf(tripFile,"%-6.5i %-6.5i %-4i %6.3f %6.3f %6.3f %6.3e %6.3e %5.3f %6.3e %6.3e %6.3e %6.3e %5.3f\n",run,tID,bin[4],mb1,mb2,m[bin[2]],a1,P1,e1,Pf1,Tc1,a2,P2,e2);
  //  fflush(tripFile);
}


void getquadfinvals(int bin1[], int bin2[], double m[],double r[][3], double v[][3], double finqval[], int run, int qID, int typ, double vperp) //CALCULATE THE FINAL E,L,a,e,P for quadruples
{
  //  cout << "getquadfinvals" << endl;

  int i;
  double a1,e1,P1,mb11,mb12,Pf1,Tc1;
  double a2,e2,P2,mb21,mb22,Pf2,Tc2;
  double aq,eq,Pq;
  double circ[3];
  int prim=-1;

  //FOR hierarchical quadruple
  if (bin1[6] == 11 || bin1[6] == 28 || bin1[6] == 30 || bin1[6] == 40 || bin1[6] == 41 || bin1[6] == 42 || bin1[6] == 65 || bin1[6] == 66 || bin1[6] == 70 || bin1[6] == 71 || bin1[6] == 74 || bin1[6] == 80 || bin1[6] == 81 || bin1[6] == 82 || bin1[6] == 83 || bin1[6] == 89 || bin1[6] == 90 || bin1[6] == 91 || bin1[6] == 92 || bin1[6] == 103 || bin1[6] == 104 || bin1[6] == 105 || bin1[6] == 106 || bin1[6] == 120 || bin1[6] == 121 || bin1[6] == 122) {
	 //INNER TRIPLE
	 double fintval[14]; 
	 gettripfinvals(bin1,m,r,v,fintval,run,qID,typ,vperp);
	 for (i=0;i<11;i++) finqval[i]=fintval[i];
	 a1=finqval[2]/AUtkm; // AU
	 e1=finqval[3];
	 P1=finqval[4]/8.64e4;  //days
	 a2=finqval[7]/AUtkm; // AU
	 e2=finqval[8];
	 P2=finqval[9]/8.64e4;  //days
	 Pf1=fintval[12];
	 Tc1=fintval[13];
	 finqval[15]=Pf1;
	 finqval[16]=Tc1;
	 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY (for inner binary)
	 prim=-1;
	 if (m[bin1[0]] > m[bin1[1]]) prim=0; else prim=1;
	 mb11=m[bin1[prim]];
	 mb12=m[bin1[abs(1-prim)]];
	 mb21=m[bin1[2]];
	 mb22=m[bin1[3]];

	 //OUTER ORBIT
	 double rcmhq[2][3],vcmhq[2][3],mhquad[2],finvalhq[7];
	 int binhq[4];
	 for (i=0; i<3; i++){
		rcmhq[0][i]=(1./(m[bin1[0]]+m[bin1[1]]+m[bin1[2]]))*(m[bin1[0]]*r[bin1[0]][i]+m[bin1[1]]*r[bin1[1]][i]+m[bin1[2]]*r[bin1[2]][i]);
		vcmhq[0][i]=(1./(m[bin1[0]]+m[bin1[1]]+m[bin1[2]]))*(m[bin1[0]]*v[bin1[0]][i]+m[bin1[1]]*v[bin1[1]][i]+m[bin1[1]]*v[bin1[1]][i]);
		rcmhq[1][i]=r[bin1[3]][i];
		vcmhq[1][i]=v[bin1[3]][i];
	 }
	 mhquad[0]=m[bin1[0]]+m[bin1[1]]+m[bin1[2]]; //inner triple
	 mhquad[1]=m[bin1[3]];  //outer star
	 binhq[0]=0;
	 binhq[1]=1;
	 binhq[2]=-1.;
	 binhq[3]=-1.;
	 getbinfinvals(binhq,mhquad,rcmhq,vcmhq,finvalhq,qID,typ,vperp);
	 aq=finvalhq[2] / AUtkm; // AU
	 eq=finvalhq[3];
	 Pq=finvalhq[4] /8.64e4;  //days
	 for (i=0; i<5; i++) finqval[i+10]=finvalhq[i];
	 Pf2=0.0;
	 Tc2=0.0;
	 finqval[17]=Pf2;
	 finqval[18]=Tc2;
  }

  //FOR quadruple composed of two binaries
  if (bin1[6] == 12 || bin1[6] == 29|| bin1[6] == 32 || bin1[6] == 43 || bin1[6] == 67 || bin1[6] == 72 || bin1[6] == 76 || bin1[6] == 85 || bin1[6] == 86 || bin1[6] == 93 || bin1[6] == 94 || bin1[6] == 107 || bin1[6] == 108 || bin1[6] == 123) {

	 //BINARY 1
	 double finval1[8];
	 getbinfinvals(bin1,m,r,v,finval1,qID,typ,vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR FIRST BINARY
	 //CONVERT VALUES TO DESIRED UNITS AND SAVE IN THE fin[] ARRAY
	 a1=finval1[2] / AUtkm; // AU
	 e1=finval1[3];
	 P1=finval1[4] /8.64e4;  //days
	 prim=-1;
	 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
	 if (m[bin1[0]] > m[bin1[1]]) prim=0; else prim=1;
	 mb11=m[bin1[prim]];
	 mb12=m[bin1[abs(1-prim)]];
	 //if (Pf1 < 0) Pf1=0.0;  //JUST INCASE (NORMALLY HAPPENS WITH TRIPLE SYSTEMS)
	 for (i=0; i<5; i++) finqval[i]=finval1[i];

	 //BINARY 2
	 double finval2[8];
	 getbinfinvals(bin2,m,r,v,finval2,qID,typ,vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR FIRST BINARY
	 //CONVERT VALUES TO DESIRED UNITS AND SAVE IN THE fin[] ARRAY
	 a2=finval2[2] / AUtkm; // AU
	 e2=finval2[3];
	 P2=finval2[4] /8.64e4;  //days
	 prim=-1;
	 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
	 if (m[bin2[0]] > m[bin2[1]]) prim=0; else prim=1;
	 mb21=m[bin2[prim]];
	 mb22=m[bin2[abs(1-prim)]];
	 //if (Pf1 < 0) Pf1=0.0;  //JUST INCASE (NORMALLY HAPPENS WITH TRIPLE SYSTEMS)
	 for (i=0; i<5; i++) finqval[i+5]=finval2[i];



	 //MUTUAL ORBIT
	 //CENTER OF MASS COORDINATES
	 double rcmq[2][3],vcmq[2][3],mquad[2],finvalq[7];
	 int binq[4];
	 //for each binary
	 for (i=0; i<3; i++){
		rcmq[0][i]=(1./(m[bin1[0]]+m[bin1[1]]))*(m[bin1[0]]*r[bin1[0]][i]+m[bin1[1]]*r[bin1[1]][i]);
		vcmq[0][i]=(1./(m[bin1[0]]+m[bin1[1]]))*(m[bin1[0]]*v[bin1[0]][i]+m[bin1[1]]*v[bin1[1]][i]);
		rcmq[1][i]=(1./(m[bin2[0]]+m[bin2[1]]))*(m[bin2[0]]*r[bin2[0]][i]+m[bin2[1]]*r[bin2[1]][i]);
		vcmq[1][i]=(1./(m[bin2[0]]+m[bin2[1]]))*(m[bin2[0]]*v[bin2[0]][i]+m[bin2[1]]*v[bin2[1]][i]);
	 }
	 mquad[0]=m[bin1[0]]+m[bin1[1]]; //binary 1
	 mquad[1]=m[bin2[0]]+m[bin2[1]]; //binary 2
	 binq[0]=0;
	 binq[1]=1;
	 binq[2]=-1.;
	 binq[3]=-1.;
	 getbinfinvals(binq,mquad,rcmq,vcmq,finvalq,qID,typ,vperp);
	 aq=finvalq[2] / 1.4960e8; // AU
	 eq=finvalq[3];
	 Pq=finvalq[4] /8.64e4;  //days
	 for (i=0; i<5; i++) finqval[i+10]=finvalq[i];
  }


  //  if (noprint != 1) fprintf(quadFile,"%-6.5i %-6.5i %-4i %6.3f %6.3f %6.3f %6.3e %6.3e %6.3e %5.3f %6.3e %6.3e %6.3e %6.3e %5.3f %6.3e %6.3e %6.3e %6.3e %5.3f\n",run,qID,bin1[6],mb11,mb12,mb21,mb22,a1,P1,e1,Pf1,Tc1,a2,P2,e2,Pf2,Tc2,aq,Pq,eq);
  //  fflush(quadFile);
}


//decode the FEWBODY output to get the array locations for the stars!
void getbinsin(char story[], int bin1[], int bin1org[])
{
  //go step by step through the possible outcomes in FEWBODY binsingle (Table 1 in Fregeau et al. 2004)
  //1. [* *] *
  //2. * * *
  //3. [[* *] *]
  //4. [*:* *]
  //5. *:* *
  //6. *:*:*
  //only three possible outcomes involving binaries
  //the type of outcome is stored in bin1[6]
  //if a binary exists, the locations are in bin1[0], bin1[1]
  //if there is a single star or tertiary the location is in bin1[2] and bin1[3]=-1


  //identify which outcome we have
  string::size_type prbr1,prbr2,plbr1,plbr2,pmg1,pmg2,p1;
  string junk,tmp1,tmp2;
  int mgflag=0;

  junk=story;
  //note, apparently C returns a large value when it doesn't find an expression within a string
  prbr1=999;
  prbr2=999;
  plbr1=999;
  plbr2=999;
  pmg1=999;
  pmg2=999;

  //  cout << junk << endl;
  prbr1=junk.find("[");
  plbr1=junk.rfind("]");
  pmg1=junk.find(":");
  pmg2=junk.rfind(":");
  //  cout << pmg1 << " " << pmg2 << endl;
  //  cout << prbr1 << " " << plbr1 << endl;
  if (pmg1 < 10) mgflag=1;
  if (pmg1 > 10 && prbr1 > 10) {//three single stars
	 bin1[6]=2;
	 bin1[0]=atoi(junk.substr(0,1).c_str());
	 bin1[1]=atoi(junk.substr(2,1).c_str()); 
	 bin1[2]=atoi(junk.substr(4,1).c_str()); 
  }
  if (pmg1 < 10 && pmg2 < 10 && pmg1 != pmg2) { //three star merger
	 bin1[6]=6;
	 bin1[0]=0;
  }
  if (pmg1 < 10 && pmg2 < 10 && pmg1 == pmg2 && prbr1 > 10 ) { //two-star merger and single star
	 bin1[6]=5;
	 bin1[0]=atoi(junk.substr(0,1).c_str());
	 p1=junk.find(" ")+1;
	 bin1[1]=atoi(junk.substr(p1,1).c_str());
  }
  if (prbr1 < 10) {
	 tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
	 //	 cout << tmp1 << endl;
	 prbr2=tmp1.find("[");
	 plbr2=tmp1.rfind("]");
	 if (prbr2 < 10 && plbr2 < 10) { //triple
		bin1[6]=3;
		tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		//		cout << tmp2 << endl;
		bin1[0]=atoi(tmp2.substr(0,1).c_str());
		p1=tmp2.find(" ")+1;
		bin1[1]=atoi(tmp2.substr(p1,1).c_str());	
		//bin1[2] has the location of the tertiary
		if (prbr2 == 0) bin1[2]=atoi(tmp1.substr(plbr2+2,1).c_str());
		if (prbr2 != 0) bin1[2]=atoi(tmp1.substr(0,1).c_str());
	 }
	 if (prbr2 > 10 && plbr2 > 10) {
		if (pmg1 < 10) { //binary containing a merger product
		  bin1[6]=4;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ")+1;
		  bin1[1]=atoi(tmp1.substr(p1,1).c_str());
		}
		if (pmg1 > 10) {
		  bin1[6]=1; //binary + single (no merger)
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ")+1;
		  bin1[1]=atoi(tmp1.substr(p1,1).c_str());
		  //bin1[2] has the location of the single star
		  if (prbr1 == 0) bin1[2]=atoi(junk.substr(plbr1+2,1).c_str());
		  if (prbr1 != 0) bin1[2]=atoi(junk.substr(0,1).c_str());
		}
	 }
  }
  //  cout << bin1[6] << endl;
  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << endl;
  //  cout << "mgflag " << mgflag << endl;

  //if a merger happened, then adjust the number so that the array positions are correct
  //but save the original values for the IDs
  for (int i=0; i<5; i++) bin1org[i]=bin1[i];
  if (mgflag == 1 && bin1[0] != -1 && bin1[1] != -1) {
	 if (bin1[1] > bin1[0]) {
		bin1[0]=0;
		bin1[1]=1;
	 } else {
		bin1[0]=1;
		bin1[1]=0;
	 }
  }
  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << " " << bin1[6] << endl;
}

//decode the FEWBODY output to get the array locations for the stars!
void getbinbin(char story[], int bin1[], int bin2[], int bin1org[])
{
  //go step by step through the possible outcomes in FEWBODY binbin (Table 2 in Fregeau et al. 2004)
  //7.  [* *] [* *]
  //8.  [* *] * *
  //9.  * * * *
  //10.  [[* *] *] *
  //11.  [[[* *] *] *]
  //12.  [[* *][* *]]
  //13.  [* *] *:*
  //14.  [*:* *] *
  //15.  *:* * *
  //16. [[*:* *] *]
  //17. [[* *] *:*]
  //18. [*:* *:*]
  //19. [*:*:* *]
  //20. *:* *:*
  //21. *:*:* *
  //22  *:*:*:*
  //eleven possible outcomes involving binaries!
  //the type of outcome is stored in bin1[6]
  //if one binary exists, the locations are in bin1[0], bin1[1]
  //if there is a single star or tertiary the location is in bin1[2] and bin1[3]=-1
  //if there is a second binary, the locations are in bin1[2], bin1[3]

  //identify which outcome we have
  string::size_type prbr1,prbr2,prbr3,plbr1,plbr2,plbr3,pmg1,pmg2,pmg3,p1,p2;
  string junk,tmp1,tmp2,tmp3;
  int mgflag=0;

  junk=story;
  //note, apparently C returns a large value when it doesn't find an expression within a string
  prbr1=999;
  prbr2=999;
  prbr3=999;
  plbr1=999;
  plbr2=999;
  plbr3=999;
  pmg1=999;
  pmg2=999;
  pmg3=999;
  

  //  cout << junk << endl;
  prbr1=junk.find("[");
  plbr1=junk.rfind("]");
  pmg1=junk.find(":");
  pmg2=junk.rfind(":");
  if (pmg1 < 10) mgflag=1;
  //  cout << pmg1 << " " << pmg2 << endl;
  if (prbr1 > 10){
	 //outcomes without a binary
	 if (pmg1 > 10) { //four single stars (no merger)
		bin1[6]=9;
		bin1[0]=atoi(junk.substr(0,1).c_str());
		bin1[1]=atoi(junk.substr(2,1).c_str()); 
		bin1[2]=atoi(junk.substr(4,1).c_str()); 
		bin1[3]=atoi(junk.substr(6,1).c_str()); 
	 }
	 if (pmg1 < 10 && pmg2 < 10 && pmg1 == pmg2) {
		bin1[6]=15; //two-star merger and two single stars
		bin1[0]=atoi(junk.substr(0,1).c_str());
		p1=junk.find(" ")+1;
		bin1[1]=atoi(junk.substr(p1,1).c_str()); 
		tmp1=junk.substr(p1+1,junk.length()-p1-1);
		//		cout << "tmp1 " << tmp1 << endl;
		p1=tmp1.find(" ")+1;
		bin1[2]=atoi(tmp1.substr(p1,1).c_str()); 
	 }
	 if (pmg1 < 10 && pmg2 < 10 && pmg1 != pmg2) {
		tmp1=junk.substr(pmg1+1,pmg2-(pmg1+1));
		pmg3=tmp1.find(":");
		//		cout << tmp1 << endl;
		if (pmg3 < 10 ) {// four-star merger
		  bin1[6]=22; 
		  bin1[0]=0;
		}
		if (pmg3 > 10) {
		  if (pmg2-pmg1 == 2) { //three-star merger and a single star
			 bin1[6]=21; 
			 bin1[0]=atoi(junk.substr(0,1).c_str());
			 p1=junk.find(" ")+1;
			 bin1[1]=atoi(junk.substr(p1,1).c_str()); 
		  }
		  if (pmg2-pmg1 !=2) {//two two-star mergers
			 bin1[6]=20;  
			 bin1[0]=atoi(junk.substr(0,1).c_str());
			 p1=junk.find(" ")+1;
			 bin1[1]=atoi(junk.substr(p1,1).c_str()); 
		  }
		}
	 }
  }
  if (prbr1 < 10) {
	 tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
	 //	 cout << "tmp1 " << tmp1 << endl;
	 prbr2=tmp1.find("[");
	 plbr2=tmp1.rfind("]");
	 if (prbr2 > 10) {
		//outcomes with only one binary
		pmg3=tmp1.find(":");
		//		cout << pmg1 << " " << pmg3 << endl;
		if (pmg3 > 10) {
		  if (pmg1 > 10) {
			 bin1[6]=8; //binary + single + single
			 bin1[0]=atoi(tmp1.substr(0,1).c_str());
			 p1=tmp1.find(" ")+1;
			 bin1[1]=atoi(tmp1.substr(p1,1).c_str());	
			 //			 cout << prbr1 << " " << plbr1 << endl;
			 if (prbr1 == 0 || prbr1 > 2) {
				if (prbr1 ==0) tmp2=junk.substr(plbr1+2,junk.length()-(plbr1+2)); else tmp2=junk.substr(0,prbr1);
				//				cout << "tmp2 " << tmp2 << endl;
				bin1[2]=atoi(tmp2.substr(0,1).c_str());
				p1=tmp2.find(" ")+1;
				bin1[3]=atoi(tmp2.substr(p1,1).c_str());
			 }
			 if (prbr1 == 2) {
				bin1[2]=atoi(junk.substr(0,1).c_str());
				bin1[3]=atoi(junk.substr(junk.length()-1,1).c_str());
			 }
		  }
		  if (pmg1 < 10) {
			 bin1[6]=13; //binary + merger
			 bin1[0]=atoi(tmp1.substr(0,1).c_str());
			 p1=tmp1.find(" ")+1;
			 bin1[1]=atoi(tmp1.substr(p1,1).c_str());	
			 if (pmg1 < prbr1) bin1[2]=atoi(junk.substr(0,1).c_str()); else bin1[2]=atoi(junk.substr(plbr1+2,1).c_str());
		  }
		}
		if (pmg3 < 10) {
		  bin1[6]=14; //binary containing merger + single star
		  bin1[0]=atoi(tmp1.substr(pmg3-1,1).c_str());
		  if (pmg3 == 1) bin1[1]=atoi(tmp1.substr(tmp1.length()-1,1).c_str()); else bin1[1]=atoi(tmp1.substr(0,1).c_str());
		  if (prbr1 == 0) bin1[2]=atoi(junk.substr(junk.length()-1,1).c_str()); else bin1[2]=atoi(junk.substr(0,1).c_str());
		}
		pmg1=tmp1.find(":");
		pmg2=tmp1.rfind(":");
		if (pmg1 < 10 && pmg2 < 10 && pmg1 != pmg2) {
		  tmp2=tmp1.substr(pmg1+1,pmg2-(pmg1+1));
		  pmg3=tmp2.find(":");
		  //		  cout << tmp2 << endl;
		  if (pmg3 > 10) {
			 if (pmg2-pmg1 == 2) {
				bin1[6]=19; //binary containing a double merger + normal star
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				bin1[1]=atoi(tmp1.substr(tmp1.find(" ")+1,1).c_str());
				bin1[2]=-1;
			 }
			 if (pmg2-pmg1 != 2) {
				bin1[6]=18; //binary containing merger + merger
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				bin1[1]=atoi(tmp1.substr(tmp1.find(" ")+1,1).c_str());
				bin1[2]=-1;
			 }
		  }
		}
	 }
	 if (prbr2 < 10) {
		//more complex systems
		if (prbr2 > plbr2) {
		  bin1[6]=7; //binary + binary (not bound)
		  tmp2=junk.substr(prbr1+1,3);
		  tmp3=tmp1.substr(prbr2+1,3);
		  //		  cout << "tmp2 " << tmp2 << endl;
		  //		  cout << "tmp3 " << tmp3 << endl;
		  bin1[0]=atoi(tmp2.substr(0,1).c_str());
		  bin1[1]=atoi(tmp2.substr(tmp2.find(" ")+1,1).c_str());
		  bin2[0]=atoi(tmp3.substr(0,1).c_str());
		  bin2[1]=atoi(tmp3.substr(tmp3.find(" ")+1,1).c_str());
		  bin2[2]=-1;
		  bin1[2]=bin2[0];
		  bin1[3]=bin2[1];
		}
		if (prbr2 < plbr2) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  //		  cout << "tmp2 " << tmp2 << endl;
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  if (plbr3 > 10) {
			 pmg1=tmp1.find(":");
			 pmg2=tmp1.rfind(":");
			 pmg3=tmp2.find(":");
			 if (pmg3 < 10) {
				bin1[6]=16; //hierarchical triple with merger in inner binary
				bin1[0]=atoi(tmp2.substr(pmg3-1,1).c_str());
				if (pmg3 == 1) bin1[1]=atoi(tmp2.substr(tmp2.length()-1,1).c_str()); else bin1[1]=atoi(tmp2.substr(0,1).c_str());
				if (prbr2 == 0) bin1[2]=atoi(junk.substr(junk.length()-2,1).c_str()); else bin1[2]=atoi(junk.substr(1,1).c_str());
			 }
			 if (pmg3 > 10 && pmg1 < 10) {
				bin1[6]=17; //hierarchical triple with merger for tertiary
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				if (prbr2 == 0) bin1[2]=atoi(junk.substr(junk.length()-4,1).c_str()); else bin1[2]=atoi(junk.substr(1,1).c_str());
			 }
			 if (pmg3 > 10 && pmg1 > 10) {
				bin1[6]=10; //hierarchical triple + single
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				if (prbr2 == 0) {
				  bin1[2]=atoi(tmp1.substr(tmp1.length()-1,1).c_str());
				  bin1[3]=atoi(junk.substr(junk.length()-1,1).c_str());
				} else {						 
				  bin1[2]=atoi(tmp1.substr(0,1).c_str());
				  bin1[3]=atoi(junk.substr(0,1).c_str());
				}
			 }
		  }
		  if (prbr3 < 10) {
			 if (prbr3 < plbr3) {
				bin1[6]=11; //hierarchical quadruple
				bin1[0]=atoi(tmp2.substr(prbr3+1,1).c_str());
				bin1[1]=atoi(tmp2.substr(plbr3-1,1).c_str());
				if (prbr3 == 0) {
				  bin1[2]=atoi(tmp2.substr(plbr3+2,1).c_str());
				  bin1[3]=atoi(tmp1.substr(tmp1.length()-1,1).c_str());
				} else {
				  bin1[2]=atoi(tmp2.substr(0,1).c_str());
				  bin1[3]=atoi(tmp1.substr(0,1).c_str());
				}
			 }
			 if (prbr3 > plbr3) {
				bin1[6]=12; //binary + binary in quadruple
				tmp2=tmp1.substr(prbr2+1,3);
				tmp3=tmp1.substr(plbr2-3,3);
				//				cout << "tmp2 " << tmp2 << endl;
				//				cout << "tmp3 " << tmp3 << endl;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(tmp2.find(" ")+1,1).c_str());
				bin2[0]=atoi(tmp3.substr(0,1).c_str());
				bin2[1]=atoi(tmp3.substr(tmp3.find(" ")+1,1).c_str());
				bin2[2]=-1;
				bin1[2]=bin2[0];
				bin1[3]=bin2[1];
			 }
		  }
		}
	 }
  }
  
  //  cout << bin1[6] << endl;
  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << " " << bin1[3] << endl;
  //  cout << bin2[0] << " " << bin2[1] << " " << bin2[2] << " " << bin2[3] << endl;

  //  cout << "mgflag " << mgflag << endl;

  //if a merger happened, then adjust the number so that the array positions are correct
  //but save the original values for the IDs
  for (int i=0; i<5; i++) bin1org[i]=bin1[i];
  //fix the merger numbers
  if (mgflag > 0) {
	 int found;
	 for (int i=0; i<5; i++){
		found=-1;
		for (int j=0; j<5; j++) {
		  if (bin1[j] == i) {
			 found=j;
			 break;
		  }
		}
		//	 cout << "found " << i << " " << found << endl;
		if (found == -1 && i < 4) {
		  for (int j=0; j<5; j++) {
			 if (bin1[j] == i+1) {
				found=j;
				break;
			 }
		  }
		  if (found >= 0) bin1[found]=bin1[found]-1; //two star mergers
		  if (found == -1 && i < 3) {
			 for (int j=0; j<5; j++) {
				if (bin1[j] == i+2) {
				  found=j;
				  break;
				}
			 }
			 if (found >= 0) bin1[found]=bin1[found]-2; //three star mergers
		  }
		}
	 }
  }

 
  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << " " << bin1[3] << " " << bin1[6] << endl;

}




void gettripbin(char story[], int bin1[], int bin2[], int bin1org[])
{
  //go step by step through the possible outcomes in FEWBODY triplebin (Nathan's table)
  //23. [[* *] *] [* *]
  //24. [* *] [* *] *
  //25. [* *] * * *
  //26. [[* *] *] * *
  //27. * * * * *
  //28. [[[* *] *] *] *
  //29. [[* *] [* *]] *
  //30. [[[[* *] *] *] *]
  //31. [[[* *] *] [* *]]
  //32. [[[* *] [* *]] *]
  //33. [[* *] *] *:*
  //34. [[*:* *] *] *
  //35. [[* *] *:*] *
  //36. [*:* *] * *
  //37. [* *] *:* *
  //38. *:* * * *
  //39. [*:* *] [* *]
  //40. [[[*:* *] *] *]
  //41. [[[* *] *:*] *]
  //42. [[[* *] *] *:*]
  //43. [[*:* *][* *]]
  //44. [*:* *:*] *
  //45. [*:* *] *:*
  //46. [[*:* *] *:*]
  //47. [[*:* *:*] *]
  //48. *:* *:* *
  //49. [[*:*:* *] *]
  //50. [[* *] *:*:*]
  //51. [*:*:* *] *
  //52. [* *] *:*:*
  //53. *:*:* * * 
  //54. [*:*:* *:*]
  //55. *:*:* *:*
  //56. [*:*:*:* *]
  //57. *:*:*:* *
  //58. *:*:*:*:*
  //the type of outcome is stored in bin1[6]
  //the rest of the stars are stored in order from bin1[0] to bin1[4]
  //if there is a merger (or mergers) then there will be blank spaces marked with bin1[*]=-1
  //if there is a second binary it is stored in bin2[0] and bin2[1]
  //(some redundancy here, oh well...)

  //identify which outcome we have
  string::size_type prbr1,prbr2,prbr3,prbr4,plbr1,plbr2,plbr3,plbr4,pmg1,pmg2,pmg3,p1,p2,p3,p4;
  string junk,tmp1,tmp2,tmp3,tmp4;
  int mgflag=0;
  int i,bcheck;

  junk=story;
  
  int sz=junk.length();
  char fixstory[sz],tmpstory[sz];
  fixstory[0]='\0';
  tmpstory[0]='\0';

  //note, apparently C returns a large value when it doesn't find an expression within a string
  prbr1=999;
  prbr2=999;
  prbr3=999;
  plbr1=999;
  plbr2=999;
  plbr3=999;
  pmg1=999;
  pmg2=999;
  pmg3=999;
  
 //clean up the input; place any singles on the end
  bcheck=0;
  for (i=0;i<junk.length();i++){
	 if (junk[i] == '[') bcheck++;
	 if (bcheck > 0) strcat(fixstory,junk.substr(i,1).c_str()); else strcat(tmpstory,junk.substr(i,1).c_str());
	 if (junk[i] == ']') bcheck--;
  }
  strcat(fixstory,tmpstory);
  junk=fixstory;

  prbr1=junk.find("[");
  plbr1=junk.rfind("]");
  pmg1=junk.find(":");
  pmg2=junk.rfind(":");
  if (pmg1 > 20) { //no merger
	 if (prbr1 > 20){ //five single stars (no merger)
		bin1[6]=27;
		bin1[0]=atoi(junk.substr(0,1).c_str());
		bin1[1]=atoi(junk.substr(2,1).c_str()); 
		bin1[2]=atoi(junk.substr(4,1).c_str()); 
		bin1[3]=atoi(junk.substr(6,1).c_str()); 
		bin1[4]=atoi(junk.substr(8,1).c_str()); 
	 }
	 if (prbr1 < 20 && bin1[6] == -1){ //there is at least one binary
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		if (prbr2 > 20 && bin1[6] == -1) { //binary + three singles
		  bin1[6]=25;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  bin1[1]=atoi(tmp1.substr(2,1).c_str()); 
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[2]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[3]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[4]=atoi(tmp2.substr(p1,1).c_str());
		}
		if (prbr2 < 20 && bin1[6] == -1) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  if (prbr3 > 20 && plbr3 > 20 && bin1[6] == -1) {
			 if (prbr2 > plbr2) { //two binaries unbound + single
				bin1[6]=24;
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				bin1[1]=atoi(tmp1.substr(2,1).c_str());
				tmp2=tmp1.substr(prbr2+1,tmp2.length()-prbr2-1);
				bin1[2]=atoi(tmp2.substr(0,1).c_str());
				bin1[3]=atoi(tmp2.substr(2,1).c_str());
				if (prbr2 == plbr2+2 || prbr2 == plbr2+1) tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); else tmp2=tmp1.substr(plbr2+1,prbr2-plbr2-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[4]=atoi(tmp2.substr(p1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
			 if (prbr2 < plbr2) { //hierarchical triple + 2 singles
				bin1[6]=26;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				tmp2=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[2]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[3]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[4]=atoi(tmp2.substr(p1,1).c_str());
			 }
		  }
		}
		if (((prbr3 < 20 && plbr3 > 20) || (prbr3 > 20 && plbr3 < 20)) && bin1[6] == -1){ //triple + binary unbound
		  bin1[6]=23;
		  p1=junk.find("[[");
		  if (p1 < 20) {
			 tmp2=junk.substr(p1+2,junk.length()-p1-2);
			 p2=tmp2.find("]");
			 bin1[2]=atoi(tmp2.substr(p2+2,1).c_str());
			 tmp2=tmp2.substr(0,p2);
		  }
		  if (p1 > 20) {
			 p1=junk.find("]]");
			 tmp2=junk.substr(0,p1);
			 p2=tmp2.rfind("[");
			 bin1[2]=atoi(tmp2.substr(p2-2,1).c_str());
			 tmp2=tmp2.substr(p2+1,tmp2.length()-p2-1);
		  }
		  bin1[0]=atoi(tmp2.substr(0,1).c_str());
		  bin1[1]=atoi(tmp2.substr(2,1).c_str());
		  tmp2=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //second binary
		  p1=tmp2.find("[");
		  if (p1 < 20) {
			 bin1[3]=atoi(tmp2.substr(p1+1,1).c_str());
			 bin1[4]=atoi(tmp2.substr(p1+3,1).c_str());
		  }
		  if (p1 > 20) {
			 bin1[3]=atoi(tmp2.substr(0,1).c_str());
			 bin1[4]=atoi(tmp2.substr(2,1).c_str());
		  }
		  bin2[0]=bin1[3];
		  bin2[1]=bin1[4];
		}
		if (prbr3 < 20 && plbr3 < 20 && bin1[6] == -1) { 
		  tmp3=tmp2.substr(prbr3+1,plbr3-(prbr3+1));
		  prbr4=tmp3.find("[");
		  plbr4=tmp3.rfind("]");
		  if (prbr4 > 20 && plbr4 > 20 && bin1[6] == -1) {
			 if (prbr3 < plbr3) {//hierarchical quadruple + single star
				bin1[6]=28;
				bin1[0]=atoi(tmp3.substr(0,1).c_str());
				bin1[1]=atoi(tmp3.substr(2,1).c_str());
				tmp3=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //tertiary
				p1=tmp3.find_first_not_of(" ");
				bin1[2]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quad
				p1=tmp3.find_first_not_of(" ");
				bin1[3]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
				p1=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p1,1).c_str());
			 }
			 if (prbr3 > plbr3 && bin1[6] == -1){ //quadruple composed of 2 binaries + single star
				bin1[6]=29;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				bin1[2]=atoi(tmp3.substr(0,1).c_str());
				bin1[3]=atoi(tmp3.substr(2,1).c_str());
				tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
				p1=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
		  }
		  if (prbr4 < 20 && bin1[6] == -1) { 
			 p1=junk.find("[[[[");
			 if (p1 > 20) p1=junk.find("]]]]");
			 if (p1 < 20) {//hierarchical quintuple
				bin1[6]=30;
				tmp4=tmp3.substr(prbr4+1,plbr4-(prbr4+1));
				bin1[0]=atoi(tmp4.substr(0,1).c_str());
				bin1[1]=atoi(tmp4.substr(2,1).c_str());
				tmp4=tmp3.substr(0,prbr4)+tmp3.substr(plbr4+1,tmp3.length()-plbr4-1); //tertiary
				p1=tmp4.find_first_not_of(" ");
				bin1[2]=atoi(tmp4.substr(p1,1).c_str());
				tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //quad
				p1=tmp4.find_first_not_of(" ");
				bin1[3]=atoi(tmp4.substr(p1,1).c_str());			
				tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quint
				p1=tmp4.find_first_not_of(" ");
				bin1[4]=atoi(tmp4.substr(p1,1).c_str());
			 }
		  }
		}
		if (( (prbr4 < 20 && plbr4 > 20) || (plbr4 < 20 && prbr4 > 20)) && bin1[6] == -1) {

		  //same as triple + binary (23), but starting at tmp1
		  bin1[6]=31;
		  p1=tmp1.find("[[");
		  if (p1 < 20) {
			 tmp3=tmp1.substr(p1+2,tmp1.length()-p1-2);
			 p2=tmp3.find("]");
			 bin1[2]=atoi(tmp3.substr(p2+2,1).c_str());
			 tmp3=tmp3.substr(0,p2);
		  }
		  if (p1 > 20) {
			 p1=tmp1.find("]]");
			 tmp3=tmp1.substr(0,p1);
			 p2=tmp3.rfind("[");
			 bin1[2]=atoi(tmp3.substr(p2-2,1).c_str());
			 tmp3=tmp3.substr(p2+1,tmp3.length()-p2-1);
		  }
		  bin1[0]=atoi(tmp3.substr(0,1).c_str());
		  bin1[1]=atoi(tmp3.substr(2,1).c_str());
		  tmp3=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //second binary
		  p1=tmp3.find("[");
		  if (p1 < 20) {
			 bin1[3]=atoi(tmp3.substr(p1+1,1).c_str());
			 bin1[4]=atoi(tmp3.substr(p1+3,1).c_str());
		  }
		  if (p1 > 20) {
			 bin1[3]=atoi(tmp3.substr(0,1).c_str());
			 bin1[4]=atoi(tmp3.substr(2,1).c_str());
		  }
		  bin2[0]=bin1[3];
		  bin2[1]=bin1[4];
		}
		if (prbr4 < 20 && plbr4 < 20 && bin1[6] == -1) {
		  bin1[6]=32;
		  bin1[0]=atoi(tmp3.substr(0,1).c_str());
		  bin1[1]=atoi(tmp3.substr(2,1).c_str());
		  p1=tmp3.find("[");
		  bin1[2]=atoi(tmp3.substr(p1+1,1).c_str());
		  bin1[3]=atoi(tmp3.substr(p1+3,1).c_str());
		  bin2[0]=bin1[2];
		  bin2[1]=bin1[3];
		  tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //single star
		  p1=tmp4.find_first_not_of(" ");
		  bin1[4]=atoi(tmp4.substr(p1,1).c_str());
		}

	 } 
  }
  if (pmg1 < 20 && pmg2 == pmg1) { //single merger
	 mgflag=1;
	 if (prbr1 > 20 && bin1[6] == -1) { //two-star merger + three singles
		bin1[6]=38;
		bin1[0]=atoi(junk.substr(0,1).c_str());
		p1=junk.find(" ");
		tmp1=junk.substr(p1+1,junk.length()-p1-1);
		bin1[1]=atoi(tmp1.substr(0,1).c_str());
		p1=tmp1.find(" ");
		tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		bin1[2]=atoi(tmp1.substr(0,1).c_str());
		p1=tmp1.find(" ");
		tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		bin1[3]=atoi(tmp1.substr(0,1).c_str());
	 }
	 if (prbr1 < 20) {
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		if (prbr2 > 20 && pmg1 >prbr1 && pmg1 < plbr1 && bin1[6] == -1) { //binary containing merger + two singles
		  bin1[6]=36;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  bin1[1]=atoi(tmp1.substr(p1+1,1).c_str());
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //other stars
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[2]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find_last_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[3]=atoi(tmp2.substr(0,1).c_str());
		}
		if (prbr2 > 20 && (pmg1 < prbr1 || pmg1 > plbr1) && bin1[6] == -1) { //binary + two-star merger + singles
		  bin1[6]=37;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  bin1[1]=atoi(tmp1.substr(2,1).c_str());
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //other stars
		  p1=tmp2.find_first_not_of(" ");
		  bin1[2]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find(" ");
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[3]=atoi(tmp2.substr(p1,1).c_str());
		}
		if (prbr2 < 20 && bin1[6] == -1) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  if (prbr3 > 20 && plbr3 > 20 && bin1[6] == -1) {
			 p1=junk.find("[[");
			 if (p1 > 20) p1=junk.find("]]");
			 if (p1 > 20 && bin1[6] == -1) { // binary + binary containing 2-star merger
				bin1[6]=39;
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				p2=tmp1.find(" ");
				bin1[1]=atoi(tmp1.substr(p2+1,1).c_str());
				bin1[2]=atoi(tmp2.substr(0,1).c_str());
				p2=tmp2.find(" ");
				bin1[3]=atoi(tmp2.substr(p2+1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
			 if (p1 < 20 && bin1[6] == -1) {
				p2=tmp1.find(":");
				p3=tmp2.find(":");
				if (p2 > 20 && p3 > 20 && bin1[6] == -1) {//hierarchical triple + 2-star merger
				  bin1[6]=33;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  bin1[1]=atoi(tmp2.substr(2,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				}
				if (p2 < 20 && p3 < 20 && bin1[6] == -1) {//hierarchical triple with 2-star merger in inner binary + singel
				  bin1[6]=34;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  p4=tmp2.find(" ");
				  bin1[1]=atoi(tmp2.substr(p4+1,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				}
				if (p2 < 20 && p3 > 20 && bin1[6] == -1) {//hierarchical triple with 2-star merger as tertiary + single
				  bin1[6]=35;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  bin1[1]=atoi(tmp2.substr(2,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				}			 
			 }
		  }
		  if (prbr3 < 20 && bin1[6] == -1) {
			 tmp3=tmp2.substr(prbr3+1,plbr3-(prbr3+1));
			 p1=junk.find("[[[");
			 if (p1 > 20) p1=junk.find("]]]");
			 if (p1 > 20 && bin1[6] == -1) {
				bin1[6]=43;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				p2=tmp2.find(" ");
				bin1[1]=atoi(tmp2.substr(p2+1,1).c_str());
				tmp3=tmp2.substr(prbr3+1,tmp2.length()-prbr3-1);
				bin1[2]=atoi(tmp3.substr(0,1).c_str());
				p2=tmp3.find(" ");
				bin1[3]=atoi(tmp3.substr(p2+1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
			 if (p1 < 20) {
				p2=tmp2.find(":");
				p3=tmp3.find(":");
				bin1[0]=atoi(tmp3.substr(0,1).c_str());
				p4=tmp3.find(" ");
				bin1[1]=atoi(tmp3.substr(p4+1,1).c_str());
				tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //tertiary
				p4=tmp4.find_first_not_of(" ");
				bin1[2]=atoi(tmp4.substr(p4,1).c_str());
				tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quad
				p4=tmp4.find_first_not_of(" ");
				bin1[3]=atoi(tmp4.substr(p4,1).c_str());
				
				if (p2 < 20 && p3 < 20 && bin1[6] == -1){ //hierarchical quadruple with 2-star merger in inner binary
				  bin1[6]=40;
				}
				if (p2 < 20 && p3 > 20 && bin1[6] == -1){ //hierarchical quadruple with 2-star merger as tertiary
				  bin1[6]=41;
				}
				if (p2 > 20 && p3 > 20 && bin1[6] == -1) { //hierarchical quadruple with 2-star merger ar quad
				  bin1[6]=42;
				}
			 }
		  }
		}
	 }
  }
  if (pmg1 < 20 && pmg2 != pmg1) { //two mergers
	 mgflag=1;
	 if (prbr1 > 20) { 
		tmp2=junk.substr(pmg1+1,pmg2-pmg1-1);
		pmg3=tmp2.find(":");
		if (pmg3 > 20 && bin1[6] == -1) {
		  bin1[0]=atoi(junk.substr(0,1).c_str());
		  p1=junk.find(" ");
		  tmp1=junk.substr(p1+1,junk.length()-p1-1);
		  bin1[1]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  bin1[2]=atoi(tmp1.substr(0,1).c_str());
		  if (pmg2 == pmg1+2) {//3-star merger + two singles
			 bin1[6]=53;
		  } else bin1[6]=48;//two 2-star mergers + single
		}
		if (pmg3 < 20 && bin1[6] == -1) {
		  bin1[0]=atoi(junk.substr(0,1).c_str());
		  p1=junk.find(" ");
		  if (p1 < 20 && bin1[6] == -1) {
			 tmp1=junk.substr(p1+1,junk.length()-p1-1);
			 tmp2=junk.substr(0,p1);
			 bin1[1]=atoi(tmp1.substr(0,1).c_str());
			 p2=tmp1.find(":");
			 p3=tmp2.find(":");
			 if ((p2 > 20 && p3 < 20) || (p2 < 20 && p3 > 20) && bin1[6] == -1) bin1[6]=57; // 4-star merger + single
			 if (p2 < 20 && p3 < 20 && bin1[6] == -1) bin1[6]=55; //3-star merger + 2-star merger
		  } 
		  if (p1 > 20 && bin1[6] == -1) bin1[6]=58; //5-star merger;
		}
	 }
	 if (prbr1 < 20 && bin1[6] == -1) {
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		tmp2=junk.substr(pmg1+1,pmg2-pmg1-1);
		pmg3=tmp2.find(":");
		if (plbr2 > 20 && pmg3 > 20) {
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp2=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  bin1[1]=atoi(tmp2.substr(0,1).c_str());
		  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
		  p1=tmp3.find_first_not_of(" ");
		  bin1[2]=atoi(tmp3.substr(p1,1).c_str());
		  if (pmg1 > prbr1 && pmg2 > prbr1 && pmg1 < plbr1 && pmg2 < plbr1 && pmg2 > pmg1+2 && bin1[6] == -1) bin1[6]=44; //binary composed of two 2-star mergers + single
		  if ((pmg1 > prbr1 && pmg1 < plbr1 && (pmg2 > plbr1 || pmg2 < prbr1)) || (pmg2 > prbr1 && pmg2 < plbr1 && (pmg1 > plbr1 || pmg1 < prbr1)) && bin1[6] == -1) bin1[6]=45; //binary composed of 2-star merger + 2-star merger
		  if (pmg1 > prbr1 && pmg2 > prbr1 && pmg1 < plbr1 && pmg2 < plbr1 && pmg2 == pmg1+2 && bin1[6] == -1) bin1[6]=51; //binary composed of two 2-star mergers + single
		  if ( (pmg1 < prbr1 && pmg2 < prbr1) || (pmg1 > plbr1 && pmg2 > plbr1) && bin1[6] == -1) bin1[6]=52; //binary + 2-star merger
		}
		if (plbr2 < 20 && pmg3 > 20 && bin1[6] == -1) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  bin1[0]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find(" ");
		  bin1[1]=atoi(tmp2.substr(p1+1,1).c_str());
		  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
		  p1=tmp3.find_first_not_of(" ");
		  bin1[2]=atoi(tmp3.substr(p1,1).c_str());
		  p3=tmp3.find(":");
		  p2=tmp2.find(":");
		  if (p3 < 20 && p2 < 20 && bin1[6] == -1) bin1[6]=46; //hierarchical triple with 2-star merger in inner binary and 2-star merger as tertiary
		  if (p3 > 20 && p2 < 20 && pmg2 > pmg1+2 && bin1[6] == -1) bin1[6]=47; //hierarchical triple with two 2-star mergers in inner binary
		  if (p3 > 20 && p2 < 20 && pmg2 == pmg1+2 && bin1[6] == -1) bin1[6]=49; //hierarchical triple with 3-star merger in inner binary
		  if (p3 < 20 && p2 > 20 && pmg2 == pmg1+2 && bin1[6] == -1) bin1[6]=50; //hierarchical triple with 3-star merger as tertiary
	   }
		if (plbr2 > 20 && pmg3 < 20 && bin1[6] == -1){
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp2=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  tmp3=tmp1.substr(0,p1);
		  bin1[1]=atoi(tmp2.substr(0,1).c_str());
		  p2=tmp2.find(":");
		  p3=tmp3.find(":");
		  if ((p2 > 20 && p3 < 20) || (p2 < 20 && p3 > 20) && bin1[6] == -1) bin1[6]=56; //binary with 4-star merger
		  if (p2 < 20 && p3 < 20 && bin1[6] == -1) bin1[6]=54; // binary with 3-star merger + 2-star merger
		}

		  
	 }
  }

  //fix the merger numbers
 //but save the original values for the IDs
  for (int i=0; i<5; i++) bin1org[i]=bin1[i];
 
  if (mgflag > 0) {
	 int found;
	 for (int i=0; i<5; i++){
		found=-1;
		for (int j=0; j<5; j++) {
		  if (bin1[j] == i) {
			 found=j;
			 break;
		  }
		}
		if (found == -1 && i < 4) {
		  for (int j=0; j<5; j++) {
			 if (bin1[j] == i+1) {
				found=j;
				break;
			 }
		  }
		  if (found >= 0) bin1[found]=bin1[found]-1; //two star mergers
		  if (found == -1 && i < 3) {
			 for (int j=0; j<5; j++) {
				if (bin1[j] == i+2) {
				  found=j;
				  break;
				}
			 }
			 if (found >= 0) bin1[found]=bin1[found]-2; //three star mergers
			 if (found == -1 && i < 2) {
				for (int j=0; j<5; j++) {
				  if (bin1[j] == i+3) {
					 found=j;
					 break;
				  }
				}
				if (found >= 0) bin1[found]=bin1[found]-3; //four star mergers
				if (found == -1 && i < 1) {
				  for (int j=0; j<5; j++) {
					 if (bin1[j] == i+4) {
						found=j;
						break;
					 }
				  }
				  if (found >= 0) bin1[found]=bin1[found]-4; //five star mergers
				}
			 }
		  }
		}
	 }
  }
			 
  if (bin2[0] != -1 && mgflag > 0) {
	 bin2[0]=bin1[2];
	 bin2[1]=bin1[3];
  }

  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << " " << bin1[3] << " " << bin1[4] << " " << bin1[6] << endl;
  //  cout << bin2[0] << " " << bin2[1] << " " << bin2[2] << " " << bin2[3] << " " << bin2[4] << " " << bin2[6] << endl;

}

void gettriptrip(char story[], int bin1[], int bin2[], int bin1org[])
{
  //go step by step through the possible outcomes in FEWBODY triplebin (Nathan's table)
  //59. [[* *] *][[* *] *]
  //60. [[* *] *][* *] *
  //61. [[* *] *] * * *
  //62. [* *] * * * *
  //63. * * * * * *
  //64. [* *][* *][* *]
  //65. [[[* *] *] *][* *]
  //66. [[[[[* *] *] *] *] *]
  //67. [[[* * ][* *]][* *]]
  //68. [[[* *] *][[* *] *]]
  //69. [[[[* *] *][* *]] *]
  //70. [[[[* *] *] *][* *]]
  //71. [[[[* *] *] *] *] *
  //72. [[[* *][* *]] *] *
  //73. [[[* *] *][* *]] *
  //74. [[[* *] *] *] * *
  //75. [* *][* *] * *
  //76. [[* *][* *]] * *
  //77. [* *] *][*:* *]
  //78. [[*:* *] *][* *]
  //79. [[* *] *:*][* *]
  //80. [[[[*:* *] *] *] *]
  //81. [[[[* *] *:*] *] *]
  //82. [[[[* *] *] *:*] *]
  //83. [[[[* *] *] *] *:*]
  //84. [[[*:* *][* *]] *]
  //85. [[[*:* *][* *]] *]
  //86. [[[* *][* *]] *:*]
  //87. [[[* *] *][*:* *]]
  //88. [[[* *] *:*][* *]]
  //89. [[[*:* *] *] *] *
  //90. [[[* *] *:*] *] *
  //91. [[[* *] *] *:*] *
  //92. [[[* *] *] *] *:*
  //93. [[*:* *][* *]] *
  //94. [[* *][* *]] *:*
  //95. [*:* *][* *] *
  //96. [* *][* *] *:*
  //97. [[*:* *] *] * *
  //98. [[* *] *:*] * *
  //99. [[* *] *] *:* *
  //100. [*:* *] * * *
  //101. [* *] *:* * *
  //102. *:* * * * *
  //103. [[[*:* *:*] *] *]
  //104. [[[*:* *] *:*] *]
  //105. [[[*:* *] *] *:*]
  //106. [[[* *] *:*] *:*]
  //107. [[*:* *][*:* *]]
  //108. [[*:* *:*][* *]]
  //109. [*:* *][*:* *]
  //110. [*:* *:*][* *]
  //111. [[*:* *:*] *] *
  //112. [[*:* *] *:*] *
  //113. [[*:* *] *:*] *
  //114. [[*:* *] *] *:*
  //115. [[* *] *:*] *:*
  //116. [*:* *] *:* *
  //117. [*:* *:*] * *
  //118. [* *] *:* *:*
  //119. *:* *:* * *
  //120. [[[*:*:* *] *] *]
  //121. [[[* *] *:*:*] *]
  //122. [[[* *] *] *:*:*]
  //123. [[*:*:* *][* *]]
  //124. [[*:*:* *] *] *								
  //125. [[* *] *:*:*] *
  //126. [* *] *] *:*:*
  //127. [*:*:* *] * *
  //128. [* *] *:*:* *
  //129. *:*:* * * *
  //130. [[*:* *:*] *:*]
  //131. [*:* *:*] *:*
  //132. *:* *:* *:*
  //133. [[*:*:* *:*] *]
  //134. [[*:*:* *] *:*]
  //135. [[*:* *] *:*:*]
  //136. [*:*:* *] *:*
  //137. [*:* *] *:*:*
  //138. [*:*:* *:*] *
  //139. *:*:* *:* *
  //140. [[*:*:*:* *] *]
  //141. [[* *] *:*:*:*]
  //142. [*:*:*:* *] *
  //143. [* *] *:*:*:*
  //144. *:*:*:* * *
  //145. [*:*:* *:*:*]
  //146. [*:*:*:*:* *]
  //147. [*:*:*:* *:*]
  //148. *:*:* *:*:*
  //149. *:*:*:*:* *
  //150. *:*:*:* *:*
  //151. *:*:*:*:*:*
  //the type of outcome is stored in bin1[6]
  //the rest of the stars are stored in order from bin1[0] to bin1[4]
  //if there is a merger (or mergers) then there will be blank spaces marked with bin1[*]=-1
  //if there is a second binary it is stored in bin2[0] and bin2[1]
  //(some redundancy here, oh well...)

  //identify which outcome we have
  string::size_type prbr1,prbr2,prbr3,prbr4,plbr1,plbr2,plbr3,plbr4,pmg1,pmg2,pmg3,p1,p2,p3,p4,p5,p6;
  string junk,tmp1,tmp2,tmp3,tmp4,tmp5;
  int mgflag=0;
  int np1,np2,np3,i,nmg,bcheck,nbrack,nobj;

  junk=story;
  
  int sz=junk.length();
  char fixstory[sz],tmpstory[sz];
  fixstory[0]='\0';
  tmpstory[0]='\0';

  //note, apparently C returns a large value when it doesn't find an expression within a string
  prbr1=999;
  prbr2=999;
  prbr3=999;
  plbr1=999;
  plbr2=999;
  plbr3=999;
  pmg1=999;
  pmg2=999;
  pmg3=999;
  
  //clean up the input; place any singles on the end
  bcheck=0;
  for (i=0;i<junk.length();i++){
	 if (junk[i] == '[') bcheck++;
	 if (bcheck > 0) strcat(fixstory,junk.substr(i,1).c_str()); else strcat(tmpstory,junk.substr(i,1).c_str());
	 if (junk[i] == ']') bcheck--;
  }
  strcat(fixstory,tmpstory);
  junk=fixstory;


  //count the number of brackets
  nbrack=0;
  p1=junk.find("[");
  tmp1=junk;
  while (p1 < 50) {
	 nbrack++;
	 tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
	 p1=tmp1.find("[");
  }

  //count the number of objects
  nobj=1;
  p1=junk.find("][");
  if (p1 > 50) p1=junk.find("] [");
  tmp1=junk;
  while (p1 < 50) {
	 nobj++;
	 tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
	 p1=tmp1.find("][");
	 if (p1 > 50) p1=tmp1.find("] [");
  }

  //count the number of mergers
  nmg=0;
  for (i=0; i<junk.length(); i++) if (junk[i] == ':') nmg++; 

  prbr1=junk.find("[");
  plbr1=junk.rfind("]");
  pmg1=junk.find(":");
  pmg2=junk.rfind(":");
  if (pmg1 > 50) { //no merger
	 if (prbr1 > 50){ //six single stars (no merger)
		bin1[6]=63;
		bin1[0]=atoi(junk.substr(0,1).c_str());
		bin1[1]=atoi(junk.substr(2,1).c_str()); 
		bin1[2]=atoi(junk.substr(4,1).c_str()); 
		bin1[3]=atoi(junk.substr(6,1).c_str()); 
		bin1[4]=atoi(junk.substr(8,1).c_str()); 
		bin1[5]=atoi(junk.substr(10,1).c_str()); 
	 }
	 if (prbr1 < 50 && bin1[6] == -1){ //there is at least one binary
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		if (prbr2 > 50 && bin1[6] == -1) { //binary + four singles
		  bin1[6]=62;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  bin1[1]=atoi(tmp1.substr(2,1).c_str()); 
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[2]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[3]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[4]=atoi(tmp2.substr(p1,1).c_str());
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  bin1[5]=atoi(tmp2.substr(p1,1).c_str());
		}
		if (prbr2 < 50 && bin1[6] == -1) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  if (prbr3 > 50 && plbr3 > 50 && bin1[6] == -1) {
			 prbr3=tmp1.rfind("[");
			 plbr3=tmp1.find("]");
			 if (plbr3 < prbr2 && prbr3 > plbr2 && prbr3 != prbr2 && plbr3 != prbr3) { //three binaries (unbound)
				bin1[6]=64;
				prbr3=100;
				plbr3=100;
				p1=junk.find("[");
				p2=junk.find("]");
				tmp3=junk.substr(p1+1,p2-1);
				bin1[0]=atoi(tmp3.substr(0,1).c_str());
				bin1[1]=atoi(tmp3.substr(2,1).c_str());
				tmp3=junk.substr(p2+1,junk.length()-p2-1);
				p1=tmp3.find("[");
				p2=tmp3.find("]");
				tmp4=tmp3.substr(p1+1,p2-1);
				bin1[2]=atoi(tmp4.substr(0,1).c_str());
				bin1[3]=atoi(tmp4.substr(2,1).c_str());
				tmp4=tmp3.substr(p2+1,tmp3.length()-p2-1);
				p1=tmp4.find("[");
				p2=tmp4.find("]");
				tmp5=tmp4.substr(p1+1,p2-1);
				bin1[4]=atoi(tmp5.substr(0,1).c_str());
				bin1[5]=atoi(tmp5.substr(2,1).c_str());	
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
			 if (prbr2 > plbr2 && bin1[6] == -1) { //two binaries unbound + two singles
				bin1[6]=75;
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				bin1[1]=atoi(tmp1.substr(2,1).c_str());
				tmp2=tmp1.substr(prbr2+1,tmp2.length()-prbr2-1);
				bin1[2]=atoi(tmp2.substr(0,1).c_str());
				bin1[3]=atoi(tmp2.substr(2,1).c_str());
				if (prbr2 == plbr2+2 || prbr2 == plbr2+1) tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); else tmp2=tmp1.substr(plbr2+1,prbr2-plbr2-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[4]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[5]=atoi(tmp2.substr(p1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
			 if (prbr2 < plbr2 && bin1[6] == -1) { //hierarchical triple + 3 singles
				bin1[6]=61;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				tmp2=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[2]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[3]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[4]=atoi(tmp2.substr(p1,1).c_str());
				tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[5]=atoi(tmp2.substr(p1,1).c_str());
			 }
		  }
		}
		if (((prbr3 < 50 && plbr3 > 50) || (prbr3 > 50 && plbr3 < 50)) && bin1[6] == -1){ //triple + binary unbound + single
		  bin1[6]=60;
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
		  p1=tmp2.find_first_not_of(" ");
		  bin1[5]=atoi(tmp2.substr(p1,1).c_str());
		  p1=junk.find("[[");
		  if (p1 < 50) {
			 tmp2=junk.substr(p1+2,junk.length()-p1-2);
			 p2=tmp2.find("]");
			 bin1[2]=atoi(tmp2.substr(p2+2,1).c_str());
			 tmp2=tmp2.substr(0,p2);
		  }
		  if (p1 > 50) {
			 p1=junk.find("]]");
			 tmp2=junk.substr(0,p1);
			 p2=tmp2.rfind("[");
			 bin1[2]=atoi(tmp2.substr(p2-2,1).c_str());
			 tmp2=tmp2.substr(p2+1,tmp2.length()-p2-1);
		  }
		  bin1[0]=atoi(tmp2.substr(0,1).c_str());
		  bin1[1]=atoi(tmp2.substr(2,1).c_str());
		  tmp2=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //second binary
		  p1=tmp2.find("[");
		  if (p1 < 50) {
			 bin1[3]=atoi(tmp2.substr(p1+1,1).c_str());
			 bin1[4]=atoi(tmp2.substr(p1+3,1).c_str());
		  }
		  if (p1 > 50) {
			 bin1[3]=atoi(tmp2.substr(0,1).c_str());
			 bin1[4]=atoi(tmp2.substr(2,1).c_str());
		  }
		  bin2[0]=bin1[3];
		  bin2[1]=bin1[4];
		}
		

		if (prbr3 < 50 && plbr3 < 50 && bin1[6] == -1) { 
		  tmp3=tmp2.substr(prbr3+1,plbr3-(prbr3+1));
		  prbr4=tmp3.find("[");
		  plbr4=tmp3.rfind("]");
		  if (prbr4 > 50 && plbr4 > 50 && bin1[6] == -1) {
			 if (prbr3 < plbr3) {//hierarchical quadruple + two single stars
				bin1[6]=74;
				bin1[0]=atoi(tmp3.substr(0,1).c_str());
				bin1[1]=atoi(tmp3.substr(2,1).c_str());
				tmp3=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //tertiary
				p1=tmp3.find_first_not_of(" ");
				bin1[2]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quad
				p1=tmp3.find_first_not_of(" ");
				bin1[3]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
				p1=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=tmp3.substr(p1+1,tmp3.length()-p1-1);
				p1=tmp3.find_first_not_of(" ");
				bin1[5]=atoi(tmp3.substr(p1,1).c_str());
			 }
			 if (prbr3 > plbr3 && bin1[6] == -1){ //quadruple composed of 2 binaries + two single stars
				bin1[6]=76;
				bin1[0]=atoi(tmp2.substr(0,1).c_str());
				bin1[1]=atoi(tmp2.substr(2,1).c_str());
				bin1[2]=atoi(tmp3.substr(0,1).c_str());
				bin1[3]=atoi(tmp3.substr(2,1).c_str());
				tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
				p1=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p1,1).c_str());
				tmp3=tmp3.substr(p1+1,tmp3.length()-p1-1);
				p1=tmp3.find_first_not_of(" ");
				bin1[5]=atoi(tmp3.substr(p1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
			 }
		  }
		  if (prbr4 < 50 && bin1[6] == -1) {	


			 //sextuples
			 if (nbrack == 5) {

				p1=junk.find("[[[[[");
				if (p1 > 50) p1=junk.find("]]]]]");
				if (p1 < 50) {//hierarchical sextuple
				  bin1[6]=66;
				  tmp4=tmp3.substr(prbr4+1,plbr4-(prbr4+1));
				  p2=tmp4.find("[");
				  p3=tmp4.find("]");
				  tmp5=tmp4.substr(p2+1,p3-p2-1);
				  bin1[0]=atoi(tmp5.substr(0,1).c_str());
				  bin1[1]=atoi(tmp5.substr(2,1).c_str());
				  tmp5=tmp4.substr(0,p2)+tmp4.substr(p3+1,tmp4.length()-p3-1); //tertiary
				  p1=tmp5.find_first_not_of(" ");
				  bin1[2]=atoi(tmp5.substr(p1,1).c_str());
				  tmp4=tmp3.substr(0,prbr4)+tmp3.substr(plbr4+1,tmp3.length()-plbr4-1); //quad
				  p1=tmp4.find_first_not_of(" ");
				  bin1[3]=atoi(tmp4.substr(p1,1).c_str());
				  tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //quint
				  p1=tmp4.find_first_not_of(" ");
				  bin1[4]=atoi(tmp4.substr(p1,1).c_str());			
				  tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //sext
				  p1=tmp4.find_first_not_of(" ");
				  bin1[5]=atoi(tmp4.substr(p1,1).c_str());
				  
				}

				p1=tmp1.find("] [");
				if (p1 > 50) p1=tmp1.find("][");
				if (p1 < 50) {
				  tmp2=tmp1.substr(0,p1+1);
				  tmp3=tmp1.substr(p1+1,tmp1.length()-p1-1);

				  p2=tmp3.find("[[");
				  if (p2 > 50) p2=tmp3.find("]]");
				  if (p2 < 50 && tmp2.length() == tmp3.length()) {//stable sextuple composed of two triples
					 bin1[6]=68;
					 
					 
					 tmp1=tmp2;
					 //first triple
					 p1=tmp1.find("[");
					 p2=tmp1.rfind("]");
					 tmp1=tmp1.substr(p1+1,p2-p1-1);
					 p1=tmp1.find("[");
					 p2=tmp1.find("]");
					 tmp4=tmp1.substr(p1+1,p2-p1-1);
					 p3=tmp4.find_first_not_of(" ");
					 bin1[0]=atoi(tmp4.substr(p3,1).c_str());
					 bin1[1]=atoi(tmp4.substr(p3+2,1).c_str());
					 tmp4=tmp1.substr(0,p1)+tmp1.substr(p2+1,tmp1.length()-p2-1); //tertiary
					 p3=tmp4.find_first_not_of(" ");
					 bin1[2]=atoi(tmp4.substr(p3,1).c_str());
					 tmp1=tmp3;
					 //second triple
					 p1=tmp1.find("[");
					 p2=tmp1.rfind("]");
					 tmp1=tmp1.substr(p1+1,p2-p1-1);
					 p1=tmp1.find("[");
					 p2=tmp1.find("]");
					 tmp4=tmp1.substr(p1+1,p2-p1-1);
					 p3=tmp4.find_first_not_of(" ");
					 bin1[3]=atoi(tmp4.substr(p3,1).c_str());
					 bin1[4]=atoi(tmp4.substr(p3+2,1).c_str());
					 tmp4=tmp1.substr(0,p1)+tmp1.substr(p2+1,tmp1.length()-p2-1); //tertiary
					 p3=tmp4.find_first_not_of(" ");
					 bin1[5]=atoi(tmp4.substr(p3,1).c_str());
					 bin2[0]=bin1[3];
					 bin2[1]=bin1[4];
				  }
				  p3=junk.find("[[");
				  p4=junk.rfind("]]");
				  if (p2 < 50 && tmp2.length() != tmp3.length() && p3 == 0 && p4 == junk.length()-2 && nobj == 3 && bin1[6] == -1) {//stable sextuple composed of hierarchical triple composed of three binaries
					 p1=tmp1.find("]] [");
					 if (p1 > 50) p1=tmp1.find("]][");
					 if (p1 > 50) p1=tmp1.find("][[");
					 if (p1 > 50) p1=tmp1.find("] [[");
					 if (p1 < 50) {
						tmp2=tmp1.substr(0,p1+1);
						tmp3=tmp1.substr(p1+1,tmp1.length()-p1-1);
					 }

					 bin1[6]=67;
					 if (tmp2.length() > tmp3.length()) tmp4=tmp2; else tmp4=tmp3;
					 p1=tmp4.find("[");
					 p2=tmp4.find("]");
					 while (p1 == 0 || p2 == 0) {
						if (p1 == 0) tmp4=tmp4.substr(1,tmp4.length()-1);
						if (p2 == 0) tmp4=tmp4.substr(1,tmp4.length()-1);
						p1=tmp4.find("[");
						p2=tmp4.find("]");
					 }
					 bin1[0]=atoi(tmp4.substr(0,1).c_str());
					 bin1[1]=atoi(tmp4.substr(2,1).c_str());
					 p1=tmp4.find("[");
					 bin1[2]=atoi(tmp4.substr(p1+1,1).c_str());
					 bin1[3]=atoi(tmp4.substr(p1+3,1).c_str());
					 tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
					 p1=tmp4.find_first_not_of(" ");
					 p2=tmp1.find("[[");
					 p3=tmp1.find("]]");
					 tmp4=tmp1.substr(0,p2)+tmp1.substr(p3+2,tmp1.length()-p3-2); //other binary
					 p4=tmp4.find("[");
					 bin1[4]=atoi(tmp4.substr(p4+1,1).c_str());
					 bin1[5]=atoi(tmp4.substr(p4+3,1).c_str());
					 bin2[0]=bin1[4];
					 bin2[1]=bin1[5];
				  }
				}

				///////////////////
				p3=junk.find("[[[");
				if (p3 > 50) p3=junk.find("]]]");
				
				if (p3 < 50 && bin1[6] == -1) {
				  p2=tmp1.find("][");
				  if (p2 > 50) p2=junk.find("] [");
				  if (p2 < 50) {
					 p3=tmp1.find("][");
					 if (p3 > 50) {
						p3=tmp1.find("] [");
						p3+=1;
					 }
					 tmp2=tmp1.substr(0,p3+1);
					 tmp3=tmp1.substr(p3+1,tmp1.length()-p3-1);
					 p1=tmp3.find("[[");
					 if (p1 > 50) p1=tmp3.find("]]");
					 p2=tmp2.find("[[");
					 if (p2 > 50) p2=tmp2.find("]]");
					 
					 p3=junk.find("[[");
					 p4=junk.rfind("]]");
					 p5=junk.find("[[[[");
					 if (p5 > 50) p5=junk.find("]]]]");
					 if ((p1 > 50 || p2 > 50) && p3 == 0 && p4 == junk.length()-2 && bin1[6] == -1) {//stable sextuple composed of hierarchical quad + binary
						bin1[6]=70;
						//binary
						p1=tmp3.find("[[[");
						if (p1 > 50) p1=tmp3.find("]]]");
						if (p1 > 50) tmp1=tmp3; else tmp1=tmp2;
						p4=tmp1.find("[");
						while (p4 < 50) {
						  tmp1=tmp1.substr(0,p4)+tmp1.substr(p4+1,tmp1.length()-p4-1);
						  p4=tmp1.find("[");
						}
						p4=tmp1.find_first_not_of(" ");
						bin1[4]=atoi(tmp1.substr(p4,1).c_str());
						bin1[5]=atoi(tmp1.substr(p4+2,1).c_str());
						bin2[0]=bin1[4];
						bin2[1]=bin1[5];
						//quadruple
						if (p1 > 50) tmp1=tmp2; else tmp1=tmp3;
						p1=tmp1.find("[");
						p2=tmp1.rfind("]");
						tmp2=tmp1.substr(p1+1,p2-p1-1);
						p1=tmp2.find("[");
						p2=tmp2.rfind("]");
						tmp3=tmp2.substr(0,p1)+tmp2.substr(p2+1,tmp2.length()-p2-1);
						p3=tmp3.find_first_not_of(" ");
						bin1[3]=atoi(tmp3.substr(p3,1).c_str()); //quad
						p1=tmp2.find("[");
						p2=tmp2.rfind("]");
						tmp2=tmp2.substr(p1+1,p2-p1-1);
						p1=tmp2.find("[");
						p2=tmp2.rfind("]");
						tmp3=tmp2.substr(0,p1)+tmp2.substr(p2+1,tmp2.length()-p2-1);
						p3=tmp3.find_first_not_of(" ");
						bin1[2]=atoi(tmp3.substr(p3,1).c_str()); //tertiary
						p1=tmp2.find("[");
						p2=tmp2.rfind("]");
						tmp2=tmp2.substr(p1+1,p2-p1-1);
						p1=tmp2.find("[");
						if (p1 > 50) p1=tmp2.find("]");
						if (p1 < 50) tmp2=tmp2.substr(0,p1)+tmp2.substr(p1+1,tmp2.length()-p1-1); //inner binary
						p3=tmp2.find_first_not_of(" ");
						bin1[0]=atoi(tmp2.substr(p3,1).c_str());
						bin1[1]=atoi(tmp2.substr(p3+2,1).c_str());
					 }
					 
					 p3=junk.find("[[[");
					 if (p3 > 50) p3=junk.find("]]]");
					 if (p1 < 50 && p3 < 50 &&  nbrack == 5 && bin1[6] == -1) {//stable sextuple composed of hierachical triple + binary + single (similar to 72)
						bin1[6]=69;
						p1=tmp1.find("][");
						if (p1 > 50) p1=tmp1.find("] [");
						tmp2=tmp1.substr(0,p1);
						tmp3=tmp1.substr(p1+2,tmp1.length()-p1-2);
						if (tmp2.length() > tmp3.length()) tmp1=tmp2; else tmp1=tmp3;
						//triple
						p1=tmp1.find("[[");
						if (p1 < 50) tmp4=tmp1.substr(p1,tmp1.length()-p1); else tmp4=tmp1;
						p4=tmp4.rfind("[");
						bin1[0]=atoi(tmp4.substr(p4+1,1).c_str());
						bin1[1]=atoi(tmp4.substr(p4+3,1).c_str());
						tmp4=tmp4.substr(0,p4)+tmp4.substr(p4+4,tmp4.length()-p4-4);
						p4=tmp4.find("[");
						while (p4 < 50) {
						  tmp4=tmp4.substr(0,p4)+tmp4.substr(p4+1,tmp4.length()-p4-1);
						  p4=tmp4.find("[");
						}
						p4=tmp4.find("]");
						while (p4 < 50) {
						  tmp4=tmp4.substr(0,p4)+tmp4.substr(p4+1,tmp4.length()-p4-1);
						  p4=tmp4.find("]");
						}
						p4=tmp4.find_first_not_of(" ");
						bin1[2]=atoi(tmp4.substr(p4,1).c_str());
						tmp4=tmp4.substr(p4+1,tmp4.length()-p4-1);
						if (p1 < 50) tmp4=tmp4+tmp1.substr(0,p1);
						//binary plus single
						if (tmp2.length() > tmp3.length()) tmp1=tmp3+tmp4; else tmp1=tmp2+tmp4;
						p3=tmp1.find_first_not_of(" ");
						tmp1=tmp1.substr(p3,tmp1.length()-p3);
						p3=tmp1.find_first_not_of("[");
						tmp4=tmp1.substr(p3,tmp1.length()-p3);
						p3=tmp4.rfind("[");
						if (p3 < 50) tmp4=tmp4.substr(p3+1,tmp4.length()-p3-1);
						p4=tmp4.find_first_not_of(" ");
						bin1[3]=atoi(tmp4.substr(p4,1).c_str());
						bin1[4]=atoi(tmp4.substr(p4+2,1).c_str());
						p1=tmp1.rfind("[");
						if (p1 < 50) tmp5=tmp1.substr(0,p1); else tmp5="";
						p2=tmp1.find("]");
						if (p2 < 50) tmp5=tmp5+tmp1.substr(p2+1,tmp1.length()-p2-1); else tmp5=tmp5+tmp4.substr(p4+3,tmp4.length()-p4-3);
						p1=tmp5.find("[");
						while (p1 < 50) {
						  tmp5=tmp5.substr(0,p1)+tmp5.substr(p1+1,tmp5.length()-p1-1);
						  p1=tmp5.find("[");
						}
						p1=tmp5.find("]");
						while (p1 < 50) {
						  tmp5=tmp5.substr(0,p1)+tmp5.substr(p1+1,tmp5.length()-p1-1);
						  p1=tmp5.find("]");
						}
						p4=tmp5.find_first_not_of(" ");
						bin1[5]=atoi(tmp5.substr(p4,1).c_str());
						bin2[0]=bin1[3];
						bin2[1]=bin1[4];
					 }
				  }
				}
			 }
			 p3=junk.find("[[[[");
			 if (p3 > 50) p3=junk.find("]]]]");
			 if (p2 > 50 && p3 < 50 && bin1[6] == -1){//hierarchical quintuple + single star
				bin1[6]=71;
				tmp4=tmp3.substr(prbr4+1,plbr4-(prbr4+1));
				bin1[0]=atoi(tmp4.substr(0,1).c_str());
				bin1[1]=atoi(tmp4.substr(2,1).c_str());
				tmp4=tmp3.substr(0,prbr4)+tmp3.substr(plbr4+1,tmp3.length()-plbr4-1); //tertiary
				p1=tmp4.find_first_not_of(" ");
				bin1[2]=atoi(tmp4.substr(p1,1).c_str());
				tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //quad
				p1=tmp4.find_first_not_of(" ");
				bin1[3]=atoi(tmp4.substr(p1,1).c_str());			
				tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quint
				p1=tmp4.find_first_not_of(" ");
				bin1[4]=atoi(tmp4.substr(p1,1).c_str());
				tmp4=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
				p1=tmp4.find_first_not_of(" ");
				bin1[5]=atoi(tmp4.substr(p1,1).c_str());
			 }
		  }
		}

		if (( (prbr4 < 50 && plbr4 > 50) || (plbr4 < 50 && prbr4 > 50)) && bin1[6] == -1) {
		  p1=junk.find("[[[");
		  if (p1 > 50) p1=junk.find("]]]");
		  p2=junk.find("[");
		  p3=junk.rfind("]");
		  if (p1 > 50 && p2 == 0 && p3 == junk.length()-1 && bin1[6] == -1) {//triple + triple
			 bin1[6]=59;
			 p2=tmp1.find("]");	
			 p3=tmp2.find_first_not_of(" ");
			 bin1[0]=atoi(tmp2.substr(p3,1).c_str());
			 bin1[1]=atoi(tmp2.substr(p3+2,1).c_str());
			 tmp2=tmp1.substr(0,prbr2)+tmp1.substr(p2+1,tmp1.length()-p2-1); //for tertiary
			 p3=tmp2.rfind("[");
			 p3=tmp2.find_first_not_of(" ");
			 bin1[2]=atoi(tmp2.substr(p3,1).c_str());
			 p3=tmp2.find("[");
			 tmp3=tmp2.substr(p3+1,tmp2.length()-p3-1);
			 p3=tmp3.find("[");
			 p4=tmp3.find("]");
			 tmp4=tmp3.substr(p3+1,p4-(p3+1));
			 bin1[3]=atoi(tmp4.substr(0,1).c_str());
			 bin1[4]=atoi(tmp4.substr(2,1).c_str());
			 tmp4=tmp3.substr(0,p3)+tmp3.substr(p4+1,tmp3.length()-p4-1); //for tertiary
			 p3=tmp4.find_first_not_of(" ");
			 bin1[5]=atoi(tmp4.substr(p3,1).c_str());
			 bin2[0]=bin1[3];
			 bin2[1]=bin1[4];
		  }
		  p2=junk.find("[[");
		  p3=junk.rfind("]]");
		  if (p1 < 50 || (p1 > 50 && p2 < 50 && p3 < 50) && bin1[6] == -1) {
			 //same as triple + binary + single (60), but starting at tmp1
			 tmp4=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
			 p1=tmp4.find_first_not_of(" ");
			 if (p1 > 50) { //hierarchical quadruple and binary
				bin1[6]=65;
				p1=junk.find("] [");
				if (p1 > 50) p1=junk.find("][");
				if (p1 < 50) {
				  tmp1=junk.substr(0,p1+1);
				  tmp2=junk.substr(p1+1,junk.length()-p1-1);
				}
				p1=tmp1.find("[[[");
				if (p1 > 50) p1=tmp1.find("]]]");
				if (p1 > 50) tmp3=tmp1; else tmp3=tmp2;
				//binary
				p2=tmp3.find("[");
				tmp4=tmp3.substr(p2+1,tmp3.length()-p2-1);
				bin1[4]=atoi(tmp4.substr(0,1).c_str());
				bin1[5]=atoi(tmp4.substr(2,1).c_str());
				bin2[0]=bin1[4];
				bin2[1]=bin1[5];
				if (p1 > 50) tmp3=tmp2; else tmp3=tmp1;
				//quadruple
				p1=tmp3.find("[");
				p2=tmp3.rfind("]");
				tmp1=tmp3.substr(p1+1,p2-p1-1);
				p1=tmp1.find("[");
				p2=tmp1.rfind("]");
				tmp2=tmp1.substr(0,p1)+tmp1.substr(p2+1,junk.length()-p2-1);
				p3=tmp2.find_first_not_of(" ");
				bin1[3]=atoi(tmp2.substr(p3,1).c_str());
				tmp2=tmp1.substr(p1+1,p2-p1-1);
				p1=tmp2.find("[");
				p2=tmp2.rfind("]");
				tmp3=tmp2.substr(0,p1)+tmp2.substr(p2+1,junk.length()-p2-1);
				p3=tmp3.find_first_not_of(" ");
				bin1[2]=atoi(tmp3.substr(p3,1).c_str());
				tmp1=tmp2.substr(p1+1,p2-p1-1);
				p3=tmp1.find_first_not_of(" ");
				bin1[0]=atoi(tmp1.substr(p3,1).c_str());
				bin1[1]=atoi(tmp1.substr(p3+2,1).c_str());
		
			 }
			 if (p1 < 50 && bin1[6] == -1) { //stable quintuple composed of binary + triple + single star
				bin1[6]=73;
				bin1[5]=atoi(tmp4.substr(p1,1).c_str());
				p1=tmp1.find("[[");
				if (p1 < 50) {
				  tmp4=tmp1.substr(p1+2,tmp1.length()-p1-2);
				  p2=tmp4.find("]");
				  bin1[2]=atoi(tmp4.substr(p2+2,1).c_str());
				  tmp4=tmp4.substr(0,p2);
				}
				if (p1 > 50) {
				  p1=tmp1.find("]]");
				  tmp4=tmp1.substr(0,p1);
				  p2=tmp4.rfind("[");
				  bin1[2]=atoi(tmp4.substr(p2-2,1).c_str());
				  tmp4=tmp4.substr(p2+1,tmp4.length()-p2-1);
				}
				bin1[0]=atoi(tmp4.substr(0,1).c_str());
				bin1[1]=atoi(tmp4.substr(2,1).c_str());
				tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //second binary
				p1=tmp4.find("[");
				if (p1 < 50) {
				  bin1[3]=atoi(tmp4.substr(p1+1,1).c_str());
				  bin1[4]=atoi(tmp4.substr(p1+3,1).c_str());
				}
				if (p1 > 50) {
				  bin1[3]=atoi(tmp4.substr(0,1).c_str());
				  bin1[4]=atoi(tmp4.substr(2,1).c_str());
				}
				bin2[0]=bin1[3];
				bin2[1]=bin1[4];
			 }
		  }
		}
		if (prbr4 < 50 && plbr4 < 50 && bin1[6] == -1) { 
		  p1=tmp3.find("[");
		  if (p1 == 0) tmp3=tmp3.substr(1,tmp3.length()-1);
		  bin1[0]=atoi(tmp3.substr(0,1).c_str());
		  bin1[1]=atoi(tmp3.substr(2,1).c_str());
		  p1=tmp3.find("[");
		  bin1[2]=atoi(tmp3.substr(p1+1,1).c_str());
		  bin1[3]=atoi(tmp3.substr(p1+3,1).c_str());
		  tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
		  p1=tmp4.find_first_not_of(" ");
		  if (p1 > 50) { 
			 p1=junk.find("] [");
			 if (p1 > 50) p1=junk.find("][");
			 if (p1 < 50) {
				tmp2=junk.substr(0,p1+1);
				tmp3=junk.substr(p1+1,junk.length()-p1-1);
			 }
		  }
		  if (p1 < 50 && bin1[6] == -1) { //stable quintuple composed of hierarchical triple with two binaries in inner binary + single star
			 bin1[6]=72;
			 p1=tmp4.find_first_not_of(" ");
			 bin1[4]=atoi(tmp4.substr(p1,1).c_str());
			 tmp4=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single star
			 p1=tmp4.find_first_not_of(" ");
			 bin1[5]=atoi(tmp4.substr(p1,1).c_str());
			 bin2[0]=bin1[2];
			 bin2[1]=bin1[3];
		  }
		}
	 } 
  }

  //  if (nmg == 1) { //single merger
  if (pmg1 < 50 && pmg2 == pmg1) { //single merger
	 mgflag=1;
	 if (prbr1 > 50 && bin1[6] == -1) { //two-star merger + four singles
		bin1[6]=102;
		bin1[0]=atoi(junk.substr(0,1).c_str());
		p1=junk.find(" ");
		tmp1=junk.substr(p1+1,junk.length()-p1-1);
		bin1[1]=atoi(tmp1.substr(0,1).c_str());
		p1=tmp1.find(" ");
		tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		bin1[2]=atoi(tmp1.substr(0,1).c_str());
		p1=tmp1.find(" ");
		tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		bin1[3]=atoi(tmp1.substr(0,1).c_str());
		p1=tmp1.find(" ");
		tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		bin1[4]=atoi(tmp1.substr(0,1).c_str());
	 }
	 if (prbr1 < 50) {
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1);
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		if (prbr2 > 50 && pmg1 >prbr1 && pmg1 < plbr1 && bin1[6] == -1) { //binary containing merger + three singles
		  bin1[6]=100;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  bin1[1]=atoi(tmp1.substr(p1+1,1).c_str());
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //other stars
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[2]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find(" ");
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[3]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find(" ");
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[4]=atoi(tmp2.substr(0,1).c_str());
		}
		if (prbr2 > 50 && (pmg1 < prbr1 || pmg1 > plbr1) && bin1[6] == -1) { //binary + two-star merger + singles
		  bin1[6]=101;
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  bin1[1]=atoi(tmp1.substr(2,1).c_str());
		  tmp2=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //other stars
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[2]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find(" ");
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[3]=atoi(tmp2.substr(0,1).c_str());
		  p1=tmp2.find(" ");
		  tmp2=tmp2.substr(p1+1,tmp2.length()-p1-1);
		  p1=tmp2.find_first_not_of(" ");
		  tmp2=tmp2.substr(p1,tmp2.length()-p1);
		  bin1[4]=atoi(tmp2.substr(0,1).c_str());
		}
		if (prbr2 < 50 && bin1[6] == -1) {
		  tmp2=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
		  prbr3=tmp2.find("[");
		  plbr3=tmp2.rfind("]");
		  if (prbr3 > 50 && plbr3 > 50 && bin1[6] == -1) {
			 p1=junk.find("[[");
			 if (p1 > 50) p1=junk.find("]]");
			 if (p1 > 50 && bin1[6] == -1) { 
				bin1[0]=atoi(tmp1.substr(0,1).c_str());
				p2=tmp1.find(" ");
				bin1[1]=atoi(tmp1.substr(p2+1,1).c_str());
				bin1[2]=atoi(tmp2.substr(0,1).c_str());
				p2=tmp2.find(" ");
				bin1[3]=atoi(tmp2.substr(p2+1,1).c_str());
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];
				p1=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p1,1).c_str());
				p4=tmp3.find(":");
				if (p4 > 50) bin1[6]=95; // binary + binary containing 2-star merger + single star
				if (p4 < 50) bin1[6]=96; // binary + binary + 2-star merger
			 }
			 if (p1 < 50 && bin1[6] == -1) {
				p2=tmp1.find(":");
				p3=tmp2.find(":");
				if (p2 > 50 && p3 > 50 && bin1[6] == -1) {//hierarchical triple + 2-star merger + single star
				  bin1[6]=99;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  bin1[1]=atoi(tmp2.substr(2,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //singles
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=tmp3.substr(p4,tmp3.length()-p4);
				  p4=tmp3.find(" ");
				  bin1[4]=atoi(tmp3.substr(p4+1,1).c_str());
				}
				if (p2 < 50 && p3 < 50 && bin1[6] == -1) {//hierarchical triple with 2-star merger in inner binary + 2 singles
				  bin1[6]=97;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  p4=tmp2.find(" ");
				  bin1[1]=atoi(tmp2.substr(p4+1,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //singles
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=tmp3.substr(p4,tmp3.length()-p4);
				  p4=tmp3.find(" ");
				  bin1[4]=atoi(tmp3.substr(p4+1,1).c_str());
				}
				if (p2 < 50 && p3 > 50 && bin1[6] == -1) {//hierarchical triple with 2-star merger as tertiary + 2 singles
				  bin1[6]=98;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  bin1[1]=atoi(tmp2.substr(2,1).c_str());
				  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				  p4=tmp3.find_first_not_of(" ");
				  bin1[2]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
				  p4=tmp3.find_first_not_of(" ");
				  bin1[3]=atoi(tmp3.substr(p4,1).c_str());
				  tmp3=tmp3.substr(p4,tmp3.length()-p4);
				  p4=tmp3.find(" ");
				  bin1[4]=atoi(tmp3.substr(p4+1,1).c_str());
				}
			 }
		  }
		  p1=junk.find("[[[[");
		  if (p1 > 50) p1=junk.find("]]]]");
		  if (p1 < 50) { //quintuple
			 tmp3=tmp2.substr(prbr3+1,plbr3-(prbr3+1));
			 prbr4=tmp3.find("[");
			 plbr4=tmp3.find("]");
			 tmp4=tmp3.substr(prbr4+1,plbr4-(prbr4+1));
			 p1=tmp4.find(":");
			 if (p1 < 50) bin1[6]=80; //hierarchical quintiuple with 2-star merger in inner binary
			 bin1[0]=atoi(tmp3.substr(prbr4+1,1).c_str());
			 bin1[1]=atoi(tmp3.substr(plbr4-1,1).c_str());
			 tmp4=tmp3.substr(0,prbr4)+tmp3.substr(plbr4+1,tmp3.length()-plbr4-1); //tertiary
			 p1=tmp4.find_first_not_of(" ");
			 bin1[2]=atoi(tmp4.substr(p1,1).c_str());
			 p1=tmp4.find(":");
			 if (p1 < 50) bin1[6]=81; //hierarchical quintiuple with 2-star merger in tertiary
			 tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //quad
			 p1=tmp4.find_first_not_of(" ");
			 bin1[3]=atoi(tmp4.substr(p1,1).c_str());		
			 p1=tmp4.find(":");
			 if (p1 < 50) bin1[6]=82; //hierarchical quintiuple with 2-star merger in quad
			 tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quint
			 p1=tmp4.find_first_not_of(" ");
			 bin1[4]=atoi(tmp4.substr(p1,1).c_str());
			 p1=tmp4.find(":");
			 if (p1 < 50) bin1[6]=83; //hierarchical quintiuple with 2-star merger in quint
		  }
		  if ( ((prbr3 > 50 && plbr3 < 50) || (prbr3 < 50 && plbr3 > 50)) && bin1[6] == -1) {
			 p1=junk.find("] [");
			 if (p1 > 50) p1=junk.find("][");
			 if (p1 < 50) {
				tmp2=junk.substr(0,p1+1);
				tmp3=junk.substr(p1+1,junk.length()-p1-1);
			 }
			 //triple
			 p2=tmp2.find("[[");
			 if (p2 > 50) p2=tmp2.find("]]");
			 if (p2 < 50) tmp1=tmp2; else tmp1=tmp3;
			 p1=tmp1.find("[");
			 p2=tmp1.rfind("]");
			 tmp1=tmp1.substr(p1+1,p2-p1-1);
			 p1=tmp1.find("[");
			 p2=tmp1.find("]");
			 tmp4=tmp1.substr(p1+1,p2-p1-1);
			 p3=tmp4.find_first_not_of(" ");
			 bin1[0]=atoi(tmp4.substr(p3,1).c_str());
			 bin1[1]=atoi(tmp4.substr(p3+2,1).c_str());
			 tmp4=tmp1.substr(0,p1)+tmp1.substr(p2+1,tmp1.length()-p2-1); //tertiary
			 p3=tmp4.find_first_not_of(" ");
			 bin1[2]=atoi(tmp4.substr(p3,1).c_str());
			 p3=tmp1.find(":");
			 if (p3 < 50) bin1[6]=78; //hierarchical triple with 2-star merger in inner binary + binary
			 p3=tmp4.find(":");
			 if (p3 < 50) bin1[6]=79; //hierarchical triple with 2-star merger for tertiary + binary
			 //binary
			 p2=tmp2.find("[[");
			 if (p2 > 50) p2=tmp2.find("]]");
			 if (p2 > 50) tmp1=tmp2; else tmp1=tmp3;
			 p1=tmp1.find("[");			 
			 bin1[3]=atoi(tmp1.substr(p1+1,1).c_str());
			 p1=tmp1.find("]");			 
			 bin1[4]=atoi(tmp1.substr(p1-1,1).c_str());
			 p3=tmp1.find(":");
			 if (p3 < 50) bin1[6]=77; //hierarchical triple + binary with 2-star merger
			 bin2[0]=bin1[3];
			 bin2[1]=bin1[4];
		  }
		  if (prbr3 < 50 && bin1[6] == -1) {
			 if (tmp3.length() == 0) {
				p1=tmp1.find("] [");
				if (p1 > 50) p1=tmp1.find("][");
				if (p1 < 50) {
				  tmp2=tmp1.substr(0,p1+1);
				  tmp3=tmp1.substr(p1+1,tmp1.length()-p1-1);
				}
				p1=tmp1.find("[[");
				p2=tmp1.find("]]");
				if (p1 < 50 && p2 < 50){
				  tmp4=tmp1.substr(0,p1)+tmp1.substr(p2+2,tmp1.length()-p2-2); //single				  
				  p3=tmp4.find_first_not_of(" ");
				  bin1[4]=atoi(tmp4.substr(p3,1).c_str());
				  p4=tmp4.find(":");
				  if (p4 < 50) bin1[6]=86; // stable quintuple in triple config with two binaries  + 2-star merger
				  if (p4 > 50) bin1[6]=85; // stable quintuple in triple config with two binaries (one containing 2-star merger) + single
				  tmp4=tmp1.substr(p1+2,p2-p1-2);
				  p1=tmp4.find("]");
				  tmp2=tmp4.substr(0,p1);
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  bin1[1]=atoi(tmp2.substr(tmp2.length()-1,1).c_str());
				  p1=tmp4.find("[");
				  tmp2=tmp4.substr(p1+1,tmp4.length()-p1-1);
				  bin1[2]=atoi(tmp2.substr(0,1).c_str());
				  bin1[3]=atoi(tmp2.substr(tmp2.length()-1,1).c_str());

				  bin2[0]=bin1[2];
				  bin2[1]=bin1[3];
				} else if (bin1[6] == -1) {
				  //same as 77, but starting from tmp1
				  bin1[6]=84; //stable quintuple composed of binary (composed of triple with 2-star merger in inner binary + binary)
				  //triple
				  p2=tmp2.find("[[");
				  if (p2 > 50) p2=tmp2.find("]]");
				  if (p2 < 50) tmp1=tmp2; else tmp1=tmp3;
				  p1=tmp1.find("[");
				  p2=tmp1.rfind("]");
				  tmp1=tmp1.substr(p1+1,p2-p1-1);
				  p1=tmp1.find("[");
				  p2=tmp1.find("]");
				  tmp4=tmp1.substr(p1+1,p2-p1-1);
				  p3=tmp4.find_first_not_of(" ");
				  bin1[0]=atoi(tmp4.substr(p3,1).c_str());
				  bin1[1]=atoi(tmp4.substr(p3+2,1).c_str());
				  tmp4=tmp1.substr(0,p1)+tmp1.substr(p2+1,tmp1.length()-p2-1); //tertiary
				  p3=tmp4.find_first_not_of(" ");
				  bin1[2]=atoi(tmp4.substr(p3,1).c_str());
				  p3=tmp1.find(":");
				  p3=tmp4.find(":");
				  if (p3 < 50) bin1[6]=84; //hierarchical triple with 2-star merger in inner binary + binary in quint config
				  p3=tmp4.find(":");
				  if (p3 < 50) bin1[6]=88; //hierarchical triple with 2-star merger for tertiary + binary in quint config
				  //binary
				  p2=tmp2.find("[[");
				  if (p2 > 50) p2=tmp2.find("]]");
				  if (p2 > 50) tmp1=tmp2; else tmp1=tmp3;
				  p1=tmp1.find("[");			 
				  bin1[3]=atoi(tmp1.substr(p1+1,1).c_str());
				  p1=tmp1.find("]");			 
				  bin1[4]=atoi(tmp1.substr(p1-1,1).c_str());
				  p3=tmp1.find(":");
				  if (p3 < 50) bin1[6]=87; //hierarchical triple + binary with 2-star merger in quint config
				  bin2[0]=bin1[3];
				  bin2[1]=bin1[4];				
				}
			 } else if (bin1[6] == -1) {
				p2=tmp3.find_first_not_of(" ");
				bin1[4]=atoi(tmp3.substr(p2,1).c_str());
				p5=tmp3.find(":");
				tmp3=tmp2.substr(prbr3+1,plbr3-(prbr3+1));
				p1=junk.find("[[[");
				if (p1 > 50) p1=junk.find("]]]");
				if (p1 > 50 && bin1[6] == -1) { 
				  bin1[6]=93;
				  bin1[0]=atoi(tmp2.substr(0,1).c_str());
				  p2=tmp2.find(" ");
				  bin1[1]=atoi(tmp2.substr(p2+1,1).c_str());
				  tmp3=tmp2.substr(prbr3+1,tmp2.length()-prbr3-1);
				  bin1[2]=atoi(tmp3.substr(0,1).c_str());
				  p2=tmp3.find(" ");
				  bin1[3]=atoi(tmp3.substr(p2+1,1).c_str());
				  bin2[0]=bin1[2];
				  bin2[1]=bin1[3];
				  if (p5 > 50) bin1[6]=93; //stable quadruple composed of two binaries, one of which contains a merger + single
				  if (p5 < 50) bin1[6]=94; //stable quadruple composed of two binaries +  2-star merger
				}
				if (p1 < 50 && bin1[6] == -1) {
				  p2=tmp2.find(":");
				  p3=tmp3.find(":");
				  bin1[0]=atoi(tmp3.substr(0,1).c_str());
				  p4=tmp3.find(" ");
				  bin1[1]=atoi(tmp3.substr(p4+1,1).c_str());
				  tmp4=tmp2.substr(0,prbr3)+tmp2.substr(plbr3+1,tmp2.length()-plbr3-1); //tertiary
				  p4=tmp4.find_first_not_of(" ");
				  bin1[2]=atoi(tmp4.substr(p4,1).c_str());
				  tmp4=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quad
				  p4=tmp4.find_first_not_of(" ");
				  bin1[3]=atoi(tmp4.substr(p4,1).c_str());
				  
				  if (p2 < 50 && p3 < 50 && bin1[6] == -1){ //hierarchical quadruple with 2-star merger in inner binary + single
					 bin1[6]=89;
				  }
				  if (p2 < 50 && p3 > 50 && bin1[6] == -1){ //hierarchical quadruple with 2-star merger as tertiary + single
					 bin1[6]=90;
				  }
				  if (p2 > 50 && p3 > 50 && p5 > 50 && bin1[6] == -1) { //hierarchical quadruple with 2-star merger as quad + single
					 bin1[6]=91;
				  }
				  if (p2 > 50 && p3 > 50 && p5 < 50 && bin1[6] == -1) { //hierarchical quadruple with 2-star merger as quad + single
					 bin1[6]=92;
				  }
				}
			 }
		  }
		}
	 }
  }
  //  if (nmg == 2) { //two mergers
  if (pmg1 < 50 && pmg2 != pmg1) { //two mergers
	 mgflag=1;
	 if (prbr1 > 50) { 
		tmp2=junk.substr(pmg1+1,pmg2-pmg1-1);
		pmg3=tmp2.find(":");
		if (pmg3 > 50 && bin1[6] == -1) {
		  bin1[0]=atoi(junk.substr(0,1).c_str());
		  p1=junk.find(" ");
		  tmp1=junk.substr(p1+1,junk.length()-p1-1);
		  bin1[1]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  bin1[2]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp1=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  bin1[3]=atoi(tmp1.substr(0,1).c_str());
		  if (pmg2 == pmg1+2) {//3-star merger + three singles
			 bin1[6]=129;
		  } else bin1[6]=119;//two 2-star mergers + two single
		}
		if (pmg3 < 50 && bin1[6] == -1) {
		  bin1[0]=atoi(junk.substr(0,1).c_str());
		  p1=junk.find(" ");
		  if (p1 < 50 && bin1[6] == -1) {
			 tmp1=junk.substr(p1+1,junk.length()-p1-1);
			 tmp2=junk.substr(0,p1);
			 p4=tmp1.find(" ");
			 if (p4 < 50) {
				tmp3=tmp1.substr(p4+1,tmp1.length()-p4-1);
				tmp1=tmp1.substr(0,p4);
			 }
			 if (p4 > 50) {
				p4=tmp2.find(" ");
				if (p4 < 50) {
				  tmp3=tmp2.substr(p4+1,tmp2.length()-p4-1);
				  tmp2=tmp2.substr(0,p4);
				}
			 }
			 if (p4 < 50) {
				bin1[1]=atoi(tmp1.substr(0,1).c_str());
				bin1[2]=atoi(tmp3.substr(0,1).c_str());
				p2=tmp1.find(":");
				p3=tmp2.find(":");
				p4=tmp3.find(":");
				if ((p2 < 50 && p3 > 50 && p4 > 50) || (p2 > 50 && p3 < 50 && p4 > 50) || (p2 > 50 && p3 > 50 && p4 < 50) && bin1[6] == -1) bin1[6]=144; // 4-star merger + 2 singles
				if ((p2 < 50 && p3 < 50 && p4 > 50) || (p2 < 50 && p3 > 50 && p4 < 50) || (p2 > 50 && p3 < 50 && p4 < 50) && bin1[6] == -1) bin1[6]=139; //3-star merger + 2-star merger + single
				if (p2 < 50 && p3 < 50 && p4 < 50 && bin1[6] == -1) bin1[6]=132; //three 2-star mergers
			 } else if (nmg == 4){
				//tmp1 and tmp2 are reversed here for some reason
				tmp3=tmp1;
				tmp1=tmp2;
				tmp2=tmp3;
				p1=tmp1.find_first_not_of(" ");
				bin1[0]=atoi(tmp1.substr(p1,1).c_str());
				p1=tmp2.find_first_not_of(" ");
				bin1[1]=atoi(tmp2.substr(p1,1).c_str());
				np1=0;
				for (i=0; i<tmp1.length(); i++) if (tmp1[i] == ':') np1++; //merger in star 1
				np2=0;
				for (i=0; i<tmp2.length(); i++) if (tmp2[i] == ':') np2++; //merger in star 2
				if (np1 == 2 && np2 == 2 && bin1[6] == -1) bin1[6]=148; //two 3-star mergers (unbound)
				if ((np1 == 4 || np2 == 4) && bin1[6] == -1) bin1[6]=149; //5-star merger + single (unbound)
				if (((np1 == 3 && np2 == 1) || (np1 == 1 && np2 == 3)) && bin1[6] == -1) bin1[6]=150; //4-star merger + 2-star merger (unbound)
			 }
		  }
		  if (p1 > 50 && bin1[6] == -1) bin1[6]=151; //6-star merger;
		}
	 }
	 /////////
	 if (prbr1 < 50 && bin1[6] == -1) {
		tmp1=junk.substr(prbr1+1,plbr1-(prbr1+1));
		prbr2=tmp1.find("[");
		plbr2=tmp1.rfind("]");
		tmp2=junk.substr(pmg1+1,pmg2-pmg1-1);
		pmg3=tmp2.find(":");
		if (plbr2 > 50 && pmg3 > 50) {
		  bin1[0]=atoi(tmp1.substr(0,1).c_str());
		  p1=tmp1.find(" ");
		  tmp2=tmp1.substr(p1+1,tmp1.length()-p1-1);
		  bin1[1]=atoi(tmp2.substr(0,1).c_str());
		  tmp1=tmp1.substr(0,p1);
		  tmp3=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //singles
		  p1=tmp3.find_first_not_of(" ");
		  bin1[2]=atoi(tmp3.substr(p1,1).c_str());
		  p1=tmp3.find_last_not_of(" ");
		  bin1[3]=atoi(tmp3.substr(p1,1).c_str());
		  p1=tmp3.rfind(" ");
		  if (p1 == tmp3.length()-1) tmp3=tmp3.substr(0,p1);
		  p1=tmp3.rfind(" ");
		  tmp4=tmp3.substr(p1+1,tmp3.length()-p1-1);
		  tmp3=tmp3.substr(0,p1);
		 

		  p1=tmp1.find(":"); //merger in binary (1st star)
		  p2=tmp2.find(":"); //merger in binary (2nd star)
		  p3=tmp3.find(":"); //merger in singles (1st star)
		  p4=tmp4.find(":"); //merger in singles (2nd star)
		  if ((p1 < 50 || p2 < 50) && (p3 < 50 || p4 < 50) && bin1[6] == -1) bin1[6]=116; //binary with 2-star merger + 2 singles, one of which is 2-star merger
		  if ((p1 < 50 && p2 < 50) && (p3 > 50 && p4 > 50) && bin1[6] == -1) bin1[6]=117; //binary with two 2-star merger + 2 singles
		  if ((p1 > 50 && p2 > 50) && (p3 < 50 && p4 < 50) && bin1[6] == -1) bin1[6]=118; //binary + 2 singles, both of which are 2-star mergers
		  if (((p1 < 50 && p2 > 50) || (p1 > 50 && p2 < 50))  && (p3 > 50 && p4 > 50) && bin1[6] == -1) bin1[6]=127; //binary with 3-star merger + 2 singles
		  if ((p1 > 50 && p2 > 50) && ((p3 < 50 && p4 > 50) || (p3 > 50 && p4 < 50)) && bin1[6] == -1) bin1[6]=128; //binary + 3-star merger + single
		}
		if (plbr2 < 50 && pmg3 > 50 && bin1[6] == -1) {
		  p1=junk.find("[[[");
		  if (p1 > 50) p1=junk.find("]]]");
		  if (p1 > 50 && bin1[6] == -1) {
			 p5=junk.find("[[");
			 p6=junk.find("]]");
			 if (((p5 < 50 && p6 > 50) || (p5 > 50 && p6 < 50)) && bin1[6] == -1) {
				tmp4=junk.substr(0,prbr1)+junk.substr(plbr1+1,junk.length()-plbr1-1); //single
				p1=tmp4.find_first_not_of(" ");
				bin1[3]=atoi(tmp4.substr(p1,1).c_str());
				tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
				p1=tmp3.find_first_not_of(" ");
				bin1[2]=atoi(tmp3.substr(p1,1).c_str());
				prbr3=tmp1.find("[");
				plbr3=tmp1.find("]");
				tmp2=tmp1.substr(prbr3+1,plbr3-prbr3-1);
				p1=tmp2.find_first_not_of(" ");
				bin1[0]=atoi(tmp2.substr(p1,1).c_str());
				p1=tmp2.find_last_not_of(" ");
				bin1[1]=atoi(tmp2.substr(p1,1).c_str());
				p1=tmp2.find(" ");
				tmp1=tmp2.substr(0,p1);
				tmp2=tmp2.substr(p1,tmp2.length()-p1);

				p4=tmp4.find(":"); //merger in single
				p3=tmp3.find(":"); //merger in tertiary
				p2=tmp2.find(":"); //merger in inner binary (2nd star)
				p1=tmp1.find(":"); //merger in inner binary (1st star)

				if (p1 < 50 && p2 < 50 && p3 > 50 && p4 > 50 && bin1[6] == -1) bin1[6]=111; //hierarchical triple with two 2-star mergers in inner binary + single
				if ((p1 < 50 || p2 < 50) && p3 < 50 && p4 > 50 && bin1[6] == -1) bin1[6]=112; //hierarchical triple with 2-star merger in inner binary and 2-star merger in tertiary + single
				//NOTE: 113 is duplicated with 112 in Nathan's list
				if ((p1 < 50 || p2 < 50) && p3 > 50 && p4 < 50 && bin1[6] == -1) bin1[6]=114; //hierarchical triple with 2-star merger in inner binary + 2-star merger as single
				if (p1 > 50 && p2 > 50 && p3 < 50 && p4 < 50 && bin1[6] == -1) bin1[6]=115; //hierarchical triple with 2-star merger in tertiary + 2-star merger as single
				if (((p1 < 50 && p2 > 50) || (p1 > 50 && p2 < 50)) && p3 > 50 && p4 > 50 && bin1[6] == -1) bin1[6]=124; //hierarchical triple with 3-star merger in inner binary + single
				if (p1 > 50 && p2 > 50 && p3 < 50 && p4 > 50 && bin1[6] == -1) bin1[6]=125; //hierarchical triple with 3-star merger in tertiary + single
				if (p1 > 50 && p2 > 50 && p3 > 50 && p4 < 50 && bin1[6] == -1) bin1[6]=126; //hierarchical triple + 3-star merger as single
			 }
			 if (((p5 < 50 && p6 < 50) || (p5 > 50 && p6 > 50)) && bin1[6] == -1) {
				if (p5 > 50) tmp1=junk;
				p2=tmp1.find("] [");
				if (p2 > 50) p2=tmp1.find("][");
				if (p2 < 50) {
				  tmp2=tmp1.substr(0,p2+1);
				  tmp3=tmp1.substr(p2+1,tmp1.length()-p2-1);
				}
				p3=tmp2.find("[");
				p4=tmp2.find("]");
				bin1[0]=atoi(tmp2.substr(p3+1,1).c_str());
				bin1[1]=atoi(tmp2.substr(p4-1,1).c_str());
				p3=tmp3.find("[");
				p4=tmp3.find("]");
				bin1[2]=atoi(tmp3.substr(p3+1,1).c_str());
				bin1[3]=atoi(tmp3.substr(p4-1,1).c_str());			 
				bin2[0]=bin1[2];
				bin2[1]=bin1[3];

				p1=tmp2.find(" ");
				tmp1=tmp2.substr(0,p1);
				tmp2=tmp2.substr(p1,tmp2.length()-p1);
				p1=tmp3.find(" ");
				tmp4=tmp3.substr(p1,tmp3.length()-p1);
				tmp3=tmp3.substr(0,p1);
				p1=tmp1.find(":");//merger in first binary (1st star)
				p2=tmp2.find(":");//merger in first binary (2nd star)
				p3=tmp3.find(":");//merger in second binary (1st star)
				p4=tmp4.find(":");//merger in second binary (2nd star)

				
				if ((p1 < 50 || p2 < 50) && (p3 < 50 || p4 < 50) && p5 < 50 && bin1[6] == -1) bin1[6]=107; //stable quad composed of two binaries, each with one 2-star merger
				if (((p1 < 50 && p2 < 50) || (p3 < 50 && p4 < 50)) && p5 < 50 && bin1[6] == -1) bin1[6]=108; //stable quad composed of two binaries, one with two 2-star mergers
				if ((p1 < 50 || p2 < 50) && (p3 < 50 || p4 < 50) && p5 > 50 && bin1[6] == -1) bin1[6]=109; //two binaries (unbound), each with one 2-star merger 
				if (((p1 < 50 && p2 < 50) || (p3 < 50 && p4 < 50)) && p5 > 50 && bin1[6] == -1) bin1[6]=110; //two binaries (unbound), one with two 2-star merger 
				if ( ( (((p1 < 50 && p2 > 50) || (p1 > 50 && p2 < 50)) && p3 > 50 && p4 > 50) || (((p3 < 50 && p4 > 50) || (p3 > 50 && p4 < 50)) && p1 > 50 && p2 > 50) ) && p5 < 50 && bin1[6] == -1) bin1[6]=123; //stable quad composed of two binaries, one with 3-star merger
			 }
		  }
		  if (p1 < 50 && bin1[6] == -1) {
			 tmp2=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //quad
			 p1=tmp2.find_first_not_of(" ");
			 bin1[3]=atoi(tmp2.substr(p1,1).c_str());
			 tmp3=tmp1.substr(prbr2+1,plbr2-(prbr2+1));
			 prbr3=tmp3.find("[");
			 plbr3=tmp3.rfind("]");
			 tmp4=tmp3.substr(0,prbr3)+tmp3.substr(plbr3+1,tmp3.length()-plbr3-1); //tertiary
			 p1=tmp4.find_first_not_of(" ");
			 bin1[2]=atoi(tmp4.substr(p1,1).c_str());
			 tmp5=tmp3.substr(prbr3+1,plbr3-(prbr3+1)); //inner binary
			 p1=tmp5.find_first_not_of(" ");
			 bin1[0]=atoi(tmp5.substr(p1,1).c_str());
			 p1=tmp5.find_last_not_of(" ");
			 bin1[1]=atoi(tmp5.substr(p1,1).c_str());
			 
			 p1=tmp5.find(" ");
			 tmp3=tmp5.substr(0,p1);
			 tmp5=tmp5.substr(p1,tmp5.length()-p1);

			 p1=tmp3.find(":"); //merger in inner binary (1st star)
			 p2=tmp5.find(":"); //merger in inner binary (2nd star)
			 p3=tmp4.find(":"); //merger in tertiary
			 p4=tmp2.find(":"); //merger in quad
			 if (p1 < 50 && p2 < 50 && p3 > 50 && p4 > 50 && bin1[6] == -1) bin1[6]=103; //hierarchical quad with two 2-star mergers in inner binary
			 if ((p1 < 50 || p2 < 50) && p3 < 50 && p4 > 50 && bin1[6] == -1) bin1[6]=104; //hierarchical quad with 2-star merger in inner binary and 2-star merger in tertiary
			 if ((p1 < 50 || p2 < 50) && p3 > 50 && p4 < 50 && bin1[6] == -1) bin1[6]=105; //hierarchical quad with 2-star merger in inner binary and 2-star merger in quad
			 if (p1 > 50 && p2>50 && p3 < 50 && p4 < 50 && bin1[6] == -1) bin1[6]=106; //hierarchical quad with 2-star merger in tertiary and 2-star merger in quad
			 if (((p1 < 50 && p2 > 50) || (p1 > 50 && p2 < 50)) && p3 > 50 && p4 > 50 && bin1[6] == -1) bin1[6]=120; //hierarchical quad with 3-star merger in inner binary
			 if (p1 > 50 && p2 > 50 && p3 < 50 && p4 > 50 && bin1[5] == -1) bin1[6]=121; //hierarchical quad with 3-star merger in tertiary
			 if (p1 > 50 && p2 > 50 && p3 > 50 && p4 < 50 && bin1[5] == -1) bin1[6]=122; //hierarchical quad with 3-star merger in quad
		  }
		}
		if (nmg == 3) {
		  p5=junk.find("[[");
		  if (p5 > 50) p5=junk.find("]]");
		  if (p5 > 50) {//no triple
			 tmp1=junk; 
			 prbr2=prbr1;
			 plbr2=plbr1;
		  }
		  tmp2=tmp1.substr(prbr2+1,plbr2-prbr2-1);
		  p2=tmp2.find_first_not_of(" ");
		  bin1[0]=atoi(tmp2.substr(p2,1).c_str());
		  p2=tmp2.find(" ");
		  bin1[1]=atoi(tmp2.substr(p2+1,1).c_str());
		  tmp3=tmp1.substr(0,prbr2)+tmp1.substr(plbr2+1,tmp1.length()-plbr2-1); //tertiary
		  p3=tmp3.find_first_not_of(" ");
		  bin1[2]=atoi(tmp3.substr(p3,1).c_str());
		  tmp1=tmp2.substr(0,p2);
		  tmp2=tmp2.substr(p2,tmp2.length()-p2);
		  
		  np1=0;
		  for (i=0; i<tmp1.length(); i++) if (tmp1[i] == ':') np1++; //merger in star 1
		  np2=0;
		  for (i=0; i<tmp2.length(); i++) if (tmp2[i] == ':') np2++; //merger in star 2
		  np3=0;
		  for (i=0; i<tmp3.length(); i++) if (tmp3[i] == ':') np3++; //merger in star 3
		  
		  if (np1 > 0 && np2 > 0 && np3 > 0 && p5 < 50 && bin1[6] == -1) bin1[6]=130; //hierarchical triple with 2-star merger for each star in inner binary and 2-star merer for tertiary
		  if (np1 > 0 && np2 > 0 && np3 > 0 && p5 > 50 && bin1[6] == -1) bin1[6]=131; //binary with 2-star merger for each star  + 2-star merger
		  if (np1 > 0 && np2 > 0 && np3 == 0 && p5 < 50 && bin1[6] == -1) bin1[6]=133; //hierarchical triple with 3-star merger and 2-star merger in inner binary
		  if (np1 > 0 && np2 > 0 && np3 == 0 && p5 > 50 && bin1[6] == -1) bin1[6]=138; //binary with 3-star merger and 2-star merger + single
		  if (((np1 == 2 && np2 == 0) || (np1 == 0 && np2 == 2)) && np3 > 0 && p5 < 50 && bin1[6] == -1) bin1[6]=134; //hierarchical triple with 3-star merger in inner binary and 2-star merger in tertiary
		  if (((np1 == 2 && np2 == 0) || (np1 == 0 && np2 == 2)) && np3 > 0 && p5 > 50 && bin1[6] == -1) bin1[6]=136; //binary with 3-star merger + 2-star merger
		  if (((np1 == 1 && np2 == 0) || (np1 == 0 && np2 == 1)) && np3 == 2 && p5 < 50 && bin1[6] == -1) bin1[6]=135; //hierarchical triple with 2-star merger in inner binary and 3-star merger in tertiary
		  if (((np1 == 1 && np2 == 0) || (np1 == 0 && np2 == 1)) && np3 == 2 && p5 > 50 && bin1[6] == -1) bin1[6]=137; //binary with 2-star merger + 3-star merger
		  if ((np1 == 3 || np2 == 3) && np3 == 0 && p5 < 50 && bin1[6] == -1) bin1[6]=140; //hierarchical triple with 4-star merger in inner binary
		  if ((np1 == 3 || np2 == 3) && np3 == 0 && p5 > 50 && bin1[6] == -1) bin1[6]=142; //binary with 4-star merger + single
		  if (np1 == 0 && np2 == 0 && np3 == 3 && p5 < 50 && bin1[6] == -1) bin1[6]=141; //hierarchical triple with 4-star merger in tertiary
		  if (np1 == 0 && np2 == 0 && np3 == 3 && p5 > 50 && bin1[6] == -1) bin1[6]=143; //binary + 4-star merger
		}
		if (nmg == 4) {
		  p5=junk.find("[");
		  if (p5 > 50) tmp1=junk;
		  p1=tmp1.find(" ");
		  tmp2=tmp1.substr(p1,tmp1.length()-p1);
		  tmp1=tmp1.substr(0,p1);
		  p1=tmp1.find_first_not_of(" ");
		  bin1[0]=atoi(tmp1.substr(p1,1).c_str());
		  p1=tmp2.find_first_not_of(" ");
		  bin1[1]=atoi(tmp2.substr(p1,1).c_str());

		  np1=0;
		  for (i=0; i<tmp1.length(); i++) if (tmp1[i] == ':') np1++; //merger in star 1
		  np2=0;
		  for (i=0; i<tmp2.length(); i++) if (tmp2[i] == ':') np2++; //merger in star 2

		  if (np1 == 2 && np2 == 2 && p5 < 50 && bin1[6] == -1) bin1[6]=145; //binary with two 3-star mergers
		  if (((np1 == 4 && np2 == 0) || (np1 == 0 && np2 == 4)) && p5 < 50 && bin1[6] == -1) bin1[6]=146; //binary with 5-star merger
		  if (((np1 == 3 && np2 == 1) || (np1 == 1 && np2 == 3)) && p5 < 50 && bin1[6] == -1) bin1[6]=147; //binary with 4-star merger and 2-star merger
		}
	 }
  }

  //fix the merger numbers
  //but save the original values for the IDs
  for (int i=0; i<6; i++) bin1org[i]=bin1[i];
  if (mgflag > 0) {
	 int found;
	 for (int i=0; i<6; i++){
		found=-1;
		for (int j=0; j<6; j++) {
		  if (bin1[j] == i) {
			 found=j;
			 break;
		  }
		}
		if (found == -1 && i < 5) {
		  for (int j=0; j<6; j++) {
			 if (bin1[j] == i+1) {
				found=j;
				break;
			 }
		  }
		  if (found >= 0) bin1[found]=bin1[found]-1; //two star mergers
		  if (found == -1 && i < 4) {
			 for (int j=0; j<6; j++) {
				if (bin1[j] == i+2) {
				  found=j;
				  break;
				}
			 }
			 if (found >= 0) bin1[found]=bin1[found]-2; //three star mergers
			 if (found == -1 && i < 3) {
				for (int j=0; j<6; j++) {
				  if (bin1[j] == i+3) {
					 found=j;
					 break;
				  }
				}
				if (found >= 0) bin1[found]=bin1[found]-3; //four star mergers
				if (found == -1 && i < 2) {
				  for (int j=0; j<6; j++) {
					 if (bin1[j] == i+4) {
						found=j;
						break;
					 }
				  }
				  if (found >= 0) bin1[found]=bin1[found]-4; //five star mergers
				  if (found == -1 && i < 1) {
					 for (int j=0; j<6; j++) {
						if (bin1[j] == i+5) {
						  found=j;
						  break;
						}
					 }
				
					 if (found >= 0) bin1[found]=bin1[found]-5; //six star mergers
				  }
				}
			 }
		  }
		}
	 }
  }
			 
  if (bin2[0] != -1 && mgflag > 0) {
	 bin2[0]=bin1[2];
	 bin2[1]=bin1[3];
  }

  //  cout << bin1[0] << " " << bin1[1] << " " << bin1[2] << " " << bin1[3] << " " << bin1[4] << " " << bin1[5] << " " << bin1[6] << endl;
  //  cout << bin2[0] << " " << bin2[1] << " " << bin2[2] << " " << bin2[3] << " " << bin2[4] << " " << bin2[5] << " " << bin2[6] << endl;

}



void getparam12(param_bb_t parambb[], cluster_t cluster[], int i, int ic)
{
  double x1,x2;
  double Ptmp,etmp;
  double mu,vcrit;
  int bad = 1;
  double RL1 = 0;
  double q1 = 0;
  double vhs = 0;

  int trial,Ntrial;
  Ntrial = 1000;
  while (bad == 1) {
	 trial = 0;
	 //primary mass and radius
	 //	 parambb[i].n = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].n > maxm || parambb[i].n < minm) parambb[i].n=getKTGmass(); else parambb[i].n = usemass;
	 if (usemass == -1 && usemass0 == -1) {
		if (useKTG == -1){
		  parambb[i].n = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].n = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].n = usemass; else parambb[i].n = usemass0;
	 }
	 if (useradius == -1 && useradius0 == -1) parambb[i].g=getradius(parambb[i].n,cluster[ic].Z); else {
		if (usemass != -1) parambb[i].g = useradius; else parambb[i].g = useradius0;
	 }

	 //single star mass and radius
	 //	 parambb[i].m = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].m > maxm || parambb[i].m < minm) parambb[i].m=getKTGmass(); else parambb[i].m = usemass;
	 if (usemass == -1 && usemass2 == -1) {
		if (useKTG == -1){
		  parambb[i].m = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].m = getKTGmass(minm, maxm);
		} 
	 } else {
		if (usemass != -1) parambb[i].m = usemass; else parambb[i].m = usemass2;
	 }
	 if (useradius == -1 && useradius2 == -1) parambb[i].r=getradius(parambb[i].m,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].r = useradius; else parambb[i].r = useradius2;
	 }

	 while (bad == 1 && trial < Ntrial) {

		//secondary mass and radius
		parambb[i].o = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass1 == -1) {
		  if (parambb[i].n > minm){
			 while (parambb[i].o > parambb[i].n || parambb[i].o < minm) parambb[i].o=(rand() % 1000000)/1000000. * parambb[i].n;
		  } else parambb[i].o = minm;
		} else {
		  if (usemass != -1) parambb[i].o = usemass; else parambb[i].o = usemass1;
		}
 		if (useradius == -1 && useradius1 == -1) parambb[i].i=getradius(parambb[i].o,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].i = useradius; else parambb[i].i = useradius1;
		}
		
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		if (ecdist == -1) {
		  etmp = (rand() % 1000000)/1000000.;
		} else if (ecdist == -2){
		  etmp=-999.;
		  while((etmp) < 0. || (etmp) > 1.) {
			 x1=(rand() % 1000000)/1000000.;
			 x2=(rand() % 1000000)/1000000.;
			 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
			 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
			 etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
		  } 
		} else etmp = ecdist;
		parambb[i].e=etmp;
		
		//check the hard-soft boundary
		if (doplim == 1){
		  vhs = pow(3.,0.5)*cluster[ic].sigma0;
		  Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].n*parambb[i].o/parambb[i].m),(3./2.)) * 1./ ( pow( (parambb[i].n + parambb[i].o), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  parambb[i].phs = Pmax;
		}

		if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma11 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
		  if (usesma == -1 && usesma11 == -1){
			 if (pedist == -1) {
				Ptmp=-999.;
				while((Ptmp < Pmin) || (Ptmp > Pmax)) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				}
			 } else if (pedist == -2) {
				Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
			 } else Ptmp = log10(pedist);
			 
			 Ptmp=pow(10.,Ptmp);   //period in days
			 parambb[i].period1=Ptmp;
			 //calculate the semi-major axis in AU
			 parambb[i].a=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].n+parambb[i].o),1./3.);
		  } else {
			 if (usesma != -1) parambb[i].a = usesma;
			 if (usesma11 != -1) parambb[i].a = usesma11;
			 parambb[i].period1 = pow(pow(parambb[i].a/(1.95753e-2),3)/(parambb[i].n+parambb[i].o),0.5);
		  }

		  bad = 0;

		  //check the Roche radius for the binary
		  if (doplim == 1){
			 q1 = parambb[i].n/parambb[i].o;
			 //			 amin = 
			 RL1 = parambb[i].g/aursun/(parambb[i].a* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );

			 if (RL1 >= 1 && usesma == -1 && usesma11 != -1 && pedist <= 0) bad = 1;
		  //cout << trial << " " << bad << " " << parambb[i].m << " " << parambb[i].n << " " << parambb[i].o << " " << q1 << " " << parambb[i].a << " " << RL1 << endl;
		  }
		} else bad = 1;
	 	//check the hard-soft boundary
	 	//	 vhs = pow(G*parambb[i].n*parambb[i].o/(2.*parambb[i].m*parambb[i].a*AUtkm) , 0.5); //[km/s]
	 	//	 if (vhs < pow(3.,0.5)*cluster[ic].sigma0) bad = 1;
		
		//		cout << bad << " " << parambb[i].n << " " << RL1 << " " << (parambb[i].g/aursun) << " " << parambb[i].a << " " << vhs << " " << pow(3.,0.5)*cluster[ic].sigma0 << " " << parambb[i].period1 << endl;
		
		if (bad == 1) trial ++; else trial = 0;
		if (trial >= Ntrial) break;
	 }
	 
  }
  
  //normalize the velocity
  mu=(parambb[i].n+parambb[i].o)*parambb[i].m/(parambb[i].m+parambb[i].n+parambb[i].o);
  if (mu == 0) {
	 fprintf(stdout,"%s\n","\nWARNING!! ZERO MASS BINARY: for vcrit\n");
	 vcrit=1.;
  } else vcrit=sqrt(G/mu*(parambb[i].n*parambb[i].o/(parambb[i].a*1.496e8)));// 
  if (vinf == -1){
	 parambb[i].v = parambb[i].vel/vcrit;
  } else {
	 parambb[i].v = vinf;
	 parambb[i].vel = vinf*vcrit;
  }

}



void getparam22(param_bb_t parambb[], cluster_t cluster[], int i, int ic)
{
  double x1,x2;
  double Ptmp,etmp;
  double mu,vcrit;
  int bad = 1;
  double RL1 = 0;
  double q1 = 0;
  double vhs = 0;
 
  int trial,Ntrial;
  Ntrial = 1000;
  while (bad == 1) {
	 trial = 0;
	 //primary mass and radius of first binary
	 //	 parambb[i].m = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].m > maxm || parambb[i].m < minm) parambb[i].m=getKTGmass(); else parambb[i].m = usemass;
	 if (usemass == -1 && usemass0 == -1) {
		if (useKTG == -1) {
		  parambb[i].m = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].m = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].m = usemass; else parambb[i].m = usemass0;
	 }
	 if (useradius == -1 && useradius0 == -1) parambb[i].r=getradius(parambb[i].m,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].r = useradius; else parambb[i].r = useradius0;
	 }
	 
	 //primary mass and radius of second binary
	 //	 parambb[i].o = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].o > maxm || parambb[i].o < minm) parambb[i].o=getKTGmass(); else parambb[i].o = usemass;
	 if (usemass == -1 && usemass2 == -1) {
		if (useKTG == -1){ 
		  parambb[i].o = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].o = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].o = usemass; else parambb[i].o = usemass2;
	 }
	 if (useradius == -1 && useradius2 == -1) parambb[i].i=getradius(parambb[i].o,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].i = useradius; else parambb[i].i = useradius2;
	 }
	 
	 while (bad == 1 && trial < Ntrial) {

		//secondary mass and radius of first binary
	 	parambb[i].n = 10.*maxm;
		//	 while (parambb[i].n > maxm || parambb[i].n < minm) parambb[i].n=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass1 == -1) {
		  if (parambb[i].m > minm){
			  while (parambb[i].n > parambb[i].m || parambb[i].n < minm) parambb[i].n=(rand() % 1000000)/1000000. * parambb[i].m;
		  } else parambb[i].n = minm;
		} else {
		  if (usemass != -1) parambb[i].n = usemass; else parambb[i].n = usemass1;
		}
	 	if (useradius == -1 && useradius1 == -1) parambb[i].g=getradius(parambb[i].n,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].g = useradius; else parambb[i].g = useradius1;
		}
		
		//secondary mass and radius
		parambb[i].p = 10.*maxm;
		//		while (parambb[i].p > maxm || parambb[i].p < minm) parambb[i].p=getKTGmass();
		if (usemass == -1 && usemass3 == -1) {
		  if (parambb[i].o > minm){
			  while (parambb[i].p > parambb[i].o || parambb[i].p < minm) parambb[i].p = (rand() % 1000000)/1000000. * parambb[i].o;
		  } else parambb[i].p = minm;
		} else {
		  if (usemass != -1) parambb[i].p = usemass; else parambb[i].p = usemass3;
		}
		if (useradius == -1 && useradius3 == -1) parambb[i].j=getradius(parambb[i].p,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].j = useradius; else parambb[i].j = useradius3;
		}

		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		if (ecdist == -1){
		  etmp = (rand() % 1000000)/1000000.;
		} else if (ecdist == -2){
		  etmp=-999.;
		  while((etmp) < 0. || (etmp) > 1.) {
			 x1=(rand() % 1000000)/1000000.;
			 x2=(rand() % 1000000)/1000000.;
			 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
			 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
			 etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
		  }
		} else etmp = ecdist;
		parambb[i].e=etmp;
		
		//check the hard-soft boundary
		if (doplim == 1){
		  vhs = pow(3.,0.5)*cluster[ic].sigma0;
		  Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].m*parambb[i].n/(parambb[i].p+parambb[i].o)),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  parambb[i].phs = Pmax;
		  //		cout << Pmin << " " << Pmax << endl;
		}

		if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma11 != -1) {
		  if (usesma == -1 && usesma11 == -1){
			 if (pedist == -1) {
				//choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
				Ptmp=-999.;
				while((Ptmp < Pmin) || (Ptmp > Pmax)) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				}
			 } else if (pedist == -2) {
				Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
			 } else Ptmp = log10(pedist);
			 Ptmp=pow(10.,Ptmp);   //period in days
			 parambb[i].period1=Ptmp;
			 //calculate the semi-major axis in AU
			 parambb[i].a=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].n+parambb[i].o),1./3.);
		  } else {
			 if (usesma != -1) parambb[i].a = usesma;
			 if (usesma11 != -1) parambb[i].a = usesma11;
			 parambb[i].period1 = pow(pow(parambb[i].a/(1.95753e-2),3)/(parambb[i].n+parambb[i].o),0.5);
		  }

		  ///////////
		  //second binary
		  ////////////
		  
		  
		  
		  //choose the second eccentricity to my fit to the Raghavan eccentricity distribution
		  //just choose from a flat distribution
		  if (ecdist == -1){
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (ecdist == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 }
		  } else etmp = ecdist;
		  parambb[i].f=etmp;
		  
		  //check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].p*parambb[i].o/(parambb[i].m+parambb[i].n)),(3./2.)) * 1./ ( pow( (parambb[i].p + parambb[i].o), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
			 if (Pmax > parambb[i].phs) {
				parambb[i].phs = Pmax;
			 }
			 //	                 cout << Pmin << " " << Pmax << " 2" << endl;
		  }

		  if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma21 != -1){
			 if (usesma == -1 && usesma21 == -1){
				if (pedist == -1){
				  //choose the second separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  } 
				} else if (pedist == -2){
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(pedist);
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period2=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].o+parambb[i].p),1./3.);
			 } else {
				if (usesma != -1) parambb[i].q = usesma;
				if (usesma21 != -1) parambb[i].q = usesma21;
				parambb[i].period2 = pow(pow(parambb[i].q/(1.95753e-2),3)/(parambb[i].o+parambb[i].p),0.5);
			 }
			 
			 bad=0;
			 
			 //check the Roche radius for the first binary
			 if (doplim == 1){
				q1 = parambb[i].m/parambb[i].n;
				RL1 = parambb[i].r/aursun/(parambb[i].a* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
				if (RL1 >= 1 && usesma == -1 && usesma11 == -1 && pedist <= 0) bad = 1;
			 			 //			 cout << parambb[i].m << " " << parambb[i].n << " " << q1 << " " << parambb[i].a << " " << RL1 << " " << bad << endl;
				//check the hard-soft boundary compared to the first binary
				//	 vhs = pow(G*parambb[i].m*parambb[i].n/(2.*(parambb[i].p+parambb[i].o)*parambb[i].a*AUtkm) , 0.5); //[km/s]
				//	 if (vhs >= pow(3.,0.5)*cluster[ic].sigma0) bad = 1;
				
				//check the Roche radius for the second binary
				q1 = parambb[i].p/parambb[i].o;
				RL1 = parambb[i].j/aursun/(parambb[i].q* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
				if (RL1 >= 1 && usesma == -1 && usesma21 == -1 && pedist <= 0) bad = 1;
				//			 cout << parambb[i].p << " " << parambb[i].o << " " << q1 << " " << parambb[i].q << " " << RL1 << " " << bad << endl;
			 }

		  } else bad = 1;
		} else bad = 1;
		//check the hard-soft boundary compared to the first binary
		//	 vhs = pow(G*parambb[i].p*parambb[i].o/(2.*(parambb[i].m+parambb[i].n)*parambb[i].q*AUtkm) , 0.5); //[km/s]
		//	 if (vhs >= pow(3.,0.5)*cluster[ic].sigma0) bad = 1;
		
		if (bad == 1) trial ++; else trial = 0;
		if (trial >= Ntrial) break;
		
	 }
	 
  }
  //normalize the velocity
  mu=(parambb[i].m+parambb[i].n)*(parambb[i].o+parambb[i].p)/(parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p);
  if (mu == 0) {
	 fprintf(stdout,"%s\n","\nWARNING!! ZERO MASS BINARY: for vcrit\n");
	 vcrit=1.;
  } else vcrit=sqrt(G/mu*((parambb[i].m*parambb[i].n/(parambb[i].a*1.496e8)) + (parambb[i].o*parambb[i].p/(parambb[i].q*1.496e8))));
  if (vinf == -1){
	 parambb[i].v=parambb[i].vel/vcrit;
  } else {
	 parambb[i].v = vinf;
	 parambb[i].vel = vinf*vcrit;
  }
  

}


void getparam13(param_bb_t parambb[], cluster_t cluster[], int i, int ic)
{
  double x1,x2;
  double Ptmp,etmp,astab;
  double mu,vcrit;
  int bad = 1;
  double RL1 = 0;
  double q1 = 0;
  double vhs = 0;

  int trial,Ntrial;
  Ntrial = 1000;
  while (bad == 1) {
	 trial = 0;
	 //triple primary mass and radius
	 //	 parambb[i].n = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].n > maxm || parambb[i].n < minm) parambb[i].n=getKTGmass(); else parambb[i].n = usemass;
	 if (usemass == -1 && usemass0 == -1) {
		if (useKTG == -1){
		  parambb[i].m = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].m = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].m = usemass; else parambb[i].m = usemass0;
	 }
	 if (useradius == -1 && useradius0 == -1) parambb[i].r = getradius(parambb[i].m,cluster[ic].Z); else {
		if (usemass != -1) parambb[i].r = useradius; else parambb[i].r = useradius0;
	 }

	 //single star mass and radius
	 //	 parambb[i].m = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].m > maxm || parambb[i].m < minm) parambb[i].m=getKTGmass(); else parambb[i].m = usemass;
	 if (usemass == -1 && usemass3 == -1) {
		if (useKTG == -1){
		  parambb[i].p = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].p = getKTGmass(minm, maxm);
		} 
	 } else {
		if (usemass != -1) parambb[i].p = usemass; else parambb[i].m = usemass3;
	 }
	 if (useradius == -1 && useradius3 == -1) parambb[i].j=getradius(parambb[i].p,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].j = useradius; else parambb[i].j = useradius3;
	 }

	 while (bad == 1 && trial < Ntrial) {

		//triple secondary mass and radius
		parambb[i].n = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass1 == -1) {
		  if (parambb[i].m > minm){
			 while (parambb[i].n > parambb[i].m || parambb[i].n < minm) parambb[i].n=(rand() % 1000000)/1000000. * parambb[i].m;
		  } else parambb[i].n = minm;
		} else {
		  if (usemass != -1) parambb[i].n = usemass; else parambb[i].n = usemass1;
		}
		if (useradius == -1 && useradius1 == -1) parambb[i].g=getradius(parambb[i].n,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].g = useradius; else parambb[i].g = useradius1;
		}

		//triple tertiary mass and radius
		parambb[i].o = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass2 == -1) while (parambb[i].o > (parambb[i].m + parambb[i].n)  || parambb[i].o < minm) parambb[i].o=(rand() % 1000000)/1000000. * (parambb[i].m + parambb[i].n); else {
		  if (usemass != -1) parambb[i].o = usemass; else parambb[i].o = usemass2;
		}
		if (useradius == -1 && useradius2 == -1) parambb[i].i=getradius(parambb[i].o,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].i = useradius; else parambb[i].i = useradius2;
		}
		
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		if (ecdist == -1) {
		  etmp = (rand() % 1000000)/1000000.;
		} else if (ecdist == -2){
		  etmp=-999.;
		  while((etmp) < 0. || (etmp) > 1.) {
			 x1=(rand() % 1000000)/1000000.;
			 x2=(rand() % 1000000)/1000000.;
			 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
			 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
			 etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
		  } 
		} else etmp = ecdist;
		parambb[i].e=etmp;
		
		//check the hard-soft boundary
		if (doplim == 1){
		  vhs = pow(3.,0.5)*cluster[ic].sigma0;
		  Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].m*parambb[i].n/parambb[i].o),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  parambb[i].phs = Pmax;
		}

		if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma11) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
		  if (usesma == -1 && usesma11 == -1){
			 if (pedist == -1) {
				Ptmp=-999.;
				while((Ptmp < Pmin) || (Ptmp > Pmax)) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				}
			 } else if (pedist == -2) {
				Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
			 } else Ptmp = log10(pedist);
			 
			 Ptmp=pow(10.,Ptmp);   //period in days
			 parambb[i].period1=Ptmp;
			 //calculate the semi-major axis in AU
			 parambb[i].a=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n),1./3.);
		  } else {
			 if (usesma != -1) parambb[i].a = usesma;
			 if (usesma11 != -1) parambb[i].a = usesma11;
			 parambb[i].period1 = pow(pow(parambb[i].a/(1.95753e-2),3)/(parambb[i].m+parambb[i].n),0.5);
		  }

		  bad = 0;

		  //check the Roche radius for the binary
		  if (doplim == 1){
			 q1 = parambb[i].n/parambb[i].m;
			 RL1 = parambb[i].r/aursun/(parambb[i].a* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
			 if (RL1 >= 1 && usesma == -1 && usesma11 == -1 && pedist <= 0) bad = 1;
		  //cout << trial << " " << bad << " " << parambb[i].m << " " << parambb[i].n << " " << parambb[i].o << " " << q1 << " " << parambb[i].a << " " << RL1 << endl;
		  }
		} else bad = 1;

		//////////////
		// tertiary //
		//////////////
		if (bad == 0) {
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		  if (usee2 == -1) {
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (usee2 == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 } 
		  } else etmp = usee2;
		  parambb[i].F = etmp;
		
		//check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow(((parambb[i].n + parambb[i].m)*parambb[i].o/parambb[i].p),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n + parambb[i].o), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  }


		  if (Pmax > Pmin || usep2 > 0 || usesma2 != -1 || usesma12 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
			 if (usesma2 == -1 && usesma12 == -1){
				if (usep2 == -1) {
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  }
				} else if (usep2 == -2) {
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(usep2);
				
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period2=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].Q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n+parambb[i].o),1./3.);
			 } else {
				if (usesma2 != -1) parambb[i].Q = usesma2;
				if (usesma12 != -1) parambb[i].Q = usesma12;
				parambb[i].period2 = pow(pow(parambb[i].Q/(1.95753e-2),3)/(parambb[i].m+parambb[i].n+parambb[i].o),0.5);
			 }
			 

		  //check stability (but don't know the random inclination angle that fewbody will choose, so here set to 0)
			 astab = checkstab(parambb[i].o/(parambb[i].m+parambb[i].n),parambb[i].F,0.,parambb[i].a); 
			 if (parambb[i].Q < astab) bad = 1;
		  }
		} else bad = 1;

		
		if (bad == 1) trial ++; else trial = 0;
		if (trial >= Ntrial) break;
	 }
	 
  }
  
  //normalize the velocity
  mu=(parambb[i].m+parambb[i].n+parambb[i].o)*parambb[i].p/(parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p);
  if (mu == 0) {
	 fprintf(stdout,"%s\n","\nWARNING!! ZERO MASS BINARY: for vcrit\n");
	 vcrit=1.;
  } else vcrit=sqrt(G/mu*((parambb[i].m+parambb[i].n)*parambb[i].o/(parambb[i].Q*AUtkm) + parambb[i].m*parambb[i].n/(parambb[i].a*AUtkm)) );// 
  if (vinf == -1){
	 parambb[i].v = parambb[i].vel/vcrit;
  } else {
	 parambb[i].v = vinf;
	 parambb[i].vel = vinf*vcrit;
  }
}




void getparam23(param_bb_t parambb[], cluster_t cluster[], int i, int ic)
{
  double x1,x2;
  double Ptmp,etmp,astab;
  double mu,vcrit;
  int bad = 1;
  double RL1 = 0;
  double q1 = 0;
  double vhs = 0;

  int trial,Ntrial;
  Ntrial = 1000;
  while (bad == 1) {
	 trial = 0;
	 //triple primary mass and radius
	 //	 parambb[i].n = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].n > maxm || parambb[i].n < minm) parambb[i].n=getKTGmass(); else parambb[i].n = usemass;
	 if (usemass == -1 && usemass0 == -1) {
		if (useKTG == -1){
		  parambb[i].m = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].m = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].m = usemass; else parambb[i].m = usemass0;
	 }
	 if (useradius == -1 && useradius0 == -1) parambb[i].r = getradius(parambb[i].m,cluster[ic].Z); else {
		if (usemass != -1) parambb[i].r = useradius; else parambb[i].r = useradius0;
	 }

	 //binary primary mass and radius
	 //	 parambb[i].m = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].m > maxm || parambb[i].m < minm) parambb[i].m=getKTGmass(); else parambb[i].m = usemass;
	 if (usemass == -1 && usemass3 == -1) {
		if (useKTG == -1){
		  parambb[i].p = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].p = getKTGmass(minm, maxm);
		} 
	 } else {
		if (usemass != -1) parambb[i].p = usemass; else parambb[i].m = usemass3;
	 }
	 if (useradius == -1 && useradius3 == -1) parambb[i].j=getradius(parambb[i].p,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].j = useradius; else parambb[i].j = useradius3;
	 }

	 while (bad == 1 && trial < Ntrial) {

		//triple secondary mass and radius
		parambb[i].n = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass1 == -1) {
		  if (parambb[i].m > minm){
			 while (parambb[i].n > parambb[i].m || parambb[i].n < minm) parambb[i].n=(rand() % 1000000)/1000000. * parambb[i].m;
		  } else parambb[i].n = minm;
		} else {
		  if (usemass != -1) parambb[i].n = usemass; else parambb[i].n = usemass1;
		}
		if (useradius == -1 && useradius1 == -1) parambb[i].g=getradius(parambb[i].n,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].g = useradius; else parambb[i].g = useradius1;
		}

		//tertiary mass and radius
		parambb[i].o = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass2 == -1) while (parambb[i].o > (parambb[i].m + parambb[i].n)  || parambb[i].o < minm) parambb[i].o=(rand() % 1000000)/1000000. * (parambb[i].m + parambb[i].n); else {
		  if (usemass != -1) parambb[i].o = usemass; else parambb[i].o = usemass2;
		}
		if (useradius == -1 && useradius2 == -1) parambb[i].i=getradius(parambb[i].o,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].i = useradius; else parambb[i].i = useradius2;
		}
		
		//binary secondar mass and radius
		parambb[i].l = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass4 == -1) {
		  if (parambb[i].p > minm){
			 while (parambb[i].l > parambb[i].p  || parambb[i].l < minm) parambb[i].l=(rand() % 1000000)/1000000. * parambb[i].p;
		  } else parambb[i].l = minm;
		} else {
		  if (usemass != -1) parambb[i].l = usemass; else parambb[i].l = usemass4;
		}
		if (useradius == -1 && useradius4 == -1) parambb[i].u=getradius(parambb[i].l,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].u = useradius; else parambb[i].u = useradius4;
		}
		
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		if (ecdist == -1) {
		  etmp = (rand() % 1000000)/1000000.;
		} else if (ecdist == -2){
		  etmp=-999.;
		  while((etmp) < 0. || (etmp) > 1.) {
			 x1=(rand() % 1000000)/1000000.;
			 x2=(rand() % 1000000)/1000000.;
			 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
			 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
			 etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
		  } 
		} else etmp = ecdist;
		parambb[i].e=etmp;
		
		//check the hard-soft boundary
		if (doplim == 1){
		  vhs = pow(3.,0.5)*cluster[ic].sigma0;
		  Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].m*parambb[i].n/(parambb[i].p + parambb[i].l)),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  parambb[i].phs = Pmax;
		}

		if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma11 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
		  if (usesma == -1 && usesma11 == -1){
			 if (pedist == -1) {
				Ptmp=-999.;
				while((Ptmp < Pmin) || (Ptmp > Pmax)) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				}
			 } else if (pedist == -2) {
				Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
			 } else Ptmp = log10(pedist);
			 
			 Ptmp=pow(10.,Ptmp);   //period in days
			 parambb[i].period1=Ptmp;
			 //calculate the semi-major axis in AU
			 parambb[i].a=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n),1./3.);
		  } else {
			 if (usesma != -1) parambb[i].a = usesma;
			 if (usesma11 != -1) parambb[i].a = usesma11;
			 parambb[i].period1 = pow(pow(parambb[i].a/(1.95753e-2),3)/(parambb[i].m+parambb[i].n),0.5);
		  }

		  bad = 0;

		  //check the Roche radius for the binary
		  if (doplim == 1){
			 q1 = parambb[i].n/parambb[i].m;
			 RL1 = parambb[i].r/aursun/(parambb[i].a* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
			 if (RL1 >= 1 && usesma == -1 && usesma11 == -1 && pedist <= 0) bad = 1;
		  //cout << trial << " " << bad << " " << parambb[i].m << " " << parambb[i].n << " " << parambb[i].o << " " << q1 << " " << parambb[i].a << " " << RL1 << endl;
		  }
		} else bad = 1;

		//////////////
		// tertiary //
		//////////////
		if (bad == 0) {
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		  if (usee2 == -1) {
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (usee2 == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 } 
		  } else etmp = usee2;
		  parambb[i].F = etmp;
		
		//check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow(((parambb[i].n + parambb[i].m)*parambb[i].o/(parambb[i].p+parambb[i].l)),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n + parambb[i].o), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  }


		  if (Pmax > Pmin || usep2 > 0 || usesma2 != -1 || usesma12 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
			 if (usesma2 == -1 && usesma12 == -1){
				if (usep2 == -1) {
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  }
				} else if (usep2 == -2) {
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(usep2);
				
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period2=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].Q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n+parambb[i].o),1./3.);
			 } else {
			 if (usesma2 != -1) parambb[i].Q = usesma2;
			 if (usesma12 != -1) parambb[i].Q = usesma12;
				parambb[i].period2 = pow(pow(parambb[i].Q/(1.95753e-2),3)/(parambb[i].m+parambb[i].n+parambb[i].o),0.5);
			 }
			 

		  //check stability (but don't know the random inclination angle that fewbody will choose, so here set to 0)
			 astab = checkstab(parambb[i].o/(parambb[i].m+parambb[i].n),parambb[i].F,0.,parambb[i].a); 
			 if (parambb[i].Q < astab) bad = 1;
		  } else bad = 1;
		}

		///////////
		//second binary
		////////////
		  
		if (bad == 0){
		  //choose the second eccentricity to my fit to the Raghavan eccentricity distribution
		  //just choose from a flat distribution
		  if (ecdist == -1){
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (ecdist == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 }
		  } else etmp = ecdist;
		  parambb[i].f=etmp;
		  
		  //check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].p*parambb[i].l/(parambb[i].m+parambb[i].n+parambb[i].o)),(3./2.)) * 1./ ( pow( (parambb[i].p + parambb[i].l), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
			 //	                 cout << Pmin << " " << Pmax << " 2" << endl;
		  }
		  
		  if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma21 != -1){
			 if (usesma == -1 && usesma21 == -1){
				if (pedist == -1){
				  //choose the second separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  } 
				} else if (pedist == -2){
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(pedist);
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period3=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].l+parambb[i].p),1./3.);
			 } else {
			 if (usesma != -1) parambb[i].q = usesma;
			 if (usesma21 != -1) parambb[i].q = usesma21;
				parambb[i].period3 = pow(pow(parambb[i].q/(1.95753e-2),3)/(parambb[i].l+parambb[i].p),0.5);
			 }
			 
			 //check the Roche radius for the second binary
			 q1 = parambb[i].l/parambb[i].p;
			 RL1 = parambb[i].j/aursun/(parambb[i].q* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
			 if (RL1 >= 1 && usesma == -1 && usesma21 == -1 && pedist <= 0) bad = 1;
			 //			 cout << parambb[i].p << " " << parambb[i].o << " " << q1 << " " << parambb[i].q << " " << RL1 << " " << bad << endl;
		  } else bad = 1;
		  
		}
		  
		if (bad == 1) trial ++; else trial = 0;
		if (trial >= Ntrial) break;
	 }
	 
  }
  
  //normalize the velocity
  mu=(parambb[i].m+parambb[i].n+parambb[i].o)*(parambb[i].p+parambb[i].l)/(parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p+parambb[i].l);
  if (mu == 0) {
	 fprintf(stdout,"%s\n","\nWARNING!! ZERO MASS BINARY: for vcrit\n");
	 vcrit=1.;
  } else vcrit=sqrt(G/mu*((parambb[i].m+parambb[i].n)*parambb[i].o/(parambb[i].Q*AUtkm) + parambb[i].m*parambb[i].n/(parambb[i].a*AUtkm) + parambb[i].p*parambb[i].l/(parambb[i].q*AUtkm))) ;// 
  if (vinf == -1){
	 parambb[i].v = parambb[i].vel/vcrit;
  } else {
	 parambb[i].v = vinf;
	 parambb[i].vel = vinf*vcrit;
  }
}

void getparam33(param_bb_t parambb[], cluster_t cluster[], int i, int ic)
{
  double x1,x2;
  double Ptmp,etmp,astab;
  double mu,vcrit;
  int bad = 1;
  double RL1 = 0;
  double q1 = 0;
  double vhs = 0;

  int trial,Ntrial;
  Ntrial = 1000;
  while (bad == 1) {
	 trial = 0;
	 //triple1 primary mass and radius
	 //	 parambb[i].n = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].n > maxm || parambb[i].n < minm) parambb[i].n=getKTGmass(); else parambb[i].n = usemass;
	 if (usemass == -1 && usemass0 == -1) {
		if (useKTG == -1){
		  parambb[i].m = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].m = getKTGmass(minm, maxm);
		}
	 } else {
		if (usemass != -1) parambb[i].m = usemass; else parambb[i].m = usemass0;
	 }
	 if (useradius == -1 && useradius0 == -1) parambb[i].r = getradius(parambb[i].m,cluster[ic].Z); else {
		if (usemass != -1) parambb[i].r = useradius; else parambb[i].r = useradius0;
	 }

	 //triple2 primary mass and radius
	 //	 parambb[i].m = 10.*maxm;
	 //	 if (usemass == -1) while (parambb[i].m > maxm || parambb[i].m < minm) parambb[i].m=getKTGmass(); else parambb[i].m = usemass;
	 if (usemass == -1 && usemass3 == -1) {
		if (useKTG == -1){
		  parambb[i].p = getLEIGHMFmass(cluster[ic].Nstars, minm, maxm); 
		} else {
		  parambb[i].p = getKTGmass(minm, maxm);
		} 
	 } else {
		if (usemass != -1) parambb[i].p = usemass; else parambb[i].m = usemass3;
	 }
	 if (useradius == -1 && useradius3 == -1) parambb[i].j=getradius(parambb[i].p,cluster[ic].Z); else {
		if (useradius != -1) parambb[i].j = useradius; else parambb[i].j = useradius3;
	 }

	 while (bad == 1 && trial < Ntrial) {

		//triple1 secondary mass and radius
		parambb[i].n = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass1 == -1) {
		  if (parambb[i].m > minm){
			 while (parambb[i].n > parambb[i].m || parambb[i].n < minm) parambb[i].n=(rand() % 1000000)/1000000. * parambb[i].m;
		  } else parambb[i].n = minm;
		} else {
		  if (usemass != -1) parambb[i].n = usemass; else parambb[i].n = usemass1;
		}
		if (useradius == -1 && useradius1 == -1) parambb[i].g=getradius(parambb[i].n,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].g = useradius; else parambb[i].g = useradius1;
		}

		//triple1 tertiary mass and radius
		parambb[i].o = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass2 == -1) while (parambb[i].o > (parambb[i].m + parambb[i].n)  || parambb[i].o < minm) parambb[i].o=(rand() % 1000000)/1000000. * (parambb[i].m + parambb[i].n); else {
		  if (usemass != -1) parambb[i].o = usemass; else parambb[i].o = usemass2;
		}
		if (useradius == -1 && useradius2 == -1) parambb[i].i=getradius(parambb[i].o,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].i = useradius; else parambb[i].i = useradius2;
		}
		
		//triple2 secondary mass and radius
		parambb[i].l = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass4 == -1) {
		  if (parambb[i].p > minm){
			 while (parambb[i].l > parambb[i].p  || parambb[i].l < minm) parambb[i].l=(rand() % 1000000)/1000000. * parambb[i].p;
		  } else parambb[i].l = minm;
		} else {
		  if (usemass != -1) parambb[i].l = usemass; else parambb[i].l = usemass4;
		}
		if (useradius == -1 && useradius4 == -1) parambb[i].u=getradius(parambb[i].l,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].u = useradius; else parambb[i].u = useradius4;
		}

		//triple2 tertiary mass and radius
		parambb[i].M = 10.*maxm;
		//	 while (parambb[i].o > parambb[i].n	|| parambb[i].o < minm) parambb[i].o=getKTGmass();
		//draw from uniform mass-ratio distribution
		if (usemass == -1 && usemass5 == -1) while (parambb[i].M > (parambb[i].p + parambb[i].l)  || parambb[i].M < minm) parambb[i].M=(rand() % 1000000)/1000000. * (parambb[i].p + parambb[i].l); else {
		  if (usemass != -1) parambb[i].M = usemass; else parambb[i].M = usemass5;
		}
		if (useradius == -1 && useradius5 == -1) parambb[i].L=getradius(parambb[i].M,cluster[ic].Z); else {
		  if (useradius != -1) parambb[i].L = useradius; else parambb[i].L = useradius5;
		}
		
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		if (ecdist == -1) {
		  etmp = (rand() % 1000000)/1000000.;
		} else if (ecdist == -2){
		  etmp=-999.;
		  while((etmp) < 0. || (etmp) > 1.) {
			 x1=(rand() % 1000000)/1000000.;
			 x2=(rand() % 1000000)/1000000.;
			 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
			 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
			 etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
		  } 
		} else etmp = ecdist;
		parambb[i].e=etmp;
		
		//check the hard-soft boundary
		if (doplim == 1){
		  vhs = pow(3.,0.5)*cluster[ic].sigma0;
		  Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].m*parambb[i].n/(parambb[i].p + parambb[i].l + parambb[i].M) ),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  parambb[i].phs = Pmax;
		}

		if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma11 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
		  if (usesma == -1 && usesma11 == -1){
			 if (pedist == -1) {
				Ptmp=-999.;
				while((Ptmp < Pmin) || (Ptmp > Pmax)) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				}
			 } else if (pedist == -2) {
				Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
			 } else Ptmp = log10(pedist);
			 
			 Ptmp=pow(10.,Ptmp);   //period in days
			 parambb[i].period1=Ptmp;
			 //calculate the semi-major axis in AU
			 parambb[i].a=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n),1./3.);
		  } else {
			 if (usesma != -1) parambb[i].a = usesma;
			 if (usesma11 != -1) parambb[i].a = usesma11;
			 parambb[i].period1 = pow(pow(parambb[i].a/(1.95753e-2),3)/(parambb[i].m+parambb[i].n),0.5);
		  }

		  bad = 0;

		  //check the Roche radius for the binary
		  if (doplim == 1){
			 q1 = parambb[i].n/parambb[i].m;
			 RL1 = parambb[i].r/aursun/(parambb[i].a* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
			 if (RL1 >= 1 && usesma == -1 && usesma11 == -1 && pedist <= 0) bad = 1;
		  //cout << trial << " " << bad << " " << parambb[i].m << " " << parambb[i].n << " " << parambb[i].o << " " << q1 << " " << parambb[i].a << " " << RL1 << endl;
		  }
		} else bad = 1;

		//////////////
		// tertiary //
		//////////////
		if (bad == 0) {
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
		  if (usee2 == -1) {
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (usee2 == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 } 
		  } else etmp = usee2;
		  parambb[i].F = etmp;
		
		//check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow(((parambb[i].n + parambb[i].m)*parambb[i].o/(parambb[i].p+parambb[i].l+parambb[i].M)),(3./2.)) * 1./ ( pow( (parambb[i].m + parambb[i].n + parambb[i].o), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
		  }


		  if (Pmax > Pmin || usep2 > 0 || usesma2 != -1 || usesma12 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
			 if (usesma2 == -1 && usesma12 == -1){
				if (usep2 == -1) {
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  }
				} else if (usep2 == -2) {
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(usep2);
				
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period2=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].Q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].m+parambb[i].n+parambb[i].o),1./3.);
			 } else {
			 if (usesma2 != -1) parambb[i].Q = usesma2;
			 if (usesma12 != -1) parambb[i].Q = usesma12;
				parambb[i].period2 = pow(pow(parambb[i].Q/(1.95753e-2),3)/(parambb[i].m+parambb[i].n+parambb[i].o),0.5);
			 }
			 

		  //check stability (but don't know the random inclination angle that fewbody will choose, so here set to 0)
			 astab = checkstab(parambb[i].o/(parambb[i].m+parambb[i].n),parambb[i].F,0.,parambb[i].a); 
			 if (parambb[i].Q < astab) bad = 1;
		  } else bad = 1;
		}

		///////////
		//second triple
		////////////
		  
		if (bad == 0){
		  //choose the second eccentricity to my fit to the Raghavan eccentricity distribution
		  //just choose from a flat distribution
		  if (ecdist == -1){
			 etmp = (rand() % 1000000)/1000000.;
		  } else if (ecdist == -2){
			 etmp=-999.;
			 while((etmp) < 0. || (etmp) > 1.) {
				x1=(rand() % 1000000)/1000000.;
				x2=(rand() % 1000000)/1000000.;
				while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
			 }
		  } else etmp = ecdist;
		  parambb[i].f=etmp;
		  
		  //check the hard-soft boundary
		  if (doplim == 1){
			 vhs = pow(3.,0.5)*cluster[ic].sigma0;
			 Pmax = log10(PI/pow(2.,0.5)*G * pow((parambb[i].p*parambb[i].l/(parambb[i].m+parambb[i].n+parambb[i].o)),(3./2.)) * 1./ ( pow( (parambb[i].p + parambb[i].l), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
			 //	                 cout << Pmin << " " << Pmax << " 2" << endl;
		  }
		  
		  if (Pmax > Pmin || pedist > 0 || usesma != -1 || usesma21 != -1){
			 if (usesma == -1 && usesma21 == -1){
				if (pedist == -1){
				  //choose the second separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
				  Ptmp=-999.;
				  while((Ptmp < Pmin) || (Ptmp > Pmax)) {
					 x1=(rand() % 1000000)/1000000.;
					 x2=(rand() % 1000000)/1000000.;
					 while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
					 while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
					 Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
				  } 
				} else if (pedist == -2){
				  Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				} else Ptmp = log10(pedist);
				Ptmp=pow(10.,Ptmp);   //period in days
				parambb[i].period3=Ptmp;
				//calculate the semi-major axis in AU
				parambb[i].q=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].l+parambb[i].p),1./3.);
			 } else {
			 if (usesma != -1) parambb[i].q = usesma;
			 if (usesma21 != -1) parambb[i].q = usesma21;
				parambb[i].period3 = pow(pow(parambb[i].q/(1.95753e-2),3)/(parambb[i].l+parambb[i].p),0.5);
			 }
			 
			 //check the Roche radius for the second binary
			 q1 = parambb[i].l/parambb[i].p;
			 RL1 = parambb[i].j/aursun/(parambb[i].q* 0.49*pow(q1,(2./3.)) / (0.6*pow(q1,(2./3.)) + log(1. + pow(q1,(1./3.)) ) ) );
			 if (RL1 >= 1 && usesma == -1 && usesma21 == -1 && pedist <= 0) bad = 1;
			 //			 cout << parambb[i].p << " " << parambb[i].o << " " << q1 << " " << parambb[i].q << " " << RL1 << " " << bad << endl;
		  } else bad = 1;
		  

		//////////////
		// tertiary //
		//////////////
		  if (bad == 0) {
		//choose the eccentricity to my fit to the Raghavan eccentricity distribution
		//just use a flat eccentricity distribution
			 if (usee2 == -1) {
				etmp = (rand() % 1000000)/1000000.;
			 } else if (usee2 == -2){
				etmp=-999.;
				while((etmp) < 0. || (etmp) > 1.) {
				  x1=(rand() % 1000000)/1000000.;
				  x2=(rand() % 1000000)/1000000.;
				  while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
				  while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
				  etmp=0.31*sqrt(-2.*log(x1))*cos(2.*PI*x2)+0.39;
				} 
			 } else etmp = usee2;
			 parambb[i].P = etmp;
		
		//check the hard-soft boundary
			 if (doplim == 1){
				vhs = pow(3.,0.5)*cluster[ic].sigma0;
				Pmax = log10(PI/pow(2.,0.5)*G * pow(((parambb[i].p + parambb[i].l)*parambb[i].M/(parambb[i].m + parambb[i].n + parambb[i].o)),(3./2.)) * 1./ ( pow( (parambb[i].p + parambb[i].l + parambb[i].M), 0.5) * pow(vhs,3.)) / 86400.); //log10([day])
			 }


			 if (Pmax > Pmin || usep2 > 0 || usesma2 != -1 || usesma22 != -1) {
		  //choose the separation to follow the field period distribution out to Pmax (from Raghavan et al. 2010)
				if (usesma2 == -1 & usesma22 == -1) {
				  if (usep2 == -1) {
					 Ptmp=-999.;
					 while((Ptmp < Pmin) || (Ptmp > Pmax)) {
						x1=(rand() % 1000000)/1000000.;
						x2=(rand() % 1000000)/1000000.;
						while(x1 == 0) {x1=(rand() % 1000000)/1000000.;}
						while(x2 == 0) {x2=(rand() % 1000000)/1000000.;}
						Ptmp=2.28*sqrt(-2.*log(x1))*cos(2.*PI*x2)+5.03;
					 }
				  } else if (usep2 == -2) {
					 Ptmp = (rand() % 1000000)/1000000. * (Pmax - Pmin) + Pmin;
				  } else Ptmp = log10(usep2);
				
				  Ptmp=pow(10.,Ptmp);   //period in days
				  parambb[i].period4=Ptmp;
				//calculate the semi-major axis in AU
				  parambb[i].O=1.95753e-2*pow(Ptmp*Ptmp*(parambb[i].p+parambb[i].l+parambb[i].M),1./3.);
				} else {
				  if (usesma2 != -1) parambb[i].O = usesma2;
				  if (usesma22 != -1) parambb[i].O = usesma22;
				  parambb[i].period4 = pow(pow(parambb[i].O/(1.95753e-2),3)/(parambb[i].p+parambb[i].l+parambb[i].M),0.5);
			 }
			 

		  //check stability (but don't know the random inclination angle that fewbody will choose, so here set to 0)
				astab = checkstab(parambb[i].M/(parambb[i].p+parambb[i].l),parambb[i].P,0.,parambb[i].q); 
				if (parambb[i].O < astab) bad = 1;
			 } else bad = 1;
		  }


		}
		  
		if (bad == 1) trial ++; else trial = 0;
		if (trial >= Ntrial) break;
	 }
	 
  }
  
  //normalize the velocity
  mu=(parambb[i].m+parambb[i].n+parambb[i].o)*(parambb[i].p+parambb[i].l+parambb[i].M)/(parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p+parambb[i].l+parambb[i].M);
  if (mu == 0) {
	 fprintf(stdout,"%s\n","\nWARNING!! ZERO MASS BINARY: for vcrit\n");
	 vcrit=1.;
  } else vcrit=sqrt(G/mu*((parambb[i].m+parambb[i].n)*parambb[i].o/(parambb[i].Q*AUtkm) + parambb[i].m*parambb[i].n/(parambb[i].a*AUtkm) + (parambb[i].p+parambb[i].l)*parambb[i].M/(parambb[i].O*AUtkm) + parambb[i].p*parambb[i].l/(parambb[i].q*AUtkm))) ;// 
  if (vinf == -1){
	 parambb[i].v = parambb[i].vel/vcrit;
  } else {
	 parambb[i].v = vinf;
	 parambb[i].vel = vinf*vcrit;
  }
}


//set up and run the scattering experiments through FEWBODY
void runsinbintrip(param_bb_t parambb[], fin_bb_t finbb[], cluster_t cluster[], int run, int ecnum, char* bout1, char* bout2)
{
  int j,recount,totcount=50;
  int redone=0;
  int skip=0;
  int bin1pos[6];
  for (j=0; j<6; j++) bin1pos[j]=-1;
  double mtot,mtoto;
  char command[5000];
  command[0]= '\0';
  string::size_type p1,p2;

  //Initialize the arrays
  int i=run;
  initialfinbb(finbb,i); 

  if (ecnum == 1) { //binsingle
		
		//total mass for the check below
	 mtot=parambb[i].m+parambb[i].n+parambb[i].o;
	 
	 sprintf(command, "./binsingle -m %12.6e -r %12.6e -n %12.6e -o %12.6e -g %12.6e -i %12.6e -a %12.6e -e %12.6e -v %12.6e -b %12.6e -s %f -k %i -c %12.6e -t %12.6e -x %12.6e -z %12.6e -R %12.6e 1>%s 2>%s\n", parambb[i].m, parambb[i].r, parambb[i].n, parambb[i].o, parambb[i].g, parambb[i].i, parambb[i].a, parambb[i].e, parambb[i].v, parambb[i].b, rseed, FB_KS, FB_CPUSTOP, FB_TSTOP, FB_XFAC, tidaltol, FB_RELACC, bout1, bout2);
	 
  }
  
  if (ecnum == 2) { //binbin
	 
	 //total mass for the check below
	 mtot=parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p;
	 
	 sprintf(command, "./binbin -m %12.6e -n %12.6e -o %12.6e -p %12.6e -r %12.6e -g %12.6e -i %12.6e -j %12.6e -a %12.6e -q %12.6e -e %12.6e -f %12.6e -v %12.6e -b %12.6e -s %f -k %i -c %12.6e -t %12.6e -x %12.6e -z %12.6e -R %12.6e 1>%s 2>%s\n", parambb[i].m, parambb[i].n, parambb[i].o, parambb[i].p, parambb[i].r, parambb[i].g, parambb[i].i, parambb[i].j, parambb[i].a, parambb[i].q, parambb[i].e, parambb[i].f, parambb[i].v, parambb[i].b, rseed, FB_KS, FB_CPUSTOP, FB_TSTOP, FB_XFAC, tidaltol, FB_RELACC, bout1, bout2);
	 
  }

  if (ecnum == 3) { //triplesin

	 //total mass for check below
	 mtot=parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p;
			 
	 sprintf(command, "./triplesin -m %12.6e -n %12.6e -o %12.6e -p %12.6e -r %12.6e -g %12.6e -i %12.6e -j %12.6e -a %12.6e -Q %12.6e -e %12.6e -F %12.6e  -v %12.6e -b %12.6e -s %f -k %i -c %12.6e -t %12.6e -x %12.6e -z %12.6e -R %12.6e 1>%s 2>%s\n", parambb[i].m, parambb[i].n, parambb[i].o, parambb[i].p, parambb[i].r, parambb[i].g, parambb[i].i, parambb[i].j, parambb[i].a, parambb[i].Q, parambb[i].e, parambb[i].F, parambb[i].v, parambb[i].b, rseed, FB_KS, FB_CPUSTOP, FB_TSTOP, FB_XFAC, tidaltol, FB_RELACC, bout1, bout2);

  }

  if (ecnum == 4) { //triplebin

	 //total mass for check below
	 mtot=parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p+parambb[i].l;
			 
	 sprintf(command, "./triplebin -m %12.6e -n %12.6e -o %12.6e -p %12.6e -l %12.6e -r %12.6e -g %12.6e -i %12.6e -j %12.6e -u %12.6e -a %12.6e -Q %12.6e -q %12.6e -e %12.6e -F %12.6e -f %12.6e -v %12.6e -b %12.6e -s %f -k %i -c %12.6e -t %12.6e -x %12.6e -z %12.6e -R %12.6e 1>%s 2>%s\n", parambb[i].m, parambb[i].n, parambb[i].o, parambb[i].p, parambb[i].l, parambb[i].r, parambb[i].g, parambb[i].i, parambb[i].j, parambb[i].u, parambb[i].a, parambb[i].Q, parambb[i].q, parambb[i].e, parambb[i].F, parambb[i].f, parambb[i].v, parambb[i].b, rseed, FB_KS, FB_CPUSTOP, FB_TSTOP, FB_XFAC, tidaltol, FB_RELACC, bout1, bout2);

  }

  if (ecnum == 5) { //tripletriple

	 //total mass for check below
	 mtot=parambb[i].m+parambb[i].n+parambb[i].o+parambb[i].p+parambb[i].l+parambb[i].M;
			 
	 sprintf(command, "./tripletriple -m %12.6e -n %12.6e -o %12.6e -p %12.6e -l %12.6e -M %12.6e -r %12.6e -g %12.6e -i %12.6e -j %12.6e -u %12.6e -L %12.6e -a %12.6e -Q %12.6e -q %12.6e -O %12.6e -e %12.6e -F %12.6e -f %12.6e -P %12.6e -v %12.6e -b %12.6e -s %f -k %i -c %12.6e -t %12.6e -x %12.6e -z %12.6e -R %12.6e 1>%s 2>%s\n", parambb[i].m, parambb[i].n, parambb[i].o, parambb[i].p, parambb[i].l, parambb[i].M, parambb[i].r, parambb[i].g, parambb[i].i, parambb[i].j, parambb[i].u, parambb[i].L, parambb[i].a, parambb[i].Q, parambb[i].q, parambb[i].O, parambb[i].e, parambb[i].F, parambb[i].f, parambb[i].P, parambb[i].v, parambb[i].b, rseed, FB_KS, FB_CPUSTOP, FB_TSTOP, FB_XFAC, tidaltol, FB_RELACC, bout1, bout2);

  }

  //one final check for nan values!!!
  string junk=command;
  p2=junk.find(" ");
  p1=junk.find("nan");
  if (p1 > 1e5) p1=junk.find("NaN");
  if (p1 > 1e5) p1=junk.find("Inf");
  if (p1 > 1e5) p1=junk.find("inf");
  if (p1 > 1e5 && p2 > 3) {

	 //run the encounter
	 cout << command;
	 cout << flush;
	 system(command); 
	 
	 //change the random seed--I'm just going to add 1 so that I can reproduce results (but this isn't truly random)
	 rseed++;

	 //read in and analyze the outut
	 
	 int n_in=getn_in(bout1); //FINDS THE NUMBER OF PARTICLES TO SET ARRAY SIZES
	 double t[n_in],m[n_in],rr[n_in][3],vv[n_in][3],R_eff[n_in];
	 double M0[1],V0[1],D0[1],T0[1],t_fin[1],Rmean[1],Rmin[1];
	 char story[storylen],endstory[100];
	 story[0]='\0';
	 endstory[0]='\0';
	 sprintf(story,"%s","");								  
	 getdata(bout1,bout2,t,m,rr,vv,R_eff,story,M0,V0,D0,T0,t_fin,n_in,junk,Rmean,Rmin); //READ IN THE DATA
	 //	 fprintf(stdout,"%20.14e %20.14e\n",vcrit,V0[0]);
	 
	 //check the masses that come out -- sometime the file binout.txt is corrupted and this leads to NaN values below
	 mtoto=0.0;
	 recount=0;
	 for (int k=0; k<n_in; k++) mtoto+=m[k];
	 if (mtoto > 1.1*mtot || mtoto < 0) { //using 1.1 here just incase the precision is off a bit
		fprintf(stdout,"\nWARNING!!! Bad masses in output!!!");
		for (int k=0; k<n_in; k++) fprintf(stdout,"%g ",m[k]);
		fprintf(stdout,"%g\n",mtot);
		fprintf(stdout,"%s\n","  Re-running FEWBODY command...");
		redone=0;
		while (recount < totcount && redone != 1) {
		  fprintf(stdout,"%s\n",command);
		  system(command); 
		  n_in=getn_in(bout1); //FINDS THE NUMBER OF PARTICLES TO SET ARRAY SIZES
		  sprintf(story,"%s","");								  
		  getdata(bout1,bout2,t,m,rr,vv,R_eff,story,M0,V0,D0,T0,t_fin,n_in,junk,Rmean,Rmin); //READ IN THE DATA
		  mtoto=0.0;
		  for (int k=0; k<n_in; k++) {
			 mtoto+=m[k];
			 fprintf(stdout,"%g ",m[k]);
		  }
		  fprintf(stdout,"%g\n",mtot);
		  if (mtoto > 1.1*mtot || mtoto < 0) recount+=1; else {
			 redone=1;
			 fprintf(stdout,"%s\n","   Fixed");
		  }
		  if (recount >=totcount) {
			 fprintf(stdout,"%s\n","  DID NOT RESOLVE PROBLEM--skipping this encounter!!!");
			 skip=1;
		  }
		}
	 }
	 
	 
	 if (skip == 0) {
		int prim,bin1org[7],bin1[7],bin2[7],bin3[7]; //the bin2 array is not used in binsingle output
		for (int k=0; k<7; k++) {
		  bin1org[k]=-1;
		  bin1[k]=-1;
		  bin2[k]=-1;
		  bin3[k]=-1;
		}
		
		//identify the last story line
		junk=story;
		p1=junk.rfind("|");
		junk=junk.substr(0,p1);
		p1=junk.rfind("|")+1;
		if (p1 > junk.length()) p1=0;
		junk=junk.substr(p1,junk.size()-p1);
		p1=junk.find_first_not_of(" ");
		p2=junk.find_last_not_of(" ");
		junk=junk.substr(p1,p2-p1+1);
		//		cout << "endstory " << junk.c_str() << endl;
		sprintf(endstory,"%s",junk.c_str());
		
		//get the locations of the binary and single stars in the m,v,r arrays
		if (ecnum == 1) getbinsin(endstory,bin1,bin1org);
		if (ecnum == 2 || ecnum == 3) getbinbin(endstory,bin1,bin2,bin1org);
		if (ecnum == 4) gettripbin(endstory,bin1,bin2,bin1org);
		if (ecnum == 5) gettriptrip(endstory,bin1,bin2,bin1org);
	
		
		double finval1[8],finval2[8]; //the finval2 array is not used in binsingle output
		for (int k=0; k<8; k++) {
		  finval1[k]=-1.;
		  finval2[k]=-1.;
		}
		double fintval[14]; //this array is returned from gettripfinvals and contains orbital information for the given triple
		for (int k=0; k<14; k++) fintval[k]=-1;
		double finqval[19]; //this array is returned from getquadfinvals and contains orbital information for the given quadruple (but this is currently not used in the code).
		for (int k=0; k<19; k++) finqval[k]=-1;

		initialfinbb(finbb,i);
		double vperp = parambb[i].vperp;

		//if there are no binaries still keep the output
		//now only for 1+2 or 2+2 encounters
		finbb[i].type=bin1[6];
		finbb[i].story=story;	
		finbb[i].t_fin=t_fin[0];
		finbb[i].Rmean=Rmean[0];
		finbb[i].Rmin=Rmin[0];

		//only come from binsingle and binbin output
		if (bin1[6] == 2 || bin1[6] == 5 || bin1[6] == 6 || bin1[6] == 9 || bin1[6] == 15 || bin1[6] == 20 || bin1[6] ==21 || bin1[6] == 22) {
		  finbb[i].mp1=m[bin1[0]];
		  if (bin1[1] != -1) finbb[i].ms1=m[bin1[1]];
		  if (bin1[2] != -1) finbb[i].mp2=m[bin1[2]];
		  if (bin1[3] != -1) finbb[i].ms2=m[bin1[3]];
		  //finbb[i].vb1=sqrt(vv[bin1[0]][0]*vv[bin1[0]][0]+vv[bin1[0]][1]*vv[bin1[0]][1]+vv[bin1[0]][2]*vv[bin1[0]][2]);
		  //if (bin1[1] != -1) finbb[i].vb2=sqrt(vv[bin1[1]][0]*vv[bin1[1]][0]+vv[bin1[1]][1]*vv[bin1[1]][1]+vv[bin1[1]][2]*vv[bin1[1]][2]);
		  //if (bin1[2] != -1) finbb[i].vs1=sqrt(vv[bin1[2]][0]*vv[bin1[2]][0]+vv[bin1[2]][1]*vv[bin1[2]][1]+vv[bin1[2]][2]*vv[bin1[2]][2]);
		  //if (bin1[3] != -1) finbb[i].vs2=sqrt(vv[bin1[3]][0]*vv[bin1[3]][0]+vv[bin1[3]][1]*vv[bin1[3]][1]+vv[bin1[3]][2]*vv[bin1[3]][2]);
//add on the perpendicular component of the velocity not originally sent into FEWBODY
		  finbb[i].vb1=sqrt(vv[bin1[0]][0]*vv[bin1[0]][0] + (vv[bin1[0]][1] + vperp)*(vv[bin1[0]][1] + vperp) + vv[bin1[0]][2]*vv[bin1[0]][2]);
		  if (bin1[1] != -1) finbb[i].vb2=sqrt(vv[bin1[1]][0]*vv[bin1[1]][0] + (vv[bin1[1]][1] + vperp)*(vv[bin1[1]][1] + vperp) + vv[bin1[1]][2]*vv[bin1[1]][2]);
		  if (bin1[2] != -1) finbb[i].vs1=sqrt(vv[bin1[2]][0]*vv[bin1[2]][0] + (vv[bin1[2]][1] + vperp)*(vv[bin1[2]][1] + vperp) + vv[bin1[2]][2]*vv[bin1[2]][2]);
		  if (bin1[3] != -1) finbb[i].vs2=sqrt(vv[bin1[3]][0]*vv[bin1[3]][0] + (vv[bin1[3]][1] + vperp)*(vv[bin1[3]][1] + vperp) + vv[bin1[3]][2]*vv[bin1[3]][2]);

		}
		
		//check for triples 
		if (bin1[6] == 3 || bin1[6] == 10 ||  bin1[6] == 16 || bin1[6] == 17 || bin1[6] == 23 || bin1[6] == 26 || bin1[6] == 31 || bin1[6] == 33 || bin1[6] == 34 || bin1[6] == 35 || bin1[6] == 46 || bin1[6] == 47 || bin1[6] == 49 || bin1[6] == 50 || bin1[6] == 59 || bin1[6] == 60 || bin1[6] == 61 || bin1[6] == 68 || bin1[6] == 69 || bin1[6] == 73 || bin1[6] == 77 || bin1[6] == 79 || bin1[6] == 84 || bin1[6] == 87 || bin1[6] == 88 || bin1[6] == 97 || bin1[6] == 98 || bin1[6] == 99 || bin1[6] == 111 || bin1[6] == 112 || bin1[6] == 114 || bin1[6] == 115 || bin1[6] == 124 || bin1[6] == 125 || bin1[6] == 126 || bin1[6] == 130 || bin1[6] == 133 || bin1[6] == 134 || bin1[6] == 135 || bin1[6] == 140 || bin1[6] == 141){
		  gettripfinvals(bin1,m,rr,vv,fintval,run,parambb[i].id,bin1[6],vperp);
		  if (ecnum == 1 || ecnum == 2) {
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=fintval[2] / 1.4960e8; // AU
			 finbb[i].e1=fintval[3];
			 finbb[i].P1=fintval[4] /8.64e4;  //days
			 finbb[i].a2=fintval[7] / 1.4960e8; // AU
			 finbb[i].e2=fintval[8];
			 finbb[i].P2=fintval[9] /8.64e4;  //days
			 //NEED TO FIGURE OUT THE INCLINATION!
			 //		  finbb[i].iangle= 
			 finbb[i].mp1=m[bin1[0]];
			 finbb[i].ms1=m[bin1[1]];
			 finbb[i].mt1=m[bin1[2]];
			 finbb[i].vb1=fintval[10];
			 if (fintval[11] != -1) finbb[i].vs1=fintval[11];
			 if (bin1[3] != -1) if (m[bin1[3]] != 0) finbb[i].ms2=m[bin1[3]];

		  }
		  if (ecnum == 3 || ecnum == 4 || ecnum == 5) {
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;	
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=fintval[2] / 1.4960e8; // AU
			 finbb[i].e1=fintval[3];
			 finbb[i].P1=fintval[4] /8.64e4;  //days
			 finbb[i].a2=fintval[7] / 1.4960e8; // AU
			 finbb[i].e2=fintval[8];
			 finbb[i].P2=fintval[9] /8.64e4;  //days
			 finbb[i].vt1=fintval[10];
			 
			 //NEED TO FIGURE OUT THE INCLINATION!
			 //		  finbb[i].iangle= 
			 finbb[i].mp1=m[bin1[0]];
			 finbb[i].ms1=m[bin1[1]];
			 finbb[i].mt1=m[bin1[2]];
			 
			 //if we also have a binary
			 if (bin1[6] == 23 || bin1[6] ==  31 || bin1[6] == 60 || bin1[6] == 73 || bin1[6] == 77 || bin1[6] == 78 || bin1[6] == 79 || bin1[6] == 84 || bin1[6] == 87 || bin1[6] == 88) {
				getbinfinvals(bin2,m,rr,vv,finval1,parambb[i].id,bin1[6],vperp); 
				finbb[i].a3=finval1[2] / 1.4960e8; // AU
				finbb[i].e3=finval1[3];
				finbb[i].P3=finval1[4] /8.64e4;  //days
				finbb[i].vb1=finval1[6];
				finbb[i].mp2=m[bin2[0]];
				finbb[i].ms2=m[bin2[1]];
			 }
			 
			 //if we have a second triple
			 if (bin1[6] == 59 || bin1[6] == 68) {
				gettripfinvals(bin2,m,rr,vv,fintval,run,parambb[i].id,bin1[6],vperp);
				finbb[i].a3=fintval[2] / 1.4960e8; // AU
				finbb[i].e3=fintval[3];
				finbb[i].P3=fintval[4] /8.64e4;  //days
				finbb[i].a4=fintval[7] / 1.4960e8; // AU
				finbb[i].e4=fintval[8];
				finbb[i].P4=fintval[9] /8.64e4;  //days
				finbb[i].vt2=fintval[10];
				finbb[i].mp2=m[bin2[0]];
				finbb[i].ms2=m[bin2[1]];
				finbb[i].mt2=m[bin2[2]];
			 }
		  }
		}
		
		//check for quadruples (or higher order systems) and print to the quadruple file
		if (bin1[6] == 11 || bin1[6] == 12 || bin1[6] == 28 || bin1[6] == 29 || bin1[6] == 30 || bin1[6] == 32 || bin1[6] == 40 || bin1[6] == 41 || bin1[6] == 42 || bin1[6] == 43 || bin1[6] == 65 || bin1[6] == 66 || bin1[6] == 67 || bin1[6] == 70 || bin1[6] == 71 || bin1[6] == 72 || bin1[6] == 76 || bin1[6] == 80 || bin1[6] == 81 || bin1[6] == 82 || bin1[6] == 83 || bin1[6] == 85 || bin1[6] == 86 || bin1[6] == 89 || bin1[6] == 90 || bin1[6] == 91 || bin1[6] == 92 || bin1[6] == 93 || bin1[6] == 94 || bin1[6] == 103 || bin1[6] == 104 || bin1[6] == 105 || bin1[6] == 106 || bin1[6] == 107 || bin1[6] == 108 || bin1[6] == 120 || bin1[6] == 121 || bin1[6] == 122 || bin1[6] == 123) {
		  getquadfinvals(bin1,bin2,m,rr,vv,finqval,run,parambb[i].id,bin1[6],vperp);
		  if (ecnum == 2 || ecnum == 3) {
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;	
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=finqval[2] / 1.4960e8; // AU
			 finbb[i].e1=finqval[3];
			 finbb[i].P1=finqval[4] /8.64e4;  //days
			 finbb[i].a2=finqval[7] / 1.4960e8; // AU
			 finbb[i].e2=finqval[8];
			 finbb[i].P2=finqval[9] /8.64e4;  //days
			 if (bin1[6] == 11) {
				//input the inner triple values from quad  
				//				cout << "Taking inner triple of quad." << endl;
				finbb[i].mp1=m[bin1[0]];
				finbb[i].ms1=m[bin1[1]];
				finbb[i].mt1=m[bin1[2]];
				if (bin1[3] != -1) if (m[bin1[3]] != 0) finbb[i].ms2=m[bin1[3]];
			 }
			 if (bin1[6] == 12) {
				//treat these as two separate binaries
				finbb[i].mp1=m[bin1[0]];
				finbb[i].ms1=m[bin1[1]];
				finbb[i].mp2=m[bin2[0]];
				finbb[i].ms2=m[bin2[1]];
			 }
		  }
		  if (ecnum >= 4 && ecnum <= 6) {
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;	
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=finqval[2] / 1.4960e8; // AU
			 finbb[i].e1=finqval[3];
			 finbb[i].P1=finqval[4] /8.64e4;  //days
			 if (bin1[6] == 28 || bin1[6] == 30 || bin1[6] == 40 || bin1[6] == 41 || bin1[6] == 42 || bin1[6] == 65 || bin1[6] == 66 || bin1[6] == 70 || bin1[6] == 71 || bin1[6] == 74 || bin1[6] == 80 || bin1[6] == 81 || bin1[6] == 82 || bin1[6] == 83 || bin1[6] == 89 || bin1[6] == 90 || bin1[6] == 91 || bin1[6] == 92 || bin1[6] == 103 || bin1[6] == 104 || bin1[6] == 105 || bin1[6] == 106 || bin1[6] == 120 || bin1[6] == 121 || bin1[6] == 122) {
				//input the inner triple values from quad  
				//				cout << "Taking inner triple of quad." << endl;
				finbb[i].mp1=m[bin1[0]];
				finbb[i].ms1=m[bin1[1]];
				finbb[i].mt1=m[bin1[2]];
				finbb[i].a2=finqval[7] / 1.4960e8; // AU
				finbb[i].e2=finqval[8];
				finbb[i].P2=finqval[9] /8.64e4;  //days
			 }
			 if (bin1[6] == 29|| bin1[6] == 32 || bin1[6] == 43 || bin1[6] == 67 || bin1[6] == 72 || bin1[6] == 76 || bin1[6] == 85 || bin1[6] == 86 || bin1[6] == 93 || bin1[6] == 94 || bin1[6] == 107 || bin1[6] == 108 || bin1[6] == 123) {
				//treat these as two separate binaries
				finbb[i].mp1=m[bin1[0]];
				finbb[i].ms1=m[bin1[1]];
				finbb[i].mp2=m[bin2[0]];
				finbb[i].ms2=m[bin2[1]];
				finbb[i].a3=finqval[7] / 1.4960e8; // AU
				finbb[i].e3=finqval[8];
				finbb[i].P3=finqval[9] /8.64e4;  //days
			 } 
		  }
		}
		
		
		//if we have binaries (only)
		if (bin1[6] == 1 || bin1[6] == 4 || bin1[6] == 7 || bin1[6] == 8 || bin1[6] == 13 || bin1[6] == 14 || bin1[6] == 18 || bin1[6] == 19 || bin1[6] == 24 || bin1[6] == 25 || bin1[6] == 36 || bin1[6] == 37 || bin1[6] == 39 || bin1[6] == 44 || bin1[6] == 45 || bin1[6] == 51 || bin1[6] == 52 || bin1[6] == 54 || bin1[6] == 56 || bin1[6] == 62 || bin1[6] == 64 || bin1[6] == 65 || bin1[6] == 75 || bin1[6] == 77 || bin1[6] == 78 || bin1[6] == 79 || bin1[6] == 96 || bin1[6] == 100 || bin1[6] == 101 || bin1[6] == 109 || bin1[6] == 110 || bin1[6] == 116 || bin1[6] == 117 || bin1[6] == 118 || bin1[6] == 127 || bin1[6] == 128 || bin1[6] == 131 || bin1[6] == 136 || bin1[6] == 137 || bin1[6] == 138 || bin1[6] == 142 || bin1[6] == 143 || bin1[6] == 145 || bin1[6] == 146 || bin1[6] == 147) {
		  
		  getbinfinvals(bin1,m,rr,vv,finval1,parambb[i].id,bin1[6],vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR FIRST BINARY
		  
		  if (ecnum == 1 || ecnum == 2) {
			 //deal with the binary encounters
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;	
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=finval1[2] / 1.4960e8; // AU
			 finbb[i].e1=finval1[3];
			 finbb[i].P1=finval1[4] /8.64e4;  //days
			 if (finval1[5] != 0) finbb[i].vs1=finval1[5];
			 if (finval1[7] != 0) finbb[i].vs2=finval1[7];
			 finbb[i].vb1=finval1[6];
			 prim=-1;
			 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
			 if (m[bin1[0]] >m[bin1[1]]) prim=0; else prim=1;
			 finbb[i].mp1=m[bin1[prim]];
			 if (prim == 0) finbb[i].ms1=m[bin1[1]]; else finbb[i].ms1=m[bin1[0]];
			 if (bin1[2] != -1) if (m[bin1[2]] != 0) finbb[i].mp2=m[bin1[2]];
			 if (bin1[3] != -1) if (m[bin1[3]] != 0) finbb[i].ms2=m[bin1[3]];
		  }
		  
		  if (ecnum >= 3 && ecnum <= 5) {
			 //deal with the triple encounters
			 finbb[i].type=bin1[6];
			 finbb[i].story=story;	
			 finbb[i].t_fin=t_fin[0];
			 finbb[i].a1=finval1[2] / 1.4960e8; // AU
			 finbb[i].e1=finval1[3];
			 finbb[i].P1=finval1[4] /8.64e4;  //days
			 finbb[i].vs1=finval1[5];
			 finbb[i].vb1=finval1[6];
			 prim=-1;
			 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
			 if (m[bin1[0]] >m[bin1[1]]) prim=0; else prim=1;
			 finbb[i].mp1=m[bin1[prim]];
			 if (prim == 0) finbb[i].ms1=m[bin1[1]]; else finbb[i].ms1=m[bin1[0]];
		  }


		}
		
		//if we have a second binary
		if (bin1[6] == 7 || bin1[6] == 24 || bin1[6] == 31 || bin1[6] == 39 || bin1[6] == 75 || bin1[6] == 95 || bin1[6] == 109 || bin1[6] == 110 || bin1[6] == 64) {
		  
		  getbinfinvals(bin2,m,rr,vv,finval2,parambb[i].id2,bin1[6],vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR SECOND BINARY

		  if (ecnum == 1 || ecnum == 2) {
			 //deal with the binary encounters
			 finbb[i].a2=finval2[2] / 1.4960e8; // AU
			 finbb[i].e2=finval2[3];
			 finbb[i].P2=finval2[4] /8.64e4;  //days
			 finbb[i].vs2=finval2[5];
			 finbb[i].vb2=finval2[6];
			 prim=-1;
			 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
			 if (m[bin2[0]] >m[bin2[1]]) prim=0; else prim=1;
			 finbb[i].mp2=m[bin2[prim]];
			 if (prim == 0) finbb[i].ms2=m[bin2[1]]; else finbb[i].ms2=m[bin2[0]];
		  }
		  if (ecnum >= 4 || ecnum <= 6) {
			 //deal with the triple encounters
			 finbb[i].a3=finval2[2] / 1.4960e8; // AU
			 finbb[i].e3=finval2[3];
			 finbb[i].P3=finval2[4] /8.64e4;  //days
			 finbb[i].vs2=finval2[5];
			 finbb[i].vb2=finval2[6];
			 prim=-1;
			 //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
			 if (m[bin2[0]] >m[bin2[1]]) prim=0; else prim=1;
			 finbb[i].mp2=m[bin2[prim]];
			 if (prim == 0) finbb[i].ms2=m[bin2[1]]; else finbb[i].ms2=m[bin2[0]];
		  }


		}

		//if we have a third binary
		if (bin1[6] == 64 || bin1[6] == 67) {
		  bin3[0]=bin1[4];
		  bin3[1]=bin1[5];
		  getbinfinvals(bin3,m,rr,vv,finval2,parambb[i].id,bin1[6],vperp); //CALCULATE THE FINAL BINARY CHARACTERISTICS FOR SECOND BINARY
		  finbb[i].a4=finval2[2] / 1.4960e8; // AU
		  finbb[i].e4=finval2[3];
		  finbb[i].P4=finval2[4] /8.64e4;  //days
		  finbb[i].vb3=finval2[6];
		  prim=-1;
		  //SWITCH THE PRIMARY AND SECONDARY MASSES IF NECESSARY
		  if (m[bin3[0]] >m[bin3[1]]) prim=0; else prim=1;
		  finbb[i].mp3=m[bin3[prim]];
		  if (prim == 0) finbb[i].ms3=m[bin3[1]]; else finbb[i].ms3=m[bin3[0]];
		}


	 }
	 //get the time until another encounter
	 if (docluster == 1) gettau_enc(cluster,finbb,i);

  } else {
    fprintf(stdout,"%s %i\n","\nWARNING!! NaN or Inf FOUND IN FEWBODY COMMAND! for id=%d ecnum=%i\n",parambb[i].id,ecnum);
  }

  
}



//print the output to the file
void printbenc(param_bb_t parambb[], fin_bb_t finbb[], int run,  FILE * bencFile)
{
  //  cout << "printenc" << endl;
 
  int i=run;

  fprintf(bencFile,"%-9.8i %s ",parambb[i].id,"|");
  fprintf(bencFile,"%12.6e ",parambb[i].m);
  fprintf(bencFile,"%12.6e ",parambb[i].n);
  fprintf(bencFile,"%12.6e ",parambb[i].o);
  fprintf(bencFile,"%12.6e ",parambb[i].p);
  fprintf(bencFile,"%12.6e ",parambb[i].l);
  fprintf(bencFile,"%12.6e ",parambb[i].M);
  fprintf(bencFile,"%12.6e ",parambb[i].r);
  fprintf(bencFile,"%12.6e ",parambb[i].g);
  fprintf(bencFile,"%12.6e ",parambb[i].i);
  fprintf(bencFile,"%12.6e ",parambb[i].j);
  fprintf(bencFile,"%12.6e ",parambb[i].u);
  fprintf(bencFile,"%12.6e ",parambb[i].L);
  fprintf(bencFile,"%12.6e ",parambb[i].a);
  fprintf(bencFile,"%12.6e ",parambb[i].Q);
  fprintf(bencFile,"%12.6e ",parambb[i].q);
  fprintf(bencFile,"%12.6e ",parambb[i].O);
  fprintf(bencFile,"%12.6e ",parambb[i].e);
  fprintf(bencFile,"%12.6e ",parambb[i].F);
  fprintf(bencFile,"%12.6e ",parambb[i].f);
  fprintf(bencFile,"%12.6e ",parambb[i].P);
  fprintf(bencFile,"%12.6e ",parambb[i].b);
  fprintf(bencFile,"%12.6e ",parambb[i].v);
  fprintf(bencFile,"%12.6e ",parambb[i].vel);
  fprintf(bencFile,"%12.6e ",parambb[i].period1);
  fprintf(bencFile,"%12.6e ",parambb[i].period2);
  fprintf(bencFile,"%12.6e ",parambb[i].period3);
  fprintf(bencFile,"%12.6e ",parambb[i].period4);
  fprintf(bencFile,"%s","| ");
  fprintf(bencFile,"%12.6e ",finbb[i].mp1);
  fprintf(bencFile,"%12.6e ",finbb[i].ms1);
  fprintf(bencFile,"%12.6e ",finbb[i].mt1);
  fprintf(bencFile,"%12.6e ",finbb[i].mp2);
  fprintf(bencFile,"%12.6e ",finbb[i].ms2);
  fprintf(bencFile,"%12.6e ",finbb[i].mt2);
  fprintf(bencFile,"%12.6e ",finbb[i].mp3);
  fprintf(bencFile,"%12.6e ",finbb[i].ms3);
  fprintf(bencFile,"%16.14f ",finbb[i].e1);
  fprintf(bencFile,"%16.14f ",finbb[i].e2);
  fprintf(bencFile,"%16.14f ",finbb[i].e3);
  fprintf(bencFile,"%16.14f ",finbb[i].e4);
  fprintf(bencFile,"%20.14e ",finbb[i].a1);
  fprintf(bencFile,"%20.14e ",finbb[i].a2);
  fprintf(bencFile,"%20.14e ",finbb[i].a3);
  fprintf(bencFile,"%20.14e ",finbb[i].a4);
  fprintf(bencFile,"%12.6e ",finbb[i].P1);
  fprintf(bencFile,"%12.6e ",finbb[i].P2);
  fprintf(bencFile,"%12.6e ",finbb[i].P3);
  fprintf(bencFile,"%12.6e ",finbb[i].P4);
  fprintf(bencFile,"%12.6e ",finbb[i].vb1);
  fprintf(bencFile,"%12.6e ",finbb[i].vb2);
  fprintf(bencFile,"%12.6e ",finbb[i].vb3);
  fprintf(bencFile,"%12.6e ",finbb[i].vs1);
  fprintf(bencFile,"%12.6e ",finbb[i].vs2);
  fprintf(bencFile,"%12.6e ",finbb[i].vt1);
  fprintf(bencFile,"%12.6e ",finbb[i].vt2);
  fprintf(bencFile,"%20.14e ",finbb[i].t_fin);
  fprintf(bencFile,"%20.14e ",finbb[i].Rmean/aursun);
  fprintf(bencFile,"%20.14e ",finbb[i].Rmin/aursun);
  fprintf(bencFile,"%20.14e ",finbb[i].tau_enc);
  fprintf(bencFile,"%3i   ",finbb[i].type);
  fprintf(bencFile,"%s%s\n","|",finbb[i].story.c_str());

  fflush(bencFile);

  //  if (finbb[i].t_fin == 0) exit(0);

}


/* print the usage */
void print_usage(FILE *stream)
{
	fprintf(stream, "USAGE:\n");
	fprintf(stream, "  montesinbintrip [options...]\n");
	fprintf(stream, "\n");
	fprintf(stream, "OPTIONS:\n");
	fprintf(stream, "\nGeneral parameters\n");
  	fprintf(stream, "  -c  --ecnum              : choose either [=1] 1+2 or [=2] 2+2 [=3] 1+3 [=4] 3+2 [=5] 3+3 [%i]\n", ecnum);
	fprintf(stream, "  -n  --nenc               : number of encounters to run [%i]\n", n_bs);
	fprintf(stream, "  -o  --ofile              : output file name for encounter summaries [%s]\n", ofile.c_str());
	fprintf(stream, "  -s  --seed               : set the random seed for FEWBODY ([=0] then taken from system time) [%.6g]\n", rseed);
	fprintf(stream, "  -h  --help               : display this help text\n");

	fprintf(stream, "\nStar cluster parameters\n");
	fprintf(stream, "  -A  --docluster          : [=1] use a Plummer model and assume encounters inside a cluster, [=0] assume encounters in the field\n");
	fprintf(stream, "                             (if field, additional parameters -U -W must be set) [%i]\n", docluster);
	fprintf(stream, "  -f  --feh                : set the metallicity (e.g., used to calculate radii) [%.6g]\n", FeH);
	fprintf(stream, "  -m  --mcl <Msun>         : cluster mass [%.6g]\n", Mcl);
	fprintf(stream, "  -N  --nstars             : [!=-1] sets the number of stars, otherwise code estimates a value consistent with MF [%.6g]\n", Nstars);
	fprintf(stream, "  -U  --sigma0 <km/s>      : 1-D velocity dispersion for Maxwellian distribution [%.6g]\n", sigma0);
	fprintf(stream, "  -W  --vesc <km/s>        : 1-D escape velocity for Maxwellian distribution [%.6g]\n", vesc);
	fprintf(stream, "  -r  --rhm <pc>           : set the cluster half-mass radius [%.6g]\n", Rhm);
	fprintf(stream, "  -t  --age <Myr>          : set the age of the cluster (for determining the main-sequence turnoff mass) [%.6g]\n", age);
	fprintf(stream, "  -b  --fb <percent>       : set the field binary frequency (only applied if --fieldfb != -1)  [%.6g]\n", fb_field);
	fprintf(stream, "  -F  --fieldfb            : [!=-1] use the Raghavan et al. period distribution with Pmin, Pmax, and --fb to estimate the\n");
   fprintf(stream, "                             binary frequency (default is to draw fb from Leigh et al. (2013), MNRAS, 428, 897) [%.6g]\n", usefieldfb);
	fprintf(stream, "      --meanm <MSun>       : [!=-1] set the mean stellar mass (regardless of true mean mass from MF)  [%.6g]\n", usemeanm);

	fprintf(stream,"\nBinary orbital parameters and masses\n");
	fprintf(stream, "  -e  --ecd                : choose eccentricity distribution: [=-1] flat eccentricity distribution , [=-2] normal distribution\n");
	fprintf(stream, "                             fit to M35 binaries, [>=0] sets the eccentricity for all binaries [%i]\n", ecdist);
	fprintf(stream, "      --e2                 : set the outer eccentricity of all triples, same format as --ecd [%.6g]\n", usee2);
	fprintf(stream, "  -p  --pd                 : choose the period distribution: [=-1]  log-normal period distribution (from Raghavan 2010),\n");
	fprintf(stream, "                             [=-2] uniform in log(period), [>0] sets the period <log10(days)> for all binaries (only if --sma = -1) [%i]\n", pedist);
	fprintf(stream, "      --p2 <log10(days)>   : set the outer period of all triples, same format as --pd (only if --sma = -1) [%.6g]\n", usep2);
	fprintf(stream, "  -a  --sma <AU>           : set the semi-major axes of all binaries  [%.6g]\n", usesma);
	fprintf(stream, "      --sma2 <AU>          : set the outer semi-major axes of all triples  [%.6g]\n", usesma2);
	fprintf(stream, "      --sma11 <AU>         : set the inner semi-major axes of triple/binary 1  [%.6g]\n", usesma11);
	fprintf(stream, "      --sma12 <AU>         : set the outer semi-major axes of all triple 1  [%.6g]\n", usesma12);
	fprintf(stream, "      --sma21 <AU>         : set the innter semi-major axes of triple/binary 2 [%.6g]\n", usesma21);
	fprintf(stream, "      --sma22 <AU>         : set the outer semi-major axes of triple 2  [%.6g]\n", usesma22);
	fprintf(stream, "  -l  --plim               : [=1] limit the period according the the hard-soft boundary (Pmax) and Roche limit (Pmin),\n");
	fprintf(stream, "                             otherwise take the Pmin and Pmax as limits [%i]\n", doplim);
	fprintf(stream, "  -G  --lpmin <log10(days)>: set the minimum period (Pmin) for the period distribution (only applied if --plim != 1) [%.6g]\n", Pmin);
	fprintf(stream, "  -H  --lpmax <log10(days)>: set the maximum period (Pmax) for the period distribution (only applied if --plim != 1) [%.6g]\n", Pmax);
	fprintf(stream, "  -P  --minm <MSun>        : minimum mass to draw from the mass function [%.6g]\n", minm);
	fprintf(stream, "  -Q  --maxm <MSun>        : maximum mass to draw from the mass function (only used if docluster = 0) [%.6g]\n", maxm);
	fprintf(stream, "  -M  --mall <MSun>        : [!=-1] set the masses for all stars [%.6g]\n", usemass);
	fprintf(stream, "  -R  --rall <RSun>        : [!=-1] set the radii for all stars [%.6g]\n", useradius);
	fprintf(stream, "  -u  --m0 <MSun>          : set the mass of star 0 ([=-1] then taken from the IMF) [%.6g]\n", usemass0);
	fprintf(stream, "  -v  --m1 <MSun>          : set the mass of star 1 ([=-1] then taken from the IMF) [%.6g]\n", usemass1);
	fprintf(stream, "  -w  --m2 <MSun>          : set the mass of star 2 ([=-1] then taken from the IMF) [%.6g]\n", usemass2);
	fprintf(stream, "  -x  --m3 <MSun>          : set the mass of star 3 ([=-1] then taken from the IMF and only applicable for 1+2 encounters) [%.6g]\n", usemass3);
	fprintf(stream, "      --m4 <MSun>          : set the mass of star 4 ([=-1] then taken from the IMF and only applicable for 1+2 encounters) [%.6g]\n", usemass4);
	fprintf(stream, "      --m5 <MSun>          : set the mass of star 5 ([=-1] then taken from the IMF and only applicable for 1+2 encounters) [%.6g]\n", usemass5);
	fprintf(stream, "  -k  --r0 <RSun>          : set the radius of star 0 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748) [%.6g]\n", useradius0);
	fprintf(stream, "  -q  --r1 <RSun>          : set the radius of star 1 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748) [%.6g]\n", useradius1);
	fprintf(stream, "  -y  --r2 <RSun>          : set the radius of star 2 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748) [%.6g]\n", useradius2);
	fprintf(stream, "  -z  --r3 <RSun>          : set the radius of star 3 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748\n");
	fprintf(stream, "                             and only applicable for 1+2 encounters) [%.6g]\n", useradius3);
	fprintf(stream, "      --r4 <RSun>          : set the radius of star 4 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748\n");
	fprintf(stream, "                             and only applicable for 1+2 encounters) [%.6g]\n", useradius4);
	fprintf(stream, "      --r5 <RSun>          : set the radius of star 5 ([=-1] then calculated from the formulae in Tout et al. 1997, MNRAS, 732, 748\n");
	fprintf(stream, "                             and only applicable for 1+2 encounters) [%.6g]\n", useradius5);
	fprintf(stream, "  -K  --KTG                : [!=-1] to draw masses from the Kroupa, Tout & Gilmore (1993), MNRAS 262, 545 IMF (default is to use the MF from\n");
	fprintf(stream, "                             Leigh et al. (2012),  MNRAS, 422, 1592) [%.6g]\n", useKTG);

	fprintf(stream,"\nEncounter parameters\n");
	fprintf(stream, "  -B  --bmin <b/a>         : set the minimum impact parameter [%.6g]\n", bmin);
	fprintf(stream, "  -d  --bsc <b/a>          : set the range for the impact parameter uniform distribution,\n");
   fprintf(stream, "                             if [<0] take average inter-particle spacing in cluster ( =rho_N[r=0]^(-1./3.) ) as maximum value [%.6g]\n", bscale);
	fprintf(stream, "  -V  --vinf_min <v/vcrit> : [!=-1] set the minimum velocity at infinity [%.6g]\n", vinf_min);
	fprintf(stream, "  -L  --vinf_sc <v/vcrit>  : set the range for the velocity at infinity uniform distribution (only used if vinf_min != -1) [%.6g]\n", vinf_sc);
	fprintf(stream, "  -T  --tidaltol           : set the tidal-to-binary force ratio (tidaltol in FEWBODY) [%.6g]\n", tidaltol);
	fprintf(stream, "  -i  --rmi                : star ID1 for which to print the minimum approach from FEWBODY [%i]\n", Rmin_i);
	fprintf(stream, "  -j  --rmj                : star ID2 for which to print the minimum approach from FEWBODY [%i]\n", Rmin_j);
	fprintf(stream, "  -C  --tcpustop <sec>     : set cpu stopping time in FEWBODY [%.6g]\n", FB_CPUSTOP);
	fprintf(stream, "  -D  --tstop <tstop/tdyn> : set stopping time in FEWBODY [%.6g]\n", FB_TSTOP);
	fprintf(stream, "  -S  --ks                 : turn K-S regularization on [=1] or off [=0] in FEWBODY [%.6g]\n", FB_KS);
	fprintf(stream, "  -X  --fexp               : set expansion factor of merger product in FEWBODY [%.6g]\n", FB_XFAC);
	fprintf(stream, "  -E  --relacc             : relative accuracy of the integrator in FEWBODY [%.6g]\n", FB_RELACC);
	fprintf(stream, "      --bplummer <AU>      : [!=-1] draw impact parameters from a Plummer density distribution out to a maximum value of bplummer (THIS OPTION IS NOT WORKING YET), \n");
	fprintf(stream, "                             [<-1] take average interparticle distance as impact parameter  [%.6g]\n", bplummer);

}

int main(int argc, char* argv[])
{

 //SOME TIMER INFORMATION
  time_t start,end,rawtime;
  struct tm * timeinfo;
  time (&start);
  timeinfo = localtime (&start);
  fprintf(stderr,"\n%s\n",asctime (timeinfo));

  //gather the input 
  const char * short_opts = "a:A:b:B:c:C:d:D:e:E:f:F:g:G:hH:i:j:k:K:l:L:m:M:n:N:o:p:P:q:Q:r:R:s:S:t:T:u:U:v:V:w:W:x:X:y:z:0:";
  struct option long_opts[] =
	 {
		{"sma",    required_argument, NULL, 'a'},
		{"fb",     required_argument, NULL, 'b'},
		{"ecnum",  required_argument, NULL, 'c'},
		{"bsc",    required_argument, NULL, 'd'},
		{"ecd",    required_argument, NULL, 'e'},
		{"feh",    required_argument, NULL, 'f'},
		{"lpmin",  required_argument, NULL, 'G'},
		{"lpmax",  required_argument, NULL, 'H'},
		{"help",   no_argument,       NULL, 'h'},
		{"rmi",    required_argument, NULL, 'i'},
		{"rmj",    required_argument, NULL, 'j'},
		{"plim",   required_argument, NULL, 'l'},
		{"mcl",    required_argument, NULL, 'm'},
		{"nenc",   required_argument, NULL, 'n'},
		{"ofile",  required_argument, NULL, 'o'},
		{"pd",     required_argument, NULL, 'p'}, 
		{"rhm",    required_argument, NULL, 'r'},
		{"seed",   required_argument, NULL, 's'},
		{"age",    required_argument, NULL, 't'},
		{"m0",     required_argument, NULL, 'u'},
		{"m1",     required_argument, NULL, 'v'},
		{"m2",     required_argument, NULL, 'w'},
		{"m3",     required_argument, NULL, 'x'},
		{"r0",     required_argument, NULL, 'k'},
		{"r1",     required_argument, NULL, 'q'},
		{"r2",     required_argument, NULL, 'y'},
		{"r3",     required_argument, NULL, 'z'},
		{"bmin",   required_argument, NULL, 'B'},
		{"KTG",    required_argument, NULL, 'K'},
		{"fieldfb",required_argument, NULL, 'F'},
		{"mall",   required_argument, NULL, 'M'},
		{"rall",   required_argument, NULL, 'R'},
		{"tidaltol",required_argument, NULL,'T'},
		{"vinf_min",required_argument,NULL, 'V'},
		{"vinf_sc", required_argument,NULL, 'L'},
		{"tcpustop",required_argument, NULL,'C'},
		{"ks",     required_argument, NULL, 'S'},
		{"fexp",   required_argument, NULL, 'X'},
		{"nstars", required_argument, NULL, 'N'},
		{"tstop",  required_argument, NULL, 'D'},
		{"docluster",required_argument,NULL,'A'},
		{"minm",   required_argument, NULL, 'P'},
		{"maxm",   required_argument, NULL, 'Q'},
		{"sigma0", required_argument, NULL, 'U'},
		{"vesc",   required_argument, NULL, 'W'},
		{"relacc", required_argument, NULL, 'E'},
		{"bplummer",required_argument,NULL, '0'},
		{"m4",     required_argument, NULL, '0'},
		{"m5",     required_argument, NULL, '0'},
		{"r4",     required_argument, NULL, '0'},
		{"r5",     required_argument, NULL, '0'},
		{"sma2",   required_argument, NULL, '0'},
		{"sma11",  required_argument, NULL, '0'},
		{"sma12",  required_argument, NULL, '0'},
		{"sma21",  required_argument, NULL, '0'},
		{"sma22",  required_argument, NULL, '0'},
		{"p2",     required_argument, NULL, '0'},
		{"e2",     required_argument, NULL, '0'},
		{"meanm",  required_argument, NULL, '0'},
		
		{NULL, 0, NULL, 0}
	 };

  //want input of Mcl[MSun], Rhm[pc], nrun, ecnum (later maybe choosing between log-normal or flat in a)
  int ch;
  int opts_index = 0;
  while((ch =  getopt_long(argc, argv, short_opts, long_opts, &opts_index)) != -1){ 
	 switch (ch){
	 case '0':
		if (long_opts[opts_index].name == "bplummer") bplummer = atof(optarg);
		if (long_opts[opts_index].name == "m4") usemass4 = atof(optarg);
		if (long_opts[opts_index].name == "m5") usemass5 = atof(optarg);
		if (long_opts[opts_index].name == "r4") useradius4 = atof(optarg);
		if (long_opts[opts_index].name == "r5") useradius5 = atof(optarg);
		if (long_opts[opts_index].name == "sma2") usesma2 = atof(optarg);
		if (long_opts[opts_index].name == "sma11") usesma11 = atof(optarg);
		if (long_opts[opts_index].name == "sma12") usesma12 = atof(optarg);
		if (long_opts[opts_index].name == "sma21") usesma21 = atof(optarg);
		if (long_opts[opts_index].name == "sma22") usesma22 = atof(optarg);
		if (long_opts[opts_index].name == "p2") usep2 = atof(optarg);
		if (long_opts[opts_index].name == "e2") usee2 = atof(optarg);
		if (long_opts[opts_index].name == "meanm") usemeanm = atof(optarg);
		break;
	 case 'a':
		//		cout << " case a " << ch << " " << optarg << endl;
		usesma = atof(optarg);
		break;
	 case 'b':
		//		cout << " case b " << ch << " " << endl;
		fb_field = atof(optarg);
		break;
	 case 'c':
		//		cout << " case c " << ch << " " << endl;
		ecnum = atoi(optarg);
		break;
	 case 'd':
		//		cout << " case d " << ch << " " << endl;
		bscale = atof(optarg);
		break;
	 case 'e':
		//		cout << " case e " << ch << " "  << endl;
		ecdist = atof(optarg);
		break;
	 case 'f':
		//		cout << " case f " << ch << " " << endl;
		FeH = atof(optarg);
		break;
	 case 'G':
		//		cout << " case G " << ch << " " << endl;
		Pmin = atof(optarg);
		break;
	 case 'H':
		//		cout << " case H " << ch << " " << endl;
		Pmax = atof(optarg);
		break;
	 case 'h':
		//		cout << " case h " << ch << " " << endl;
		print_usage(stdout);
		return(0);
		break;
	 case 'i':
		//		cout << " case i " << ch << " " << endl;
		Rmin_i = atoi(optarg);
		break;
	 case 'j':
		//		cout << " case j " << ch << " " << endl;
		Rmin_j = atoi(optarg);
		break;
	 case 'l':
		//		cout << " case l " << ch << " " << endl;
		doplim = atoi(optarg);
		break;
	 case 'm':
		//		cout << " case m " << ch << " " << endl;
		Mcl = atof(optarg);
		break;
	 case 'n':
		//		cout << " case n " << ch << " " << endl;
		n_bs = atoi(optarg);
		break;
	 case 'o':
		//		cout << " case o " << ch << " " << optarg << endl;
 		ofile= optarg;
		break;
	 case 'p':
		//		cout << " case p " << ch << " " << endl;
		pedist = atof(optarg);
		break;
	 case 'r':
		//		cout << " case r " << ch << " " << endl;
		Rhm = atof(optarg);
		break;
	 case 's':
		//		cout << " case s " << ch << " " << endl;
		rseed = atof(optarg);
		break;
	 case 't':
		//		cout << " case t " << ch << " " << endl;
		age = atof(optarg);
		break;
	 case 'u':
		//		cout << " case u " << ch << " " << endl;
		usemass0 = atof(optarg);
		break;
	 case 'v':
		//		cout << " case v " << ch << " " << endl;
		usemass1 = atof(optarg);
		break;
	 case 'w':
		//		cout << " case w " << ch << " " << endl;
		usemass2 = atof(optarg);
		break;
	 case 'x':
		//		cout << " case x " << ch << " " << endl;
		usemass3 = atof(optarg);
		break;
	 case 'k':
		//		cout << " case k " << ch << " " << endl;
		useradius0 = atof(optarg);
		break;
	 case 'q':
		//		cout << " case q " << ch << " " << endl;
		useradius1 = atof(optarg);
		break;
	 case 'y':
		//		cout << " case y " << ch << " " << endl;
		useradius2 = atof(optarg);
		break;
	 case 'z':
		//		cout << " case z " << ch << " " << endl;
		useradius3 = atof(optarg);
		break;
	 case 'B':
		//		cout << " case B " << ch << " " << endl;
		bmin = atof(optarg);
		break;
	 case 'K':
		//		cout << " case K " << ch << " " << endl;
		useKTG = atof(optarg);
		break;
	 case 'F':
		//		cout << " case F " << ch << " " << endl;
		usefieldfb = atof(optarg);
		break;
	 case 'M':
		//		cout << " case M " << ch << " " << endl;
		usemass = atof(optarg);
		break;
	 case 'R':
		//		cout << " case R " << ch << " " << endl;
		useradius = atof(optarg);
		break;
	 case 'T':
		//		cout << " case X " << ch << " " << endl;
		tidaltol = atof(optarg);
		break;
	 case 'V':
		//		cout << " case V " << ch << " " << endl;
		vinf_min = atof(optarg);
		break;
	 case 'L':
		//		cout << " case L " << ch << " " << endl;
		vinf_sc = atof(optarg);
		break;
	 case 'C':
		//		cout << " case C " << ch << " " << endl;
		FB_CPUSTOP = atof(optarg);
		break;
	 case 'S':
		//		cout << " case S " << ch << " " << endl;
		FB_KS = atoi(optarg);
		break;
	 case 'X':
		//		cout << " case V " << ch << " " << endl;
		FB_XFAC = atof(optarg);
		break;
	 case 'N':
		//		cout << " case V " << ch << " " << endl;
		Nstars = atof(optarg);
		break;
	 case 'D':
		//		cout << " case V " << ch << " " << endl;
		FB_TSTOP = atof(optarg);
		break;
	 case 'A':
		//		cout << " case A " << ch << " " << endl;
		docluster = atoi(optarg);
		break;
	 case 'P':
		//		cout << " case P " << ch << " " << endl;
		minm = atof(optarg);
		break;
	 case 'Q':
		//		cout << " case Q " << ch << " " << endl;
		maxm = atof(optarg);
		break;
	 case 'U':
		//		cout << " case U " << ch << " " << endl;
		sigma0 = atof(optarg);
		break;
	 case 'W':
		//		cout << " case W " << ch << " " << endl;
		vesc = atof(optarg);
		break;
	 case 'E':
		//		cout << " case E " << ch << " " << endl;
		FB_RELACC = atof(optarg);
		break;

	 case ':':
		cout << "unrecognized input :" << ch << endl; // Inform the user of how to use the program
		exit(1);
		break;
	 }
  }


  int run=0;

  cluster_t cluster[1];
  param_bb_t parambb[n_bs];
  fin_bb_t finbb[n_bs];
  double tinv,tinb;

  //INITIAILZE RANDOM SEED
  if (rseed == -1) rseed=time(NULL);
  srand(rseed);
  fprintf(stdout,"RANDOM SEED = %100.50f\n\n",rseed);


  //test
  /*
  double m,lfv,fv,gd[2];
  double Ntot = 1e5;
  for (int i=0; i<1e4; i++) {
	 m = getLEIGHMFmass(Ntot, 0.1, 1.5);
	 getLEIGHad(m,gd);
	 lfv = gd[0]*log10(Ntot/1000.) + gd[1];
	 fv = pow(10.,lfv);
	 cout << m << " " << fv  << " " << gd[0] << " " << gd[1] << endl;
  }
  exit(1);
  */
  /*  
  cluster[0].sigma0 = 10.;
  cluster[0].meanm = 0.5;
  cluster[0].vesc = 50.;
  double v;
  for (int i=0; i<1e4; i++) {
	 //	 v = getlmvel(cluster, 0, 5.0); 
	 //	 cout << v << endl;
	 v = getvinf(cluster,0, parambb, 0, 0.5, 1.0);
  }
  exit(1);
  */

  //get two random file names for output from binsingle and binbin
  char bout1 [L_tmpnam];
  char bout2 [L_tmpnam];
  tmpnam (bout1);
  tmpnam (bout2);
  //  printf("%s %s\n",bout1, bout2);

  //calculate the cluster-wide values
  initialcluster(cluster,0);
  cluster[0].FeH = FeH;
  //from Tout et al. (1996) : 
  //[Fe/H] = log10(Z/Zsun) - log10(X/Xsun) = log10(Z/Zsun * Xsun/X)
  //X = 0.76 - 3.*Z
  cluster[0].Z = 0.76 / (Xsun/Zsun * (1./pow(10.,FeH)) + 3.);

  if (docluster == 1){
	 cluster[0].age = age;
	 cluster[0].Rhm = Rhm;
	 cluster[0].Mcl = Mcl;
	 cluster[0].fb_field = fb_field;
	 cluster[0].MSTO = getMSTO(cluster[0].age);

  //redefine the maximum mass as the MSTO calculated above
	 maxm = cluster[0].MSTO;
	 if (Nstars == -1) cluster[0].Nstars = getNstars(cluster[0].Mcl); else cluster[0].Nstars = Nstars;
	 getcluster(cluster,0);
	 if (sigma0 != -1) cluster[0].sigma0 = sigma0;
	 if (vesc != -1) cluster[0].vesc = vesc;

  } else {
	 useKTG = 1;
	 if (vinf_min == -1 && (sigma0 == -1 || vesc == -1)){
		cout << "For the field, you must at least set the velocity dispersion (-U) and escape velocity (-W) or the vinf values (-V and possibly -L)" << endl;
		exit(0);
	 }
	 if (sigma0 != -1 && vesc != -1){
		cluster[0].sigma0 = sigma0;
		cluster[0].vesc = vesc;
	 } else {
		if (doplim == 1){
		  cout << "WARNING: you did not set the velocity dispersion (-U), needed for hard-soft boundary.  Setting here to 5 km/s. " << endl;
		  cluster[0].sigma0 = 5.;
		}
	 }
  }

  if (usemeanm != -1) cluster[0].meanm = usemeanm;

  //test Plummer r
  //  double btmp;
  //  for (int i=0; i<500000; i++){
  //	 btmp = 10*bplummer;
  //	 while (btmp > bplummer) btmp = getPlummer_r(cluster,0);
  //	 cout << btmp << endl;
  //  }
  //  exit(0);

  //get all of the initial conditions (so that we can derive the period distribution and calculate the cluster binary frequency)
  for (int run=0; run<n_bs; run++) {
	 //	 cout << run << endl;
	 initialparambb(parambb,run);
	 parambb[run].id=run;
	 parambb[run].id2=run+1;

	 //get the impact parameter
	 //NOTE: FEWBODY DECLARES b IN UNITS OF (b/a1 for binsingle or b/(a1+a2) for binbin)
	 if (bplummer == -1){
		//draw a random impact paramenter 
		if (bscale < 0) bscale = pow(cluster[0].rho0,-1./3.) * pctAU;
		parambb[run].b=(rand() % 1000000)/1000000. * bscale + bmin;
	 } else {
		//if bplummer < -1 then take the average interparticle spacing as the maximum value for bplummer
		if (bplummer < -1){
		  bplummer = pow(cluster[0].rho0,-1./3.) * pctAU;
		}
		//draw from a Plummer density distribution
		parambb[run].b = 10*bplummer;
		while (parambb[run].b > bplummer) parambb[run].b = getPlummer_r(cluster,0);
		//will normalize this after getparam
	 } 


	 //set the relative velocity at infinity [km/s] (will be set below if vinf_min == -1)
	 if (vinf_min != -1) {
		vinf=(rand() % 1000000)/1000000. * vinf_sc + vinf_min;
	 }

	 if (ecnum == 1) { //binsingle
		getparam12(parambb,cluster,run,0);
		if (bplummer != -1) parambb[run].b = parambb[run].b/parambb[run].a;
		if (vinf_min == -1) vinf = getvinf(cluster, 0, parambb, run, parambb[run].m, parambb[run].n + parambb[run].o);
	 }
	 if (ecnum == 2) { //binbin
		getparam22(parambb,cluster,run,0);
		if (bplummer != -1) parambb[run].b = parambb[run].b/(parambb[run].a + parambb[run].q);
		if (vinf_min == -1) vinf = getvinf(cluster, 0, parambb, run, parambb[run].m + parambb[run].n, parambb[run].o + parambb[run].p);
	 }
	 if (ecnum == 3) { //triplesin
		getparam13(parambb,cluster,run,0);
		if (bplummer != -1) parambb[run].b = parambb[run].b/(parambb[run].Q);
		if (vinf_min == -1) vinf = getvinf(cluster, 0, parambb, run, parambb[run].p, parambb[run].m + parambb[run].n + parambb[run].o);
	 }
	 if (ecnum == 4) { //triplebin
		getparam23(parambb,cluster,run,0);
		if (bplummer != -1) parambb[run].b = parambb[run].b/(parambb[run].Q + parambb[run].q);
		if (vinf_min == -1) vinf = getvinf(cluster, 0, parambb, run, parambb[run].p + parambb[run].l, parambb[run].m + parambb[run].n + parambb[run].o);
	 }
	 if (ecnum == 5) { //tripletriple
		getparam33(parambb,cluster,run,0);
		if (bplummer != -1) parambb[run].b = parambb[run].b/(parambb[run].Q + parambb[run].O);
		if (vinf_min == -1) vinf = getvinf(cluster, 0, parambb, run, parambb[run].p + parambb[run].l + parambb[run].M, parambb[run].m + parambb[run].n + parambb[run].o);
	 }


  }

  //now use the hard-soft boundaries calculated above to get the cluster hard binary frequency
  if (usefieldfb == -1) {
	 getLEIGHfbc(cluster,parambb,0);
  } else {
	 getclusterfb(cluster,parambb,0);
  }

  //open the binary encounter output file
  FILE * bencFile;
  bencFile = fopen (ofile.c_str(),"w");
  if (bencFile == NULL) {
	 cout << "Error: " << ofile.c_str() << "output file cannot be opened \n";
	 exit(1);
  }
  //print a header line with cluster information
  if (docluster == 1){
	 fprintf(bencFile,"%s\n","cluster properties:");
	 fprintf(bencFile,"%s\n","age[Myr]             Mass[Msun]            Nstars   Rhm[pc]              rpl[pc]              sigma0[km/s]         vesc[km/s]          [Fe/H]                Z                    rho0[N/pc^3]         <m [MSun]>           MSTO[Msun]           fb_hard              fb_field             <log10(P_hs[days])>");
	 fprintf(bencFile,"%s\n","--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------");
	 fprintf(bencFile,"%20.13e ",cluster[0].age);
	 fprintf(bencFile,"%20.13e ",cluster[0].Mcl);
	 fprintf(bencFile,"%8li ",cluster[0].Nstars);
	 fprintf(bencFile,"%20.13e ",cluster[0].Rhm);
	 fprintf(bencFile,"%20.13e ",cluster[0].rpl);
	 fprintf(bencFile,"%20.13e ",cluster[0].sigma0);
	 fprintf(bencFile,"%20.13e ",cluster[0].vesc);
	 fprintf(bencFile,"%20.13e ",cluster[0].FeH);
	 fprintf(bencFile,"%20.13e ",cluster[0].Z);
	 fprintf(bencFile,"%20.13e ",cluster[0].rho0);
	 fprintf(bencFile,"%20.13e ",cluster[0].meanm);
	 fprintf(bencFile,"%20.13e ",cluster[0].MSTO);
	 fprintf(bencFile,"%20.13e ",cluster[0].fb_hard);
	 fprintf(bencFile,"%20.13e ",cluster[0].fb_field);
	 fprintf(bencFile,"%20.13e\n",cluster[0].meanphs);
	 fprintf(bencFile,"%s\n","");
	 fflush(bencFile);
  }

  //now a header for the rest
  fprintf(bencFile,"%s\n","id        | -m(M0)       -n(M1)       -o(M2)       -p(M3)       -l(M4)       -M(M4)       -r(R0)       -g(R1)       -i(R2)       -j(R3)       -u(R4)       -L(R5)       -a(a1)       -Q(a1_1)     -q(a2)       -O(a2_2)     -e(e1)     -F(e1_1)     -f(e2)     -P(e2_1)     -b           -v           velocity     period1      period2      period3      period4      | Mp1          Ms1          Mt1          Mp2          Ms2          Mt2          Mp3          Ms3          e1               e2               e3               e4               a1(AU)               a2(AU)               a3(AU)               a4(AU)               P1(day)      P2(day)      P3(day)      P4(day)      vb1(km/s)    vb2(km/s)    vb3(km/s)    vs1(km/s)    vs2(km/s)    vt1(km/s)    vt2(km/s)    t_final(yr)          Rmean(AU)            Rmin(AU)             t_enc(yr)             type | story");

  fprintf(bencFile,"%s\n","------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------");


  for (int run=0; run<n_bs; run++) {

	 //run the encounter
	 runsinbintrip(parambb, finbb, cluster, run, ecnum, bout1, bout2);

	 //print the results
	 printbenc(parambb, finbb, run, bencFile); //print the results to the binary encounter file

  }

  fprintf(stdout,"\nFINISHED.\n");

  //close output files
  fclose(bencFile);

  //PRINT HOW LONG THIS PROGRAM TOOK TO RUN
  time (&end);
  timeinfo = localtime (&end);
  fprintf(stderr,"\n%s",asctime (timeinfo));
  double diff=difftime (end,start)/60.;
  fprintf(stderr,"%f %s\n",diff,"minutes");
 
  remove(bout1);
  remove(bout2);
  return 0;
}
	 
	 
