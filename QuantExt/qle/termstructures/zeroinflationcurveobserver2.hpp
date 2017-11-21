/*
Copyright (C) 2016 Quaternion Risk Management Ltd
All rights reserved.

This file is part of ORE, a free-software/open-source library
for transparent pricing and risk analysis - http://opensourcerisk.org

ORE is free software: you can redistribute it and/or modify it
under the terms of the Modified BSD License.  You should have received a
copy of the license along with this program.
The license is also available online at <http://opensourcerisk.org>

This program is distributed on the basis that it will form a useful
contribution to risk analytics and model standardisation, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file zeroinflationcurve2.hpp
    \brief Observable inflation term structure based on the interpolation of zero rate quotes,
           but with floating reference date
*/

#ifndef quantext_zero_inflation_curve_observer2_hpp
#define quantext_zero_inflation_curve_observer2_hpp

#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/comparison.hpp>
#include <ql/patterns/lazyobject.hpp>

namespace QuantLib {

//! Inflation term structure based on the interpolation of zero rates, with floating reference date
/*! \ingroup inflationtermstructures */
template<class Interpolator>
class ZeroInflationCurveObserver2
    : public ZeroInflationTermStructure,
    protected InterpolatedCurve<Interpolator>,
    public LazyObject {
public:
    ZeroInflationCurveObserver2(Natural settlementDays,
        const Calendar& calendar,
        const DayCounter& dayCounter,
        const Period& lag,
        Frequency frequency,
        bool indexIsInterpolated,
        const Handle<YieldTermStructure>& yTS,
        const std::vector<Time>& times,
        const std::vector<Handle<Quote> >& rates,
        const boost::shared_ptr<Seasonality> &seasonality = boost::shared_ptr<Seasonality>(),
        const Interpolator &interpolator = Interpolator());

    //! \name InflationTermStructure interface
    //@{
    Date baseDate() const;
    Time maxTime() const;
    Date maxDate() const;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<Time>& times() const;
    const std::vector<Real>& data() const;
    const std::vector<Rate>& rates() const;
    //std::vector<std::pair<Time, Rate> > nodes() const;
    const std::vector<Handle<Quote> >& quotes() const { return quotes_; };
    //@}

    //! \name Observer interface
    //@{
    void update();
    //@}

private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}


protected:
    //! \name ZeroInflationTermStructure Interface
    //@{
    Rate zeroRateImpl(Time t) const;
    //@}
    std::vector<Handle<Quote> > quotes_;
    mutable Date baseDate_;

};

// template definitions

template <class Interpolator>
ZeroInflationCurveObserver2<Interpolator>::
    ZeroInflationCurveObserver2(Natural settlementDays,
        const Calendar& calendar,
        const DayCounter& dayCounter,
        const Period& lag,
        Frequency frequency,
        bool indexIsInterpolated,
        const Handle<YieldTermStructure>& yTS,
        const std::vector<Time>& times,
        const std::vector<Handle<Quote> >& rates,
        const boost::shared_ptr<Seasonality> &seasonality,
        const Interpolator& interpolator)
    : ZeroInflationTermStructure(settlementDays, calendar, dayCounter, rates[0]->value(),
        lag, frequency, indexIsInterpolated, yTS, seasonality),
    InterpolatedCurve<Interpolator>(std::vector<Time>(), std::vector<Real>(), interpolator),
    quotes_(rates) {

    QL_REQUIRE(times.size() > 1, "too few times: " << times.size());
    this->times_.resize(times.size());
    this->times_[0] = times[0];
    for (Size i = 1; i < times.size(); i++) {
        QL_REQUIRE(times[i] > times[i - 1], "times not sorted");
        this->times_[i] = times[i];
    }

    QL_REQUIRE(this->quotes_.size() == this->times_.size(),
        "quotes/times count mismatch: "
        << this->quotes_.size() << " vs " << this->times_.size());

    // initalise data vector, values are copied from quotes in performCalculations()
    this->data_.resize(this->times_.size());
    for (Size i = 0; i < this->times_.size(); i++)
        this->data_[0] = 0.0;

    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(),
            this->times_.end(),
            this->data_.begin());
    this->interpolation_.update();

    // register with each of the quotes
    for (Size i = 0; i < this->quotes_.size(); i++)
        registerWith(this->quotes_[i]);
}

template <class T>
Date ZeroInflationCurveObserver2<T>::baseDate() const {
    // if indexIsInterpolated we fixed the dates in the constructor
    calculate();
    return baseDate_;
}

template <class T>
Time ZeroInflationCurveObserver2<T>::maxTime() const {
    return times_.back();
}

template <class T>
Date ZeroInflationCurveObserver2<T>::maxDate() const {
    return this->maxDate_;
}

template <class T>
inline Rate ZeroInflationCurveObserver2<T>::zeroRateImpl(Time t) const {
    calculate();
    return this->interpolation_(t, true);
}

template <class T>
inline const std::vector<Time>&
    ZeroInflationCurveObserver2<T>::times() const {
    return this->times_;
}

template <class T>
inline const std::vector<Rate>&
    ZeroInflationCurveObserver2<T>::rates() const {
    calculate();
    return this->data_;
}

template <class T>
inline const std::vector<Real>&
    ZeroInflationCurveObserver2<T>::data() const {
    calculate();
    return this->data_;
}
//
//template <class T>
//inline std::vector<std::pair<Time, Rate> >
//    ZeroInflationCurveObserver2<T>::nodes() const {
//    calculate();
//    std::vector<std::pair<Time, Rate> > results(this->times_.size());
//    for (Size i = 0; i<this->times_.size(); ++i)
//        results[i] = std::make_pair(this->times_[i], this->data_[i]);
//    return results;
//}

template <class T>
inline void ZeroInflationCurveObserver2<T>::update() {
    LazyObject::update();
    ZeroInflationTermStructure::update();
}

template <class T>
inline void ZeroInflationCurveObserver2<T>::performCalculations() const {

    Date d = Settings::instance().evaluationDate(); 
    baseDate_ = d - this->observationLag();

    for (Size i = 0; i<this->times_.size(); ++i)
        this->data_[i] = quotes_[i]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(),
            this->times_.end(),
            this->data_.begin());
    this->interpolation_.update();
}
}


#endif