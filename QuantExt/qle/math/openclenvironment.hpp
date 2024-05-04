/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/math/openclenvironment.hpp
    \brief opencl compute env implementation
*/

#pragma once

#include <qle/math/computeenvironment.hpp>

#include <map>

#ifdef ORE_ENABLE_OPENCL
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#endif

#define MAX_N_DEVICES 8U

namespace QuantExt {

class OpenClFramework : public ComputeFramework {
public:
    OpenClFramework();
    ~OpenClFramework() override final;
    std::set<std::string> getAvailableDevices() const override final;
    ComputeContext* getContext(const std::string& deviceName) override final;

private:
    std::map<std::string, ComputeContext*> contexts_;
    cl_uint nDevices_;
    cl_device_id devices_[MAX_N_DEVICES];
    cl_context context_[MAX_N_DEVICES];
    cl_command_queue queue_[MAX_N_DEVICES];
};

} // namespace QuantExt
