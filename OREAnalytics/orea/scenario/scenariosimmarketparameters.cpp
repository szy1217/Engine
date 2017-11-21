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

#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace ore::data;

namespace ore {
namespace analytics {

namespace {
const vector<Period>& returnTenors(const map<string, vector<Period>>& m, const string& k) {
    if (m.count(k) > 0) {
        return m.at(k);
    } else if (m.count("") > 0) {
        return m.at("");
    } else
        QL_FAIL("no period vector for key \"" << k << "\" found.");
}
} // namespace

const vector<Period>& ScenarioSimMarketParameters::yieldCurveTenors(const string& key) const {
    return returnTenors(yieldCurveTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::capFloorVolExpiries(const string& key) const {
    return returnTenors(capFloorVolExpiries_, key);
}

const vector<Period>& ScenarioSimMarketParameters::defaultTenors(const string& key) const {
    return returnTenors(defaultTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::equityDividendTenors(const string& key) const {
    return returnTenors(equityDividendTenors_, key);
}
    
const vector<Period>& ScenarioSimMarketParameters::equityForecastTenors(const string& key) const {
        return returnTenors(equityForecastTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::zeroInflationTenors(const string& key) const {
    return returnTenors(zeroInflationTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::yoyInflationTenors(const string& key) const {
    return returnTenors(yoyInflationTenors_, key);
}

const vector<Period>& ScenarioSimMarketParameters::cpiCapFloorVolExpiries(const string& key) const {
    return returnTenors(cpiCapFloorVolExpiries_, key);
}

const vector<Period>& ScenarioSimMarketParameters::yoyCapFloorVolExpiries(const string& key) const {
    return returnTenors(yoyCapFloorVolExpiries_, key);
}

void ScenarioSimMarketParameters::setYieldCurveTenors(const string& key, const std::vector<Period>& p) {
    yieldCurveTenors_[key] = p;
}

void ScenarioSimMarketParameters::setCapFloorVolExpiries(const string& key, const std::vector<Period>& p) {
    capFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setDefaultTenors(const string& key, const std::vector<Period>& p) {
    defaultTenors_[key] = p;
}

void ScenarioSimMarketParameters::setEquityDividendTenors(const string& key, const std::vector<Period>& p) {
    equityDividendTenors_[key] = p;
}
    
void ScenarioSimMarketParameters::setEquityForecastTenors(const string& key, const std::vector<Period>& p) {
    equityForecastTenors_[key] = p;
}

void ScenarioSimMarketParameters::setZeroInflationTenors(const string& key, const std::vector<Period>& p) {
    zeroInflationTenors_[key] = p;
}

void ScenarioSimMarketParameters::setYoyInflationTenors(const string& key, const std::vector<Period>& p) {
    yoyInflationTenors_[key] = p;
}

void ScenarioSimMarketParameters::setCpiCapFloorVolExpiries(const string& key, const vector<Period>& p) {
    cpiCapFloorVolExpiries_[key] = p;
}

void ScenarioSimMarketParameters::setYoyCapFloorVolExpiries(const string& key, const vector<Period>& p) {
    yoyCapFloorVolExpiries_[key] = p;
}

bool ScenarioSimMarketParameters::operator==(const ScenarioSimMarketParameters& rhs) {

    if (baseCcy_ != rhs.baseCcy_ || ccys_ != rhs.ccys_ || yieldCurveNames_ != rhs.yieldCurveNames_ ||
        yieldCurveCurrencies_ != rhs.yieldCurveCurrencies_ || yieldCurveTenors_ != rhs.yieldCurveTenors_ ||
        indices_ != rhs.indices_ || swapIndices_ != rhs.swapIndices_ || interpolation_ != rhs.interpolation_ ||
        extrapolate_ != rhs.extrapolate_ || swapVolTerms_ != rhs.swapVolTerms_ || swapVolCcys_ != rhs.swapVolCcys_ ||
        swapVolSimulate_ != rhs.swapVolSimulate_ || swapVolExpiries_ != rhs.swapVolExpiries_ ||
        swapVolDecayMode_ != rhs.swapVolDecayMode_ || capFloorVolSimulate_ != rhs.capFloorVolSimulate_ ||
        capFloorVolCcys_ != rhs.capFloorVolCcys_ || capFloorVolExpiries_ != rhs.capFloorVolExpiries_ ||
        capFloorVolStrikes_ != rhs.capFloorVolStrikes_ || capFloorVolDecayMode_ != rhs.capFloorVolDecayMode_ ||
        defaultNames_ != rhs.defaultNames_ || defaultTenors_ != rhs.defaultTenors_ ||
        cdsVolSimulate_ != rhs.cdsVolSimulate_ || cdsVolNames_ != rhs.cdsVolNames_ ||
        cdsVolExpiries_ != rhs.cdsVolExpiries_ || cdsVolDecayMode_ != rhs.cdsVolDecayMode_ ||
        equityNames_ != rhs.equityNames_ || equityDividendTenors_ != rhs.equityDividendTenors_ || equityForecastTenors_ != rhs.equityForecastTenors_ ||
        equityNamesSimulate_ != rhs.equityNamesSimulate_ || fxVolSimulate_ != rhs.fxVolSimulate_ || fxVolExpiries_ != rhs.fxVolExpiries_ ||
        fxVolDecayMode_ != rhs.fxVolDecayMode_ || fxVolCcyPairs_ != rhs.fxVolCcyPairs_ ||
        fxCcyPairs_ != rhs.fxCcyPairs_ || equityVolSimulate_ != rhs.equityVolSimulate_ ||
        equityVolExpiries_ != rhs.equityVolExpiries_ || equityVolDecayMode_ != rhs.equityVolDecayMode_ ||
        equityVolNames_ != rhs.equityVolNames_ || equityIsSurface_ != rhs.equityIsSurface_ || 
        equityVolSimulateATMOnly_ != rhs.equityVolSimulateATMOnly_ || equityMoneyness_ != rhs.equityMoneyness_ ||
        additionalScenarioDataIndices_ != rhs.additionalScenarioDataIndices_ ||
        additionalScenarioDataCcys_ != rhs.additionalScenarioDataCcys_ || securities_ != rhs.securities_ ||
        baseCorrelationSimulate_ != rhs.baseCorrelationSimulate_ ||
        baseCorrelationNames_ != rhs.baseCorrelationNames_ || baseCorrelationTerms_ != rhs.baseCorrelationTerms_ ||
        baseCorrelationDetachmentPoints_ != rhs.baseCorrelationDetachmentPoints_ ||
        zeroInflationIndices_ != rhs.zeroInflationIndices_ || zeroInflationTenors_ != rhs.zeroInflationTenors_ || 
        yoyInflationIndices_ != rhs.yoyInflationIndices_ || yoyInflationTenors_ != rhs.yoyInflationTenors_ ||
        cpiCapFloorVolSimulate_ != rhs.cpiCapFloorVolSimulate_ ||
        cpiCapFloorVolIndices_ != rhs.cpiCapFloorVolIndices_ || cpiCapFloorVolExpiries_ != rhs.cpiCapFloorVolExpiries_ ||
        cpiCapFloorVolStrikes_ != rhs.cpiCapFloorVolStrikes_ || cpiCapFloorVolDecayMode_ != rhs.cpiCapFloorVolDecayMode_ ||
        yoyCapFloorVolSimulate_ != rhs.yoyCapFloorVolSimulate_ ||
        yoyCapFloorVolIndices_ != rhs.yoyCapFloorVolIndices_ || yoyCapFloorVolExpiries_ != rhs.yoyCapFloorVolExpiries_ ||
        yoyCapFloorVolStrikes_ != rhs.yoyCapFloorVolStrikes_ || yoyCapFloorVolDecayMode_ != rhs.yoyCapFloorVolDecayMode_) {
        return false;
    } else {
        return true;
    }
}

bool ScenarioSimMarketParameters::operator!=(const ScenarioSimMarketParameters& rhs) { return !(*this == rhs); }

void ScenarioSimMarketParameters::fromXML(XMLNode* root) {
    DLOG("ScenarioSimMarketParameters::fromXML()");

    XMLNode* sim = XMLUtils::locateNode(root, "Simulation");
    XMLNode* node = XMLUtils::getChildNode(sim, "Market");
    XMLUtils::checkNode(node, "Market");

    yieldCurveTenors_.clear();
    capFloorVolExpiries_.clear();
    defaultTenors_.clear();
    equityDividendTenors_.clear();
    equityForecastTenors_.clear();
    swapIndices_.clear();
    zeroInflationTenors_.clear();
    yoyInflationTenors_.clear();
    cpiCapFloorVolExpiries_.clear();
    yoyCapFloorVolExpiries_.clear();

    // TODO: add in checks (checkNode or QL_REQUIRE) on mandatory nodes
    DLOG("Loading Currencies");

    baseCcy_ = XMLUtils::getChildValue(node, "BaseCurrency");
    ccys_ = XMLUtils::getChildrenValues(node, "Currencies", "Currency");

    DLOG("Loading BenchmarkCurve");

    XMLNode* nodeChild = XMLUtils::getChildNode(node, "BenchmarkCurves");
    yieldCurveNames_.clear();
    yieldCurveCurrencies_.clear();
    if (nodeChild) {
        for (XMLNode* n = XMLUtils::getChildNode(nodeChild, "BenchmarkCurve"); n != nullptr;
             n = XMLUtils::getNextSibling(n, "BenchmarkCurve")) {
            yieldCurveNames_.push_back(XMLUtils::getChildValue(n, "Name", true));
            yieldCurveCurrencies_.push_back(XMLUtils::getChildValue(n, "Currency", true));
        }
    }

    DLOG("Loading YieldCurves");

    nodeChild = XMLUtils::getChildNode(node, "YieldCurves");
    if (nodeChild) {
        nodeChild = XMLUtils::getChildNode(nodeChild, "Configuration");
        yieldCurveTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
        // TODO read other keys
        interpolation_ = XMLUtils::getChildValue(nodeChild, "Interpolation", true);
        extrapolate_ = XMLUtils::getChildValueAsBool(nodeChild, "Extrapolate");
    }

    indices_ = XMLUtils::getChildrenValues(node, "Indices", "Index");

    nodeChild = XMLUtils::getChildNode(node, "SwapIndices");
    if (nodeChild) {
        for (XMLNode* n = XMLUtils::getChildNode(nodeChild, "SwapIndex"); n != nullptr;
             n = XMLUtils::getNextSibling(n, "SwapIndex")) {
            string name = XMLUtils::getChildValue(n, "Name");
            string disc = XMLUtils::getChildValue(n, "DiscountingIndex");
            swapIndices_[name] = disc;
        }
    }

    DLOG("Loading FX Rates");

    nodeChild = XMLUtils::getChildNode(node, "FxRates");
    if (nodeChild)
        fxCcyPairs_ = XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true);
    else {
        fxCcyPairs_.resize(0);
        for (auto ccy : ccys_) {
            if (ccy != baseCcy_)
                fxCcyPairs_.push_back(ccy + baseCcy_);
        }
    }

    DLOG("Loading SwaptionVolatilities Rates");

    nodeChild = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    if (nodeChild) {
        XMLNode* swapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (swapVolSimNode) {
            swapVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(swapVolSimNode));
            swapVolTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
            swapVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
            swapVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
            swapVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
            XMLNode* cubeNode = XMLUtils::getChildNode(nodeChild, "Cube");
            if (cubeNode) {
                swapVolIsCube_ = true;
                XMLNode* atmOnlyNode = XMLUtils::getChildNode(cubeNode, "SimulateATMOnly");
                if (atmOnlyNode) {
                    swapVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(cubeNode, "SimulateATMOnly", true);
                } else {
                    swapVolSimulateATMOnly_ = false;
                }
                if (!swapVolSimulateATMOnly_)
                    swapVolStrikeSpreads_ = XMLUtils::getChildrenValuesAsDoublesCompact(cubeNode, "StrikeSpreads", true);
            } else {
                swapVolIsCube_ = false;
            }
        }
    }

    nodeChild = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    if (nodeChild) {
        capFloorVolSimulate_ = false;
        XMLNode* capVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (capVolSimNode)
            capFloorVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(capVolSimNode));
        capFloorVolExpiries_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        // TODO read other keys
        capFloorVolStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "Strikes", true);
        capFloorVolCcys_ = XMLUtils::getChildrenValues(nodeChild, "Currencies", "Currency", true);
        capFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    DLOG("Loading DefaultCurves Rates");

    survivalProbabilitySimulate_ = false;
    recoveryRateSimulate_ = false;
    nodeChild = XMLUtils::getChildNode(node, "DefaultCurves");
    if (nodeChild) {
        defaultNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        defaultTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
        // TODO read other keys
        XMLNode* survivalProbabilitySimNode = XMLUtils::getChildNode(nodeChild, "SimulateSurvivalProbabilities");
        if (survivalProbabilitySimNode)
            survivalProbabilitySimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(survivalProbabilitySimNode));
        XMLNode* recoveryRateSimNode = XMLUtils::getChildNode(nodeChild, "SimulateRecoveryRates");
        if (recoveryRateSimNode)
            recoveryRateSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(recoveryRateSimNode));
    }

    DLOG("Loading Equities Rates");

    nodeChild = XMLUtils::getChildNode(node, "Equities");
    equityNames_.clear();
    if (nodeChild) {
        XMLNode* equityNamesSimNode = XMLUtils::getChildNode(nodeChild, "SimulateEquityNames");
        if (equityNamesSimNode)
            equityNamesSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(equityNamesSimNode));
        equityNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true); 
        equityDividendTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "DividendTenors", true);
        equityForecastTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "ForecastTenors", true);
    } else {
        equityDividendTenors_.clear();
        equityForecastTenors_.clear();
    }
    
    DLOG("Loading CDSVolatilities Rates");

    nodeChild = XMLUtils::getChildNode(node, "CDSVolatilities");
    cdsVolSimulate_ = false;
    if (nodeChild) {
        XMLNode* cdsVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (cdsVolSimNode)
            cdsVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(cdsVolSimNode));
        cdsVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        cdsVolNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        cdsVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    DLOG("Loading FXVolatilities");

    nodeChild = XMLUtils::getChildNode(node, "FxVolatilities");
    if (nodeChild) {
        fxVolSimulate_ = false;
        XMLNode* fxVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (fxVolSimNode)
            fxVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(fxVolSimNode));
        fxVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        fxVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        fxVolCcyPairs_ = XMLUtils::getChildrenValues(nodeChild, "CurrencyPairs", "CurrencyPair", true);
        XMLNode* fxSurfaceNode = XMLUtils::getChildNode(nodeChild, "Surface");
        if (fxSurfaceNode) {
            fxVolIsSurface_ = true;
            fxMoneyness_ = XMLUtils::getChildrenValuesAsDoublesCompact(fxSurfaceNode, "Moneyness", true);
        }
        else {
            fxVolIsSurface_ = false;
            fxMoneyness_ = { 0.0 };
        }
    }

    DLOG("Loading EquityVolatilities");

    nodeChild = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (nodeChild) {
        equityVolSimulate_ = XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true);
        equityVolExpiries_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        equityVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
        equityVolNames_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        XMLNode* eqSurfaceNode = XMLUtils::getChildNode(nodeChild, "Surface");
        if (eqSurfaceNode) {
            equityIsSurface_ = true;
            XMLNode* atmOnlyNode = XMLUtils::getChildNode(eqSurfaceNode, "SimulateATMOnly");
            if(atmOnlyNode) {
                equityVolSimulateATMOnly_ = XMLUtils::getChildValueAsBool(eqSurfaceNode, "SimulateATMOnly", true);
            } else {
                equityVolSimulateATMOnly_ = false;
            }
            if(!equityVolSimulateATMOnly_) 
                equityMoneyness_ = XMLUtils::getChildrenValuesAsDoublesCompact(eqSurfaceNode, "Moneyness", true);
        } else {
            equityIsSurface_ = false;
        }
    } else {
        equityVolSimulate_ = false;
        equityVolExpiries_.clear();
        equityVolNames_.clear();
    }

    DLOG("Loading CpiInflationIndexCurves");

    nodeChild = XMLUtils::getChildNode(node, "CpiInflationIndexCurves");
    if (nodeChild) {
        cpiIndices_ = XMLUtils::getChildrenValues(nodeChild, "CpiIndices", "Index", true);
    }
    else {
        cpiIndices_.clear();
    }


    DLOG("Loading ZeroInflationIndexCurves");

    nodeChild = XMLUtils::getChildNode(node, "ZeroInflationIndexCurves");
    if (nodeChild) {
        zeroInflationIndices_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        zeroInflationTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    }
    else {
        zeroInflationIndices_.clear();
        zeroInflationTenors_.clear();
    }

    DLOG("Loading CpiCapFloorVolatilities");

    nodeChild = XMLUtils::getChildNode(node, "CpiCapFloorVolatilities");
    if (nodeChild) {
        cpiCapFloorVolSimulate_ = false;
        XMLNode* cpiCapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (cpiCapVolSimNode)
            cpiCapFloorVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(cpiCapVolSimNode));
        cpiCapFloorVolExpiries_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        // TODO read other keys
        cpiCapFloorVolStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "Strikes", true);
        cpiCapFloorVolIndices_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        cpiCapFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    DLOG("Loading YYInflationIndexCurves");

    nodeChild = XMLUtils::getChildNode(node, "YYInflationIndexCurves");
    if (nodeChild) {
        yoyInflationIndices_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        yoyInflationTenors_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Tenors", true);
    }
    else {
        yoyInflationIndices_.clear();
        yoyInflationTenors_.clear();
    }


    DLOG("Loading YYCapFloorVolatilities");

    nodeChild = XMLUtils::getChildNode(node, "YYCapFloorVolatilities");
    if (nodeChild) {
        yoyCapFloorVolSimulate_ = false;
        XMLNode* yoyCapVolSimNode = XMLUtils::getChildNode(nodeChild, "Simulate");
        if (yoyCapVolSimNode)
            yoyCapFloorVolSimulate_ = ore::data::parseBool(XMLUtils::getNodeValue(yoyCapVolSimNode));
        yoyCapFloorVolExpiries_[""] = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Expiries", true);
        // TODO read other keys
        yoyCapFloorVolStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "Strikes", true);
        yoyCapFloorVolIndices_ = XMLUtils::getChildrenValues(nodeChild, "Names", "Name", true);
        yoyCapFloorVolDecayMode_ = XMLUtils::getChildValue(nodeChild, "ReactionToTimeDecay");
    }

    DLOG("Loading AggregationScenarioDataIndices");

    additionalScenarioDataIndices_ = XMLUtils::getChildrenValues(node, "AggregationScenarioDataIndices", "Index");
    additionalScenarioDataCcys_ =
        XMLUtils::getChildrenValues(node, "AggregationScenarioDataCurrencies", "Currency", true);

    nodeChild = XMLUtils::getChildNode(node, "Securities");
    if (nodeChild)
        securities_ = XMLUtils::getChildrenValues(node, "Securities", "Security");

    nodeChild = XMLUtils::getChildNode(node, "BaseCorrelations");
    if (nodeChild) {
        baseCorrelationSimulate_ = XMLUtils::getChildValueAsBool(nodeChild, "Simulate", true);
        baseCorrelationNames_ = XMLUtils::getChildrenValues(nodeChild, "IndexNames", "IndexName", true);
        baseCorrelationTerms_ = XMLUtils::getChildrenValuesAsPeriods(nodeChild, "Terms", true);
        baseCorrelationDetachmentPoints_ =
            XMLUtils::getChildrenValuesAsDoublesCompact(nodeChild, "DetachmentPoints", true);
    } else {
        baseCorrelationSimulate_ = false;
        baseCorrelationNames_.clear();
        baseCorrelationTerms_.clear();
        baseCorrelationDetachmentPoints_.clear();
    }

    DLOG("Loaded ScenarioSimMarketParameters");
}

XMLNode* ScenarioSimMarketParameters::toXML(XMLDocument& doc) {

    XMLNode* marketNode = doc.allocNode("Market");

    // currencies
    XMLUtils::addChild(doc, marketNode, "BaseCurrency", baseCcy_);
    XMLUtils::addChildren(doc, marketNode, "Currencies", "Currency", ccys_);

    // benchmark yield curves
    XMLNode* benchmarkCurvesNode = XMLUtils::addChild(doc, marketNode, "BenchmarkCurves");
    for (Size i = 0; i < yieldCurveNames_.size(); ++i) {
        XMLNode* benchmarkCurveNode = XMLUtils::addChild(doc, benchmarkCurvesNode, "BenchmarkCurve");
        XMLUtils::addChild(doc, benchmarkCurveNode, "Currency", yieldCurveCurrencies_[i]);
        XMLUtils::addChild(doc, benchmarkCurveNode, "Name", yieldCurveNames_[i]);
    }

    // yield curves
    XMLNode* yieldCurvesNode = XMLUtils::addChild(doc, marketNode, "YieldCurves");
    XMLNode* configurationNode = XMLUtils::addChild(doc, yieldCurvesNode, "Configuration");
    XMLUtils::addGenericChildAsList(doc, configurationNode, "Tenors", returnTenors(yieldCurveTenors_, ""));
    // TODO write other keys
    XMLUtils::addChild(doc, configurationNode, "Interpolation", interpolation_);
    XMLUtils::addChild(doc, configurationNode, "Extrapolation", extrapolate_);

    // indices
    XMLUtils::addChildren(doc, marketNode, "Indices", "Index", indices_);

    // swap indices
    XMLNode* swapIndicesNode = XMLUtils::addChild(doc, marketNode, "SwapIndices");
    for (auto swapIndexInterator : swapIndices_) {
        XMLNode* swapIndexNode = XMLUtils::addChild(doc, swapIndicesNode, "SwapIndex");
        XMLUtils::addChild(doc, swapIndexNode, "Name", swapIndexInterator.first);
        XMLUtils::addChild(doc, swapIndexNode, "DiscountingIndex", swapIndexInterator.second);
    }

    // default curves

    XMLNode* defaultCurvesNode = XMLUtils::addChild(doc, marketNode, "DefaultCurves");
    XMLUtils::addChildren(doc, defaultCurvesNode, "Names", "Name", defaultNames_);
    XMLUtils::addGenericChildAsList(doc, defaultCurvesNode, "Tenors", returnTenors(defaultTenors_, ""));
    // TODO write other keys
    XMLUtils::addChild(doc, defaultCurvesNode, "SimulateSurvivalProbabilities", survivalProbabilitySimulate_);
    XMLUtils::addChild(doc, defaultCurvesNode, "SimulateRecoveryRates", recoveryRateSimulate_);

    // equities
    XMLNode* equitiesNode = XMLUtils::addChild(doc, marketNode, "Equities");
    XMLUtils::addChildren(doc, equitiesNode, "Names", "Name", equityNames_);
    XMLUtils::addGenericChildAsList(doc, equitiesNode, "DividendTenors", returnTenors(equityDividendTenors_, ""));
    XMLUtils::addGenericChildAsList(doc, equitiesNode, "ForecastTenors", returnTenors(equityForecastTenors_, ""));
    // swaption volatilities
    XMLNode* swaptionVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "SwaptionVolatilities");
    XMLUtils::addChild(doc, swaptionVolatilitiesNode, "Simulate", swapVolSimulate_);
    XMLUtils::addChild(doc, swaptionVolatilitiesNode, "ReactionToTimeDecay", swapVolDecayMode_);
    XMLUtils::addChildren(doc, swaptionVolatilitiesNode, "Currencies", "Currency", swapVolCcys_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Expiries", swapVolExpiries_);
    XMLUtils::addGenericChildAsList(doc, swaptionVolatilitiesNode, "Terms", swapVolTerms_);

    // cap/floor volatilities
    XMLNode* capFloorVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CapFloorVolatilities");
    XMLUtils::addChild(doc, capFloorVolatilitiesNode, "Simulate", capFloorVolSimulate_);
    XMLUtils::addChild(doc, capFloorVolatilitiesNode, "ReactionToTimeDecay", capFloorVolDecayMode_);
    XMLUtils::addChildren(doc, capFloorVolatilitiesNode, "Currencies", "Currency", capFloorVolCcys_);
    XMLUtils::addGenericChildAsList(doc, capFloorVolatilitiesNode, "Expiries", returnTenors(capFloorVolExpiries_, ""));
    // TODO write other keys
    XMLUtils::addGenericChildAsList(doc, capFloorVolatilitiesNode, "Strikes", capFloorVolStrikes_);

    // fx volatilities
    XMLNode* fxVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "FxVolatilities");
    XMLUtils::addChild(doc, fxVolatilitiesNode, "Simulate", fxVolSimulate_);
    XMLUtils::addChild(doc, fxVolatilitiesNode, "ReactionToTimeDecay", fxVolDecayMode_);
    XMLUtils::addChildren(doc, fxVolatilitiesNode, "CurrencyPairs", "CurrencyPair", fxVolCcyPairs_);
    XMLUtils::addGenericChildAsList(doc, fxVolatilitiesNode, "Expiries", fxVolExpiries_);

    // fx rates
    XMLNode* fxRatesNode = XMLUtils::addChild(doc, marketNode, "FxRates");
    XMLUtils::addChildren(doc, fxRatesNode, "CurrencyPairs", "CurrencyPair", fxCcyPairs_);

    // eq volatilities
    XMLNode* eqVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "EquityVolatilities");
    XMLUtils::addChild(doc, eqVolatilitiesNode, "Simulate", equityVolSimulate_);
    XMLUtils::addChild(doc, eqVolatilitiesNode, "ReactionToTimeDecay", equityVolDecayMode_);
    XMLUtils::addChildren(doc, eqVolatilitiesNode, "Names", "Name", equityVolNames_);
    XMLUtils::addGenericChildAsList(doc, eqVolatilitiesNode, "Expiries", equityVolExpiries_);
    if (equityIsSurface_) {
        XMLNode* eqSurfaceNode = XMLUtils::addChild(doc, eqVolatilitiesNode, "Surface");
        XMLUtils::addGenericChildAsList(doc, eqSurfaceNode, "Moneyness", equityMoneyness_);
    }

    // additional scenario data currencies
    XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataCurrencies", "Currency",
                          additionalScenarioDataCcys_);

    // additional scenario data indices
    XMLUtils::addChildren(doc, marketNode, "AggregationScenarioDataIndices", "Index", additionalScenarioDataIndices_);

    // securities
    XMLUtils::addChildren(doc, marketNode, "Securities", "Security", securities_);

    // base correlations
    XMLNode* bcNode = XMLUtils::addChild(doc, marketNode, "BaseCorrelations");
    XMLUtils::addChild(doc, bcNode, "Simulate", baseCorrelationSimulate_);
    XMLUtils::addChildren(doc, bcNode, "IndexNames", "IndexName", baseCorrelationNames_);
    XMLUtils::addGenericChildAsList(doc, bcNode, "Terms", baseCorrelationTerms_);
    XMLUtils::addGenericChildAsList(doc, bcNode, "DetachmentPoints", baseCorrelationDetachmentPoints_);

    // zero inflation
    XMLNode* cpiNode = XMLUtils::addChild(doc, marketNode, "CpiInflationIndices");
    XMLUtils::addChildren(doc, cpiNode, "CpiIndices", "Index", cpiIndices_);

    // zero inflation
    XMLNode* zeroNode = XMLUtils::addChild(doc, marketNode, "ZeroInflationIndexCurves");
    XMLUtils::addChildren(doc, zeroNode, "Names", "Name", zeroInflationIndices_);
    XMLUtils::addGenericChildAsList(doc, zeroNode, "Tenors", returnTenors(zeroInflationTenors_, ""));

    // cpi cap/floor volatilities
    XMLNode* cpiVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "CpiCapFloorVolatilities");
    XMLUtils::addChild(doc, cpiVolatilitiesNode, "Simulate", cpiCapFloorVolSimulate_);
    XMLUtils::addChild(doc, cpiVolatilitiesNode, "ReactionToTimeDecay", cpiCapFloorVolDecayMode_);
    XMLUtils::addChildren(doc, cpiVolatilitiesNode, "Names", "Name", cpiCapFloorVolIndices_);
    XMLUtils::addGenericChildAsList(doc, cpiVolatilitiesNode, "Expiries", returnTenors(cpiCapFloorVolExpiries_, ""));
    XMLUtils::addGenericChildAsList(doc, cpiVolatilitiesNode, "Strikes", cpiCapFloorVolStrikes_);

    // yoy inflation
    XMLNode* yoyNode = XMLUtils::addChild(doc, marketNode, "YYInflationIndexCurves");
    XMLUtils::addChildren(doc, yoyNode, "Names", "Name", yoyInflationIndices_);
    XMLUtils::addGenericChildAsList(doc, yoyNode, "Tenors", returnTenors(yoyInflationTenors_, ""));

    // yoy cap/floor volatilities
    XMLNode* yoyVolatilitiesNode = XMLUtils::addChild(doc, marketNode, "YYCapFloorVolatilities");
    XMLUtils::addChild(doc, yoyVolatilitiesNode, "Simulate", yoyCapFloorVolSimulate_);
    XMLUtils::addChild(doc, yoyVolatilitiesNode, "ReactionToTimeDecay", yoyCapFloorVolDecayMode_);
    XMLUtils::addChildren(doc, yoyVolatilitiesNode, "Names", "Name", yoyCapFloorVolIndices_);
    XMLUtils::addGenericChildAsList(doc, yoyVolatilitiesNode, "Expiries", returnTenors(yoyCapFloorVolExpiries_, ""));
    XMLUtils::addGenericChildAsList(doc, yoyVolatilitiesNode, "Strikes", yoyCapFloorVolStrikes_);

    return marketNode;
}
} // namespace analytics
} // namespace ore
