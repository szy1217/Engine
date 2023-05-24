/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/simm/simmresults.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/utilities/null.hpp>

using QuantLib::Null;
using QuantLib::Real;
using std::get;
using std::make_tuple;
using std::ostream;
using std::make_pair;
using std::string;

namespace ore {
namespace analytics {

void SimmResults::add(const SimmConfiguration::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
                      const SimmConfiguration::MarginType& mt, const string& b, Real im,
                      const string& calculationCurrency, const bool overwrite) {

    // Add the value as long as the currencies are matching. If the SimmResults container does not yet have
    // a currency, we set it to be that of the incoming value
    if (ccy_.empty())
        ccy_ = calculationCurrency;
    else
        QL_REQUIRE(calculationCurrency == ccy_, "Cannot add value to SimmResults in a different currency ("
                                                    << calculationCurrency << "). Expected " << ccy_ << ".");

    QL_REQUIRE(im >= 0.0, "Cannot add negative IM " << im << " result to SimmResults for RiskClass=" << rc
                                                    << ", MarginType=" << mt << ", and Bucket=" << b);

    const auto key = make_tuple(pc, rc, mt, b);
    const bool hasResults = data_.find(key) != data_.end();
    if (hasResults && !overwrite)
        data_[key] += im;
    else
        data_[key] = im;
}

void SimmResults::convert(const boost::shared_ptr<ore::data::Market>& market, const string& currency) {
    // Get corresponding FX spot rate
    Real fxSpot = market->fxRate(ccy_ + currency)->value();

    convert(fxSpot, currency);
}

void SimmResults::convert(Real fxSpot, const string& currency) {
    // Check that target currency is valid
    QL_REQUIRE(ore::data::checkCurrency(currency), "Cannot convert SIMM results. The target currency ("
                                                       << currency << ") must be a valid ISO currency code");

    // Skip if already in target currency
    if (ccy_ == currency)
        return;

    // Convert SIMM results to target currency
    for (auto& sr : data_)
        sr.second *= fxSpot;

    // Update currency
    ccy_ = currency;
}

Real SimmResults::get(const SimmConfiguration::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
                      const SimmConfiguration::MarginType& mt, const string b) const {
    if (has(pc, rc, mt, b)) {
        return data_.at(make_tuple(pc, rc, mt, b));
    } else {
        return Null<Real>();
    }
}

bool SimmResults::has(const SimmConfiguration::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
                      const SimmConfiguration::MarginType& mt, const string b) const {
    return data_.count(make_tuple(pc, rc, mt, b)) > 0;
}

bool SimmResults::empty() const { return data_.empty(); }

void SimmResults::clear() { data_.clear(); }

ostream& operator<<(ostream& out, const SimmResults::Key& k) {
    return out << "[" << get<0>(k) << ", " << get<1>(k) << ", " << get<2>(k) << ", " << get<3>(k) << "]";
}

} // namespace analytics
} // namespace ore
