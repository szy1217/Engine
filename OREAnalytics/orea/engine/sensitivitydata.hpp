/*
  Copyright (C) 2017 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*!
  \file oreap/simm/sensitivitydata.hpp
  \brief sensitivity data representations
 */

#pragma once

#include <orea/scenario/scenario.hpp>

#include <ql/types.hpp>

#include <map>
#include <string>
#include <vector>

using QuantLib::Size;

using namespace ore::analytics;

namespace ore {
namespace analytics {

class SensitivityData {
public:
    SensitivityData() {}
    virtual ~SensitivityData() {}
    virtual bool next() = 0;
    virtual void reset() = 0;
    virtual std::string tradeId() const = 0;
    virtual bool isCrossGamma() const = 0;
    virtual boost::shared_ptr<RiskFactorKey> factor1() const = 0;
    virtual boost::shared_ptr<RiskFactorKey> factor2() const = 0;
    // such as tenors, strikes (.../10Y/15Y/ATM)
    virtual std::vector<string> additionalTokens1() const = 0;
    virtual std::vector<string> additionalTokens2() const = 0;
    virtual double value() const = 0;  // delta or cross gamma
    virtual double value2() const = 0; // diagonal gamma
    virtual bool hasFactor(const RiskFactorKey& key) const = 0;
};

class SensitivityDataInMemory : public SensitivityData {
public:
    SensitivityDataInMemory() : index_(0) {}
    virtual void add(const std::string& tradeId, const std::string& factor, const std::string& factor2,
                     const double value, const double value2);
    virtual bool next() override;
    virtual void reset() override;
    virtual std::string tradeId() const override;
    virtual bool isCrossGamma() const override;
    virtual boost::shared_ptr<RiskFactorKey> factor1() const override;
    virtual boost::shared_ptr<RiskFactorKey> factor2() const override;
    virtual std::vector<string> additionalTokens1() const override;
    virtual std::vector<string> additionalTokens2() const override;
    virtual double value() const override;
    virtual double value2() const override;
    virtual bool hasFactor(const RiskFactorKey& key) const override;

private:
    Size index_;
    std::vector<std::string> tradeId_;
    std::vector<std::vector<std::string>> addTokens1_, addTokens2_;
    std::vector<boost::shared_ptr<RiskFactorKey>> keys1_, keys2_;
    std::vector<double> value_, value2_;
};

boost::shared_ptr<RiskFactorKey> parseRiskFactorKey(const std::string& str, std::vector<std::string>& addTokens);
void loadSensitivityDataFromCsv(SensitivityDataInMemory& data, const std::string& fileName, const char delim = '\n');
void loadMappingTableFromCsv(std::map<std::string, std::string>& data, const std::string& fileName,
                             const char delim = '\n');

} // namespace analytics
} // namespace oreplus
