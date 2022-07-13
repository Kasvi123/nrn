#include <../../nrnconf.h>
#include "nrnmpiuse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#if NRNMPI_DYNAMICLOAD /* to end of file */

#include "nrnwrap_dlfcn.h"

#include "nrnmpi.h"

extern char* cxx_char_alloc(size_t);
extern std::string corenrn_mpi_library;

#if DARWIN
extern void nrn_possible_mismatched_arch(const char*);
#endif

#if DARWIN || defined(__linux__)
extern const char* path_prefix_to_libnrniv();
#endif

#include <cstddef>

#include "mpispike.h"
#include "nrnmpi_def_cinc" /* nrnmpi global variables */
extern "C" {
#include "nrnmpi_dynam_cinc" /* autogenerated file */
}
#include "nrnmpi_dynam_wrappers.inc" /* autogenerated file */
#include "nrnmpi_dynam_stubs.cpp"

static void* load_mpi(const char* name, char* mes) {
    int flag = RTLD_NOW | RTLD_GLOBAL;
    void* handle = dlopen(name, flag);
    if (!handle) {
#if DARWIN
        nrn_possible_mismatched_arch(name);
#endif
        sprintf(mes, "load_mpi: %s\n", dlerror());
    } else {
        sprintf(mes, "load_mpi: %s successful\n", name);
    }
    return handle;
}

static void* load_nrnmpi(const char* name, char* mes) {
    int i;
    int flag = RTLD_NOW | RTLD_GLOBAL;
    void* handle = dlopen(name, flag);
    if (!handle) {
        sprintf(mes, "load_nrnmpi: %s\n", dlerror());
        return 0;
    }
    sprintf(mes, "load_nrnmpi: %s successful\n", name);
    for (i = 0; ftable[i].name; ++i) {
        void* p = dlsym(handle, ftable[i].name);
        if (!p) {
            sprintf(mes + strlen(mes), "load_nrnmpi: %s\n", dlerror());
            return 0;
        }
        *ftable[i].ppf = p;
    }
    {
        char* (**p)(size_t) = (char* (**) (size_t)) dlsym(handle, "p_cxx_char_alloc");
        if (!p) {
            sprintf(mes + strlen(mes), "load_nrnmpi: %s\n", dlerror());
            return 0;
        }
        *p = cxx_char_alloc;
    }
    return handle;
}

char* nrnmpi_load(int is_python) {
    int ismes = 0;
    char* pmes;
    void* handle = NULL;
    pmes = static_cast<char*>(malloc(4096));
    assert(pmes);
    pmes[0] = '\0';

    // If libmpi already in memory, find name and dlopen that.
    void* sym = dlsym(RTLD_DEFAULT, "MPI_Initialized");
    if (sym) {
        Dl_info info;
        if (dladdr(sym, &info)) {
            if (info.dli_fname[0] == '/' || strchr(info.dli_fname, ':')) {
                sprintf(pmes,
                        "<libmpi> is loaded in the sense the MPI_Initialized has an address\n");
                handle = load_mpi(info.dli_fname, pmes + strlen(pmes));
                if (handle) {
                    corenrn_mpi_library = info.dli_fname;
                    printf("already loaded: %s\n", info.dli_fname);
                } else {
                    ismes = 1;
                }
            }
        }
    }

#if DARWIN
    if (!handle) {
        sprintf(pmes, "Try loading libmpi\n");
        handle = load_mpi("libmpi.dylib", pmes + strlen(pmes));
    }
    /**
     * If libmpi.dylib is not in the standard location and dlopen fails
     * then try to use user provided or ctypes.find_library() provided
     * mpi library path.
     */
    if (!handle) {
        const char* mpi_lib_path = getenv("MPI_LIB_NRN_PATH");
        if (mpi_lib_path) {
            handle = load_mpi(mpi_lib_path, pmes + strlen(pmes));
            if (!handle) {
                sprintf(pmes, "Can not load libmpi.dylib and %s\n", mpi_lib_path);
            }
        }
    }
    if (handle) {
        /* loaded but is it openmpi or mpich */
        if (dlsym(handle, "ompi_mpi_init")) { /* it is openmpi */
                                              /* see man dyld */
            if (!load_nrnmpi("@loader_path/libnrnmpi_ompi.dylib", pmes + strlen(pmes))) {
                return pmes;
            }
            corenrn_mpi_library = "@loader_path/libcorenrnmpi_ompi.dylib";
        } else { /* must be mpich. Could check for MPID_nem_mpich_init...*/
            if (!load_nrnmpi("@loader_path/libnrnmpi_mpich.dylib", pmes + strlen(pmes))) {
                return pmes;
            }
            corenrn_mpi_library = "@loader_path/libcorenrnmpi_mpich.dylib";
        }
    } else {
        ismes = 1;
        sprintf(pmes + strlen(pmes),
                "Is openmpi or mpich installed? If not in default location, "
                "need a LD_LIBRARY_PATH on Linux or DYLD_LIBRARY_PATH on Mac OS. "
                "On Mac OS, full path to a MPI library can be provided via "
                "environmental variable MPI_LIB_NRN_PATH\n");
    }
#else /*not DARWIN*/
#ifdef MINGW
    if (!handle) {
        sprintf(pmes, "Try loading msmpi\n");
        handle = load_mpi("msmpi.dll", pmes + strlen(pmes));
    }
    if (handle) {
        if (!load_nrnmpi("libnrnmpi_msmpi.dll", pmes + strlen(pmes))) {
            return pmes;
        }
        corenrn_mpi_library = "libcorenrnmpi_msmpi.dll";
    } else {
        ismes = 1;
        return pmes;
    }
#else  /*not MINGW so must be __linux__*/

    /**
     * libmpi.so is not standard but used by most of the implemenntation
     * (mpich, openmpi, intel-mpi, parastation-mpi, hpe-mpt) but not cray-mpich.
     * we first load libmpi and then libmpich.so as a fallaback for cray system.
     */
    if (!handle) {
        sprintf(pmes, "Try loading libmpi\n");
        handle = load_mpi("libmpi.so", pmes + strlen(pmes));
    }

    // like osx, check if user has provided library via MPI_LIB_NRN_PATH
    if (!handle) {
        const char* mpi_lib_path = getenv("MPI_LIB_NRN_PATH");
        if (mpi_lib_path) {
            handle = load_mpi(mpi_lib_path, pmes + strlen(pmes));
            if (!handle) {
                sprintf(pmes, "Can not load libmpi.so and %s", mpi_lib_path);
            }
        }
    }

    if (!handle) {
        sprintf(pmes, "Try loading libmpi and libmpich\n");
        handle = load_mpi("libmpich.so", pmes + strlen(pmes));
    }

    if (handle) {
        /* with CMAKE the problem of Python launch on LINUX not resolving
           variables from already loaded shared libraries has returned.
        */
        {
            std::string error{"Promoted none of"};
            auto const promote_to_global = [&error](const char* lib) {
                if (!dlopen(lib, RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL)) {
                    char const* dlerr = dlerror();
                    error = error + ' ' + lib + " (" + (dlerr ? dlerr : "nullptr") + ')';
                    return false;
                }
                return true;
            };
            if (!promote_to_global("libnrniv.so") &&
                !promote_to_global("libnrniv-without-nvidia.so")) {
                std::cerr << error << " to RTLD_GLOBAL" << std::endl;
            }
        }
        // Figure out where to find lib[core]nrnmpi{...} libraries.
        auto const libnrnmpi_prefix = []() -> std::string {
            if (const char* nrn_home = std::getenv("NRNHOME")) {
                // TODO: what about windows path separators?
                return std::string{nrn_home} + "/lib/";
            } else {
                // Use the directory libnrniv.so is in
                return path_prefix_to_libnrniv();
            }
        }();
        // `handle` refers to "libmpi.so", figure out which MPI implementation that
        // is.
        auto const mpi_implementation = [handle] {
            if (dlsym(handle, "ompi_mpi_init")) {
                // OpenMPI
                return "ompi";
            } else if (dlsym(handle, "MPI_SGI_vtune_is_running")) {
                // Got sgi-mpt. MPI_SGI_init exists in both mpt and hmpt, so we look
                // for MPI_SGI_vtune_is_running which only exists in the non-hmpt
                // version.
                return "mpt";
            } else {
                // Assume mpich. Could check for MPID_nem_mpich_init...
                return "mpich";
            }
        }();
        auto const nrn_mpi_library = libnrnmpi_prefix + "libnrnmpi_" + mpi_implementation + ".so";
        corenrn_mpi_library = libnrnmpi_prefix + "libcorenrnmpi_" + mpi_implementation + ".so";
        if (!load_nrnmpi(nrn_mpi_library.c_str(), pmes + strlen(pmes))) {
            return pmes;
        }
    } else {
        ismes = 1;
        sprintf(pmes + strlen(pmes),
                "Is openmpi, mpich, intel-mpi, sgi-mpt etc. installed? If not in default location, "
                "need a LD_LIBRARY_PATH or MPI_LIB_NRN_PATH.\n");
    }
#endif /*not MINGW*/
#endif /* not DARWIN */
    if (!handle) {
        sprintf(pmes + strlen(pmes), "could not dynamically load libmpi.so or libmpich.so\n");
        return pmes;
    }
    free(pmes);
    return 0;
}
#endif