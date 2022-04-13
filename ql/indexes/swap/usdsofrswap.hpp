/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022 Guillaume Horel

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file usdsofrswap.hpp
    \brief %USD %Sofr %Swap indexes
*/

#ifndef quantlib_usdsofrswap_hpp
#define quantlib_usdsofrswap_hpp

#include <ql/indexes/swapindex.hpp>

namespace QuantLib {

    //! %UsdSofrSwapIceFix index base class
    /*! %USD %Sofr %Swap indexes fixed by ICE Benchmark Administration Limited
        at 11am New York.
        Annual ACT/360 vs Annual ACT/360.

        Further info can be found at <https://www.theice.com/iba/ice-swap-rate>

    */
    class UsdSofrSwapIceFix : public OvernightIndexedSwapIndex {
      public:
        UsdSofrSwapIceFix(const Period& tenor,
                          const Handle<YieldTermStructure>& h =
                          Handle<YieldTermStructure>());

    };
}

#endif
