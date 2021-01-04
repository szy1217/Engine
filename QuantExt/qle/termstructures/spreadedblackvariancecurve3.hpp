/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadedblackvariancecurve3.hpp
    \brief Spreaded Black volatility curve modelled as variance curve
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Spreaedd Black volatility curve modelled as variance curve
class SpreadedBlackVarianceCurve3 : public LazyObject, public BlackVarianceTermStructure {
public:
    //! times should be consistent with reference ts day counter
    SpreadedBlackVarianceCurve3(const Handle<BlackVolTermStructure>& referenceVol, const std::vector<Time>& times,
                                const std::vector<Handle<Quote>>& volSpreads);
    Date maxDate() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    Real minStrike() const override;
    Real maxStrike() const override;
    void update() override;

private:
    void performCalculations() const override;
    virtual Real blackVarianceImpl(Time t, Real) const override;

    Handle<BlackVolTermStructure> referenceVol_;
    std::vector<Time> times_;
    std::vector<Handle<Quote>> volSpreads_;
    mutable std::vector<Real> data_;
    boost::shared_ptr<Interpolation> interpolation_;
};

} // namespace QuantExt
