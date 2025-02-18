#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "netcdf_filter_build.h"

/* WARNING:
Starting with HDF5 version 1.10.x, the plugin code MUST be
careful when using the standard *malloc()*, *realloc()*, and
*free()* function.

In the event that the code is allocating, reallocating, or
free'ing memory that either came from or will be exported to the
calling HDF5 library, then one MUST use the corresponding HDF5
functions *H5allocate_memory()*, *H5resize_memory()*,
*H5free_memory()* [5] to avoid memory failures.

Additionally, if your filter code leaks memory, then the HDF5 library
will generate an error.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "netcdf_filter_build.h"
#include <netcdf_json.h>

#include "H5Zzstd.h"

/* Forward */
static htri_t H5Z_zstd_can_apply(hid_t dcpl_id, hid_t type_id, hid_t space_id);
static size_t H5Z_filter_zstd(unsigned flags,size_t cd_nelmts,const unsigned cd_values[],
                    size_t nbytes,size_t *buf_size,void**buf);

const H5Z_class2_t H5Z_ZSTD[1] = {{
    H5Z_CLASS_T_VERS,       /* H5Z_class_t version */
    (H5Z_filter_t)H5Z_FILTER_ZSTD,  /* Filter id number             */
    1,              /* encoder_present flag (set to true) */
    1,              /* decoder_present flag (set to true) */
    "zstd",                  /* Filter name for debugging    */
    (H5Z_can_apply_func_t)H5Z_zstd_can_apply, /* The "can apply" callback  */
    NULL,                       /* The "set local" callback     */
    (H5Z_func_t)H5Z_filter_zstd,         /* The actual filter function   */
}};

/* External Discovery Functions */
DLLEXPORT
H5PL_type_t
H5PLget_plugin_type(void)
{
    return H5PL_TYPE_FILTER;
}

DLLEXPORT
const void*
H5PLget_plugin_info(void)
{
    return H5Z_ZSTD;
}

/* Make this explicit */
/*
 * The "can_apply" callback returns positive a valid combination, zero for an
 * invalid combination and negative for an error.
 */
static htri_t
H5Z_zstd_can_apply(hid_t dcpl_id, hid_t type_id, hid_t space_id)
{
    return 1; /* Assume it can always apply */
}

static size_t
H5Z_filter_zstd(unsigned int flags, size_t cd_nelmts,
                     const unsigned int cd_values[], size_t nbytes,
                     size_t *buf_size, void **buf)
{
  char *outbuf = NULL;
  size_t outbuflen;
  char *inbuf = NULL;
  size_t inbuflen;
  size_t outdatalen;
  size_t err;
  unsigned long long dsize;

  /* Get level parameter */
  if(cd_nelmts != 1) {
      fprintf(stderr, "zstd: no level specified\n");
      goto cleanupAndFail;
  }

  /* Prepare the input buffer. */
  inbuflen = *buf_size;
  inbuf = (char*)*buf;

  if (flags & H5Z_FLAG_REVERSE) { /** Decompress data. */
    dsize = ZSTD_getFrameContentSize(inbuf, inbuflen);
    err = (size_t)dsize;
    if(ZSTD_isError(err)) {
	fprintf(stderr,"zstd: %s\n", ZSTD_getErrorName(err));
	goto cleanupAndFail;
    }

    /* Prepare the output buffer. */
    outbuflen = (size_t)dsize;
    outbuf = H5allocate_memory(outbuflen,0);
    if (outbuf == NULL) {
      fprintf(stderr, "memory allocation failed for zstd compression\n");
      goto cleanupAndFail;
    }
    dsize = ZSTD_decompress(outbuf, outbuflen, inbuf, inbuflen);
    err = (size_t)dsize;
    if(ZSTD_isError(err)) {
	fprintf(stderr,"zstd: %s\n", ZSTD_getErrorName(err));
	goto cleanupAndFail;
    }

  } else { /* Compress */
    int level;

    /* Prepare the output buffer. */
    dsize = ZSTD_compressBound(inbuflen);
    err = (size_t)dsize;
    if(ZSTD_isError(err)) {
	fprintf(stderr,"zstd: %s\n", ZSTD_getErrorName(err));
	goto cleanupAndFail;
    }

    /* Prepare the output buffer. */
    outbuflen = (size_t)dsize;
    outbuf = H5allocate_memory(outbuflen,0);
    if (outbuf == NULL) {
      fprintf(stderr, "memory allocation failed for zstd compression\n");
      goto cleanupAndFail;
    }

    level = (int)cd_values[0];
    dsize = ZSTD_compress(outbuf, outbuflen, inbuf, inbuflen, level);
    err = (size_t)dsize;
    if(ZSTD_isError(err)) {
	fprintf(stderr,"zstd: %s\n", ZSTD_getErrorName(err));
	goto cleanupAndFail;
    }
  }

  outdatalen = (size_t)dsize;

  /* Always replace the input buffer with the output buffer. */
  H5free_memory(*buf);
  *buf = outbuf;
  *buf_size = outbuflen;
  return outdatalen;

cleanupAndFail:
   if (outbuf)
     H5free_memory(outbuf);
   return 0;
}

/**************************************************/
/* NCZarr Filter Objects */

/* Provide the codec support for the HDF5 zstandard library */

static int NCZ_zstd_codec_to_hdf5(const char* codec, size_t* nparamsp, unsigned** paramsp);
static int NCZ_zstd_hdf5_to_codec(size_t nparams, const unsigned* params, char** codecp);

static NCZ_codec_t NCZ_zstd_codec = {/* NCZ_codec_t  codec fields */ 
  NCZ_CODEC_CLASS_VER,	/* Struct version number */
  NCZ_CODEC_HDF5,	/* Struct sort */
  "zstd",	        /* Standard name/id of the codec */
  H5Z_FILTER_ZSTD,   /* HDF5 alias for zstd */
  NULL, /*NCZ_zstd_codec_initialize*/
  NULL, /*NCZ_zstd_codec_finalize*/
  NCZ_zstd_codec_to_hdf5,
  NCZ_zstd_hdf5_to_codec,
  NULL, /*NCZ_zstd_modify_parameters*/
};

/* External Export API */
DLLEXPORT
const void*
NCZ_get_codec_info(void)
{
    return (void*)&NCZ_zstd_codec;
}

static int
NCZ_zstd_codec_to_hdf5(const char* codec_json, size_t* nparamsp, unsigned** paramsp)
{
    int stat = NC_NOERR;
    NCjson* jcodec = NULL;
    NCjson* jtmp = NULL;
    unsigned* params = NULL;
    struct NCJconst jc;
  
    if(nparamsp == NULL || paramsp == NULL)
        {stat = NC_EINTERNAL; goto done;}
    
    if((params = (unsigned*)calloc(1,sizeof(unsigned)))== NULL)
        {stat = NC_ENOMEM; goto done;}

    /* parse the JSON */
    if(NCJparse(codec_json,0,&jcodec))
        {stat = NC_EFILTER; goto done;}
    if(NCJsort(jcodec) != NCJ_DICT) {stat = NC_EPLUGIN; goto done;}
    /* Verify the codec ID */
    if(NCJdictget(jcodec,"id",&jtmp))
        {stat = NC_EFILTER; goto done;}
    if(jtmp == NULL || !NCJisatomic(jtmp)) {stat = NC_EFILTER; goto done;}
    if(strcmp(NCJstring(jtmp),NCZ_zstd_codec.codecid)!=0) {stat = NC_EINVAL; goto done;}

    /* Get Level */
    if(NCJdictget(jcodec,"level",&jtmp))
        {stat = NC_EFILTER; goto done;}
    if(NCJcvt(jtmp,NCJ_INT,&jc))
        {stat = NC_EFILTER; goto done;}
    if(jc.ival < 0 || jc.ival > NC_MAX_UINT) {stat = NC_EINVAL; goto done;}
    params[0] = (unsigned)jc.ival;
    *nparamsp = 1;
    *paramsp = params; params = NULL;
    
done:
    if(params) free(params);
    NCJreclaim(jcodec);
    return stat;
}

static int
NCZ_zstd_hdf5_to_codec(size_t nparams, const unsigned* params, char** codecp)
{
    int stat = NC_NOERR;
    unsigned level = 0;
    char json[1024];

    if(nparams == 0 || params == NULL)
        {stat = NC_EFILTER; goto done;}

    level = params[0];
    snprintf(json,sizeof(json),"{\"id\": \"%s\", \"level\": \"%u\"}",NCZ_zstd_codec.codecid,level);
    if(codecp) {
        if((*codecp = strdup(json))==NULL) {stat = NC_ENOMEM; goto done;}
    }
    
done:
    return stat;
}

