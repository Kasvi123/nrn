#include "nrnapi.h"
#include "../../nrnconf.h"
#include "nrnmpi.h"
#include "nrnmpiuse.h"
#include "ocfunc.h"
#include "ocjump.h"
#include "nrniv_mf.h"

#include "parse.hpp"

/****************************************
 * Connections to the rest of NEURON
 ****************************************/
extern int nrn_nobanner_;
extern int diam_changed;
extern int nrn_try_catch_nest_depth;
extern "C" void nrnpy_set_pr_etal(int (*cbpr_stdoe)(int, char *), int (*cbpass)());
int ivocmain_session(int, const char **, const char **, int start_session);
void simpleconnectsection();
extern Object *hoc_newobj1(Symbol *, int);
extern void nrn_change_nseg(Section*, int);

/****************************************
 * Initialization
 ****************************************/

int nrn_init(int argc, const char **argv) {
  nrn_nobanner_ = 1;
  int exit_status = ivocmain_session(argc, argv, nullptr, 0);
#if NRNMPI_DYNAMICLOAD
  nrnmpi_stubs();
#endif
  return exit_status;
}

void nrn_redirect_stdout(int (*myprint)(int, char *)) {
  // the first argument of myprint is an integer indicating the output stream
  // if the int is 1, then stdout, else stderr
  // the char* is the message to display
  nrnpy_set_pr_etal(myprint, nullptr);
}

/****************************************
 * Sections
 ****************************************/

Section *nrn_new_section(char const *const name) {
  // TODO: check for memory leaks; should we free the symbol, pitm, etc?
  Symbol *symbol = new Symbol;
  auto pitm = new hoc_Item *;
  char *name_ptr = new char[strlen(name)];
  strcpy(name_ptr, name);
  symbol->name = name_ptr;
  symbol->type = 1;
  symbol->u.oboff = 0;
  symbol->arayinfo = 0;
  hoc_install_object_data_index(symbol);
  new_sections(nullptr, symbol, pitm, 1);
  return (*pitm)->element.sec;
}

void nrn_connect_sections(Section *child_sec, double child_x,
                          Section *parent_sec, double parent_x) {
  nrn_pushsec(child_sec);
  hoc_pushx(child_x);
  nrn_pushsec(parent_sec);
  hoc_pushx(parent_x);
  simpleconnectsection();
}

void nrn_set_section_length(Section *sec, double length) {
  // TODO: call can_change_morph(sec) to check pt3dconst_; how should we handle
  // that?
  sec->prop->dparam[2] = length;
  // dparam[7].val is for Ra
  // nrn_length_change updates 3D points if needed
  nrn_length_change(sec, length);
  diam_changed = 1;
  sec->recalc_area_ = 1;
}

double nrn_get_section_length(Section *sec) {
  return section_length(sec);
}

double nrn_get_section_Ra(Section* sec) {
    return nrn_ra(sec);
}

void nrn_set_section_Ra(Section* sec, double val) {
    // TODO: ensure val > 0
    sec->prop->dparam[7] = val;
    diam_changed = 1;
    sec->recalc_area_ = 1;
}

char const *nrn_secname(Section *sec) { return secname(sec); }

void nrn_push_section(Section *sec) { nrn_pushsec(sec); }

void nrn_pop_section(void) { nrn_sec_pop(); }

void nrn_insert_mechanism(Section *sec, Symbol *mechanism) {
  // TODO: throw exception if mechanism is not an insertable mechanism?
  mech_insert1(sec, mechanism->subtype);
}

/****************************************
 * Segments
 ****************************************/

int nrn_get_nseg(Section const * const sec) {
    // always one more node than nseg
    return sec->nnode - 1;
}

void nrn_set_nseg(Section* const sec, const int nseg) {
    nrn_change_nseg(sec, nseg);
}


void nrn_set_segment_diam(Section* const sec, const double x, const double diam) {
    Node* const node = node_exact(sec, x);
    // TODO: this is fine if no 3D points; does it work if there are 3D points?
    for (auto prop = node->prop; prop; prop=prop->next) {
        if (prop->_type == MORPHOLOGY) {
            prop->param[0] = diam;
            diam_changed = 1;
            node->sec->recalc_area_ = 1;
            break;
        }
    }
}

double* nrn_get_rangevar_ptr(Section* const sec, Symbol* const sym, double const x) {
    return nrn_rangepointer(sec, sym, x);
}


/****************************************
 * Functions, objects, and the stack
 ****************************************/

Symbol *nrn_get_symbol(char const *const name) { return hoc_lookup(name); }

int nrn_get_symbol_type(Symbol *sym) {
  // TODO: these types are in parse.hpp and are not the same between versions,
  // so we really should wrap
  return sym->type;
}

double *nrn_get_symbol_ptr(Symbol *sym) { return sym->u.pval; }

void nrn_push_double(double val) { hoc_pushx(val); }

double nrn_pop_double(void) { return hoc_xpop(); }

void nrn_push_double_ptr(double *addr) { hoc_pushpx(addr); }

double *nrn_pop_double_ptr(void) { return hoc_pxpop(); }

void nrn_push_str(char **str) { hoc_pushstr(str); }

char **nrn_pop_str(void) { return hoc_strpop(); }

void nrn_push_int(int i) { hoc_pushi(i); }

int nrn_pop_int(void) { return hoc_ipop(); }

Object *nrn_pop_object(void) {
  // NOTE: the returned object should be unref'd when no longer needed
  Object **obptr = hoc_objpop();
  Object *new_ob_ptr = *obptr;
  new_ob_ptr->refcount++;
  hoc_tobj_unref(obptr);
  return new_ob_ptr;
}

int nrn_stack_type(void) {
  switch (hoc_stack_type()) {
  case STRING:
    return STACK_IS_STR;
  case VAR:
    return STACK_IS_VAR;
  case NUMBER:
    return STACK_IS_NUM;
  case OBJECTVAR:
    return STACK_IS_OBJVAR;
  case OBJECTTMP:
    return STACK_IS_OBJTMP;
  case USERINT:
    return STACK_IS_USERINT;
  case SYMBOL:
    return STACK_IS_SYM;
  case STKOBJ_UNREF:
    return STACK_IS_OBJUNREF;
  }
  return STACK_UNKNOWN;
}

char const *const nrn_stack_type_name(int id) {
  switch (id) {
  case STACK_IS_STR:
    return "STRING";
  case STACK_IS_VAR:
    return "VAR";
  case STACK_IS_NUM:
    return "NUMBER";
  case STACK_IS_OBJVAR:
    return "OBJECTVAR";
  case STACK_IS_OBJTMP:
    return "OBJECTTMP";
  case STACK_IS_USERINT:
    return "USERINT";
  case STACK_IS_SYM:
    return "SYMBOL";
  case STACK_IS_OBJUNREF:
    return "STKOBJ_UNREF";
  }
  return "UNKNOWN";
}

Object *nrn_new_object(Symbol *sym, int narg) { return hoc_newobj1(sym, narg); }

Symbol *nrn_get_method_symbol(Object *obj, char const *const name) {
  return hoc_table_lookup(name, obj->ctemplate->symtable);
}

void nrn_call_method(Object *obj, Symbol *method_sym, int narg) {
  OcJump::execute_throw_on_exception(obj, method_sym, narg);
}

void nrn_call_function(Symbol *sym, int narg) {
  // NOTE: this differs from hoc_call_func in that the response remains on the stack
  OcJump::execute_throw_on_exception(sym, narg);
}

void nrn_unref_object(Object *obj) { hoc_obj_unref(obj); }

/****************************************
 * Miscellaneous
 ****************************************/
int nrn_call_hoc(char const *const command) { return hoc_oc(command); }

SectionListIterator::SectionListIterator(hoc_Item *my_sectionlist) {
  initial = my_sectionlist;
  current = my_sectionlist->next;
}

Section *SectionListIterator::next(void) {
  // NOTE: if no next element, returns nullptr
  while (true) {
    Section *sec = current->element.sec;

    if (sec->prop) {
      current = current->next;
      return sec;
    }
    hoc_l_delete(current);
    section_unref(sec);
    current = current->next;
    if (current == initial) {
      return nullptr;
    }
  }
}

int SectionListIterator::done(void) {
  if (initial == current) {
    return 1;
  }
  return 0;
}

// copy semantics isn't great, but only two data items
// and is cleaner to use in a for loop than having to free memory at the end
SectionListIterator *nrn_new_sectionlist_iterator(hoc_Item *my_sectionlist) {
  return new SectionListIterator(my_sectionlist);
}

void nrn_free_sectionlist_iterator(SectionListIterator *sl) { delete sl; }

Section *nrn_sectionlist_iterator_next(SectionListIterator *sl) {
  return sl->next();
}

int nrn_sectionlist_iterator_done(SectionListIterator *sl) {
  return sl->done();
}

int nrn_vector_capacity(Object *vec) {
  // TODO: throw exception if vec is not a Vector
  return vector_capacity((IvocVect *)vec->u.this_pointer);
}

double *nrn_vector_data_ptr(Object *vec) {
  // TODO: throw exception if vec is not a Vector
  return vector_vec((IvocVect *)vec->u.this_pointer);
}

double* nrn_get_pp_property_ptr(Object* pp, const char* name) {
    int index = hoc_table_lookup(name, pp->ctemplate->symtable)->u.rng.index;
    return &ob2pntproc_0(pp)->prop->param[index];
}

double* nrn_get_steered_property_ptr(Object* obj, const char* name) {
    assert(obj->ctemplate->steer);
    auto sym2 = hoc_table_lookup(name, obj->ctemplate->symtable);
    assert(sym2);
    hoc_pushs(sym2);
    // put the pointer for the memory location on the stack 
    obj->ctemplate->steer(obj->u.this_pointer);
    return hoc_pxpop();
}