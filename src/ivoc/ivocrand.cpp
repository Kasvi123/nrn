#include <../../nrnconf.h>

// definition of random number generation from the g++ library

#include <stdio.h>
#include <stdlib.h>

#include <InterViews/resource.h>
#include "classreg.h"
#include "oc2iv.h"
#include "utils/enumerate.h"

#include <vector>
#include <ocnotify.h>
#include "ocobserv.h"
#include <nrnran123.h>

#include "Rand.hpp"
#include "Random123RNG.hpp"
#if HAVE_IV
#include "ivoc.h"
#endif

#undef dmaxuint
#define dmaxuint 4294967295.

extern "C" void nrn_random_play();

class RandomPlay: public Observer, public Resource {
  public:
    RandomPlay(Rand*, neuron::container::data_handle<double> px);
    virtual ~RandomPlay() {}
    void play();
    void list_remove();
    virtual void update(Observable*);

  private:
    Rand* r_;
    neuron::container::data_handle<double> px_;
};

using RandomPlayList = std::vector<RandomPlay*>;
static RandomPlayList* random_play_list_;

RandomPlay::RandomPlay(Rand* r, neuron::container::data_handle<double> px)
    : r_{r}
    , px_{std::move(px)} {
    random_play_list_->push_back(this);
    ref();
    neuron::container::notify_when_handle_dies(px_, this);
    nrn_notify_when_void_freed(static_cast<Object*>(r->userdata_), this);
}
void RandomPlay::play() {
    // printf("RandomPlay::play\n");
    *px_ = (*(r_->d))(r_->gen);
}
void RandomPlay::list_remove() {
    if (auto it = std::find(random_play_list_->begin(), random_play_list_->end(), this);
        it != random_play_list_->end()) {
        // printf("RandomPlay %p removed from list cnt=%d i=%d %p\n", this, cnt, i);
        random_play_list_->erase(it);
        unref_deferred();
    }
}
void RandomPlay::update(Observable*) {
    // printf("RandomPlay::update\n");
    nrn_notify_pointer_disconnect(this);
    list_remove();
}

// syntax:
// a = new Rand([seed],[size])

static void* r_cons(Object* obj) {
    unsigned long seed = 0;
    int size = 55;

    if (ifarg(1))
        seed = long(*getarg(1));
    if (ifarg(2))
        size = int(chkarg(2, 7, 98));

    Rand* r = new Rand();
    return (void*) r;
}

// destructor -- called when no longer referenced

static void r_destruct(void* r) {
    delete (Rand*) r;
}

// Use a variant of the Linear Congruential Generator (algorithm M)
// described in Knuth, Art of Computer Programming, Vol. III in
// combination with a Fibonacci Additive Congruential Generator.
// This is a "very high quality" random number generator,
// Default size is 55, giving a size of 1244 bytes to the structure
// minimum size is 7 (total 100 bytes), maximum size is 98 (total 2440 bytes)
// syntax:
// r.ACG([seed],[size])

static double r_ACG(void* /* r */) {
    hoc_warning("ACG asked but will get a random123 instead. ACG has been removed.", nullptr);
    return 1.;
}

// Use a Multiplicative Linear Congruential Generator.  Not as high
// quality as the ACG, but uses only 8 bytes
// syntax:
// r.MLCG([seed1],[seed2])

static double r_MLCG(void* r) {
    hoc_warning("MLCG asked but will get a random123 instead. ACG has been removed.", nullptr);
    return 1.;
}

static double r_MCellRan4(void* r) {
    hoc_warning("MCellRan4 asked but will get a random123 instead. ACG has been removed.", nullptr);
    return 1.;
}

long nrn_get_random_sequence(Rand* r) {
    // FIXME: understand what I should return
    return 0;
}

void nrn_set_random_sequence(Rand* r, long seq) {
    // FIXME:
    // assert(r->type_ == 2);
    // MCellRan4* mcr = (MCellRan4*) r->gen;
    // mcr->ihigh_ = seq;
}

int nrn_random_isran123(Rand* r, uint32_t* id1, uint32_t* id2, uint32_t* id3) {
    NrnRandom123* nr = (NrnRandom123*) r->gen;
    nrnran123_getids3(nr->s_, id1, id2, id3);
    return 1;
}

static double r_nrnran123(void* r) {
    Rand* x = (Rand*) r;
    uint32_t id1 = 0;
    uint32_t id2 = 0;
    uint32_t id3 = 0;
    if (ifarg(1))
        id1 = (uint32_t) (chkarg(1, 0., dmaxuint));
    if (ifarg(2))
        id2 = (uint32_t) (chkarg(2, 0., dmaxuint));
    if (ifarg(3))
        id3 = (uint32_t) (chkarg(3, 0., dmaxuint));
    NrnRandom123* r123{};
    try {
        r123 = new NrnRandom123(id1, id2, id3);
    } catch (const std::bad_alloc& e) {
        hoc_execerror("Bad allocation for 'NrnRandom123'", e.what());
    }
    x->rand->generator(r123);
    delete x->gen;
    x->gen = x->rand->generator();
    x->type_ = 4;
    return 0.;
}

static double r_ran123_globalindex(void* r) {
    if (ifarg(1)) {
        uint32_t gix = (uint32_t) chkarg(1, 0., 4294967295.); /* 2^32 - 1 */
        nrnran123_set_globalindex(gix);
    }
    return (double) nrnran123_get_globalindex();
}

static double r_sequence(void* r) {
    Rand* x = (Rand*) r;
    if (x->type_ != 2 && x->type_ != 4) {
        hoc_execerror(
            "Random.seq() can only be used if the random generator was MCellRan4 or Random123", 0);
    }

    if (x->type_ == 4) {
        uint32_t seq;
        char which;
        if (ifarg(1)) {
            double s = chkarg(1, 0., 17179869183.); /* 2^34 - 1 */
            seq = (uint32_t) (s / 4.);
            which = char(s - seq * 4.);
            NrnRandom123* nr = (NrnRandom123*) x->gen;
            nrnran123_setseq(nr->s_, seq, which);
        }
        nrnran123_getseq(((NrnRandom123*) x->gen)->s_, &seq, &which);
        return double(seq) * 4. + double(which);
    }

    MCellRan4* mcr = (MCellRan4*) x->gen;
    if (ifarg(1)) {
        mcr->ihigh_ = (long) (*getarg(1));
    }
    return (double) mcr->ihigh_;
}

int nrn_random123_setseq(Rand* r, uint32_t seq, char which) {
    if (r->type_ != 4) {
        return 0;
    }
    nrnran123_setseq(((NrnRandom123*) r->gen)->s_, seq, which);
    return 1;
}

int nrn_random123_getseq(Rand* r, uint32_t* seq, char* which) {
    if (r->type_ != 4) {
        return 0;
    }
    nrnran123_getseq(((NrnRandom123*) r->gen)->s_, seq, which);
    return 1;
}

static double r_Isaac64(void* r) {
    Rand* x = (Rand*) r;

    uint32_t seed1 = 0;

    if (ifarg(1))
        seed1 = (uint32_t) (*getarg(1));
    Isaac64* mcr = new Isaac64(seed1);
    x->rand->generator(mcr);
    delete x->gen;
    x->gen = x->rand->generator();
    x->type_ = 3;
    return (double) mcr->seed();
}

// Pick again from the distribution last used
// syntax:
// r.repick()
static double r_repick(void* r) {
    Rand* x = (Rand*) r;
    return (*(x->rand))();
}

double nrn_random_pick(Rand* r) {
    if (r) {
        return (*(r->rand))();
    } else {
        return .5;
    }
}

void nrn_random_reset(Rand* r) {
    if (r) {
        r->gen->reset();
    }
}

Rand* nrn_random_arg(int i) {
    Object* ob = *hoc_objgetarg(i);
    check_obj_type(ob, "Random");
    Rand* r = (Rand*) (ob->u.this_pointer);
    return r;
}

// uniform random variable over the open interval [low...high)
// syntax:
//     r.uniform(low, high)

static double r_uniform(void* r) {
    Rand* x = (Rand*) r;
    double a1 = *getarg(1);
    double a2 = *getarg(2);
    delete x->rand;
    x->rand = new Uniform(a1, a2);
    return x->d(x->gen);
}

// uniform random variable over the closed interval [low...high]
// syntax:
//     r.discunif(low,high)

static double r_discunif(void* r) {
    Rand* x = (Rand*) r;
    long a1 = long(*getarg(1));
    long a2 = long(*getarg(2));
    delete x->rand;
    x->rand = new DiscreteUniform(a1, a2);
    return x->d(x->gen);
}


// normal (gaussian) distribution
// syntax:
//     r.normal(mean,variance)

static double r_normal(void* r) {
    Rand* x = (Rand*) r;
    double mean = *getarg(1);
    double variance = *getarg(2);
    delete x->rand;
    x->rand = new Normal(mean, std::sqrt(variance));
    return x->d(x->gen);
}


// logarithmic normal distribution
// syntax:
//     r.lognormal(mean)

static double r_lognormal(void* r) {
    Rand* x = (Rand*) r;
    double mean = *getarg(1);
    double variance = *getarg(2);
    delete x->rand;
    x->rand = new LogNormal(mean, std::sqrt(variance));
    return x->d(x->gen);
}


// poisson distribution
// syntax:
//   r.poisson(mean)

static double r_poisson(void* r) {
    Rand* x = (Rand*) r;
    double mean = *getarg(1);
    delete x->rand;
    x->rand = new Poisson(mean);
    return x->d(x->gen);
}


// binomial distribution, which models successfully drawing items from a pool
// n is the number items in the pool and p is the probablity of each item
// being successfully drawn (n>0, 0<=p<=1)
// syntax:
//     r.binomial(n,p)

static double r_binomial(void* r) {
    Rand* x = (Rand*) r;
    int t = int(chkarg(1, 0, 1e99));
    double p = chkarg(2, 0, 1);
    delete x->rand;
    x->rand = new Binomial(t, p);
    return x->d(x->gen);
}


// discrete geometric distribution
// Given 0<=mean<=1, returns the number of uniform random samples
// that were drawn before the sample was larger than mean (always
// greater than 0.
// syntax:
//     r.geometric(mean)

static double r_geometric(void* r) {
    Rand* x = (Rand*) r;
    double mean = chkarg(1, 0, 1);
    delete x->rand;
    x->rand = new Geometric(mean);
    return x->d(x->gen);
}


// negative exponential distribution
// syntax:
//     r.negexp(mean)

static double r_negexp(void* r) {
    Rand* x = (Rand*) r;
    double xmean = *getarg(1);
    delete x->rand;
    x->rand = new NegativeExpntl(xmean);
    return x->d(x->gen);
}


// Erlang distribution
// syntax:
//     r.erlang(mean,variance)

// static double r_erlang(void* r) {
//     Rand* x = (Rand*) r;
//     double a1 = *getarg(1);
//     double a2 = *getarg(2);
//     delete x->rand;
//     x->rand = new Erlang(a1, a2, x->gen);
//     return (*(x->rand))();
// }


// Weibull distribution
// syntax:
//     r.weibull(alpha,beta)

static double r_weibull(void* r) {
    Rand* x = (Rand*) r;
    double alpha = *getarg(1);
    double beta = *getarg(2);
    delete x->rand;
    x->rand = new Weibull(alpha, beta);
    return x->d(x->gen);
}

static double r_play(void* r) {
    new RandomPlay(static_cast<Rand*>(r), hoc_hgetarg<double>(1));
    return 0.;
}

extern "C" void nrn_random_play() {
    for (const auto& rp: *random_play_list_) {
        rp->play();
    }
}


static Member_func r_members[] = {{"ACG", r_ACG},
                                  {"MLCG", r_MLCG},
                                  {"Isaac64", r_Isaac64},
                                  {"MCellRan4", r_MCellRan4},
                                  {"Random123", r_nrnran123},
                                  {"Random123_globalindex", r_ran123_globalindex},
                                  {"seq", r_sequence},
                                  {"repick", r_repick},
                                  {"uniform", r_uniform},
                                  {"discunif", r_discunif},
                                  {"normal", r_normal},
                                  {"lognormal", r_lognormal},
                                  {"binomial", r_binomial},
                                  {"poisson", r_poisson},
                                  {"geometric", r_geometric},
                                  {"negexp", r_negexp},
                                  // {"erlang", r_erlang}, Used by 1 model (98017)
                                  {"weibull", r_weibull},
                                  {"play", r_play},
                                  {nullptr, nullptr}};

void Random_reg() {
    class2oc("Random", r_cons, r_destruct, r_members, nullptr, nullptr, nullptr);
    random_play_list_ = new RandomPlayList;
}

// Deprecated backwards-compatibility definitions
long nrn_get_random_sequence(void* r) {
    return nrn_get_random_sequence(static_cast<Rand*>(r));
}
int nrn_random_isran123(void* r, uint32_t* id1, uint32_t* id2, uint32_t* id3) {
    return nrn_random_isran123(static_cast<Rand*>(r), id1, id2, id3);
}
double nrn_random_pick(void* r) {
    return nrn_random_pick(static_cast<Rand*>(r));
}
void nrn_random_reset(void* r) {
    nrn_random_reset(static_cast<Rand*>(r));
}
int nrn_random123_getseq(void* r, uint32_t* seq, char* which) {
    return nrn_random123_getseq(static_cast<Rand*>(r), seq, which);
}
int nrn_random123_setseq(void* r, uint32_t seq, char which) {
    return nrn_random123_setseq(static_cast<Rand*>(r), seq, which);
}
void nrn_set_random_sequence(void* r, int seq) {
    nrn_set_random_sequence(static_cast<Rand*>(r), static_cast<long>(seq));
}
