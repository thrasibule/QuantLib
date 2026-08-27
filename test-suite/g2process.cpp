/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 Junjie Guo

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/processes/g2process.hpp>
#include <ql/models/shortrate/twofactormodel.hpp>
#include <ql/models/shortrate/twofactormodels/g2.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/mctraits.hpp>
#include <ql/methods/montecarlo/multipath.hpp>
#include <ql/timegrid.hpp>
#include <cmath>
#include <vector>

using namespace QuantLib;
using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(G2ProcessTests)

namespace {

    // Reference implementation of the G2++ deterministic offset phi(t),
    // copied verbatim from G2::FittingParameter::Impl::value in
    // ql/models/shortrate/twofactormodels/g2.hpp. The tests check that
    // G2Process / G2ForwardProcess match this formula.
    Real referencePhi(const Handle<YieldTermStructure>& h, Time t,
                      Real a, Real sigma, Real b, Real eta, Real rho) {
        Rate forward = h->forwardRate(t, t, Continuous, NoFrequency);
        Real temp1 = sigma*(1.0-std::exp(-a*t))/a;
        Real temp2 = eta *(1.0-std::exp(-b*t))/b;
        return 0.5*temp1*temp1 + 0.5*temp2*temp2 + rho*temp1*temp2 + forward;
    }

    Handle<YieldTermStructure> makeFlatCurve(Rate rate) {
        return Handle<YieldTermStructure>(
            ext::make_shared<FlatForward>(0, TARGET(), rate, Actual365Fixed()));
    }

}

BOOST_AUTO_TEST_CASE(testG2ProcessDynamicsIgnoreTermStructure) {

    BOOST_TEST_MESSAGE(
        "Testing that G2Process factor dynamics ignore the term structure...");

    /*  phi(t) is deliberately kept out of the simulated state, so every
        StochasticProcess method must be blind to the curve; the offset
        enters only through phi() / shortRate() / shortRatePath(). This is
        what makes drift() and expectation() exact - a shifted state would
        need phi'(t), which a YieldTermStructure cannot supply.
    */
    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.5;

    G2Process paramOnly(a, sigma, b, eta, rho);
    G2Process withCurve(a, sigma, b, eta, rho, makeFlatCurve(0.04));

    const Time t = 1.5, dt = 0.5;
    Array x(2);
    x[0] = 0.002;
    x[1] = -0.003;

    const Real tol = 1e-14;

    Array iv1 = paramOnly.initialValues(), iv2 = withCurve.initialValues();
    Array d1 = paramOnly.drift(t, x), d2 = withCurve.drift(t, x);
    Array e1 = paramOnly.expectation(t, x, dt), e2 = withCurve.expectation(t, x, dt);
    for (Size i=0; i<2; ++i) {
        if (std::fabs(iv1[i] - iv2[i]) > tol)
            BOOST_ERROR("initialValues[" << i << "] depends on the term "
                        "structure: " << iv1[i] << " vs " << iv2[i]);
        if (std::fabs(d1[i] - d2[i]) > tol)
            BOOST_ERROR("drift[" << i << "] depends on the term structure: "
                        << d1[i] << " vs " << d2[i]);
        if (std::fabs(e1[i] - e2[i]) > tol)
            BOOST_ERROR("expectation[" << i << "] depends on the term "
                        "structure: " << e1[i] << " vs " << e2[i]);
    }

    // Both components start at zero: they are the zero-mean factors.
    if (std::fabs(iv2[0]) > tol || std::fabs(iv2[1]) > tol)
        BOOST_ERROR("initialValues should be (0,0), got ("
                    << iv2[0] << ", " << iv2[1] << ")");
    if (std::fabs(withCurve.x0()) > tol || std::fabs(withCurve.y0()) > tol)
        BOOST_ERROR("x0()/y0() should be zero, got " << withCurve.x0()
                    << ", " << withCurve.y0());

    Matrix diff1 = paramOnly.diffusion(t, x), diff2 = withCurve.diffusion(t, x);
    Matrix sd1 = paramOnly.stdDeviation(t, x, dt), sd2 = withCurve.stdDeviation(t, x, dt);
    for (Size i=0; i<2; ++i)
        for (Size j=0; j<2; ++j) {
            if (std::fabs(diff1[i][j] - diff2[i][j]) > tol)
                BOOST_ERROR("diffusion(" << i << "," << j << ") depends on the "
                            "term structure: " << diff1[i][j] << " vs "
                            << diff2[i][j]);
            if (std::fabs(sd1[i][j] - sd2[i][j]) > tol)
                BOOST_ERROR("stdDeviation(" << i << "," << j << ") depends on "
                            "the term structure: " << sd1[i][j] << " vs "
                            << sd2[i][j]);
        }
}

BOOST_AUTO_TEST_CASE(testG2ProcessPhiAndShortRate) {

    BOOST_TEST_MESSAGE(
        "Testing G2Process phi(t) and shortRate(t, x, y)...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.5;
    const Rate flatRate = 0.03;
    Handle<YieldTermStructure> curve = makeFlatCurve(flatRate);

    G2Process process(a, sigma, b, eta, rho, curve);

    const Time times[] = { 0.25, 1.0, 5.0, 10.0 };

    const Real tol = 1e-12;
    for (Time t : times) {
        Real expectedPhi =
            referencePhi(curve, t, a, sigma, b, eta, rho);
        Real actualPhi = process.phi(t);
        if (std::fabs(actualPhi - expectedPhi) > tol) {
            BOOST_ERROR("G2Process::phi mismatch at t=" << t
                        << ": expected " << expectedPhi
                        << ", got " << actualPhi);
        }
    }

    // shortRate adds the offset back to the simulated factors.
    const Real xs[] = { -0.01, 0.0, 0.005 };
    const Real ys[] = { -0.002, 0.0, 0.004 };
    for (Time t : times)
        for (Real x : xs)
            for (Real y : ys) {
                Rate expected = x + y + process.phi(t);
                Rate actual = process.shortRate(t, x, y);
                if (std::fabs(actual - expected) > tol) {
                    BOOST_ERROR("G2Process::shortRate(" << t << ", " << x
                                << ", " << y << "): expected " << expected
                                << ", got " << actual);
                }
            }

    // The state starts at the origin, and r(0) = phi(0) = f(0,0).
    Array iv = process.initialValues();
    if (std::fabs(iv[0]) > tol || std::fabs(iv[1]) > tol) {
        BOOST_ERROR("initialValues should be (0,0), got ("
                    << iv[0] << ", " << iv[1] << ")");
    }
    Real expectedR0 = referencePhi(curve, 0.0, a, sigma, b, eta, rho);
    Rate r0 = process.shortRate(0.0, iv[0], iv[1]);
    if (std::fabs(r0 - expectedR0) > tol) {
        BOOST_ERROR("r(0) should equal phi(0)=f(0,0): " << r0
                    << " vs " << expectedR0);
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessPhiMatchesG2Model) {

    BOOST_TEST_MESSAGE(
        "Testing that G2Process::phi matches the G2 model fitting parameter...");

    const Real a = 0.12, sigma = 0.011, b = 0.17, eta = 0.009, rho = -0.3;
    Handle<YieldTermStructure> curve = makeFlatCurve(0.025);

    G2Process process(a, sigma, b, eta, rho, curve);
    G2 model(curve, a, sigma, b, eta, rho);
    ext::shared_ptr<TwoFactorModel::ShortRateDynamics> dyn = model.dynamics();
    BOOST_REQUIRE(dyn);

    const Real tol = 1e-12;
    for (Time t : {0.1, 0.5, 2.0, 7.5, 20.0}) {
        // dyn->shortRate(t, 0, 0) collapses to fitting_(t) = phi(t).
        Rate fromModel = dyn->shortRate(t, 0.0, 0.0);
        Rate fromProcess = process.phi(t);
        if (std::fabs(fromProcess - fromModel) > tol) {
            BOOST_ERROR("G2Process::phi disagrees with G2 model at t="
                        << t << ": process=" << fromProcess
                        << ", model=" << fromModel);
        }
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessExpectationConsistentWithCurve) {

    BOOST_TEST_MESSAGE(
        "Testing that G2Process expectation reproduces phi(t)...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.4;
    Handle<YieldTermStructure> curve = makeFlatCurve(0.035);

    G2Process process(a, sigma, b, eta, rho, curve);
    Array iv = process.initialValues();

    const Real tol = 1e-12;
    for (Time t : {0.1, 0.5, 2.0, 5.0, 10.0}) {
        // Both factors start at zero and are zero-mean, so the expected
        // short rate collapses to phi(t) - the curve-implied forward.
        Array exp_t = process.expectation(0.0, iv, t);
        Real expectedR = process.phi(t);
        Real actualR = process.shortRate(t, exp_t[0], exp_t[1]);
        if (std::fabs(actualR - expectedR) > tol) {
            BOOST_ERROR("E[r(t)] mismatch at t=" << t
                        << ": expected " << expectedR
                        << ", got " << actualR);
        }
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessExpectationOnPiecewiseForwardCurve) {

    BOOST_TEST_MESSAGE(
        "Testing G2Process short rate on a piecewise-flat forward curve...");

    /*  A curve bootstrapped on forward rates has a piecewise-constant
        instantaneous forward, so phi'(t) does not exist at the pillars.
        Nothing here differentiates phi, so the expected short rate stays
        exact on a grid that straddles them: stepping the factor
        expectation and adding phi has to reproduce phi(t) to machine
        precision, jumps included.
    */
    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.4;

    Date today = Settings::instance().evaluationDate();
    std::vector<Date> dates = { today, today + 1*Years, today + 2*Years,
                                today + 5*Years, today + 10*Years };
    std::vector<Rate> forwards = { 0.020, 0.020, 0.030, 0.035, 0.045 };

    Handle<YieldTermStructure> curve(
        ext::make_shared<ForwardCurve>(dates, forwards, Actual365Fixed(),
                                       TARGET()));

    G2Process process(a, sigma, b, eta, rho, curve);

    // A grid that lands on, just before, and just after the pillars.
    std::vector<Time> grid;
    for (Size i=1; i<dates.size(); ++i) {
        Time t = curve->timeFromReference(dates[i]);
        grid.push_back(t - 1.0e-5);
        grid.push_back(t);
        if (i+1 < dates.size())   // stay inside the curve's range
            grid.push_back(t + 1.0e-5);
    }

    const Real tol = 1e-12;
    Time t0 = 0.0;
    Array state = process.initialValues();
    for (Time t : grid) {
        state = process.expectation(t0, state, t - t0);
        t0 = t;
        Real expectedR = process.phi(t);
        Real actualR = process.shortRate(t, state[0], state[1]);
        if (std::fabs(actualR - expectedR) > tol) {
            BOOST_ERROR("stepped E[r(t)] mismatch at t=" << t
                        << ": expected " << expectedR
                        << ", got " << actualR);
        }
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessPhiRequiresTermStructure) {

    BOOST_TEST_MESSAGE(
        "Testing that G2Process::phi throws without a term structure...");

    G2Process process(0.1, 0.01, 0.2, 0.013, -0.5);
    BOOST_CHECK(process.termStructure().empty());

    // No curve means no fitted offset, hence no short rate either; the
    // factor dynamics remain usable on their own.
    BOOST_CHECK_THROW(process.phi(1.0), Error);
    BOOST_CHECK_THROW(process.shortRate(1.0, 0.01, 0.01), Error);
    BOOST_CHECK_NO_THROW(process.drift(1.0, process.initialValues()));

    Array iv = process.initialValues();
    if (std::fabs(iv[0]) > 1e-14 || std::fabs(iv[1]) > 1e-14) {
        BOOST_ERROR("initialValues should be (0,0), got ("
                    << iv[0] << ", " << iv[1] << ")");
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessObservesTermStructure) {

    BOOST_TEST_MESSAGE(
        "Testing that G2Process phi(t) updates when the curve moves...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.5;

    ext::shared_ptr<SimpleQuote> rate = ext::make_shared<SimpleQuote>(0.02);
    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(
        0, TARGET(), Handle<Quote>(rate), Actual365Fixed()));

    G2Process process(a, sigma, b, eta, rho, curve);

    const Time t = 2.0;
    Real phiBefore = process.phi(t);
    rate->setValue(0.05);
    Real phiAfter = process.phi(t);

    // Under a flat curve, increasing the instantaneous forward by 300bp
    // must raise phi by approximately the same amount.
    const Real expectedDelta = 0.03;
    if (std::fabs((phiAfter - phiBefore) - expectedDelta) > 1e-10) {
        BOOST_ERROR("G2Process did not respond to term-structure change: "
                    << "delta=" << (phiAfter - phiBefore)
                    << " (expected " << expectedDelta << ")");
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessPathGeneratorMatchesCurve) {

    BOOST_TEST_MESSAGE(
        "Testing that shortRatePath on G2Process reproduces "
        "the curve-implied short-rate expectation...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.3;
    Handle<YieldTermStructure> curve = makeFlatCurve(0.03);

    auto process = ext::make_shared<G2Process>(a, sigma, b, eta, rho, curve);

    const Time horizon = 5.0;
    const Size steps = 50;
    TimeGrid grid(horizon, steps);

    typedef PseudoRandom::rsg_type rsg_type;
    rsg_type rsg = PseudoRandom::make_sequence_generator(
        process->factors() * steps, 42);
    MultiPathGenerator<rsg_type> generator(process, grid, rsg, false);

    const Size nPaths = 20000;
    std::vector<Real> sumR(steps + 1, 0.0);
    for (Size n = 0; n < nPaths; ++n) {
        // shortRatePath adds phi(t) to the simulated factor pair, so the
        // caller never has to know about the offset.
        Path r = shortRatePath(*process, generator.next().value);
        BOOST_REQUIRE_EQUAL(r.length(), steps + 1);
        for (Size i = 0; i <= steps; ++i)
            sumR[i] += r[i];
    }

    /*  Empirical mean of r(t_i) should converge to phi(t_i). With 20k
        antithetic-free paths the MC standard error is comfortably below
        5e-4 for the parameters above.
    */
    for (Size i = 0; i <= steps; ++i) {
        Real meanR = sumR[i] / nPaths;
        Real expected = process->phi(grid[i]);
        if (std::fabs(meanR - expected) > 5e-4) {
            BOOST_ERROR("MC mean of r at t=" << grid[i]
                        << " is " << meanR
                        << ", expected " << expected
                        << " (diff " << (meanR - expected) << ")");
        }
    }

    // The path must carry the generator's time grid, and start at r(0).
    Path r = shortRatePath(*process, generator.next().value);
    BOOST_REQUIRE_EQUAL(r.timeGrid().size(), grid.size());
    for (Size i = 0; i < grid.size(); ++i)
        if (std::fabs(r.time(i) - grid[i]) > 1e-15)
            BOOST_ERROR("shortRatePath time grid differs at " << i << ": "
                        << r.time(i) << " vs " << grid[i]);
    if (std::fabs(r.front() - process->phi(0.0)) > 1e-12)
        BOOST_ERROR("short-rate path should start at phi(0): " << r.front()
                    << " vs " << process->phi(0.0));

    // A path with the wrong number of factors must be rejected.
    MultiPath wrongSize(3, grid);
    BOOST_CHECK_THROW(shortRatePath(*process, wrongSize), Error);
}

BOOST_AUTO_TEST_CASE(testG2ProcessOffsetNotIntegratedNumerically) {

    BOOST_TEST_MESSAGE(
        "Testing that the G2 offset is not integrated numerically across "
        "forward-curve pillars...");

    /*  Regression guard for the shifted-state formulation this process
        deliberately avoids.

        A curve bootstrapped on forward rates has a piecewise-constant
        instantaneous forward, so phi(t) jumps at every pillar and phi'(t)
        does not exist there. Had the state been shifted to z1 = x + phi(t),
        drift() would have had to supply a*phi(t) + phi'(t) with phi'
        finite-differenced, and stepping that drift gets the offset wrong in
        one of two ways:

          - a grid clear of the pillars never sees the jump, so the state
            only relaxes towards phi at rate a and lags it for ~1/a years;
          - a grid landing on a pillar sees jump/h instead, and overshoots
            by that times the step.

        Keeping phi out of the state avoids both: the factors stay at their
        zero mean and shortRate() adds the exact offset, jumps included.
    */
    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.4;

    Date today = Settings::instance().evaluationDate();
    std::vector<Date> dates = { today, today + 1*Years, today + 2*Years,
                                today + 5*Years, today + 10*Years };
    std::vector<Rate> forwards = { 0.020, 0.020, 0.030, 0.035, 0.045 };

    Handle<YieldTermStructure> curve(
        ext::make_shared<ForwardCurve>(dates, forwards, Actual365Fixed(),
                                       TARGET()));

    G2Process process(a, sigma, b, eta, rho, curve);

    // the offset drift the shifted-state formulation would have needed
    auto shiftedOffsetDrift = [&](Time t) {
        const Real h = 1.0e-4;
        return a*process.phi(t) + (process.phi(t+h) - process.phi(t))/h;
    };

    // one grid whose points land exactly on the pillars, one that stays
    // clear of them (0.03*k is never within h of a pillar time below 9.9)
    std::vector<Time> pillars;
    for (Size i=1; i+1<dates.size(); ++i)
        pillars.push_back(curve->timeFromReference(dates[i]));
    std::vector<TimeGrid> grids = { TimeGrid(pillars.begin(), pillars.end(), 20),
                                    TimeGrid(9.9, 330) };
    const char* labels[] = { "aligned with the pillars", "clear of the pillars" };

    for (Size g=0; g<grids.size(); ++g) {
        const TimeGrid& grid = grids[g];

        Array state = process.initialValues();     // the zero-mean factors
        Real shifted = process.phi(0.0);           // z1(0) under the shift
        Real worstShifted = 0.0;

        for (Size i=1; i<grid.size(); ++i) {
            Time t0 = grid[i-1], t = grid[i], dt = t - t0;

            // Euler step of each formulation, with the Brownian increment
            // set to zero so that both track their own expectation
            state = state + process.drift(t0, state)*dt;
            shifted += (-a*shifted + shiftedOffsetDrift(t0))*dt;

            Real exact = process.phi(t);

            // the offset is added analytically, so this is exact for any dt
            Real actual = process.shortRate(t, state[0], state[1]);
            if (std::fabs(actual - exact) > 1e-12) {
                BOOST_ERROR("E[r(t)] should equal phi(t) at t=" << t
                            << " on a grid " << labels[g] << ": got "
                            << actual << ", expected " << exact);
            }

            worstShifted = std::max(worstShifted, std::fabs(shifted - exact));
        }

        BOOST_TEST_MESSAGE("    grid " << labels[g]
                           << ": shifted-state Euler off by up to "
                           << worstShifted*10000.0 << " bp");
        if (worstShifted < 0.01) {
            BOOST_ERROR("the shifted-state scheme was expected to be off by "
                        "at least 100bp on a grid " << labels[g]
                        << ", got " << worstShifted*10000.0 << " bp; if the "
                        "finite difference has become harmless, this guard "
                        "no longer proves anything");
        }
    }
}

BOOST_AUTO_TEST_CASE(testG2ShortRatePathBuilder) {

    BOOST_TEST_MESSAGE(
        "Testing that G2ShortRatePathBuilder matches shortRatePath...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.3;
    Handle<YieldTermStructure> curve = makeFlatCurve(0.03);

    auto process = ext::make_shared<G2Process>(a, sigma, b, eta, rho, curve);

    const Size steps = 25;
    TimeGrid grid(5.0, steps);

    typedef PseudoRandom::rsg_type rsg_type;
    rsg_type rsg = PseudoRandom::make_sequence_generator(
        process->factors() * steps, 17);
    MultiPathGenerator<rsg_type> generator(process, grid, rsg, false);

    G2ShortRatePathBuilder shortRate(*process, grid);

    // the cached offsets are exactly phi on the grid
    for (Size i=0; i<grid.size(); ++i) {
        if (std::fabs(shortRate.offsets()[i] - process->phi(grid[i])) > 1e-15) {
            BOOST_ERROR("cached offset at t=" << grid[i] << " is "
                        << shortRate.offsets()[i] << ", expected "
                        << process->phi(grid[i]));
        }
    }

    for (Size n=0; n<100; ++n) {
        const MultiPath& mp = generator.next().value;
        Path cached = shortRate(mp);
        Path direct = shortRatePath(*process, mp);
        BOOST_REQUIRE_EQUAL(cached.length(), direct.length());
        for (Size i=0; i<direct.length(); ++i) {
            if (std::fabs(cached[i] - direct[i]) > 1e-15) {
                BOOST_ERROR("cached and direct short-rate paths differ at "
                            << i << ": " << cached[i] << " vs " << direct[i]);
            }
        }
    }

    // a path drawn on a different grid must be rejected
    MultiPath wrongLength(2, TimeGrid(5.0, steps+1));
    BOOST_CHECK_THROW(shortRate(wrongLength), Error);
}

BOOST_AUTO_TEST_CASE(testG2ForwardProcessPhiAndShortRate) {

    BOOST_TEST_MESSAGE(
        "Testing G2ForwardProcess phi(t) and shortRate(t, x, y)...");

    const Real a = 0.1, sigma = 0.01, b = 0.2, eta = 0.013, rho = -0.5;
    Handle<YieldTermStructure> curve = makeFlatCurve(0.035);

    G2ForwardProcess fwd(a, sigma, b, eta, rho, curve);

    const Real tol = 1e-12;
    for (Time t : {0.25, 1.0, 5.0}) {
        Real expected =
            referencePhi(curve, t, a, sigma, b, eta, rho);
        Real actual = fwd.phi(t);
        if (std::fabs(actual - expected) > tol) {
            BOOST_ERROR("G2ForwardProcess::phi mismatch at t=" << t
                        << ": expected " << expected << ", got " << actual);
        }

        // shortRate adds the offset to the simulated factors.
        Rate sr = fwd.shortRate(t, 0.002, -0.001);
        if (std::fabs(sr - (0.001 + expected)) > tol) {
            BOOST_ERROR("G2ForwardProcess::shortRate mismatch at t=" << t
                        << ": expected " << (0.001 + expected)
                        << ", got " << sr);
        }
    }

    // Without a curve there is no offset, hence no short rate.
    G2ForwardProcess paramOnly(a, sigma, b, eta, rho);
    BOOST_CHECK(paramOnly.termStructure().empty());
    BOOST_CHECK_THROW(paramOnly.phi(1.0), Error);
    BOOST_CHECK_THROW(paramOnly.shortRate(1.0, 0.01, 0.01), Error);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
