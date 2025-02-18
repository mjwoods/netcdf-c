/*! \file

Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014,
2015, 2016, 2017, 2018
University Corporation for Atmospheric Research/Unidata.

See \ref copyright file for more info.

*/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#undef DEBUG

#include "netcdf.h"
#include "nctestserver.h"

#undef NOEMBED
#undef NOLOCAL
#undef NOHOME
#define NOREDIR

#define KEEPRC

#define AUTHTESTSERVER "thredds.ucar.edu"

#define RC ".daprc"
#define SPECRC "./daprc"

#define USERPWD "authtester:auth"
#define COOKIEFILE "./cookies"

#define URL1 "https://%s@%s/thredds/dodsC/test3/testData.nc"
#define URL2 "https://thredds/%s/dodsC/test3/testData.nc"
#define URL3 "https://%s@" AUTHTESTSERVER "/thredds/dodsC/test3/testData.nc"

/* Embedded user:pwd */
static char url1[1024];

/* user:pwd from RC*/
static char url2[1024];

/* Test redirect from different machine*/
#ifndef NOREDIR
static char url3[1024];
#endif

static int testrc(const char* prefix, const char* url);
static void fillrc(const char* path);
static void killrc();

int
main(int argc, char** argv)
{
    int ncid,retval,pass;
    const char* dfaltsvc;
    const char* home;

    fprintf(stderr,"Testing: Authorization\n");

    dfaltsvc = nc_findtestserver("thredds",AUTHTESTSERVER);
    if(dfaltsvc == NULL) {
        fprintf(stderr,"WARNING: Cannot locate test server\n");
	exit(0);
    }
    
    snprintf(url1,sizeof(url1),URL1,USERPWD,dfaltsvc); /* embedded */
    snprintf(url2,sizeof(url2),URL2,dfaltsvc); /* using rc file */

#ifdef DEBUG
fprintf(stderr,"url1: %s\n",url1);
fprintf(stderr,"url2: %s\n",url2);
fflush(stderr);
#endif

    pass = 1; /* assume success */
    killrc();

    fprintf(stderr,"Testing: Http Basic Authorization\n\n");
#ifndef NOEMBED
    {
        fprintf(stderr,"Testing: Embedded user:pwd: %s\n",url1);
        retval = nc_open(url1, 0, &ncid);
        if(retval != NC_NOERR) {
            pass = 0;
            fprintf(stderr,"*** FAIL: Testing embedded user:pwd\n");
        } else {
            fprintf(stderr,"*** PASS: Testing embedded user:pwd\n");
	    retval = nc_close(ncid);
	}
        fflush(stderr);
    }
#endif

#ifndef NOLOCAL
    {
        /* Test 1: RC in ./ */
        fprintf(stderr,"Testing: user:pwd in %s/%s\n",".",RC);
	if(!testrc(".",url2)) {
	    fprintf(stderr,"user:pwd in %s/%s failed\n",".",RC);
	    exit(1);
        }
    }
#endif

#ifndef NOHOME
    {
        /* Test 1: RC in HOME  */
	home = getenv("HOME");
        fprintf(stderr,"user:pwd in %s/%s\n",home,RC);
	if(!testrc(home,url2)) {
	    fprintf(stderr,"user:pwd in %s/%s failed\n",home,RC);
	    exit(1);
        }
    }
#endif

#ifndef NOREDIR
   {
        fprintf(stderr,"Testing: Http Basic Redirect\n\n");
	snprintf(url3,sizeof(url3),URL3,USERPWD);
        fprintf(stderr,"Basic redirect: %s\n",url3);
        retval = nc_open(url3, 0, &ncid);
        if(retval != NC_NOERR) {
            fprintf(stderr,"*** XFAIL: Basic redirect\n");
        } else {
            fprintf(stderr,"*** PASS: Basic redirect\n");
	    retval = nc_close(ncid);
	}
        fflush(stderr);
    }
#endif

    return !pass;
}

static int
testrc(const char* prefix, const char* url)
{
    int pass = 1;
    int retval;
    int ncid;
    char rcpath[8192];
    FILE* rc;

    snprintf(rcpath,sizeof(rcpath),"%s/%s",prefix,RC);
    rc = NCfopen(rcpath,"w");
    if(rc == NULL) {
        fprintf(stderr,"Cannot create ./%s\n",RC);
        exit(1);
    }
    fclose(rc);
    fillrc(rcpath);
    retval = nc_open(url, 0, &ncid);
    if(retval != NC_NOERR) {
        pass = 0;
        fprintf(stderr,"*** FAIL: Testing: user:pwd in %s\n",rcpath);
    } else {
	retval = nc_close(ncid);
        fprintf(stderr,"*** PASS: Testing: user:pwd in %s\n",rcpath);
    }
    fflush(stderr);
#ifndef KEEPRC
    unlink(rcpath); /* delete the file */
#endif
    return pass;
}

static void
fillrc(const char* path)
{
    FILE* rc;
    killrc();

    rc = NCfopen(path,"w");
    if(rc == NULL) {
	fprintf(stderr,"cannot create rc file: %s\n",path);
	exit(1);
    }
#ifdef DEBUG
    fprintf(rc,"HTTP.VERBOSE=1\n");
#endif
    fprintf(rc,"HTTP.COOKIEJAR=%s\n",COOKIEFILE);
    fprintf(rc,"HTTP.VALIDATE=1\n");
    fprintf(rc,"HTTP.CREDENTIALS.USERPASSWORD=%s\n",USERPWD);
    fclose(rc);
}

static void
killrc()
{
    const char* home;
#ifdef KEEPRC
    fprintf(stderr,"kill: ./%s\n",RC);
#else 
    char path[1024];
    snprintf(path,sizeof(path),"%s/%s",".",RC);
    unlink(path); /* delete the file */
#endif
    home = getenv("HOME");
#ifdef KEEPRC
    fprintf(stderr,"kill: %s/%s\n",home,RC);
#else
    snprintf(path,sizeof(path),"%s/%s",home,RC);
    unlink(path);
#endif
}
