/*****************************************************************************
 * Zoltan Library for Parallel Applications                                  *
 * Copyright (c) 2000,2001,2002, Sandia National Laboratories.               *
 * For more info, see the README file in the top-level Zoltan directory.     *  
 *****************************************************************************/
/*****************************************************************************
 * CVS File Information :
 *    $RCSfile$
 *    $Author$
 *    $Date$
 *    $Revision$
 ****************************************************************************/


#ifdef __cplusplus
/* if C++, define the rest of this header file as extern C */
extern "C" {
#endif


#include <stdio.h>
#include <math.h>
#include <memory.h>
#include "zz_const.h"
#include "rcb.h"
#include "params_const.h"
#include "timer_const.h"
#include "ha_const.h"
#include "par_median_const.h"

/* Recursive coordinate bisectioning (RCB) routine
   operates on "dots" as defined in shared_const.h
*/

/* Original version was written by
   Steve Plimpton, Sandia National Labs, ABQ, NM  87185
   Dept 9221, MS 1111
   (505) 845-7873
   sjplimp@cs.sandia.gov
*/

/* Notes:
   dots are balanced across procs by weight (if used)
   on return, proc owns dotnum "dots" in dense array of max-length dotmax
   all dots will be inside (or on surface of) 3-d box defined by rcbbox
   input weights (if used) are real numbers > 0.0
   can extend "Dot_Struct" data structure in calling program, see shared_const.h
   returned RCB tree only contains one cut on each proc,
     need to do MPI_Allgather if wish to collect it on all procs
*/
/*****************************************************************************/
#define MYHUGE 1.0e30

/*  RCB_DEFAULT_OUTPUT_LEVEL = 0  No statistics logging */
/*  RCB_DEFAULT_OUTPUT_LEVEL = 1  Log times and counts, print summary */
/*  RCB_DEFAULT_OUTPUT_LEVEL = 2  Log times and counts, print for each proc */
#define RCB_DEFAULT_OUTPUT_LEVEL 0
#define RCB_DEFAULT_OVERALLOC 1.0
#define RCB_DEFAULT_REUSE FALSE

/*****************************************************************************/
/* function prototypes */

static int rcb_fn(ZZ *, int *, ZOLTAN_ID_PTR *, ZOLTAN_ID_PTR *, int **, int **,
  double, int, int, int, int, int, int, int, int, float *);
static void print_rcb_tree(ZZ *, int, int, struct rcb_tree *);
static int cut_dimension(int, struct rcb_tree *, int, int, int *, int *, 
  struct rcb_box *);
static int set_preset_dir(int, int, int, struct rcb_box *, int **);
static int serial_rcb(ZZ *, struct Dot_Struct *, int *, int *, int, int,
  struct rcb_box *, double, int, int, int *, int *, int, int, int, int,
  int, int, int, int, int, int *, struct rcb_tree *, int *, int,
  double *, double *, float *);


/*****************************************************************************/
/*  Parameters structure for RCB method.  Used in  */
/*  Zoltan_RCB_Set_Param and Zoltan_RCB.                   */
static PARAM_VARS RCB_params[] = {
                  { "RCB_OVERALLOC", NULL, "DOUBLE", 0 },
                  { "RCB_REUSE", NULL, "INT", 0 },
                  { "CHECK_GEOM", NULL, "INT", 0 },
                  { "RCB_OUTPUT_LEVEL", NULL, "INT", 0 },
                  { "KEEP_CUTS", NULL, "INT", 0 },
                  { "RCB_LOCK_DIRECTIONS", NULL, "INT", 0 },
                  { "RCB_SET_DIRECTIONS", NULL, "INT", 0 },
                  { "RCB_RECTILINEAR_BLOCKS", NULL, "INT", 0 },
                  { NULL, NULL, NULL, 0 } };
/*****************************************************************************/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

int Zoltan_RCB_Set_Param(
char *name,			/* name of variable */
char *val)			/* value of variable */
{
    int status;
    PARAM_UTYPE result;		/* value returned from Check_Param */
    int index;			/* index returned from Check_Param */

    status = Zoltan_Check_Param(name, val, RCB_params, &result, &index);

    return(status);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

int Zoltan_RCB(
  ZZ *zz,                       /* The Zoltan structure with info for
                                   the RCB balancer.                         */
  float *part_sizes,            /* Input:  Array of size zz->LB.Num_Global_Parts
                                   * zz->Obj_Weight_Dim
                                   containing the percentage of work to be
                                   assigned to each partition.               */
  int *num_import,              /* Returned value:  Number of non-local 
                                   objects assigned to this
                                   processor in the new decomposition.       */
  ZOLTAN_ID_PTR *import_global_ids, /* Returned value:  array of global IDs for
                                   non-local objects in this processor's new
                                   decomposition.                            */
  ZOLTAN_ID_PTR *import_local_ids,  /* Returned value:  array of local IDs for
                                   non-local objects in this processor's new
                                   decomposition.                            */
  int **import_procs,           /* Returned value:  array of processor IDs for
                                   processors owning the non-local objects in
                                   this processor's new decomposition.       */
  int **import_to_part,         /* Returned value:  array of partitions to
                                   which imported objects should be assigned. */
  int *num_export,              /* Not computed, set to -1 */
  ZOLTAN_ID_PTR *export_global_ids, /* Not computed. */
  ZOLTAN_ID_PTR *export_local_ids,  /* Not computed. */
  int **export_procs,           /* Not computed. */
  int **export_to_part          /* Not computed. */
)
{
    /* Wrapper routine to set parameter values and call the real rcb. */
    char *yo = "Zoltan_RCB";
    double overalloc;         /* amount to overallocate by when realloc
                                 of dot array must be done.     
                                 1.0 = no extra; 1.5 = 50% extra; etc. */
    int reuse;                /* (0) don't use (1) use previous cuts
                                 stored in treept at initial guesses.  */
    int wgtflag;              /* no. of weights per dot. */
    int check_geom;           /* Check input & output for consistency? */
    int stats;                /* Print timing & count summary? */
    int gen_tree;             /* (0) don't (1) generate whole treept to use
                                 later for point and box drop. */
    int reuse_dir;            /* (0) don't (1) reuse directions determined in
                                 the first iteration for future iterations. */
    int preset_dir;           /* Set order of directions: 0: don't set
                                 1: xyz,        2: xzy,      3: yzx,
                                 4: yxz,        5: zxy,      6: zyx  */
    int rectilinear_blocks;   /* (0) do (1) don't break ties in find_median */
    int i, ierr;


    Zoltan_Bind_Param(RCB_params, "RCB_OVERALLOC", (void *) &overalloc);
    Zoltan_Bind_Param(RCB_params, "RCB_REUSE", (void *) &reuse);
    Zoltan_Bind_Param(RCB_params, "CHECK_GEOM", (void *) &check_geom);
    Zoltan_Bind_Param(RCB_params, "RCB_OUTPUT_LEVEL", (void *) &stats);
    Zoltan_Bind_Param(RCB_params, "KEEP_CUTS", (void *) &gen_tree);
    Zoltan_Bind_Param(RCB_params, "RCB_LOCK_DIRECTIONS", (void *) &reuse_dir);
    Zoltan_Bind_Param(RCB_params, "RCB_SET_DIRECTIONS", (void *) &preset_dir);
    Zoltan_Bind_Param(RCB_params, "RCB_RECTILINEAR_BLOCKS",
                              (void *) &rectilinear_blocks);

    overalloc = RCB_DEFAULT_OVERALLOC;
    reuse = RCB_DEFAULT_REUSE;
    check_geom = DEFAULT_CHECK_GEOM;
    stats = RCB_DEFAULT_OUTPUT_LEVEL;
    gen_tree = 0;
    wgtflag = zz->Obj_Weight_Dim;
    reuse_dir = 0;
    preset_dir = 0;
    rectilinear_blocks = 0;

    Zoltan_Assign_Param_Vals(zz->Params, RCB_params, zz->Debug_Level, zz->Proc,
                         zz->Debug_Proc);

    /* Initializations in case of early exit. */
    *num_import = -1;
    *num_export = -1;  /* We don't compute the export map. */

    ierr = rcb_fn(zz, num_import, import_global_ids, import_local_ids,
		 import_procs, import_to_part, overalloc, reuse, wgtflag,
                 check_geom, stats, gen_tree, reuse_dir, preset_dir,
                 rectilinear_blocks, part_sizes);

    return(ierr);
}

/*---------------------------------------------------------------------------*/

static int rcb_fn(
  ZZ *zz,                       /* The Zoltan structure with info for
                                   the RCB balancer.                         */
  int *num_import,              /* Number of non-local objects assigned to this
                                   processor in the new decomposition.       */
  ZOLTAN_ID_PTR *import_global_ids, /* Returned value:  array of global IDs for
                                   non-local objects in this processor's new
                                   decomposition.                            */
  ZOLTAN_ID_PTR *import_local_ids,  /* Returned value:  array of local IDs for
                                   non-local objects in this processor's new
                                   decomposition.                            */
  int **import_procs,           /* Returned value:  array of processor IDs for
                                   processors owning the non-local objects in
                                   this processor's new decomposition.       */
  int **import_to_part,         /* Returned value:  array of partitions
                                   to which objects are imported.            */
  double overalloc,             /* amount to overallocate by when realloc
                                   of dot array must be done.     
                                   1.0 = no extra; 1.5 = 50% extra; etc.     */
  int reuse,                    /* (0) don't use (1) use previous cuts
                                   stored in treept at initial guesses.      */
  int wgtflag,                  /* No. of weights per dot                    */
  int check_geom,               /* Check input & output for consistency?     */
  int stats,                    /* Print timing & count summary?             */
  int gen_tree,                 /* (0) do not (1) do generate full treept    */
  int reuse_dir,                /* (0) don't (1) reuse directions determined in
                                   the first iteration for future iterations. */
  int preset_dir,               /* Set order of directions:     0: don't set,
                                    1: xyz,        2: xzy,      3: yzx,
                                    4: yxz,        5: zxy,      6: zyx  */
  int rectilinear_blocks,       /* (0) do (1) don't break ties in find_median*/
  float *part_sizes             /* Input:  Array of size zz->LB.Num_Global_Parts
                                   * wgtflag containing the percentage of work 
                                   to be assigned to each partition.    */
)
{
  char    yo[] = "rcb_fn";
  int     proc,nprocs;              /* my proc id, total # of procs */
  ZOLTAN_ID_PTR gidpt = NULL;       /* pointer to rcb->Global_IDs. */
  ZOLTAN_ID_PTR lidpt = NULL;       /* pointer to rcb->Local_IDs. */
  struct Dot_Struct *dotpt;         /* pointer to rcb->Dots. */
  struct rcb_box boxtmp;            /* tmp rcb box */
  int     pdotnum;                  /* # of dots - decomposition changes it */
  int     pdottop;                  /* dots >= this index are new */
  int    *dotmark = NULL;           /* which side of median for each dot */
  int     dotnum;                   /* number of dots */
  int     dotmax = 0;               /* max # of dots arrays can hold */
  int     dottop;                   /* dots >= this index are new */
  int     proclower;                /* 1st proc in lower set */
  int     procmid;                  /* 1st proc in upper set */
  int     partlower;                /* 1st part in lower set */
  int     partmid;                  /* 1st part in upper set */
  int     set;                      /* which set processor is in = 0/1 */
  int     old_set;                  /* set processor was in last cut = 0/1 */
  int     root;                     /* partition that stores last cut */
  int     num_procs;                /* number of procs in current set */
  int     num_parts;                /* number of parts in current set */
  int     dim;                      /* which of 3 axes median cut is on */
  int     ierr = ZOLTAN_OK;         /* error flag. */
  int    *proc_list = NULL;         /* temp array for reusing old cuts */
  int     outgoing;                 /* number of outgoing dots for reuse */
  double *coord = NULL;             /* temp array for median_find */
  double *wgts = NULL;              /* temp array for median_find */
  double  valuehalf;                /* median cut position */
  double  fractionlo;               /* desired wt in lower half */
  int     first_guess;              /* flag if first guess for median search */
  int     allocflag;                /* have to re-allocate space */
  double  time1,time2,time3,time4;  /* timers */
  double  timestart,timestop;       /* timers */
  double  timers[4]={0.,0.,0.,0.};  /* diagnostic timers 
			              0 = start-up time before recursion
				      1 = time before median iterations
				      2 = time in median iterations
				      3 = communication time */
  int     counters[7];              /* diagnostic counts
			              0 = # of median iterations
				      1 = # of dots sent
				      2 = # of dots received
				      3 = most dots this proc ever owns
				      4 = most dot memory this proc ever allocs
				      5 = # of times a previous cut is re-used
				      6 = # of reallocs of dot array */
  int     reuse_count[7];           /* counter (as above) for reuse to record
                                       the number of dots premoved */
  int     i,j;                      /* local variables */
  int     use_ids;                  /* When true, global and local IDs will be
                                       stored along with dots in the RCB_STRUCT.
                                       When false, storage, manipulation, and
                                       communication of IDs is avoided.     
                                       Set by call to Zoltan_RB_Use_IDs(). */
  RCB_STRUCT *rcb = NULL;           /* Pointer to data structures for RCB.  */
  struct rcb_box *rcbbox = NULL;    /* bounding box of final RCB sub-domain */
  struct rcb_tree *treept = NULL;   /* tree of RCB cuts - only single cut on 
                                       exit */

  double start_time, end_time;
  double lb_time[2];
  int    tfs[2], tmp_tfs[2];        /* added for Tflops_Special; max number
                                       of procs and parts over all processors
                                       in each iteration (while loop) of 
                                       parallel partitioning.  */
  int    old_nprocs;                /* added for Tflops_Special */
  int    old_nparts;                /* added for Tflops_Special */
  double weight[RB_MAX_WGTS];       /* weight for current set */
  double weightlo[RB_MAX_WGTS];     /* weight of lower side of cut */
  double weighthi[RB_MAX_WGTS];     /* weight of upper side of cut */
  int *dotlist = NULL;              /* list of dots used only in find_median;
                                       allocated above find_median for 
                                       better efficiency (don't necessarily
                                       have to realloc for each find_median).*/
  int lock_direction = 0;           /* flag to determine direction after first
                                       iteration */
  int level;                        /* recursion level of RCB for preset_dir */
  int *dim_spec = NULL;             /* specified direction for preset_dir */
  int fp;                           /* first partition assigned to this proc */
  int np;                           /* number of parts assigned to this proc */
  char msg[128];                    /* buffer for error messages */

  /* MPI data types and user functions */

  MPI_Comm local_comm = NULL, tmp_comm = NULL;
  MPI_Op box_op;
  MPI_Datatype box_type;
  MPI_User_function Zoltan_RCB_box_merge;

  ZOLTAN_TRACE_ENTER(zz, yo);
  if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) {
    MPI_Barrier(zz->Communicator);
    timestart = time1 = Zoltan_Time(zz->Timer);
  }

  /* setup for parallel */

  proc = zz->Proc;
  nprocs = zz->Num_Proc;
  num_parts = zz->LB.Num_Global_Parts;

  /* create MPI data and function types for box and median */

  MPI_Type_contiguous(6,MPI_DOUBLE,&box_type);
  MPI_Type_commit(&box_type);
  MPI_Op_create(&Zoltan_RCB_box_merge,1,&box_op);

  /* initializations */

  /* Check how many weights per dot there are. */
  if (wgtflag>RB_MAX_WGTS){
    sprintf(msg, "Too many weights (%1d) were given; only the first %1d will be used.", wgtflag, RB_MAX_WGTS);
    ZOLTAN_PRINT_WARN(proc, yo, msg);
    wgtflag = RB_MAX_WGTS;
  }

  /* 
   * Determine whether to store, manipulate, and communicate global and 
   * local IDs.
   */
  use_ids = Zoltan_RB_Use_IDs(zz);

  /*
   *  Build the RCB Data structure and 
   *  set pointers to information in it.
   */

  start_time = Zoltan_Time(zz->Timer);
  ierr = Zoltan_RCB_Build_Structure(zz, &pdotnum, &dotmax, wgtflag, use_ids);
  if (ierr < 0) {
    ZOLTAN_PRINT_ERROR(proc, yo, 
                   "Error returned from Zoltan_RCB_Build_Structure.");
    goto End;
  }

  rcb = (RCB_STRUCT *) (zz->LB.Data_Structure);

  gidpt  = rcb->Global_IDs;
  lidpt  = rcb->Local_IDs;
  dotpt  = rcb->Dots; 
  rcbbox = rcb->Box;
  treept = rcb->Tree_Ptr;
  end_time = Zoltan_Time(zz->Timer);
  lb_time[0] = end_time - start_time;
  start_time = end_time;

  /* local copies of calling parameters */

  dottop = dotnum = pdotnum;

  /* initialize counters */

  counters[0] = 0;
  counters[1] = 0;
  counters[2] = 0;
  counters[3] = dotnum;
  counters[4] = dotmax;
  counters[5] = 0;
  counters[6] = 0;
  for (i = 0; i < 7; i++) reuse_count[i] = 0;


  /* create mark and list arrays for dots */

  allocflag = 0;
  if (dotmax > 0) {
    if (!(dotmark = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))
     || !(coord = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
     || !(wgts = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
     || !(dotlist = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))) {
      ZOLTAN_PRINT_ERROR(proc, yo, "Insufficient memory.");
      ierr = ZOLTAN_MEMERR;
      goto End;
    }
  }
  else {
    dotmark = NULL;
    coord = NULL;
    wgts = NULL;
    dotlist = NULL;
  }

  /* if reuse is turned on, turn on gen_tree since it is needed. */
  /* Also, if this is not first time through, send dots to previous proc. */
  if (reuse) {
    gen_tree = 1;

    if (treept[0].dim != -1) {
      /* find previous location of dots */
      for (outgoing = i = 0; i < dotnum; i++) {
        ierr = Zoltan_RB_Point_Assign(zz, dotpt[i].X, &dotmark[i], NULL);
        if (ierr < 0) {
          ZOLTAN_PRINT_ERROR(proc, yo, 
                             "Error returned from Zoltan_RB_Point_Assign");
          goto End;
        }
        if (dotmark[i] != proc) outgoing++;
      }

      if (outgoing)
        if ((proc_list = (int *) ZOLTAN_MALLOC(outgoing*sizeof(int))) == NULL) {
          ZOLTAN_PRINT_ERROR(proc, yo, "Insufficient memory.");
          ierr = ZOLTAN_MEMERR;
          goto End;
        }

      for (dottop = j = i = 0; i < dotnum; i++)
        if (dotmark[i] != proc)
          proc_list[j++] = dotmark[i];
        else
          dottop++;

      /* move dots */
      allocflag = 0;
      ierr = Zoltan_RB_Send_Dots(zz, &gidpt, &lidpt, &dotpt, &dotmark, 
                                 proc_list, outgoing, 
                                 &dotnum, &dotmax, proc, &allocflag,
                                 overalloc, stats, reuse_count, use_ids,
                                 zz->Communicator);
      if (ierr < 0) {
        ZOLTAN_PRINT_ERROR(proc, yo, 
                           "Error returned from Zoltan_RB_Send_Dots.");
        goto End;
      }

      if (allocflag) {
        /*
         * gidpt, lidpt and dotpt were reallocated in Zoltan_RB_Send_Dots;
         * store their values in rcb.
         */
        rcb->Global_IDs = gidpt;
        rcb->Local_IDs = lidpt;
        rcb->Dots = dotpt;
      }

      /* update counters */
      if (dotnum > counters[3]) counters[3] = dotnum;
      if (dotmax > counters[4]) counters[4] = dotmax;
      counters[6] += reuse_count[6];

      if (outgoing) ZOLTAN_FREE(&proc_list);
    }
  }


  /* set dot weights = 1.0 if user didn't and determine total weight */

  if (!wgtflag) {
    wgtflag = 1;
    for (i = 0; i < dotnum; i++) dotpt[i].Weight[0] = 1.0;
    weightlo[0] = (double) dotnum;
  }
  else {
    for (j=0; j<wgtflag; j++) weightlo[j] = 0.0;
    for (i=0; i < dotnum; i++){
      for (j=0; j<wgtflag; j++)
        weightlo[j] += dotpt[i].Weight[j];
    }
  }
  MPI_Allreduce(weightlo, weight, wgtflag, MPI_DOUBLE, MPI_SUM, zz->Communicator);

  if (check_geom) {
    ierr = Zoltan_RB_check_geom_input(zz, dotpt, dotnum);
    if (ierr < 0) {
      ZOLTAN_PRINT_ERROR(proc, yo, 
                         "Error returned from Zoltan_RB_check_geom_input");
      goto End;
    }
  }

  /* initialize sub-domain bounding box to entire domain */

  boxtmp.lo[0] = boxtmp.lo[1] = boxtmp.lo[2] = MYHUGE;
  boxtmp.hi[0] = boxtmp.hi[1] = boxtmp.hi[2] = -MYHUGE;
  
  for (i = 0; i < dotnum; i++) {
    for (j = 0; j < 3; j++) {
      if (dotpt[i].X[j] < boxtmp.lo[j])
	boxtmp.lo[j] = dotpt[i].X[j];
      if (dotpt[i].X[j] > boxtmp.hi[j])
	boxtmp.hi[j] = dotpt[i].X[j];
    }
  }

  MPI_Allreduce(&boxtmp,rcbbox,1,box_type,box_op,zz->Communicator);

  /* if preset_dir is turned on, assign cut order according to order of 
     directions */
  if (preset_dir) {
     ierr = set_preset_dir(proc, num_parts, preset_dir, rcbbox, &dim_spec);
     if (ierr < 0) 
       goto End;
  }


  /* if reuse_dir is turned on, turn on gen_tree since it is needed. */
  /* Also, if this is not first time through, set lock_direction to reuse
     the directions determined previously */
  if (reuse_dir) {
     gen_tree = 1;
     if (treept[0].dim != -1)
        lock_direction = 1;
  }

  /* create local communicator for use in recursion */

  if (zz->Tflops_Special)
     local_comm = zz->Communicator;
  else
     MPI_Comm_dup(zz->Communicator,&local_comm);

  if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) {
    time2 = Zoltan_Time(zz->Timer);
    timers[0] = time2 - time1;
  }

  /* recursively halve until just one part or proc in set */
  
  old_nprocs = num_procs = nprocs;
  old_nparts = num_parts;
  partlower = 0;
  root = 0;
  old_set = 1;
  ierr = Zoltan_LB_Proc_To_Part(zz, proc, &np, &fp);
  for (i = fp; i < (fp + np); i++) {
    treept[i].parent = 0;
    treept[i].left_leaf = 0;
  }
  if (zz->Tflops_Special) {
    proclower = 0;
    tfs[0] = nprocs;
    tfs[1] = num_parts;
  }
  level = 0;

  while ((num_parts > 1 && num_procs > 1) || 
         (zz->Tflops_Special && tfs[0] > 1 && tfs[1] > 1)) {

    if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) 
      time1 = Zoltan_Time(zz->Timer);

    ierr = Zoltan_Divide_Machine(zz, part_sizes, proc, local_comm, &set, 
                                 &proclower, &procmid, &num_procs, 
                                 &partlower, &partmid, &num_parts, 
                                 &fractionlo);
    if (ierr < 0) {
      ZOLTAN_PRINT_ERROR(proc, yo, "Error in Zoltan_Divide_Machine.");
      goto End;
    }

    /* tfs[0] is max number of processors in all sets over all processors -
     * tfs[1] is max number of partitions in all sets over all processors -
     * force all processors to go through all levels of parallel rcb */
    if (zz->Tflops_Special) {
      tmp_tfs[0] = num_procs;
      tmp_tfs[1] = num_parts;
      MPI_Allreduce(tmp_tfs, tfs, 2, MPI_INT, MPI_MAX, local_comm);
    }

    /* create mark array and active list for dots */

    if (allocflag) {
      allocflag = 0;
      ZOLTAN_FREE(&dotmark);
      ZOLTAN_FREE(&coord);
      ZOLTAN_FREE(&wgts);
      ZOLTAN_FREE(&dotlist);
      if (!(dotmark = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))
       || !(coord = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
       || !(wgts = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
       || !(dotlist = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))) {
        ZOLTAN_PRINT_ERROR(proc, yo, "Memory error.");
        ierr = ZOLTAN_MEMERR;
        goto End;
      }
    }

    /* copy correct coordinate value into the temporary array */

    dim = cut_dimension(lock_direction, treept, partmid, 
                        preset_dir, dim_spec, &level, rcbbox);
    for (i = 0; i < dotnum; i++) {
      coord[i] = dotpt[i].X[dim];
      wgts[i] = dotpt[i].Weight[0];
    }

    /* determine if there is a first guess to use */
    /* The test on old_nparts is for the TFLOPS_SPECIAL flag */
    if (old_nparts > 1 && reuse && dim == treept[partmid].dim) {
      if (stats) counters[5]++;
      valuehalf = treept[partmid].cut;
      first_guess = 1;
    }
    else first_guess = 0;

    if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) 
      time2 = Zoltan_Time(zz->Timer);

    if (!Zoltan_RB_find_median(
                zz->Tflops_Special, coord, wgts, dotmark, dotnum, proc, 
                fractionlo, local_comm, &valuehalf, first_guess, &(counters[0]),
                nprocs, old_nprocs, proclower, old_nparts, 
                wgtflag, rcbbox->lo[dim], rcbbox->hi[dim], 
                weight[0], weightlo, weighthi,
                dotlist, rectilinear_blocks)) {
      ZOLTAN_PRINT_ERROR(proc, yo,"Error returned from Zoltan_RB_find_median.");
      ierr = ZOLTAN_FATAL;
      goto End;
    }

    if (set)    /* set weight for current partition */
      for (j=0; j<wgtflag; j++) weight[j] = weighthi[j];
    else
      for (j=0; j<wgtflag; j++) weight[j] = weightlo[j];

    if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) 
      time3 = Zoltan_Time(zz->Timer);

    /* store cut info in tree only if proc "owns" partmid */
    /* test of partmid > 0 prevents treept[0] being set when this cut is 
       only removing low-numbered processors (proclower to procmid-1) that
       have no partitions in them from the processors remaining to 
       be partitioned. */
    if (partmid > 0 && partmid == fp) {
      treept[partmid].dim = dim;
      treept[partmid].cut = valuehalf;
      treept[partmid].parent = old_set ? -(root+1) : root+1;
      /* The following two will get overwritten when the information is
         assembled if this is not a terminal cut */
      treept[partmid].left_leaf = -partlower;
      treept[partmid].right_leaf = -partmid;
    }
    if (old_nprocs > 1 && partmid > 0 && partmid != partlower + old_nparts) {  
      /* old_nprocs > 1 test: Don't reset these values if proc is in loop only 
       * because of other procs for Tflops_Special.
       * partmid > 0 test:  Don't reset these values if low-numbered processors
       * (proclower to procmid-1) have zero partitions and this cut is removing
       * them from the processors remaining to be partitioned. 
       * partmid != partlower + old_nparts test:  Don't reset these values if
       * cut is removing high-numbered processors with zero partitions from
       * the processors remaining to be partitioned.
       */
      old_set = set;
      root = partmid;
    }
    
    /* use cut to shrink RCB domain bounding box */

    if (old_nprocs > 1) {
      if (!set)
        rcbbox->hi[dim] = valuehalf;
      else
        rcbbox->lo[dim] = valuehalf;
    }

    allocflag = 0;
    ierr = Zoltan_RB_Send_Outgoing(zz, &gidpt, &lidpt, &dotpt, &dotmark,
                               &dottop, &dotnum, &dotmax,
                               set, &allocflag, overalloc,
                               stats, counters, use_ids, local_comm, proclower,
                               old_nprocs, partlower, partmid);
    if (ierr < 0) {
      ZOLTAN_PRINT_ERROR(proc, yo,
                         "Error returned from Zoltan_RB_Send_Outgoing.");
      goto End;
    }

    if (allocflag) {
      /* 
       * gidpt, lidpt and dotpt were reallocated in Zoltan_RB_Send_Outgoing;
       * store their values in rcb.
       */
      rcb->Global_IDs = gidpt;
      rcb->Local_IDs = lidpt;
      rcb->Dots = dotpt;
    }

    /* create new communicators */

    if (zz->Tflops_Special) {
       if (set) {
         proclower = procmid;
         partlower = partmid;
       }
       old_nprocs = num_procs;
       old_nparts = num_parts;
    }
    else {
       if (set) partlower = partmid;
       MPI_Comm_split(local_comm,set,proc,&tmp_comm);
       MPI_Comm_free(&local_comm);
       local_comm = tmp_comm;
       old_nprocs = num_procs;
       old_nparts = num_parts;
    }

    if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) {
      time4 = Zoltan_Time(zz->Timer);
      timers[1] += time2 - time1;
      timers[2] += time3 - time2;
      timers[3] += time4 - time3;
    }
  }

  /* have recursed all the way to a single processor sub-domain */

  /* Send dots to correct processors for their partitions.  This is needed
     most notably when a processor has zero partitions on it, but still has
     some dots after the parallel partitioning. */

  ierr = Zoltan_RB_Send_To_Part(zz, &gidpt, &lidpt, &dotpt, &dotmark, &dottop,
                               &dotnum, &dotmax, set, &allocflag, overalloc,
                               stats, counters, use_ids);
  if (ierr < 0) {
    ZOLTAN_PRINT_ERROR(zz->Proc, yo, 
                       "Error returned from Zoltan_RB_Send_To_Part");
    goto End;
  }

  if (allocflag) {
    /* 
     * gidpt, lidpt and dotpt were reallocated in Zoltan_RB_Send_To_Part;
     * store their values in rcb.
     */
    rcb->Global_IDs = gidpt;
    rcb->Local_IDs = lidpt;
    rcb->Dots = dotpt;
  }

  /* All dots are now on the processors they will end up on; now generate
   * more partitions if needed. */

  if (num_parts > 1) {
    int *dindx = (int *) ZOLTAN_MALLOC(dotnum * 2 * sizeof(int));
    int *tmpdindx = dindx + dotnum;
    if (allocflag) {
      ZOLTAN_FREE(&dotmark);
      ZOLTAN_FREE(&coord);
      ZOLTAN_FREE(&wgts);
      ZOLTAN_FREE(&dotlist);
      if (!(dotmark = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))
       || !(coord = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
       || !(wgts = (double *) ZOLTAN_MALLOC(dotmax*sizeof(double)))
       || !(dotlist = (int *) ZOLTAN_MALLOC(dotmax*sizeof(int)))) {
        ZOLTAN_PRINT_ERROR(proc, yo, "Memory error.");
        ierr = ZOLTAN_MEMERR;
        goto End;
      }
    }
    for (i = 0; i < dotnum; i++)
      dindx[i] = i;
    ierr = serial_rcb(zz, dotpt, dotmark, dotlist, old_set, root,
               rcbbox, weight[0], dotnum, num_parts,
               &(dindx[0]), &(tmpdindx[0]), partlower, 
               proc, wgtflag, lock_direction, reuse, stats, gen_tree, 
               preset_dir, 
               rectilinear_blocks, counters, treept, dim_spec, level,
               coord, wgts, part_sizes);
    ZOLTAN_FREE(&dindx);
    if (ierr < 0) {
      ZOLTAN_PRINT_ERROR(proc, yo, "Error returned from serial_rcb");
      goto End;
    }
  }

  end_time = Zoltan_Time(zz->Timer);
  lb_time[1] = end_time - start_time;

  if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) {
    MPI_Barrier(zz->Communicator);
    timestop = time1 = Zoltan_Time(zz->Timer);
  }

  /* error checking and statistics */

  if (check_geom) {
    ierr = Zoltan_RB_check_geom_output(zz, dotpt, part_sizes, np, fp,
                                       dotnum, pdotnum, rcbbox);
    if (ierr < 0) {
      ZOLTAN_PRINT_ERROR(proc, yo, 
                         "Error returned from Zoltan_RB_check_geom_output");
      goto End;
    }
  }

  if (stats || (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME)) 
    Zoltan_RB_stats(zz, timestop-timestart,dotpt,dotnum,
                timers,counters,stats,reuse_count,rcbbox,reuse);

  /* update calling routine parameters */
  
  start_time = Zoltan_Time(zz->Timer);

  pdotnum = dotnum;
  pdottop = dottop;

  /*  build return arguments */

  if (zz->LB.Return_Lists) {
    /* zz->LB.Return_Lists is true ==> use_ids is true */
    ierr = Zoltan_RB_Return_Arguments(zz, gidpt, lidpt, dotpt, num_import,
                                      import_global_ids, import_local_ids,
                                      import_procs, import_to_part, 
                                      dotnum, dottop);
    if (ierr < 0) {
       ZOLTAN_PRINT_ERROR(proc,yo,
                          "Error returned from Zoltan_RB_Return_Arguments.");
       goto End;
    }
  }

  if (gen_tree) {
    int *displ, *recvcount;
    int sendcount;

    if (!(displ = (int *) ZOLTAN_MALLOC(2 * zz->Num_Proc * sizeof(int)))) {
      ZOLTAN_PRINT_ERROR(zz->Proc, yo, "Memory error.");
      ierr = ZOLTAN_MEMERR;
      goto End;
    }
    recvcount = displ + zz->Num_Proc;

    ierr = Zoltan_RB_Tree_Gatherv(zz, sizeof(struct rcb_tree), &sendcount,
                                  recvcount, displ);

    MPI_Allgatherv(&treept[fp], sendcount, MPI_BYTE, treept, recvcount, displ,
                   MPI_BYTE, zz->Communicator);
    treept[0].dim = 0;
    for (i = 1; i < zz->LB.Num_Global_Parts; i++)
      if (treept[i].parent > 0)
        treept[treept[i].parent - 1].left_leaf = i;
      else if (treept[i].parent < 0)
        treept[-treept[i].parent - 1].right_leaf = i;
    ZOLTAN_FREE(&displ);
  }
  else {
    treept[0].dim = -1;
  }

  if (zz->Debug_Level >= ZOLTAN_DEBUG_ALL)
    print_rcb_tree(zz, np, fp, &(treept[fp]));

  end_time = Zoltan_Time(zz->Timer);
  lb_time[0] += (end_time - start_time);

  if (zz->Debug_Level >= ZOLTAN_DEBUG_ATIME) {
    if (zz->Proc == zz->Debug_Proc) {
      printf("ZOLTAN RCB Times:  \n");
    }
    Zoltan_Print_Stats(zz->Communicator, zz->Debug_Proc, lb_time[0], 
                   "ZOLTAN     Build:       ");
    Zoltan_Print_Stats(zz->Communicator, zz->Debug_Proc, lb_time[1], 
                   "ZOLTAN     RCB:         ");
  }

  if (zz->Debug_Level >= ZOLTAN_DEBUG_ALL) {
    /* zz->Debug_Level >= ZOLTAN_DEBUG_ALL ==> use_ids is true */
    Zoltan_RB_Print_All(zz, rcb->Global_IDs, rcb->Dots, 
                    pdotnum, pdottop, *num_import,
                    *import_global_ids, *import_procs);
  }

End:

  /* Free memory allocated by the algorithm. */

  if (!zz->Tflops_Special && local_comm != NULL) MPI_Comm_free(&local_comm);
  MPI_Type_free(&box_type);
  MPI_Op_free(&box_op);

  ZOLTAN_FREE(&coord);
  ZOLTAN_FREE(&wgts);
  ZOLTAN_FREE(&dotmark);
  ZOLTAN_FREE(&dotlist);
  if (preset_dir) ZOLTAN_FREE(&dim_spec);

  if (!reuse && !gen_tree) {
    /* Free all memory used. */
    Zoltan_RCB_Free_Structure(zz);
  }
  else if (rcb != NULL) {
    /* Free only Dots and IDs; keep other structures. */
    ZOLTAN_FREE(&(rcb->Global_IDs));
    ZOLTAN_FREE(&(rcb->Local_IDs));
    ZOLTAN_FREE(&(rcb->Dots));
  }

  ZOLTAN_TRACE_EXIT(zz, yo);
  return(ierr);  
}



/******************************************************************************/

/* ----------------------------------------------------------------------- */

/* MPI user-defined reduce operations */

/* min/max merge of each component of a rcb_box */

void Zoltan_RCB_box_merge(void *in, void *inout, int *len, MPI_Datatype *dptr)

{
  int i;
  struct rcb_box *box1,*box2;

  box1 = (struct rcb_box *) in;
  box2 = (struct rcb_box *) inout;

  for (i = 0; i < 3; i++) {
    if (box1->lo[i] < box2->lo[i])
      box2->lo[i] = box1->lo[i];
    if (box1->hi[i] > box2->hi[i])
      box2->hi[i] = box1->hi[i];
  }
}

/******************************************************************************/

static void print_rcb_tree(ZZ *zz, int np, int fp, struct rcb_tree *treept_arr)
{
int i;
struct rcb_tree *treept = treept_arr;

  Zoltan_Print_Sync_Start(zz->Communicator, TRUE);
  for (i = fp; i < fp+np; i++) {
    printf("Proc %d, Part %d:  Tree Struct:\n", zz->Proc, i);
    printf("                   cut        = %e\n", treept->cut);
    printf("                   dim        = %d\n", treept->dim);
    printf("                   parent     = %d\n", treept->parent);
    printf("                   left_leaf  = %d\n", treept->left_leaf);
    printf("                   right_leaf = %d\n", treept->right_leaf);
    treept++;
  }
  Zoltan_Print_Sync_End(zz->Communicator, TRUE);
}

/******************************************************************************/

static int cut_dimension(
  int lock_direction,        /* flag indicating whether directions are locked */
  struct rcb_tree *treept,   /* tree of RCB cuts */
  int partmid,               /* lowest partition in set 1. */
  int preset_dir,            /* Set order of directions:     0: don't set,
                                 1: xyz,        2: xzy,      3: yzx,
                                 4: yxz,        5: zxy,      6: zyx  */
  int *dim_spec,             /* specified direction for preset_dir */
  int *level,                /* recursion level of RCB for preset_dir */
  struct rcb_box *rcbbox     /* bounding box of RCB sub-domain */
)
{
/* compute dim = dimension (xyz = 012) to bisect on */
int dim;

  if (lock_direction)
    dim = treept[partmid].dim;
  else if (preset_dir)
    dim = dim_spec[(*level)++];
  else {
    dim = 0;
    if (rcbbox->hi[1] - rcbbox->lo[1] > rcbbox->hi[0] - rcbbox->lo[0])
      dim = 1;
    if (dim == 0 && 
        rcbbox->hi[2] - rcbbox->lo[2] > rcbbox->hi[0] - rcbbox->lo[0])
      dim = 2;
    if (dim == 1 &&
        rcbbox->hi[2] - rcbbox->lo[2] > rcbbox->hi[1] - rcbbox->lo[1])
      dim = 2;
  }

  return dim;
}

/******************************************************************************/

static int set_preset_dir(
  int proc,                  /* Current processor */
  int nparts,                /* total number of partitions */
  int preset_dir,            /* Set order of directions:     0: don't set,
                                 1: xyz,        2: xzy,      3: yzx,
                                 4: yxz,        5: zxy,      6: zyx  */
  struct rcb_box *rcbbox,    /* bounding box of RCB sub-domain */
  int **dim_spec             /* specified direction for preset_dir */
)
{
/* if preset_dir is turned on, count number of levels of recursion,
   determine number of cuts in each direction, and then assign those
   cuts to an order according to order of directions */

char *yo = "set_preset_dir";
int ix[3];                        /* temporaries for preset_dir */
double wx, wy, wz;                /* width for preset_dir */
int tmp_nparts;
int level;
int i, j;
int ierr = ZOLTAN_OK;
int dim;

  if (preset_dir < 0 || preset_dir > 6) {
    ZOLTAN_PRINT_WARN(proc, yo, 
                    "Parameter RCB_SET_DIRECTIONS out of bounds; reset to 1.");
    ierr = ZOLTAN_WARN;
    preset_dir = 1;
  }

  wx = rcbbox->hi[0] - rcbbox->lo[0];
  wy = rcbbox->hi[1] - rcbbox->lo[1];
  wz = rcbbox->hi[2] - rcbbox->lo[2];

  for (i = 0; i < 3; i++) ix[i] = 0;

  for (tmp_nparts = nparts, level = 0; tmp_nparts > 1; level++) {
    tmp_nparts = (tmp_nparts + 1)/2;
    if (wz > wx && wz > wy) {
      ix[2]++;
      wz /= 2.0;
    }
    else if (wy > wx && wy > wz) {
      ix[1]++;
      wy /= 2.0;
    }
    else {
      ix[0]++;
      wx /= 2.0;
    }
  }

  if (level > 0) {
    *dim_spec = (int *) ZOLTAN_MALLOC(level*sizeof(int));
    if (*dim_spec == NULL) {
      ZOLTAN_PRINT_ERROR(proc, yo, "Memory error.");
      ierr = ZOLTAN_MEMERR;
      goto End;
    }

    for (i = j = 0; i < level; i++) {
      if (j == 0) {
        if (preset_dir < 3)
          dim = 0;
        else if (preset_dir < 5)
          dim = 1;
        else
          dim = 2;
        if (ix[dim] == 0)
          j++;
      }
      if (j == 1) {
        if (preset_dir == 1 || preset_dir == 6)
          dim = 1;
        else if (preset_dir < 4)
          dim = 2;
        else
          dim = 0;
        if (ix[dim] == 0)
          j++;
      }
      if (j == 2) {
        if (preset_dir == 3 || preset_dir == 6)
          dim = 0;
        else if (preset_dir == 2 || preset_dir == 5)
          dim = 1;
        else
          dim = 0;
      }
      (*dim_spec)[i] = dim;
      ix[dim]--;
    }
  }

End:
  return ierr;
}

/******************************************************************************/

static int serial_rcb(
  ZZ *zz,
  struct Dot_Struct *dotpt,  /* pointer to rcb->Dots. */
  int *dotmark,              /* which side of median for each dot */
  int *dotlist,              /* list of dots used only in find_median;
                                allocated above find_median for 
                                better efficiency (don't necessarily
                                have to realloc for each find_median).*/
  int old_set,               /* Set the objects to be partitioned were in 
                                for last cut */
  int root,                  /* partition for which last cut was stored. */
  struct rcb_box *rcbbox,    /* bounding box of RCB sub-domain */
  double weight,             /* weight for current set */
  int dotnum,                /* number of input dots */
  int num_parts,             /* number of partitions to create. */
  int *dindx,                /* Index into dotpt for dotnum dots to be 
                                partitioned; reordered in serial_rcb so set0
                                dots are followed by set1 dots. */
  int *tmpdindx,             /* Temporary memory used in reordering dindx. */
  int partlower,             /* smallest partition number to be created. */
  int proc,                  /* processor number. */
  int wgtflag,               /* No. of weights per dot. */
  int lock_direction,        /* flag indicating whether directions are locked */
  int reuse,                 /* (0) don't use (1) use previous cuts
                                stored in treept at initial guesses.      */
  int stats,                 /* Print timing & count summary?             */
  int gen_tree,              /* (0) do not (1) do generate full treept    */
  int preset_dir,            /* Set order of directions:     0: don't set,
                                 1: xyz,        2: xzy,      3: yzx,
                                 4: yxz,        5: zxy,      6: zyx  */
  int rectilinear_blocks,    /* (0) do (1) don't break ties in find_median*/
  int counters[],            /* diagnostic counts */
  struct rcb_tree *treept,   /* tree of RCB cuts */
  int *dim_spec,             /* specified direction for preset_dir */
  int level,                 /* recursion level of RCB for preset_dir */
  double *coord,             /* temp array for median_find */
  double *wgts,              /* temp array for median_find */
  float *part_sizes          /* Array of size zz->LB.Num_Global_Parts
                                containing the percentage of work to be
                                assigned to each partition.               */

)
{
char *yo = "serial_rcb";
int ierr = ZOLTAN_OK;
int i;
int dim;
int first_guess;
int new_nparts;
int partmid;
double fractionlo;
double valuehalf;
double weightlo, weighthi;
int set0, set1;
struct rcb_box tmpbox;

  if (num_parts == 1) {
    for (i = 0; i < dotnum; i++)
      dotpt[dindx[i]].Part = partlower;
  }
  else {
    ierr = Zoltan_Divide_Parts(zz, part_sizes, num_parts,
                               &partlower, &partmid, &fractionlo);

    dim = cut_dimension(lock_direction, treept, partmid, 
                        preset_dir, dim_spec, &level, rcbbox);

    for (i = 0; i < dotnum; i++) {
      coord[i] = dotpt[dindx[i]].X[dim];
      wgts[i] = dotpt[dindx[i]].Weight[0];
    }

    if (reuse && dim == treept[partmid].dim) {
      if (stats) counters[5]++;
      valuehalf = treept[partmid].cut;
      first_guess = 1;
    }
    else
      first_guess = 0;

    /* Call find_median with Tflops_Special == 0; avoids communication */
    if (!Zoltan_RB_find_median(0, coord, wgts, dotmark, dotnum, proc, 
                               fractionlo, MPI_COMM_SELF, &valuehalf,
                               first_guess, &(counters[0]), 1, 1,
                               proc, num_parts, wgtflag, rcbbox->lo[dim], 
                               rcbbox->hi[dim], weight, &weightlo, &weighthi,
                               dotlist, rectilinear_blocks)) {
      ZOLTAN_PRINT_ERROR(proc, yo, "Error returned from Zoltan_RB_find_median");
      ierr = ZOLTAN_FATAL;
      goto End;
    }

    treept[partmid].dim = dim;
    treept[partmid].cut = valuehalf;
    treept[partmid].parent = old_set ? -(root+1) : root+1;
    /* The following two will get overwritten when the information is
       assembled if this is not a terminal cut */
    treept[partmid].left_leaf = -partlower;
    treept[partmid].right_leaf = -partmid;

    root = partmid;

    /* Create new dindx, grouping set 0 and set 1 dots together */
    for (set0 = 0, set1 = dotnum, i = 0; i < dotnum; i++) {
      if (dotmark[i] == 0) 
        tmpdindx[set0++] = dindx[i];
      else
        tmpdindx[--set1] = dindx[i];
    }
    memcpy(dindx, tmpdindx, dotnum * sizeof(int));

    /* If set 0 has at least one partition and at least one dot,
     * call serial_rcb for set 0 */
    new_nparts = partmid - partlower;
    if (new_nparts > 0 && set1 != 0) {
      memcpy(&tmpbox, rcbbox, sizeof(struct rcb_box));
      tmpbox.hi[dim] = valuehalf;
      ierr = serial_rcb(zz, dotpt, dotmark, dotlist, 0, root,
                        &tmpbox, weightlo, set0, new_nparts,
                        &(dindx[0]), &(tmpdindx[0]), partlower, 
                        proc, wgtflag, lock_direction, reuse, stats, gen_tree, 
                        preset_dir, 
                        rectilinear_blocks, counters, treept, dim_spec, level,
                        coord, wgts, part_sizes);
      if (ierr < 0) {
        goto End;
      }
    }

    /* If set 1 has at least one partition and at least one dot,
     * call serial_rcb for set 1 */
    new_nparts = partlower + num_parts - partmid;
    if (new_nparts > 0 && set0 != dotnum) {
      memcpy(&tmpbox, rcbbox, sizeof(struct rcb_box));
      tmpbox.lo[dim] = valuehalf;
      ierr = serial_rcb(zz, dotpt, dotmark, dotlist, 1, root,
                        &tmpbox, weighthi, dotnum-set0, new_nparts,
                        &(dindx[set1]), &(tmpdindx[set1]), partmid, 
                        proc, wgtflag, lock_direction, reuse, stats, gen_tree, 
                        preset_dir, 
                        rectilinear_blocks, counters, treept, dim_spec, level,
                        coord, wgts, part_sizes);
      if (ierr < 0) {
        goto End;
      }
    }
  }
End:

  return ierr;
}

#ifdef __cplusplus
} /* closing bracket for extern "C" */
#endif
