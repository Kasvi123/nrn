#pragma once
#include <random>
#include "Random.h"

class Binomial: public Random {
public:
    Binomial(int n, double u, RNG *gen)
        : Random(gen)
        , d(n ,u)
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::binomial_distribution<> d;
};

class DiscreteUniform: public Random {
public:
    DiscreteUniform(long low, long high, RNG *gen)
        : Random(gen)
        , d(low, high)
    {}

    double operator()() override {
        return d(*generator());
    }
private:
    std::uniform_int_distribution<> d;
};

class Erlang: public Random {
public:
    Erlang(double mean, double variance, RNG *gen)
        : Random(gen)
        , d(mean * mean / variance, variance / mean)
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::gamma_distribution<> d;
};

class Geometric: public Random {
public:
    Geometric(double mean, RNG *gen)
        : Random(gen)
        , d(1 / (mean + 1))
    {}

    double operator()() override {
        return 1. + d(*generator());
    }
private:
    std::geometric_distribution<> d;
};

class LogNormal: public Random {
public:
    LogNormal(double mean, double variance, RNG *gen)
        : Random(gen)
        , d(std::log(mean * mean / std::sqrt(variance + (mean * mean))), std::sqrt(std::log(variance/(mean * mean) + 1)))
    {}

    double operator()() override {
        return d(*generator());
    }
private:
    std::lognormal_distribution<> d;
};

class NegativeExpntl: public Random {
public:
    NegativeExpntl(double mean, RNG *gen)
        : Random(gen)
        , d(1 / mean)
    {}

    double operator()() override {
        return d(*generator());
    }
private:
    std::exponential_distribution<> d;
};

class Normal: public Random {
public:
    Normal(double mean, double variance, RNG *gen)
        : Random(gen)
        , d(mean, std::sqrt(variance))
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::normal_distribution<> d;
};

class Poisson: public Random {
public:
    Poisson(double mean, RNG *gen)
        : Random(gen)
        , d(mean)
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::poisson_distribution<> d;

};

class Uniform: public Random {
public:
    Uniform(double low, double high, RNG *gen)
        : Random(gen)
        , d(low, high)
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::uniform_real_distribution<> d;
};

class Weibull: public Random {
public:
    Weibull(double alpha, double beta, RNG *gen)
        : Random(gen)
        , d(alpha, std::pow(beta, 1 / alpha))
    {}

    double operator()() override {
        return d(*generator());
    }

private:
    std::weibull_distribution<> d;
};

