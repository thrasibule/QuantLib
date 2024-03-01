/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2015 Peter Caspers

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

/*! \file swaptionhelper.hpp
    \brief Swaption calibration helper
*/

#ifndef quantlib_swaption_calibration_helper_hpp
#define quantlib_swaption_calibration_helper_hpp

#include <ql/models/calibrationhelper.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/cashflows/rateaveraging.hpp>

namespace QuantLib {

    //! calibration helper for ATM swaption
    class FixedVsFloatingSwaptionHelper : public BlackCalibrationHelper {
      public:
        FixedVsFloatingSwaptionHelper(const Period& maturity,
                                      const Period& length,
                                      const Handle<Quote>& volatility,
                                      ext::shared_ptr<IborIndex> index,
                                      const Period& fixedLegTenor,
                                      DayCounter fixedLegDayCounter,
                                      DayCounter floatingLegDayCounter,
                                      Handle<YieldTermStructure> termStructure,
                                      BlackCalibrationHelper::CalibrationErrorType errorType =
                                      BlackCalibrationHelper::RelativePriceError,
                                      Real strike = Null<Real>(),
                                      Real nominal = 1.0,
                                      VolatilityType type = ShiftedLognormal,
                                      Real shift = 0.0,
                                      Natural settlementDays = Null<Size>());

        FixedVsFloatingSwaptionHelper(const Date& exerciseDate,
                                      const Period& length,
                                      const Handle<Quote>& volatility,
                                      ext::shared_ptr<IborIndex> index,
                                      const Period& fixedLegTenor,
                                      DayCounter fixedLegDayCounter,
                                      DayCounter floatingLegDayCounter,
                                      Handle<YieldTermStructure> termStructure,
                                      BlackCalibrationHelper::CalibrationErrorType errorType =
                                      BlackCalibrationHelper::RelativePriceError,
                                      Real strike = Null<Real>(),
                                      Real nominal = 1.0,
                                      VolatilityType type = ShiftedLognormal,
                                      Real shift = 0.0,
                                      Natural settlementDays = Null<Size>());

        FixedVsFloatingSwaptionHelper(const Date& exerciseDate,
                                      const Date& endDate,
                                      const Handle<Quote>& volatility,
                                      ext::shared_ptr<IborIndex> index,
                                      const Period& fixedLegTenor,
                                      DayCounter fixedLegDayCounter,
                                      DayCounter floatingLegDayCounter,
                                      Handle<YieldTermStructure> termStructure,
                                      BlackCalibrationHelper::CalibrationErrorType errorType =
                                      BlackCalibrationHelper::RelativePriceError,
                                      Real strike = Null<Real>(),
                                      Real nominal = 1.0,
                                      VolatilityType type = ShiftedLognormal,
                                      Real shift = 0.0,
                                      Natural settlementDays = Null<Size>());

        void addTimesTo(std::list<Time>& times) const override;
        Real modelValue() const override;
        Real blackPrice(Volatility volatility) const override;
        ext::shared_ptr<Swaption> swaption() const { calculate(); return swaption_; }
        ext::shared_ptr<FixedVsFloatingSwap> underlyingSwap() const {
            calculate();
            return swap_;
        }
    protected:
        virtual ext::shared_ptr<FixedVsFloatingSwap> makeSwap(const Swap::Type type, Rate fixedRate) const = 0;

        void performCalculations() const override;
        const Date exerciseDate_;
        mutable Date startDate_;
        mutable Date endDate_;
        const Period fixedLegTenor_;
        const ext::shared_ptr<IborIndex> index_;
        const Handle<YieldTermStructure> termStructure_;
        const DayCounter fixedLegDayCounter_, floatingLegDayCounter_;
        const Real strike_, nominal_;
        mutable Rate exerciseRate_;
        mutable ext::shared_ptr<FixedVsFloatingSwap> swap_;
        mutable ext::shared_ptr<Swaption> swaption_;
        const Natural settlementDays_;
    };

    class SwaptionHelper: public FixedVsFloatingSwaptionHelper {
        using FixedVsFloatingSwaptionHelper::FixedVsFloatingSwaptionHelper;
      private:
        ext::shared_ptr<FixedVsFloatingSwap> makeSwap(const Swap::Type type, Rate fixedRate) const override;
    };

    class OvernightIndexedSwaptionHelper: public FixedVsFloatingSwaptionHelper {
      public:
         OvernightIndexedSwaptionHelper(const Period& maturity,
                                        const Period& length,
                                        const Handle<Quote>& volatility,
                                        ext::shared_ptr<OvernightIndex> index,
                                        const Period& fixedLegTenor,
                                        DayCounter fixedLegDayCounter,
                                        DayCounter floatingLegDayCounter,
                                        Handle<YieldTermStructure> termStructure,
                                        BlackCalibrationHelper::CalibrationErrorType errorType =
                                        BlackCalibrationHelper::RelativePriceError,
                                        Real strike = Null<Real>(),
                                        Real nominal = 1.0,
                                        VolatilityType type = ShiftedLognormal,
                                        Real shift = 0.0,
                                        Natural settlementDays = Null<Size>(),
                                        RateAveraging::Type averagingMethod = RateAveraging::Compound);

         OvernightIndexedSwaptionHelper(const Date& exerciseDate,
                                        const Period& length,
                                        const Handle<Quote>& volatility,
                                        ext::shared_ptr<OvernightIndex> index,
                                        const Period& fixedLegTenor,
                                        DayCounter fixedLegDayCounter,
                                        DayCounter floatingLegDayCounter,
                                        Handle<YieldTermStructure> termStructure,
                                        BlackCalibrationHelper::CalibrationErrorType errorType =
                                        BlackCalibrationHelper::RelativePriceError,
                                        Real strike = Null<Real>(),
                                        Real nominal = 1.0,
                                        VolatilityType type = ShiftedLognormal,
                                        Real shift = 0.0,
                                        Natural settlementDays = Null<Size>(),
                                        RateAveraging::Type averagingMethod = RateAveraging::Compound);

        OvernightIndexedSwaptionHelper(const Date& exerciseDate,
                                       const Date& endDate,
                                       const Handle<Quote>& volatility,
                                       ext::shared_ptr<OvernightIndex> index,
                                       const Period& fixedLegTenor,
                                       DayCounter fixedLegDayCounter,
                                       DayCounter floatingLegDayCounter,
                                       Handle<YieldTermStructure> termStructure,
                                       BlackCalibrationHelper::CalibrationErrorType errorType =
                                       BlackCalibrationHelper::RelativePriceError,
                                       Real strike = Null<Real>(),
                                       Real nominal = 1.0,
                                       VolatilityType type = ShiftedLognormal,
                                       Real shift = 0.0,
                                       Natural settlementDays = Null<Size>(),
                                       RateAveraging::Type averagingMethod = RateAveraging::Compound);
      private:
        ext::shared_ptr<FixedVsFloatingSwap> makeSwap(const Swap::Type type, Rate fixedRate) const override;
        const ext::shared_ptr<OvernightIndex> index_;
        RateAveraging::Type averagingMethod_;
    };
}

#endif
