/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2007 StatPro Italia srl
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

#include <ql/indexes/iborindex.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/pricingengines/swaption/discretizedswaption.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/schedule.hpp>
#include <utility>

namespace QuantLib {
    FixedVsFloatingSwaptionHelper::FixedVsFloatingSwaptionHelper(
                                   const Period& maturity,
                                   const Period& length,
                                   const Handle<Quote>& volatility,
                                   ext::shared_ptr<IborIndex> index,
                                   const Period& fixedLegTenor,
                                   DayCounter fixedLegDayCounter,
                                   DayCounter floatingLegDayCounter,
                                   Handle<YieldTermStructure> termStructure,
                                   BlackCalibrationHelper::CalibrationErrorType errorType,
                                   const Real strike,
                                   const Real nominal,
                                   const VolatilityType type,
                                   const Real shift,
                                   const Natural settlementDays) :
        FixedVsFloatingSwaptionHelper(index->fixingCalendar().advance(termStructure->referenceDate(),
                                                                      maturity,
                                                                      index->businessDayConvention()),
                                      length,
                                      volatility,
                                      index,
                                      fixedLegTenor,
                                      fixedLegDayCounter,
                                      floatingLegDayCounter,
                                      termStructure,
                                      errorType,
                                         strike,
                                         nominal,
                                      type,
                                      shift,
                                      settlementDays) {}
    FixedVsFloatingSwaptionHelper::FixedVsFloatingSwaptionHelper(
                                   const Date& exerciseDate,
                                   const Period& length,
                                   const Handle<Quote>& volatility,
                                   ext::shared_ptr<IborIndex> index,
                                   const Period& fixedLegTenor,
                                   DayCounter fixedLegDayCounter,
                                   DayCounter floatingLegDayCounter,
                                   Handle<YieldTermStructure> termStructure,
                                   BlackCalibrationHelper::CalibrationErrorType errorType,
                                   const Real strike,
                                   const Real nominal,
                                   const VolatilityType type,
                                   const Real shift,
                                   const Natural settlementDays) :
        FixedVsFloatingSwaptionHelper(exerciseDate,
                                      Null<Date>(),
                                      volatility,
                                      index,
                                      fixedLegTenor,
                                      fixedLegDayCounter,
                                      floatingLegDayCounter,
                                      termStructure,
                                      errorType,
                                      strike,
                                      nominal,
                                      type,
                                      shift,
                                      settlementDays) {
        endDate_ = index->fixingCalendar().advance(startDate_, length, index->businessDayConvention());
    }
    FixedVsFloatingSwaptionHelper::FixedVsFloatingSwaptionHelper(
                                   const Date& exerciseDate,
                                   const Date& endDate,
                                   const Handle<Quote>& volatility,
                                   ext::shared_ptr<IborIndex> index,
                                   const Period& fixedLegTenor,
                                   DayCounter fixedLegDayCounter,
                                   DayCounter floatingLegDayCounter,
                                   Handle<YieldTermStructure> termStructure,
                                   BlackCalibrationHelper::CalibrationErrorType errorType,
                                   const Real strike,
                                   const Real nominal,
                                   const VolatilityType type,
                                   const Real shift,
                                   const Natural settlementDays)
    : BlackCalibrationHelper(volatility, errorType, type, shift), exerciseDate_(exerciseDate),
      endDate_(endDate),
      fixedLegTenor_(fixedLegTenor),
      index_(std::move(index)),
      termStructure_(std::move(termStructure)),
      fixedLegDayCounter_(std::move(fixedLegDayCounter)),
      floatingLegDayCounter_(std::move(floatingLegDayCounter)),
      strike_(strike), nominal_(nominal),
      settlementDays_(settlementDays) {
        startDate_ = settlementDays_ == Null<Size>() ?
            index_->valueDate(exerciseDate_) : index_->fixingCalendar().advance(exerciseDate_, settlementDays_, Days, index_->businessDayConvention());
        registerWith(index_);
        registerWith(termStructure_);
    }
    void FixedVsFloatingSwaptionHelper::addTimesTo(std::list<Time>& times) const {
        calculate();
        Swaption::arguments args;
        swaption_->setupArguments(&args);
        std::vector<Time> swaptionTimes =
            DiscretizedSwaption(args,
                                termStructure_->referenceDate(),
                                termStructure_->dayCounter()).mandatoryTimes();
        times.insert(times.end(),
                     swaptionTimes.begin(), swaptionTimes.end());
    }
    Real FixedVsFloatingSwaptionHelper::modelValue() const {
        calculate();
        swaption_->setPricingEngine(engine_);
        return swaption_->NPV();
    }
    Real FixedVsFloatingSwaptionHelper::blackPrice(Volatility sigma) const {
        calculate();
        Handle<Quote> vol(ext::shared_ptr<Quote>(new SimpleQuote(sigma)));
        ext::shared_ptr<PricingEngine> engine;
        switch(volatilityType_) {
        case ShiftedLognormal:
            engine = ext::make_shared<BlackSwaptionEngine>(
                termStructure_, vol, Actual365Fixed(), shift_);
            break;
        case Normal:
            engine = ext::make_shared<BachelierSwaptionEngine>(
                termStructure_, vol, Actual365Fixed());
            break;
        default:
            QL_FAIL("can not construct engine: " << volatilityType_);
            break;
        }
        swaption_->setPricingEngine(engine);
        Real value = swaption_->NPV();
        swaption_->setPricingEngine(engine_);
        return value;
    }
    void FixedVsFloatingSwaptionHelper::performCalculations() const {
        ext::shared_ptr<PricingEngine> swapEngine = ext::make_shared<DiscountingSwapEngine>(termStructure_, false);

        ext::shared_ptr<FixedVsFloatingSwap> temp = makeSwap(Swap::Receiver, 0.0);
        temp->setPricingEngine(swapEngine);
        Real forward = temp->fairRate();
        Swap::Type type = Swap::Receiver;
        if (strike_ == Null<Real>()) {
            exerciseRate_ = forward;
        } else {
            exerciseRate_ = strike_;
            type = strike_ <= forward ? Swap::Receiver : Swap::Payer;
        }
        swap_ = makeSwap(type, exerciseRate_);
        swap_->setPricingEngine(swapEngine);
        ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(exerciseDate_);
        swaption_ = ext::make_shared<Swaption>(swap_, exercise);
        BlackCalibrationHelper::performCalculations();
    }
    ext::shared_ptr<FixedVsFloatingSwap> SwaptionHelper::makeSwap(const Swap::Type type, Rate fixedRate) const {
        const Schedule fixedSchedule(startDate_, endDate_, fixedLegTenor_,
                                     index_->fixingCalendar(),
                                     index_->businessDayConvention(),
                                     index_->businessDayConvention(),
                                     DateGeneration::Forward, false);
        const Schedule floatSchedule(startDate_, endDate_, index_->tenor(),
                                     index_->fixingCalendar(),
                                     index_->businessDayConvention(),
                                     index_->businessDayConvention(),
                                     DateGeneration::Forward, false);
        return ext::make_shared<VanillaSwap>(
                                             type, nominal_, fixedSchedule, fixedRate, fixedLegDayCounter_,
                                             floatSchedule, index_, 0.0, floatingLegDayCounter_
                                             );
    }
    OvernightIndexedSwaptionHelper::OvernightIndexedSwaptionHelper(
                                   const Period& maturity,
                                   const Period& length,
                                   const Handle<Quote>& volatility,
                                   ext::shared_ptr<OvernightIndex> index,
                                   const Period& fixedLegTenor,
                                   DayCounter fixedLegDayCounter,
                                   DayCounter floatingLegDayCounter,
                                   Handle<YieldTermStructure> termStructure,
                                   BlackCalibrationHelper::CalibrationErrorType errorType,
                                   const Real strike,
                                   const Real nominal,
                                   const VolatilityType type,
                                   const Real shift,
                                   const Natural settlementDays,
                                   RateAveraging::Type averagingMethod) :
        FixedVsFloatingSwaptionHelper(maturity,
                                      length,
                                      volatility,
                                      index,
                                      fixedLegTenor,
                                      fixedLegDayCounter,
                                      floatingLegDayCounter,
                                      termStructure,
                                      errorType,
                                      strike,
                                      nominal,
                                      type,
                                      shift,
                                      settlementDays), averagingMethod_(averagingMethod) {}
     OvernightIndexedSwaptionHelper::OvernightIndexedSwaptionHelper(const Date& exerciseDate,
                                    const Period& length,
                                    const Handle<Quote>& volatility,
                                    ext::shared_ptr<OvernightIndex> index,
                                    const Period& fixedLegTenor,
                                    DayCounter fixedLegDayCounter,
                                    DayCounter floatingLegDayCounter,
                                    Handle<YieldTermStructure> termStructure,
                                    BlackCalibrationHelper::CalibrationErrorType errorType,
                                    const Real strike,
                                    const Real nominal,
                                    const VolatilityType type,
                                    const Real shift,
                                    const Natural settlementDays,
                                    RateAveraging::Type averagingMethod) :
         FixedVsFloatingSwaptionHelper(exerciseDate,
                                       length,
                                       volatility,
                                       index,
                                       fixedLegTenor,
                                       fixedLegDayCounter,
                                       floatingLegDayCounter,
                                       termStructure,
                                       errorType,
                                       strike,
                                       nominal,
                                       type,
                                       shift,
                                       settlementDays), averagingMethod_(averagingMethod) {}
    OvernightIndexedSwaptionHelper::OvernightIndexedSwaptionHelper(const Date& exerciseDate,
                                    const Date& endDate,
                                    const Handle<Quote>& volatility,
                                    ext::shared_ptr<OvernightIndex> index,
                                    const Period& fixedLegTenor,
                                    DayCounter fixedLegDayCounter,
                                    DayCounter floatingLegDayCounter,
                                    Handle<YieldTermStructure> termStructure,
                                    BlackCalibrationHelper::CalibrationErrorType errorType,
                                    const Real strike,
                                    const Real nominal,
                                    const VolatilityType type,
                                    const Real shift,
                                    const Natural settlementDays,
                                    RateAveraging::Type averagingMethod) :
         FixedVsFloatingSwaptionHelper(exerciseDate,
                                       endDate,
                                       volatility,
                                       index,
                                       fixedLegTenor,
                                       fixedLegDayCounter,
                                       floatingLegDayCounter,
                                       termStructure,
                                       errorType,
                                       strike,
                                       nominal,
                                       type,
                                       shift,
                                       settlementDays), averagingMethod_(averagingMethod) {}
    ext::shared_ptr<FixedVsFloatingSwap> OvernightIndexedSwaptionHelper::makeSwap(const Swap::Type type, Rate fixedRate) const {
        const Schedule fixedSchedule(startDate_, endDate_, fixedLegTenor_,
                                     index_->fixingCalendar(),
                                     index_->businessDayConvention(),
                                     index_->businessDayConvention(),
                                     DateGeneration::Forward, false);
        const Schedule overnightSchedule(startDate_, endDate_, Period(Annual),
                                         index_->fixingCalendar(),
                                         index_->businessDayConvention(),
                                         index_->businessDayConvention(),
                                         DateGeneration::Forward, false);
        return ext::make_shared<OvernightIndexedSwap>(

                                                      type, nominal_, fixedSchedule, fixedRate, fixedLegDayCounter_, overnightSchedule, index_, 0.0, 0, Following, Calendar(), true, averagingMethod_
                                                      );
    }
}
