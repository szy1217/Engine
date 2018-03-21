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

/*! \file portfolio/builders/equityoption.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <qle/pricingengines/varswapengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/version.hpp>

namespace ore {
namespace data {

//! Engine Builder for Variance Swaps
/*! Pricing engines are cached by equity/currency

    \ingroup builders
 */
class VarSwapEngineBuilder : public CachingPricingEngineBuilder<string, const string&, const Currency&> {
public:
    VarSwapEngineBuilder()
        : CachingEngineBuilder("BlackScholesMerton", "ReplicatingVarianceSwapEngine", {"VarianceSwap"}) {}

protected:
    virtual string keyImpl(const string& equityName, const Currency& ccy) override {
        return equityName + "/" + ccy.code();
    }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& equityName, const Currency& ccy) override {
        //string key = keyImpl(equityName, ccy);
        Integer numCalls = parseInteger(engineParameters_.at("NumberOfCalls"));
        Integer numPuts = parseInteger(engineParameters_.at("NumberOfPuts"));
        Real moneyStep = parseReal(engineParameters_.at("MoneynessStep"));
        
        QL_REQUIRE(numCalls > 0, "Variance swap engine: not enough calls specified. Should be at least one, more is better.");
        QL_REQUIRE(numPuts > 0, "Variance swap engine: not enough puts specified. Should be at least one, more is better.");
        QL_REQUIRE(moneyStep > 0, "Variance swap engine: moneyness step is a percentage of strike and should satisfy be greater than 0.");
        // It is not necessarily required that the step size be less than 1. The step size is scaled with sqrt(T), which can be less than 1.

        return boost::make_shared<QuantExt::VarSwapEngine>(
            equityName, 
            market_->equitySpot(equityName, configuration(MarketContext::pricing)),
            market_->equityForecastCurve(equityName, configuration(MarketContext::pricing)),
            market_->equityDividendCurve(equityName, configuration(MarketContext::pricing)),
            market_->equityVol(equityName, configuration(MarketContext::pricing)),
            market_->discountCurve(ccy.code(), configuration(MarketContext::pricing)),
            numPuts, numCalls, moneyStep);
    }
};

} // namespace data
} // namespace ore
