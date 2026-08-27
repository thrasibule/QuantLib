/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Banca Profilo S.p.A.

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

/*! \file g2process.hpp
    \brief G2 stochastic processes
*/

#ifndef quantlib_g2_process_hpp
#define quantlib_g2_process_hpp

#include <ql/methods/montecarlo/multipath.hpp>
#include <ql/processes/forwardmeasureprocess.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantLib {

    //! %G2 stochastic process
    /*! Simulates the pair of zero-mean Ornstein-Uhlenbeck factors
        \f$ (x, y) \f$ of the two-factor G2++ model. The fitted short rate
        is recovered by adding the deterministic offset that matches the
        initial term structure,
        \f[ r(t) = x(t) + y(t) + \varphi(t), \f]
        which is what shortRate() does; shortRatePath() applies it to a
        whole sample path, so callers of a MultiPathGenerator built on this
        process need not add \f$ \varphi \f$ themselves.

        Keeping \f$ \varphi \f$ out of the simulated state makes
        initialValues(), drift() and expectation() exact for any curve.
        Were the state shifted so that its two components summed to
        \f$ r(t) \f$ instead, drift() would need \f$ \varphi'(t) \f$ -
        hence the slope of the initial instantaneous forward curve, which no
        YieldTermStructure exposes analytically, and which does not exist at
        all for the piecewise-flat forward curves in common use.

        The term structure is only needed to evaluate \f$ \varphi \f$; the
        factor dynamics do not depend on it. With an empty handle the
        process is still a well-defined zero-mean OU pair, but phi(),
        shortRate() and shortRatePath() then throw.

        \ingroup processes
    */
    class G2Process : public StochasticProcess {
      public:
        G2Process(Real a, Real sigma, Real b, Real eta, Real rho,
                  const Handle<YieldTermStructure>& termStructure = {});
        //! \name StochasticProcess interface
        //@{
        Size size() const override;
        Array initialValues() const override;
        Array drift(Time t, const Array& x) const override;
        Matrix diffusion(Time t, const Array& x) const override;
        Array expectation(Time t0, const Array& x0, Time dt) const override;
        Matrix stdDeviation(Time t0, const Array& x0, Time dt) const override;
        Matrix covariance(Time t0, const Array& x0, Time dt) const override;
        //@}
        Real x0() const;
        Real y0() const;
        Real a() const;
        Real sigma() const;
        Real b() const;
        Real eta() const;
        Real rho() const;
        const Handle<YieldTermStructure>& termStructure() const;
        //! deterministic offset fitting the initial term structure
        Real phi(Time t) const;
        //! short rate implied by the factor pair, \f$ x + y + \varphi(t) \f$
        Rate shortRate(Time t, Real x, Real y) const;
      private:
        Real x0_ = 0.0, y0_ = 0.0, a_, sigma_, b_, eta_, rho_;
        ext::shared_ptr<QuantLib::OrnsteinUhlenbeckProcess> xProcess_;
        ext::shared_ptr<QuantLib::OrnsteinUhlenbeckProcess> yProcess_;
        Handle<YieldTermStructure> termStructure_;
    };

    //! %Forward %G2 stochastic process
    /*! Forward-measure counterpart of G2Process.
        The simulated state is again the zero-mean factor pair, with the
        usual T-forward convexity adjustments to the drift and to the
        conditional expectation; shortRate() and shortRatePath() add
        \f$ \varphi(t) \f$ as they do for G2Process.

        \ingroup processes
    */
    class G2ForwardProcess : public ForwardMeasureProcess {
      public:
        G2ForwardProcess(Real a, Real sigma, Real b, Real eta, Real rho,
                         const Handle<YieldTermStructure>& termStructure = {});
        //! \name StochasticProcess interface
        //@{
        Size size() const override;
        Array initialValues() const override;
        Array drift(Time t, const Array& x) const override;
        Matrix diffusion(Time t, const Array& x) const override;
        Array expectation(Time t0, const Array& x0, Time dt) const override;
        Matrix stdDeviation(Time t0, const Array& x0, Time dt) const override;
        Matrix covariance(Time t0, const Array& x0, Time dt) const override;
        //@}
        const Handle<YieldTermStructure>& termStructure() const;
        //! deterministic offset fitting the initial term structure
        Real phi(Time t) const;
        //! short rate implied by the factor pair, \f$ x + y + \varphi(t) \f$
        Rate shortRate(Time t, Real x, Real y) const;
      protected:
        Real x0_ = 0.0, y0_ = 0.0, a_, sigma_, b_, eta_, rho_;
        ext::shared_ptr<QuantLib::OrnsteinUhlenbeckProcess> xProcess_;
        ext::shared_ptr<QuantLib::OrnsteinUhlenbeckProcess> yProcess_;
        Handle<YieldTermStructure> termStructure_;
        Real xForwardDrift(Time t, Time T) const;
        Real yForwardDrift(Time t, Time T) const;
        Real Mx_T(Real s, Real t, Real T) const;
        Real My_T(Real s, Real t, Real T) const;
    };

    /*! \relates G2Process
        Short-rate path implied by a two-factor sample path: given a
        MultiPath of the factor pair \f$ (x, y) \f$ drawn from a generator
        built on the process, returns the path of
        \f$ r(t_i) = x(t_i) + y(t_i) + \varphi(t_i) \f$ on the same time
        grid. This is the curve-consistent short-rate path; its expectation
        over many draws is \f$ \varphi(t_i) \f$.

        \note \f$ \varphi \f$ is re-evaluated on every call. When
              converting many paths drawn on one time grid, hoisting
              phi() onto the grid yourself is cheaper.
    */
    Path shortRatePath(const G2Process& process, const MultiPath& path);

    /*! \relates G2ForwardProcess */
    Path shortRatePath(const G2ForwardProcess& process, const MultiPath& path);

    //! %G2 short-rate path builder caching the offset on a time grid
    /*! Same conversion as shortRatePath(), with \f$ \varphi(t_i) \f$
        evaluated once per grid instead of once per path. Use it when many
        sample paths are drawn on a single time grid, as in a Monte Carlo
        run: the term structure is then queried
        \f$ \mathrm{grid\ size} \f$ times in total rather than
        \f$ \mathrm{grid\ size} \times \mathrm{paths} \f$ times.

        \code
        G2ShortRatePathBuilder shortRate(process, grid);
        for (Size n=0; n<samples; ++n) {
            Path r = shortRate(generator.next().value);
            ...
        }
        \endcode

        The paths passed to operator() must have been drawn on the grid the
        builder was constructed with; only their length is checked.
    */
    class G2ShortRatePathBuilder {
      public:
        /*! \param process either G2Process or G2ForwardProcess
            \param grid    the grid the sample paths will be drawn on
        */
        template <class G2ProcessType>
        G2ShortRatePathBuilder(const G2ProcessType& process, TimeGrid grid)
        : grid_(std::move(grid)), offsets_(grid_.size()) {
            for (Size i=0; i<grid_.size(); ++i)
                offsets_[i] = process.phi(grid_[i]);
        }
        //! short-rate path implied by a factor-pair sample path
        Path operator()(const MultiPath& path) const;
        //! the cached offsets \f$ \varphi(t_i) \f$
        const Array& offsets() const { return offsets_; }
      private:
        TimeGrid grid_;
        Array offsets_;
    };

}


#endif

