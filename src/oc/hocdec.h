#pragma once
#define INCLUDEHOCH 1

#include "neuron/container/generic_data_handle.hpp"
#include "nrnapi.h"
#include "hocassrt.h" /* hoc_execerror instead of abort */
#include "nrnassrt.h" /* assert in case of side effects (eg. scanf) */
#include "wrap_sprintf.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>


#define gargstr hoc_gargstr
#define getarg  hoc_getarg

/* the dec alpha cxx doesn't understand struct foo* inside a struct */

struct Symbol;
struct Arrayinfo;
struct Proc;
struct Symlist;
struct cTemplate;
union Objectdata;
struct Object;
struct hoc_Item;

using Pfri = int (*)(void);
using Pfrv = void (*)(void);
using Pfrd = double (*)(void);
using Pfro = struct Object** (*) (void);
using Pfrs = const char** (*) (void);

using Pfri_vp = int (*)(void*);
using Pfrv_vp = void (*)(void*);
using Pfrd_vp = double (*)(void*);
using Pfro_vp = struct Object** (*) (void*);
using Pfrs_vp = const char** (*) (void*);

union Inst { /* machine instruction list type */
    Pfrv pf;
    Pfrd pfd;
    Pfro pfo;
    Pfrs pfs;
    Pfrv_vp pfv_vp;
    Pfrd_vp pfd_vp;
    Pfro_vp pfo_vp;
    Pfrs_vp pfs_vp;
    Inst* in;
    Symbol* sym;
    void* ptr;
    int i;
};

constexpr auto STOP = nullptr;

struct Arrayinfo {    /* subscript info for arrays */
    unsigned* a_varn; /* dependent variable number for array elms */
    int nsub;         /* number of subscripts */
    int refcount;     /* because one object always uses symbol's */
    int sub[1];       /* subscript range */
};

struct Proc {
    Inst defn;          /* FUNCTION, PROCEDURE, FUN_BLTIN */
    unsigned long size; /* length of instruction list */
    Symlist* list;      /* For constants and strings */
                        /* not used by FUN_BLTIN */
    int nauto;          /* total # local variables */
    int nobjauto;       /* the last of these are pointers to objects */
};

struct Symlist {
    Symbol* first;
    Symbol* last;
};

using Upoint = char*;

// clang-format off
constexpr auto NOTUSER =       0;
constexpr auto USERINT =       1; /* For subtype */
constexpr auto USERDOUBLE =    2;
constexpr auto USERPROPERTY =  3; /* for newcable non-range variables */
constexpr auto USERFLOAT =     4;
constexpr auto SYMBOL =        7; /* for stack type */
constexpr auto OBJECTTMP =     8; /* temporary object on stack */
constexpr auto STKOBJ_UNREF =  9; /* already unreffed temporary object on stack */
constexpr auto DYNAMICUNITS = 10; /* {modern, legacy} units pair */
constexpr auto CPLUSOBJECT =  16; /* c++ registered class */
constexpr auto JAVAOBJECT =   32; /* c++ registered class */
/* above two are bits, next must start at 64 */
constexpr auto OBJECTALIAS =   1;
constexpr auto VARALIAS =      2;
// clang-format on

struct HocSymExtension {
    float* parmlimits; /* some variables have suggested bounds */
    char* units;
    float tolerance; /* some states have cvode absolute tolerance */
};

struct Symbol { /* symbol table entry */
    char* name;
    short type;
    short subtype;            /* Flag for user integers */
    short cpublic;            /* flag set public variable. this was called `public` before C++ */
    short defined_on_the_fly; /* moved here because otherwize gcc and borland do not align the same
                                 way */
    union {
        int oboff;                             /* offset into object data pointer space */
        double* pval;                          /* User defined doubles - also for alias to scalar */
        Object* object_;                       /* alias to an object */
        char* cstr;                            /* constant string */
        double* pnum;                          /* Numbers */
        int* pvalint;                          /* User defined integers */
        float* pvalfloat;                      /* User defined floats */
        int u_auto;                            /* stack offset # for AUTO variable */
        double (*ptr)(double); /* if BLTIN */  // TODO: double as parameter?
        Proc* u_proc;
        struct {
            short type; /* Membrane type to find Prop */
            int index;  /* prop->param[index] */
        } rng;
        Symbol** ppsym; /* Pointer to symbol pointer array */
        cTemplate* ctemplate;
        Symbol* sym; /* for external */
    } u;
    unsigned s_varn;        /* dependent variable number - 0 means indep */
    Arrayinfo* arayinfo;    /* ARRAY information if null then scalar */
    HocSymExtension* extra; /* additions to symbol allow compatibility
                    with old nmodl dll's */
    Symbol* next;           /* to link to another */
};

inline bool is_array(const Symbol& sym) {
    return sym.arayinfo != nullptr;
}

using hoc_List = hoc_Item;

/** @brief Type of pdata in mechanisms.
 */
using Datum = neuron::container::generic_data_handle;

struct cTemplate {
    Symbol* sym;
    Symlist* symtable;
    int dataspace_size;
    int is_point_;   /* actually the pointtype > 0 if a point process */
    Symbol* init;    /* null if there is no initialization function */
    Symbol* unref;   /* null if there is no function to call when refcount is decremented */
    int index;       /* next  unique integer used for name for section */
    int count;       /* how many of this kind of object */
    hoc_List* olist; /* list of all instances */
    int id;
    void* observers; /* hook to c++ ClassObservable */
    void* (*constructor)(struct Object*);
    void (*destructor)(void*);
    void (*steer)(void*); /* normally nullptr */
    int (*checkpoint)(void**);
};

union Objectdata {
    double* pval;       /* pointer to array of doubles, usually just 1 */
    char** ppstr;       /* pointer to pointer to string ,allows vectors someday*/
    Object** pobj;      /* pointer to array of object pointers, usually just 1*/
    hoc_Item** psecitm; /* array of pointers to section items, usually just 1 */
    hoc_List** plist;   /* array of pointers to linked lists */
    Arrayinfo* arayinfo;
    void* _pvoid; /* Point_process */
};

struct Object {
    int refcount; /* how many object variables point to this */
    int index;    /* unique integer used for names of sections */
    union {
        Objectdata* dataspace; /* Points to beginning of object's data */
        void* this_pointer;    /* the c++ object */
    } u;
    cTemplate* ctemplate;
    void* aliases; /* more convenient names for e.g. Vector or List elements dynamically created by
                      this object*/
    hoc_Item* itm_me;        /* this object in the template list */
    hoc_Item* secelm_;       /* last of a set of contiguous section_list items used by forall */
    void* observers;         /* hook to c++ ObjObservable */
    short recurse;           /* to stop infinite recursions */
    short unref_recurse_cnt; /* free only after last return from unref callback */
};

struct VoidFunc { /* User Functions */
    const char* name;
    void (*func)(void);
};

struct DoubScal { /* User Double Scalars */
    const char* name;
    double* pdoub;
};

struct DoubVec { /* User Vectors */
    const char* name;
    double* pdoub;
    int index1;
};

struct HocParmLimits { /* recommended limits for symbol values */
    const char* name;
    float bnd[2];
};

struct HocStateTolerance { /* recommended tolerance for CVODE */
    const char* name;
    float tolerance;
};

struct HocParmUnits { /* units for symbol values */
    const char* name;
    const char* units;
};

#include "oc_ansi.h"

extern Inst *hoc_progp, *hoc_progbase, *hoc_prog, *hoc_prog_parse_recover;
extern Inst* hoc_pc;

extern Objectdata* hoc_objectdata;
extern Objectdata* hoc_top_level_data;
extern Object* hoc_thisobject;
extern Symlist* hoc_symlist;
extern Symlist* hoc_top_level_symlist;
extern Symlist* hoc_built_in_symlist;
extern Objectdata* hoc_objectdata_save(void);
extern Objectdata* hoc_objectdata_restore(Objectdata*);
#define OPVAL(sym)    hoc_objectdata[sym->u.oboff].pval
#define OPSTR(sym)    hoc_objectdata[sym->u.oboff].ppstr
#define OPOBJ(sym)    hoc_objectdata[sym->u.oboff].pobj
#define OPSECITM(sym) hoc_objectdata[sym->u.oboff].psecitm
#define OPLIST(sym)   hoc_objectdata[sym->u.oboff].plist
#define OPARINFO(sym) hoc_objectdata[sym->u.oboff + 1].arayinfo

#if LINT
#undef assert
#define assert(arg) \
    {               \
        if (arg)    \
            ;       \
    } /* so fprintf doesn't give lint */
#undef IGNORE
#define IGNORE(arg) \
    {               \
        if (arg)    \
            ;       \
    }
#define LINTUSE(arg) \
    {                \
        if (arg)     \
            ;        \
    }
char* cplint;
int ilint;
#define Strcat  cplint = strcat
#define Strncat cplint = strncat
#define Strcpy  cplint = strcpy
#define Strncpy cplint = strncpy
#define Printf  ilint = printf
#else
#undef IGNORE
#define IGNORE(arg) arg
#define LINTUSE(arg)
#define Strcat  strcat
#define Strncat strncat
#define Strcpy  strcpy
#define Strncpy strncpy
#define Printf  nrnpy_pr
#endif
using neuron::Sprintf;

#define ERRCHK(c1) c1

// No longer used because of clang format difficulty
// #define IFGUI  if (hoc_usegui) {
// #define ENDGUI }

extern int hoc_usegui; /* when 0 does not make interviews calls */
extern int nrn_istty_;

/* Enter handling for PVM  NJP 11/21/94 */
#ifdef PVM
extern int init_parallel();
int num_procs;
int* tids;
int node_num;
int mytid;
#endif
