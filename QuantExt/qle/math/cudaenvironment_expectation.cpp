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

#include <qle/math/cudaenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_opcodes.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/timer/timer.hpp>

#include <iostream>

#ifdef ORE_ENABLE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <curand.h>
#include <curand_mtgp32_host.h>
#include <nvrtc.h>
#include <cusolverDn.h>
#endif

#define MAX_N_PLATFORMS 4U
#define MAX_N_DEVICES 8U
#define MAX_N_NAME 64U
#define MAX_BUILD_LOG 65536U
#define MAX_BUILD_LOG_LOGFILE 1024U

namespace {
#ifdef ORE_ENABLE_CUDA
std::string curandGetErrorString(curandStatus_t err) {
    switch (err) {
    case CURAND_STATUS_SUCCESS:
        return "CURAND_STATUS_SUCCESS";
    case CURAND_STATUS_VERSION_MISMATCH:
        return "CURAND_STATUS_VERSION_MISMATCH";
    case CURAND_STATUS_NOT_INITIALIZED:
        return "CURAND_STATUS_NOT_INITIALIZED";
    case CURAND_STATUS_ALLOCATION_FAILED:
        return "CURAND_STATUS_ALLOCATION_FAILED";
    case CURAND_STATUS_TYPE_ERROR:
        return "CURAND_STATUS_TYPE_ERROR";
    case CURAND_STATUS_OUT_OF_RANGE:
        return "CURAND_STATUS_OUT_OF_RANGE";
    case CURAND_STATUS_LENGTH_NOT_MULTIPLE:
        return "CURAND_STATUS_LENGTH_NOT_MULTIPLE";
    case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
        return "CURAND_STATUS_DOUBLE_PRECISION_REQUIRED";
    case CURAND_STATUS_LAUNCH_FAILURE:
        return "CURAND_STATUS_LAUNCH_FAILURE";
    case CURAND_STATUS_PREEXISTING_FAILURE:
        return "CURAND_STATUS_PREEXISTING_FAILURE";
    case CURAND_STATUS_INITIALIZATION_FAILED:
        return "CURAND_STATUS_INITIALIZATION_FAILED";
    case CURAND_STATUS_ARCH_MISMATCH:
        return "CURAND_STATUS_ARCH_MISMATCH";
    case CURAND_STATUS_INTERNAL_ERROR:
        return "CURAND_STATUS_INTERNAL_ERROR";
    default:
        return "unknown curand error code " + std::to_string(err);
    }
}

std::string cusolverGetErrorString(cusolverStatus_t err) {
    switch (err) {
    case CUSOLVER_STATUS_SUCCESS:
        return "CUSOLVER_STATUS_SUCCESS";
    case CUSOLVER_STATUS_NOT_INITIALIZED:
        return "CUSOLVER_STATUS_NOT_INITIALIZED";
    case CUSOLVER_STATUS_ALLOC_FAILED:
        return "CUSOLVER_STATUS_ALLOC_FAILED";
    case CUSOLVER_STATUS_INVALID_VALUE:
        return "CUSOLVER_STATUS_INVALID_VALUE";
    case CUSOLVER_STATUS_ARCH_MISMATCH:
        return "CUSOLVER_STATUS_ARCH_MISMATCH";
    case CUSOLVER_STATUS_MAPPING_ERROR:
        return "CUSOLVER_STATUS_MAPPING_ERROR";
    case CUSOLVER_STATUS_EXECUTION_FAILED:
        return "CUSOLVER_STATUS_EXECUTION_FAILED";
    case CUSOLVER_STATUS_INTERNAL_ERROR:
        return "CUSOLVER_STATUS_INTERNAL_ERROR";
    case CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED:
        return "CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED";
    case CUSOLVER_STATUS_NOT_SUPPORTED:
        return "CUSOLVER_STATUS_NOT_SUPPORTED";
    case CUSOLVER_STATUS_ZERO_PIVOT:
        return "CUSOLVER_STATUS_ZERO_PIVOT";
    case CUSOLVER_STATUS_INVALID_LICENSE:
        return "CUSOLVER_STATUS_INVALID_LICENSE";
    case CUSOLVER_STATUS_IRS_PARAMS_NOT_INITIALIZED:
        return "CUSOLVER_STATUS_IRS_PARAMS_NOT_INITIALIZED";
    case CUSOLVER_STATUS_IRS_PARAMS_INVALID:
        return "CUSOLVER_STATUS_IRS_PARAMS_INVALID";
    case CUSOLVER_STATUS_IRS_PARAMS_INVALID_PREC:
        return "CUSOLVER_STATUS_IRS_PARAMS_INVALID_PREC";
    case CUSOLVER_STATUS_IRS_PARAMS_INVALID_REFINE:
        return "CUSOLVER_STATUS_IRS_PARAMS_INVALID_REFINE";
    case CUSOLVER_STATUS_IRS_PARAMS_INVALID_MAXITER:
        return "CUSOLVER_STATUS_IRS_PARAMS_INVALID_MAXITER";
    case CUSOLVER_STATUS_IRS_INTERNAL_ERROR:
        return "CUSOLVER_STATUS_IRS_INTERNAL_ERROR";
    case CUSOLVER_STATUS_IRS_NOT_SUPPORTED:
        return "CUSOLVER_STATUS_IRS_NOT_SUPPORTED";
    case CUSOLVER_STATUS_IRS_OUT_OF_RANGE:
        return "CUSOLVER_STATUS_IRS_OUT_OF_RANGE";
    case CUSOLVER_STATUS_IRS_NRHS_NOT_SUPPORTED_FOR_REFINE_GMRES:
        return "CUSOLVER_STATUS_IRS_NRHS_NOT_SUPPORTED_FOR_REFINE_GMRES";
    case CUSOLVER_STATUS_IRS_INFOS_NOT_INITIALIZED:
        return "CUSOLVER_STATUS_IRS_INFOS_NOT_INITIALIZED";
    case CUSOLVER_STATUS_IRS_INFOS_NOT_DESTROYED:
        return "CUSOLVER_STATUS_IRS_INFOS_NOT_DESTROYED";
    case CUSOLVER_STATUS_IRS_MATRIX_SINGULAR:
        return "CUSOLVER_STATUS_IRS_MATRIX_SINGULAR";
    case CUSOLVER_STATUS_INVALID_WORKSPACE:
        return "CUSOLVER_STATUS_INVALID_WORKSPACE";
    default:
        return "unknown cusolver error code " + std::to_string(err);
    }
#endif
}
} // namespace

namespace QuantExt {

#ifdef ORE_ENABLE_CUDA

class CudaContext : public ComputeContext {
public:
    CudaContext(size_t device, const std::vector<std::pair<std::string, std::string>>& deviceInfo,
                const std::string gpuArchitecture, const bool supportsDoublePrecision);
    ~CudaContext() override final;
    void init() override final;

    std::pair<std::size_t, bool> initiateCalculation(const std::size_t n, const std::size_t id = 0,
                                                     const std::size_t version = 0,
                                                     const Settings settings = {}) override final;
    void disposeCalculation(const std::size_t n) override final;
    std::size_t createInputVariable(double v) override final;
    std::size_t createInputVariable(double* v) override final;
    std::vector<std::vector<std::size_t>> createInputVariates(const std::size_t dim, const std::size_t steps) override final;
    std::size_t applyOperation(const std::size_t randomVariableOpCode,
                               const std::vector<std::size_t>& args) override final;
    void freeVariable(const std::size_t id) override final;
    void declareOutputVariable(const std::size_t id) override final;
    void finalizeCalculation(std::vector<double*>& output) override final;

    std::vector<std::pair<std::string, std::string>> deviceInfo() const override;
    bool supportsDoublePrecision() const override;
    const DebugInfo& debugInfo() const override final;

private:

    void cuCall(CUresult err, const std::string& description);
    void cudaCall(cudaError_t err, const std::string& description);
    void cusolverCall(cusolverStatus_t err, const std::string& descripion);
    void curandCall(curandStatus_t err, const std::string& description);
    void nvrtcCall(nvrtcResult err, const std::string& description);

    void releaseMem(double*& m, const std::string& desc);
    void releaseMem(int*& m, const std::string& desc);
    //void releaseMemCU(CUdeviceptr dptr, const std::string& description);
    void releaseModule(CUmodule& k, const std::string& desc);
    void releaseStream(cudaStream_t& stream, const std::string& desc);
    //void releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& desc);
    void updateVariatesMTGP32();
    //void updateVariatesMTGP32_dynamic();
    void updateVariatesMT19937();
    void updateVariatesMT19937_CPU();

    size_t binom_helper(size_t n, size_t k);

    enum class ComputeState { idle, createInput, createVariates, calc };

    cudaStream_t stream_;

    bool initialized_ = false;
    size_t device_;
    size_t NUM_THREADS_ = 256;
    
    std::vector<std::pair<std::string, std::string>> deviceInfo_;
    std::string gpuArchitecture_;
    bool supportsDoublePrecision_;
    size_t maxRandomVariates_;
    double* d_randomVariables_;
    //CUdeviceptr d_randomVariables_;

    // will be accumulated over all calcs
    ComputeContext::DebugInfo debugInfo_;

    // 1a vectors per current calc id

    std::vector<std::size_t> size_;
    std::vector<bool> hasKernel_;
    std::vector<bool> disposed_;
    std::vector<std::size_t> version_;
    std::vector<CUmodule> module_;
    std::vector<std::vector<CUfunction>> kernel_;
    std::vector<std::size_t> nRandomVariables_;
    std::vector<std::size_t> nOperations_;
    std::vector<std::size_t> nNUM_BLOCKS_;
    std::vector<std::size_t> nOutputVariables_;

    std::map<std::size_t, double*> dOutput_;

    // 2 curent calc

    std::size_t currentId_ = 0;
    ComputeState currentState_ = ComputeState::idle;
    std::size_t nInputVars_;
    Settings settings_;
    std::vector<size_t> inputVarOffset_;
    std::vector<double> inputVar_;

    // 2a indexed by var id
    std::vector<bool> inputVarIsScalar_;

    // 2b collection of variable ids
    std::vector<std::size_t> freedVariables_;
    std::vector<std::size_t> outputVariables_;

    // 2c variate states

    // 2d kernel source code
    std::vector<std::string> source_;

    // 3 MT19937 CPU version
    // 3 MT19937_CPU
    unsigned long long* d_mt_;
    CUmodule moduleMT19937_;
    CUfunction seedInitializationKernel_;
    CUfunction ore_twistKernel_;
    CUfunction ore_generateKernel_;

    // 4a Expectation
    std::vector<std::vector<int>> numOfExpectationInKernel_; // vector per calc id per kernel: -1 means initial kernel, -2 means expectation result id used on rhs, -3 means both intial kernel and expectation result id used on rhs, >0 means number of expectation in intermediate kernel
    std::vector<int> numOfExpectation_; //int per calc id: store the number of expectations which will be used for the size of eR
    std::string newKernelE_; // current calc: temp store the expection intermediate calculation of the next kernel
    std::vector<size_t>
        resultIdUnprocessedE_; // current calc: store the number of the result id returned from the expectation that have not been calculated
    std::vector<size_t>
        resultIdFlatE_; // current calc: store the number of the result id returned from the expectation that have been calculated
    size_t expectationCount_;
    size_t expectationKernelCount_;
    

    // 4b Conditional Expectation
    //size_t conditionalExpectationCount_;
    std::map<size_t, std::vector<std::vector<size_t>>> basisFunctionCE_; // vector per conditional expectation: store number of basis functions per conditional expectation
    std::vector<size_t> basisFunctionHelper_; // current calc: store the basis function number in the current kernel
    std::vector<size_t> lastResultIdCE_; // current calc: store the last result id used for all the operations before the conditional expectation
    std::vector<std::vector<size_t>> resultIdCE_; // current calc: store the number of the result id returned from the conditional expectation
    std::string newKernelCE_; // current calc: temp store the conditional expection calculation of the next kernel
    std::vector<size_t> idCopiedToValues_; // current calc: id that are needed in the future kernels are copied to the values
    std::map<size_t, size_t> valuesSize_; // values size per calc with conditional expectation: number of id stored in values
    std::vector<size_t> kernelOfIdCopiedToValues_; // current calc: store the kernel num of the if copied to values
};

CudaFramework::CudaFramework() {
    int nDevices;
    cuInit(0);
    cudaGetDeviceCount(&nDevices);
    for (std::size_t d = 0; d < nDevices; ++d) {
        cudaDeviceProp device_prop;
        cudaGetDeviceProperties(&device_prop, 0);
        std::string arch = "compute_" + std::to_string(device_prop.major * 10 + device_prop.minor);
        char device_name[MAX_N_NAME];
        std::vector<std::pair<std::string, std::string>> deviceInfo;
        contexts_["CUDA/DEFAULT/" + std::string(device_prop.name)] = new CudaContext(d, deviceInfo, arch, true);
    }
}

CudaFramework::~CudaFramework() {
    for (auto& [_, c] : contexts_) {
        delete c;
    }
}

CudaContext::CudaContext(
    size_t device, const std::vector<std::pair<std::string, std::string>>& deviceInfo,
                         const std::string gpuArchitecture, const bool supportsDoublePrecision)
    : initialized_(false), device_(device), deviceInfo_(deviceInfo),
      gpuArchitecture_(gpuArchitecture), supportsDoublePrecision_(supportsDoublePrecision) {}

CudaContext::~CudaContext() {
    if (initialized_) {
        //CUresult err;
        //cudaError_t cudaErr;
        //for (auto& ptr : dOutput_) {
        //    releaseMem(ptr.second, "~CudaContext()::");
        //}

        //releaseMem(dOutput_[0], "~CudaContext()::dOutput_[0]");
        //releaseMem(d_randomVariables_, "~CudaContext()::d_randomVariables_");
        //cudaErr = cudaFree(d_randomVariables_);
        //std::cout << "cudaFree(d_randomVariables_): " << cudaGetErrorString(cudaErr) << std::endl; 
        //releaseMemCU(d_randomVariables_, "~CudaContext()");


        for (Size i = 0; i < module_.size(); ++i) {
            if (disposed_[i])
                continue;
            releaseModule(module_[i], "~CudaContext()");
        }
        
        releaseStream(stream_, "~CudaContext()");
        //releaseMemCU(d_randomVariables_, "~CudaContext()");
        
    }
}

void CudaContext::cuCall(CUresult err, const std::string& description) {
    if (err != CUDA_SUCCESS) {
        const char* errorStr;
        cuGetErrorString(err, &errorStr);
        std::cerr << "CudaContext: CU error during currentId_ = " << currentId_ << ", location: " << description
                  << ", error: " << errorStr << std::endl;
    }
}

void CudaContext::cudaCall(cudaError_t err, const std::string& description) {
    QL_REQUIRE(err == cudaSuccess,
               "CudaContext: cuda error during currentId_ = " << currentId_ << ", location: " << description
                                                              << ", error: " << cudaGetErrorString(err));
}

void CudaContext::cusolverCall(cusolverStatus_t err, const std::string& description) {
    QL_REQUIRE(err == CUSOLVER_STATUS_SUCCESS, "CudaContext: cuSolver error during currentId_ = "
                                                   << currentId_ << ", location: " << description
                                                   << ", error: " << cusolverGetErrorString(err));
}

void CudaContext::curandCall(curandStatus_t err, const std::string& description) {
    QL_REQUIRE(err == CURAND_STATUS_SUCCESS,
               "CudaContext: cuRAND error during currentId_ = " << currentId_ << ", location: " << description
                                                                  << ", error: " << curandGetErrorString(err));
}

void CudaContext::nvrtcCall(nvrtcResult err, const std::string& description) {
    QL_REQUIRE(err == NVRTC_SUCCESS,
               "CudaContext: nvrtc error during currentId_ = " << currentId_ << ", location: " << description
                                                               << ", error: " << nvrtcGetErrorString(err));
}

void CudaContext::releaseMem(double*& m, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(m); err != cudaSuccess) {
        std::cerr << "CudaContext: error during cudaFree at "<< description
                  << ": " << cudaGetErrorName(err) << std::endl;
    }
}

void CudaContext::releaseMem(int*& m, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(m); err != cudaSuccess) {
        std::cerr << "CudaContext: error during cudaFree at " << description << ": " << cudaGetErrorName(err)
                  << std::endl;
    }
}
/*
void CudaContext::releaseMersenneTwisterStates(curandStateMtgp32*& rngState, const std::string& description) {
    cudaError_t err;
    if (err = cudaFree(rngState); err != cudaSuccess) {
        std::cerr << "CudaContext: error during releaseMersenneTwisterStates at " << description << ": "
                  << cudaGetErrorString(err) << std::endl;
    }
}
*/
void CudaContext::releaseModule(CUmodule& k, const std::string& description) {
    cuCall(cuModuleUnload(k), description);
    //CUresult err = cuModuleUnload(k);
    //if (err != CUDA_SUCCESS) {
    //    const char* errorStr;
    //    cuGetErrorString(err, &errorStr);
    //    std::cerr << "CudaContext: error during cuModuleUnload at " << description << ": " << errorStr << std::endl;
    //}
   // else {
   //    std::cout << "Module released successfully!" << std::endl;
   // }
}

//void CudaContext::releaseMemCU(CUdeviceptr dptr, const std::string& description) {
//    CUresult err = cuMemFree(dptr);
//    if (err != CUDA_SUCCESS) {
//        const char* errorStr;
//        cuGetErrorName(err, &errorStr);
//        std::cerr << "CudaContext: error during cuMemFree at " << description << ": " << errorStr << std::endl;
//    }
//    // else {
//    //    std::cout << "Module released successfully!" << std::endl;
//    // }
//}

void CudaContext::releaseStream(cudaStream_t& stream, const std::string& description) {
    cudaError_t err = cudaStreamDestroy(stream);
    if (err != CUDA_SUCCESS) {
        std::cerr << "CudaContext: error during cudaStreamDestroy at " << description << ": " << cudaGetErrorName(err)
                  << std::endl;
    }
    // else {
    //    std::cout << "Module released successfully!" << std::endl;
    // }
}

void CudaContext::init() {

    if (initialized_) {
        return;
    }

    debugInfo_.numberOfOperations = 0;
    debugInfo_.nanoSecondsDataCopy = 0;
    debugInfo_.nanoSecondsProgramBuild = 0;
    debugInfo_.nanoSecondsCalculation = 0;

    const char* errStr;
    maxRandomVariates_ = 0;

    // Initialize CUDA context and module
    
    //cuInit(0);

    cudaCall(cudaStreamCreate(&stream_), "init, cudaStreamCreate()");
    //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::init(): cudaStreamCreate() fails: " << cudaGetErrorString(cudaErr));
    //std::cout << "stream_ = " << stream_ << std::endl;
    
    initialized_ = true;
}

void CudaContext::disposeCalculation(const std::size_t id) {
    QL_REQUIRE(!disposed_[id - 1], "CudaContext::disposeCalculation(): id " << id << " was already disposed.");
    disposed_[id - 1] = true;
    releaseModule(module_[id - 1], "disposeCalculation");
}

std::pair<std::size_t, bool> CudaContext::initiateCalculation(const std::size_t n, const std::size_t id,
                                                              const std::size_t version, const Settings settings) {
    QL_REQUIRE(n > 0, "CudaContext::initiateCalculation(): n must not be zero");

    bool newCalc = false;
    settings_ = settings;

    if (id == 0) {

        // initiate new calcaultion

        size_.push_back(n);
        hasKernel_.push_back(false);
        disposed_.push_back(false);
        version_.push_back(version);
        //conditionalExpectationCount_ = 0;

        CUmodule cuModule;
        module_.push_back(cuModule);

        std::vector<CUfunction> function;
        kernel_.push_back(function);

        nNUM_BLOCKS_.push_back((n + NUM_THREADS_ - 1) / NUM_THREADS_);

        nOutputVariables_.push_back(0);
        nRandomVariables_.push_back(0);
        nOperations_.push_back(0);
        numOfExpectationInKernel_.emplace_back();
        numOfExpectationInKernel_.back().push_back(0);
        numOfExpectation_.push_back(0);

        currentId_ = hasKernel_.size();
        newCalc = true;

    } else {

        // initiate calculation on existing id

        QL_REQUIRE(id <= hasKernel_.size(),
                   "CudaContext::initiateCalculation(): id (" << id << ") invalid, got 1..." << hasKernel_.size());
        QL_REQUIRE(size_[id - 1] == n, "CudaContext::initiateCalculation(): size ("
                                           << size_[id - 1] << ") for id " << id << " does not match current size ("
                                           << n << ")");
        QL_REQUIRE(!disposed_[id - 1], "CudaContext::initiateCalculation(): id ("
                                           << id << ") was already disposed, it can not be used any more.");

        if (version != version_[id - 1]) {
            hasKernel_[id - 1] = false;
            version_[id - 1] = version;
            releaseModule(module_[id - 1], "initiateCalculation"); // releaseModule will also release the linked kernel
            nOutputVariables_[id - 1] = 0;
            nRandomVariables_[id - 1] = 0;
            nOperations_[id - 1] = 0;
            numOfExpectationInKernel_[id - 1].clear();
            numOfExpectationInKernel_[id - 1].push_back(0);
            numOfExpectation_[id - 1] = 0;
            if (basisFunctionCE_.find(id) != basisFunctionCE_.end()) {
                basisFunctionCE_[id].clear();
            }
            if (valuesSize_.find(id) != valuesSize_.end()) {
                valuesSize_[id] = 0;
            }
            newCalc = true;
        }

        currentId_ = id;
    }

    nInputVars_ = 0;
    inputVarIsScalar_.clear();
    inputVarOffset_.clear();
    inputVarOffset_.push_back(0);
    inputVar_.clear();

    freedVariables_.clear();
    outputVariables_.clear();

    lastResultIdCE_.clear();
    resultIdCE_.clear();
    resultIdCE_.emplace_back();
    resultIdUnprocessedE_.clear();
    resultIdFlatE_.clear();
    kernelOfIdCopiedToValues_.clear();
	source_.push_back("");
    basisFunctionHelper_.clear();
    basisFunctionHelper_.push_back(0);
    expectationCount_ = 0;
    expectationKernelCount_ = 0;
    idCopiedToValues_.clear();
    newKernelE_ = "";
    newKernelCE_ = "";

    // set state

    currentState_ = ComputeState::createInput;

    // return calc id

    return std::make_pair(currentId_, newCalc);
}

std::size_t CudaContext::createInputVariable(double v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                << ")");
    inputVarIsScalar_.push_back(true);
    inputVar_.push_back(v);
    inputVarOffset_.push_back(inputVarOffset_.back() + 1);
    return nInputVars_++;
}

std::size_t CudaContext::createInputVariable(double* v) {
    QL_REQUIRE(currentState_ == ComputeState::createInput,
               "CudaContext::createInputVariable(): not in state createInput (" << static_cast<int>(currentState_)
                                                                                  << ")");
    inputVarIsScalar_.push_back(false);
    inputVar_.reserve(inputVar_.size() + size_[currentId_ - 1]);
    inputVar_.insert(inputVar_.end(), v, v + size_[currentId_ - 1]);
    inputVarOffset_.push_back(inputVarOffset_.back() + size_[currentId_ - 1]);
    return nInputVars_++;
}

std::vector<std::vector<std::size_t>> CudaContext::createInputVariates(const std::size_t dim, const std::size_t steps) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates,
               "CudaContext::createInputVariable(): not in state createInput or createVariates ("
                   << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(currentId_ > 0, "CudaContext::createInputVariates(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::createInputVariates(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, input variates cannot be regenerated.");
    currentState_ = ComputeState::createVariates;

    std::vector<std::vector<std::size_t>> resultIds(dim, std::vector<std::size_t>(steps));
    for (std::size_t i = 0; i < dim; ++i) {
        for (std::size_t j = 0; j < steps; ++j) {
            resultIds[i][j] = nInputVars_ + nRandomVariables_[currentId_ - 1];
            nRandomVariables_[currentId_ - 1]++;
        }
    }

    //updateVariatesMTGP32();
    //updateVariatesMTGP32_dynamic();
    //updateVariatesMT19937();
    updateVariatesMT19937_CPU();

    return resultIds;
}

void CudaContext::updateVariatesMTGP32() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];
        //std::cout << "maxRandomVariates_ = " << maxRandomVariates_ << std::endl;

        // Allocate memory for random variables
        cudaCall(cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double)), "updateVariatesMTGP32, cudaMalloc(d_randomVariables_)");
        //QL_REQUIRE(cudaErr == cudaSuccess,
        //           "CudaContext::updateVariatesMTGP32(): memory allocate for d_randomVariables_ fails: "
        //               << cudaGetErrorString(cudaErr));
        /*
        cuErr = cuMemAlloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        if (cuErr != CUDA_SUCCESS) {
            const char* errorStr;
            cuGetErrorString(cuErr, &errorStr);
            std::cerr << "CudaContext: error during cuMemFree at " << "updateVariatesMTGP32()" << ": " << errorStr <<
        std::endl;
        };*/

        // Random number generator setup
        curandGenerator_t generator;
        curandCall(curandCreateGenerator(&generator, CURAND_RNG_PSEUDO_MTGP32), "updateVariatesMTGP32, curandCreateGenerator()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMTGP32(): error during curandCreateGenerator(): "
        //               << curandGetErrorString(curandErr));
        curandCall(curandSetPseudoRandomGeneratorSeed(generator, settings_.rngSeed), "updateVariatesMTGP32, curandSetPseudoRandomGeneratorSeed()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMTGP32(): error during curandSetPseudoRandomGeneratorSeed(): "
        //               << curandGetErrorString(curandErr));

        // Set stream
        curandCall(curandSetStream(generator, stream_), "updateVariatesMTGP32, curandSetStream()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMTGP32(): error during curandSetStream(): "
        //               << curandGetErrorString(curandErr));

        // Generator random numbers
        curandCall(curandGenerateNormalDouble(generator, d_randomVariables_, maxRandomVariates_, 0.0, 1.0), "updateVariatesMTGP32, curandGenerateNormalDouble()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMTGP32(): error during curandGenerateNormalDouble(): "
        //               << curandGetErrorString(curandErr));

        // Release generator
        curandCall(curandDestroyGenerator(generator), "updateVariatesMTGP32, curandDestroyGenerator()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMTGP32(): error during curandDestroyGenerator(): "
        //               << curandGetErrorString(curandErr));
    }
}
/*
void CudaContext::updateVariatesMTGP32_dynamic() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        cudaError_t cudaErr;
        curandStatus_t curandErr;
        CUresult cuErr;

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        // Allocate memory for random variables
        cudaErr = cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::createInputVariates(): memory allocate for d_randomVariables_ fails: "
                       << cudaGetErrorString(cudaErr));

        // For Mersenne Twister device API, at most 200 states can be generated one time.
        // If this passed the test, we will generate 2 separate mtStates for blocks after 200.
        // Need to test if using same seed will generate same random numbers
        QL_REQUIRE(nNUM_BLOCKS_[currentId_ - 1] <= 200,
                   "When using Mersenne Twister, at most 200 states can be generate now");

        // Random generator state
        curandStateMtgp32* mtStates;
        cudaErr = cudaMalloc(&mtStates, nNUM_BLOCKS_[currentId_ - 1] * sizeof(curandStateMtgp32));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::initMersenneTwisterRng(): memory allocate for mtStates_ fails: "
                       << cudaGetErrorString(cudaErr));

        // Define MTGP32 parameters
        mtgp32_kernel_params* kernelParams;
        cudaErr = cudaMalloc((void**)&kernelParams, sizeof(mtgp32_kernel_params));
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::initMersenneTwisterRng(): memory allocate for kernelParams fails: "
                       << cudaGetErrorString(cudaErr));

        // Initialize MTGP32 states
        curandErr = curandMakeMTGP32Constants(mtgp32dc_params_fast_11213, kernelParams);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32Constants(): "
                       << curandGetErrorString(curandErr));
        curandErr = curandMakeMTGP32KernelState(mtStates, mtgp32dc_params_fast_11213, kernelParams,
                                                nNUM_BLOCKS_[currentId_ - 1], settings_.rngSeed);
        QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
                   "CudaContext::initMersenneTwisterRng(): error during curandMakeMTGP32KernelState(): "
                       << curandGetErrorString(curandErr));

        std::string rngIncludeSource = "#include <curand_kernel.h>\n\n";

        std::string rngKernelName = "ore_rngKernel";

        std::string rngKernelSource = rngIncludeSource + "extern \"C\" __global__ void " + rngKernelName +
                                      "(curandStateMtgp32 *mtStates, int n,  double* randomVariables) {\n"
                                      "    int tid = blockIdx.x * blockDim.x + threadIdx.x;\n";
        for (size_t i = 0; i < nRandomVariables_[currentId_ - 1]; ++i) {
            rngKernelSource += "    double v" + std::to_string(i) + " = curand_normal_double(&mtStates[blockIdx.x]);\n";
            if (settings_.debug)
                debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
        }
        rngKernelSource += "    if (tid < n) {\n";
        for (size_t i = 0; i < nRandomVariables_[currentId_ - 1]; ++i) {
            rngKernelSource += "        randomVariables[tid + " + std::to_string(i * size_[currentId_ - 1]) + "] = v" +
                               std::to_string(i) + ";\n";
            if (settings_.debug)
                debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
        }
        rngKernelSource += "    }\n"
                           "}\n";

        std::cout << rngKernelSource << std::endl;
        // Compile source code
        nvrtcResult nvrtcErr;
        nvrtcProgram nvrtcProgram;
        CUmodule cuModule;
        CUfunction rngKernel;
        nvrtcErr = nvrtcCreateProgram(&nvrtcProgram, rngKernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcCreateProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        // const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    (std::string("--gpu-architecture=") + getGPUArchitecture[device_prop.name]).c_str(), " - std = c++ 17 ",
        //    nullptr};
        // const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
        //    "--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt", nullptr};
        // nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 5, compileOptions);
        const char* compileOptions[] = {
            "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
            "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
            //"--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt",
            nullptr};
        nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 4, compileOptions);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcCompileProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        // Retrieve the compiled PTX code
        size_t ptxSize;
        nvrtcErr = nvrtcGetPTXSize(nvrtcProgram, &ptxSize);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        char* ptx = new char[ptxSize];
        nvrtcErr = nvrtcGetPTX(nvrtcProgram, ptx);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcGetPTXSize(): "
                                                  << nvrtcGetErrorString(nvrtcErr));
        nvrtcErr = nvrtcDestroyProgram(&nvrtcProgram);
        QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::createInputVariates(): error during nvrtcDestroyProgram(): "
                                                  << nvrtcGetErrorString(nvrtcErr));

        cuErr = cuModuleLoadData(&cuModule, ptx);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleLoadData(): " << errStr << std::endl;
        }
        cuErr = cuModuleGetFunction(&rngKernel, cuModule, rngKernelName.c_str());
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::createInputVariates(): error during cuModuleGetFunction(): " << errStr
                      << std::endl;
        }
        delete[] ptx;

        // set kernel args

        void* args[] = {&mtStates, &size_[currentId_ - 1], &d_randomVariables_};

        // execute kernel
        cuErr = cuLaunchKernel(rngKernel, nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args, nullptr);
        if (cuErr != CUDA_SUCCESS) {
            const char* errStr;
            cuGetErrorString(cuErr, &errStr);
            std::cerr << "CudaContext::createInputVariates(): error during cuLaunchKernel(): " << errStr << std::endl;
        }

        releaseMersenneTwisterStates(mtStates, "updateVariatesMTGP32");
        releaseModule(cuModule, "updateVariatesMTGP32");
    }
*/
void CudaContext::updateVariatesMT19937() {

    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1]> maxRandomVariates_) {

        if (maxRandomVariates_ > 0)
            releaseMem(d_randomVariables_, "updateVariates()");

        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        // Allocate memory for random variables
        cudaCall(cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double)), "updateVariatesMT19937, cudaMalloc()");
        //QL_REQUIRE(cudaErr == cudaSuccess,
        //           "CudaContext::createInputVariates(): memory allocate for d_randomVariables_ fails: "
        //               << cudaGetErrorString(cudaErr));
        /*
        cuErr = cuMemAlloc(&d_randomVariables_,
                           nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double));
        if (cuErr != CUDA_SUCCESS) {
            const char* errorStr;
            cuGetErrorString(cuErr, &errorStr);
            std::cerr << "CudaContext: error during cuMemFree at " << "updateVariatesMTGP32()" << ": " << errorStr <<
        std::endl;
        };*/

        // Random number generator setup
        curandGenerator_t generator;
        curandCall(curandCreateGenerator(&generator, CURAND_RNG_PSEUDO_MT19937), "updateVariatesMT19937, curandCreateGenerator()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMT19937(): error during curandCreateGenerator(): "
        //               << curandGetErrorString(curandErr));
        curandCall(curandSetPseudoRandomGeneratorSeed(generator, settings_.rngSeed), "updateVariatesMT19937, curandSetPseudoRandomGeneratorSeed()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMT19937(): error during curandSetPseudoRandomGeneratorSeed(): "
        //               << curandGetErrorString(curandErr));

        // Set stream
        curandCall(curandSetStream(generator, stream_), "updateVariatesMT19937, curandSetStream()");
        //QL_REQUIRE(
        //    curandErr == CURAND_STATUS_SUCCESS,
        //    "CudaContext::updateVariatesMT19937(): error during curandSetStream(): " << curandGetErrorString(curandErr));

        // Generator random numbers
        curandCall(curandGenerateNormalDouble(generator, d_randomVariables_, maxRandomVariates_, 0.0, 1.0), "updateVariatesMT19937, curandGenerateNormalDouble()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMT19937(): error during curandGenerateNormalDouble(): "
        //               << curandGetErrorString(curandErr));

        // Release generator
        curandCall(curandDestroyGenerator(generator), "updateVariatesMT19937, curandDestroyGenerator()");
        //QL_REQUIRE(curandErr == CURAND_STATUS_SUCCESS,
        //           "CudaContext::updateVariatesMT19937(): error during curandDestroyGenerator(): "
        //               << curandGetErrorString(curandErr));
        /*
        // temp code
        
        // Copy random numbers back to host
        double* h_randomVariables = new double[nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1]];
        cudaErr = cudaMemcpyAsync(h_randomVariables, d_randomVariables_,
                                  nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1],
                                  cudaMemcpyDeviceToHost, stream_);
        QL_REQUIRE(cudaErr == cudaSuccess,
                   "CudaContext::updateVariatesMT19937(): memory copy for d_randomVariables_ to host fails: " << cudaGetErrorString(cudaErr));

        */
    }
}

void CudaContext::updateVariatesMT19937_CPU() {
    if (nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1] > maxRandomVariates_) {

        double* d_randomVariables_old;

        if (maxRandomVariates_ > 0) {

            cudaCall(cudaMalloc(&d_randomVariables_old, maxRandomVariates_ * sizeof(double)), "updateVariatesMT19937_CPU, cudaMalloc(d_randomVariables_old)");
            //QL_REQUIRE(cudaErr == cudaSuccess,
            //           "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_randomVariables_old fails: "
            //               << cudaGetErrorString(cudaErr));
            cudaCall(cudaMemcpyAsync(d_randomVariables_old, d_randomVariables_, maxRandomVariates_ * sizeof(double),
                                      cudaMemcpyDeviceToDevice, stream_), "updateVariatesMT19937_CPU, cudaMemcpyAsync(d_randomVariables_old, d_randomVariables_)");
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::updateVariatesMT19937_CPU(): memory copy from "
            //                                   "d_randomVariables_old to d_randomVariables_ fails: "
            //                                       << cudaGetErrorString(cudaErr));
            releaseMem(d_randomVariables_, "updateVariatesMT19937_CPU()");
        } else {
            // set kernel args
            cudaCall(cudaMalloc((void**)&d_mt_, sizeof(unsigned long long) * 624), "updateVariatesMT19937_CPU, cudaMalloc(d_mt_)");
            //QL_REQUIRE(cudaErr == cudaSuccess,
            //           "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_mt_ fails: "
            //               << cudaGetErrorString(cudaErr));
        }
        size_t previousVariates = maxRandomVariates_;
        maxRandomVariates_ = nRandomVariables_[currentId_ - 1] * size_[currentId_ - 1];

        if (previousVariates == 0) {
            // build the kernels to fill the variates pool

            std::string fpTypeStr = settings_.useDoublePrecision ? "double" : "float";
            std::string fpSuffix = settings_.useDoublePrecision ? "" : "f";
            std::string fpMaxValue = settings_.useDoublePrecision ? "0x1.fffffffffffffp1023" : "0x1.fffffep127f";

            // clang-format off
            // ported from from QuantLib::InverseCumulativeNormal
            std::string sourceInvCumN = "__device__ " + fpTypeStr + " ore_invCumN(const unsigned int x0) {\n"
                "    const " + fpTypeStr + " a1_ = -3.969683028665376e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a2_ = 2.209460984245205e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a3_ = -2.759285104469687e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a4_ = 1.383577518672690e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a5_ = -3.066479806614716e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " a6_ = 2.506628277459239e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b1_ = -5.447609879822406e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b2_ = 1.615858368580409e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b3_ = -1.556989798598866e+02" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b4_ = 6.680131188771972e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " b5_ = -1.328068155288572e+01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c1_ = -7.784894002430293e-03" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c2_ = -3.223964580411365e-01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c3_ = -2.400758277161838e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c4_ = -2.549732539343734e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c5_ = 4.374664141464968e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " c6_ = 2.938163982698783e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d1_ = 7.784695709041462e-03" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d2_ = 3.224671290700398e-01" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d3_ = 2.445134137142996e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " d4_ = 3.754408661907416e+00" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " x_low_ = 0.02425" + fpSuffix + ";\n"
                "    const " + fpTypeStr + " x_high_ = 1.0" + fpSuffix + " - x_low_;\n"
                "    const " + fpTypeStr + " x = ((" + fpTypeStr + ")x0 + 0.5" + fpSuffix + ") / 4294967296.0"+ fpSuffix + ";\n"
                "    if (x < x_low_ || x_high_ < x) {\n"
                "        if (x0 == 1U <<32 - 1) {\n"
                "          return " + fpMaxValue + ";\n"
                "        } else if(x0 == 0) {\n"
                "          return -" + fpMaxValue + ";\n"
                "        }\n"
                "        " + fpTypeStr + " z;\n"
                "        if (x < x_low_) {\n"
                "            z = sqrt(-2.0" + fpSuffix + " * log(x));\n"
                "            z = (((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
                "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0" + fpSuffix + ");\n"
                "        } else {\n"
                "            z = sqrt(-2.0f * log(1.0f - x));\n"
                "            z = -(((((c1_ * z + c2_) * z + c3_) * z + c4_) * z + c5_) * z + c6_) /\n"
                "                ((((d1_ * z + d2_) * z + d3_) * z + d4_) * z + 1.0" + fpSuffix + ");\n"
                "        }\n"
                "        return z;\n"
                "    } else {\n"
                "        " + fpTypeStr + " z = x - 0.5" + fpSuffix + ";\n"
                "        " + fpTypeStr + " r = z * z;\n"
                "        z = (((((a1_ * r + a2_) * r + a3_) * r + a4_) * r + a5_) * r + a6_) * z /\n"
                "            (((((b1_ * r + b2_) * r + b3_) * r + b4_) * r + b5_) * r + 1.0" + fpSuffix +");\n"
                "        return z;\n"
                "    }\n"
                "}\n\n";

            // from QuantLib::MersenneTwisterUniformRng

            std::string kernelSourceSeedInit = "extern \"C\" __global__ void ore_seedInitialization(const unsigned long long s, unsigned long long* mt) {\n"
                "  const unsigned long long N = 624;\n"
                "  mt[0]= s & 0xffffffffUL;\n"
                "  for (unsigned long long mti=1; mti<N; ++mti) {\n"
                "    mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);\n"
                "    mt[mti] &= 0xffffffffUL;\n"
                "  }\n"
                "}\n\n";

            std::string kernelSourceTwist = "extern \"C\" __global__ void ore_twist(unsigned long long* mt) {\n"
                " const unsigned long long N = 624;\n"
                " const unsigned long long M = 397;\n"
                " const unsigned long long MATRIX_A = 0x9908b0dfUL;\n"
                " const unsigned long long UPPER_MASK=0x80000000UL;\n"
                " const unsigned long long LOWER_MASK=0x7fffffffUL;\n"
                " const unsigned long long mag01[2]={0x0UL, MATRIX_A};\n"
                " unsigned long long kk;\n"
                " unsigned long long y;\n"
                " for (kk=0;kk<N-M;++kk) {\n"
                "     y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);\n"
                "     mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                " }\n"
                " for (;kk<N-1;kk++) {\n"
                "     y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);\n"
                "     mt[kk] = mt[(kk+M)-N] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                " }\n"
                " y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);\n"
                " mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];\n"
                "}\n\n";

            std::string kernelSourceGenerate =
                "extern \"C\" __global__ void ore_generate(const unsigned long long offset, unsigned long long* mt, " + fpTypeStr + "* output) {\n"
                "   unsigned long long mti = threadIdx.x + blockIdx.x * blockDim.x;\n"
                "   unsigned long long y = mt[mti];\n"
                "   y ^= (y >> 11);\n"
                "   y ^= (y << 7) & 0x9d2c5680UL;\n"
                "   y ^= (y << 15) & 0xefc60000UL;\n"
                "   y ^= (y >> 18);\n"
                "   output[offset + mti] = ore_invCumN((unsigned int)y);\n"
                "}\n\n";

            std::string rngKernelSource = sourceInvCumN + kernelSourceSeedInit + kernelSourceTwist + kernelSourceGenerate;

            //std::cout << rngKernelSource << std::endl;

            // Compile source code
            nvrtcProgram nvrtcProgram;
            nvrtcCall(nvrtcCreateProgram(&nvrtcProgram, rngKernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr), "updateVariatesMT19937_CPU(), nvrtcCreateProgram()");
            //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcCreateProgram(): "
            //                                          << nvrtcGetErrorString(nvrtcErr));

            const char* compileOptions[] = {
                //"--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
                "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on",
                //"--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt",
                nullptr};
            nvrtcCall(nvrtcCompileProgram(nvrtcProgram, 3, compileOptions), "updateVariatesMT19937_CPU(), nvrtcCompileProgram()");
      
   /*         size_t logSize;
            nvrtcGetProgramLogSize(nvrtcProgram, &logSize);
            std::vector<char> log(logSize);
            nvrtcGetProgramLog(nvrtcProgram, log.data());
            std::cout << log.data() << std::endl;*/

            //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcCompileProgram(): "
            //                                          << nvrtcGetErrorString(nvrtcErr));

            // Retrieve the compiled PTX code
            size_t ptxSize;
            nvrtcCall(nvrtcGetPTXSize(nvrtcProgram, &ptxSize), "updateVariatesMT19937_CPU(), nvrtcGetPTXSize()");
            //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcGetPTXSize(): "
            //                                          << nvrtcGetErrorString(nvrtcErr));
            char* ptx = new char[ptxSize];
            nvrtcCall(nvrtcGetPTX(nvrtcProgram, ptx), "updateVariatesMT19937_CPU(), nvrtcGetPTX()");
            //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcGetPTXSize(): "
            //                                          << nvrtcGetErrorString(nvrtcErr));
            nvrtcCall(nvrtcDestroyProgram(&nvrtcProgram), "updateVariatesMT19937_CPU(), nvrtcDestroyProgram()");
            //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::updateVariatesMT19937_CPU(): error during nvrtcDestroyProgram(): "
            //                                          << nvrtcGetErrorString(nvrtcErr));

            cuCall(cuModuleLoadData(&moduleMT19937_, ptx), "updateVariatesMT19937_CPU, cuModuleLoadData()");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleLoadData(): " << errStr << std::endl;
            //}
            cuCall(cuModuleGetFunction(&seedInitializationKernel_, moduleMT19937_, "ore_seedInitialization"), "updateVariatesMT19937_CPU, cuModuleGetFunction(ore_seedInitialization)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
            //              << std::endl;
            //}
            cuCall(cuModuleGetFunction(&ore_twistKernel_, moduleMT19937_, "ore_twist"),  "updateVariatesMT19937_CPU, cuModuleGetFunction(ore_twist)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
            //              << std::endl;
            //}
            cuCall(cuModuleGetFunction(&ore_generateKernel_, moduleMT19937_, "ore_generate"), "updateVariatesMT19937_CPU, cuModuleGetFunction(ore_generate)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuModuleGetFunction(): " << errStr
            //              << std::endl;
            //}

            delete[] ptx;
            // execute seed initialization kernel
            void* args[] = {&settings_.rngSeed, &d_mt_};
            cuCall(cuLaunchKernel(seedInitializationKernel_, 1, 1, 1, 1, 1, 1, 0, stream_, args, nullptr), "updateVariatesMT19937_CPU, cuLaunchKernel(seedInitializationKernel_)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            //}
        }
        maxRandomVariates_ = 624 * (maxRandomVariates_ / 624 +
                              (maxRandomVariates_ % 624 == 0 ? 0 : 1));

        // Allocate memory for random variables
        cudaCall(cudaMalloc(&d_randomVariables_, maxRandomVariates_ * sizeof(double)), "updateVariatesMT19937_CPU, cudaMalloc(d_randomVariables_)");
        //QL_REQUIRE(cudaErr == cudaSuccess,
        //           "CudaContext::updateVariatesMT19937_CPU(): memory allocate for d_randomVariables_ fails: "
        //               << cudaGetErrorString(cudaErr));
        if (previousVariates > 0) {
            cudaCall(cudaMemcpyAsync(d_randomVariables_, d_randomVariables_old, previousVariates * sizeof(double),
                                 cudaMemcpyDeviceToDevice, stream_), "updateVariatesMT19937_CPU, cudaMemcpyAsync(d_randomVariables_, d_randomVariables_old)");
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::updateVariatesMT19937_CPU(): memory copy from "
            //                                   "d_randomVariables_old to d_randomVariables_ fails: "
            //                                       << cudaGetErrorString(cudaErr));
            releaseMem(d_randomVariables_old, "updateVaraitesMT19937_CPU");
        }

        for (size_t currentVariates = previousVariates; currentVariates < maxRandomVariates_; currentVariates += 624) {
            void* args_twist[] = {&d_mt_};
            // execute seed initialization kernel
            cuCall(cuLaunchKernel(ore_twistKernel_, 1, 1, 1, 1, 1, 1, 0, stream_, args_twist, nullptr), "updateVariatesMT19937_CPU, cuLaunchKernel(ore_twistKernel_)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            //}

            void* args_generate[] = {&currentVariates, &d_mt_, &d_randomVariables_};
            cuCall(cuLaunchKernel(ore_generateKernel_, 1, 1, 1, 624, 1, 1, 0, stream_, args_generate, nullptr), "updateVariatesMT19937_CPU, cuLaunchKernel(ore_generateKernel_)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::updateVariatesMT19937_CPU(): error during cuLaunchKernel(): " << errStr << std::endl;
            //}
        }
        //std::cout << "max = " << maxRandomVariates_ << std::endl;
        // copy back to host
        //std::vector<double> temp_output(maxRandomVariates_);
        //cudaMemcpyAsync(temp_output.data(), d_randomVariables_, maxRandomVariates_ * sizeof(double), cudaMemcpyDeviceToHost, stream_);
        cudaStreamSynchronize(stream_);
        //for (size_t i = 0; i < 20; i++) {
        //    std::cout << "rng[" << i << "] = " << temp_output[i] << std::endl;
        //}
        
    }
}

std::size_t CudaContext::applyOperation(const std::size_t randomVariableOpCode,
                                          const std::vector<std::size_t>& args) {
    QL_REQUIRE(currentState_ == ComputeState::createInput || currentState_ == ComputeState::createVariates ||
                   currentState_ == ComputeState::calc,
               "CudaContext::applyOperation(): not in state createInput or calc (" << static_cast<int>(currentState_)
                                                                                     << ")");
    currentState_ = ComputeState::calc;
    QL_REQUIRE(currentId_ > 0, "CudaContext::applyOperation(): current id is not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::applyOperation(): id (" << currentId_ << ") in version "
                                                                                    << version_[currentId_ - 1]
                                                                                    << " has a kernel already.");
    // Check if variable id is in conditional expectation result id
    if (!resultIdCE_.back().empty()) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (std::find(resultIdCE_.back().begin(), resultIdCE_.back().end(), args[i]) != resultIdCE_.back().end()) {
                // end the current kernel, start a new kernel
                lastResultIdCE_.push_back(nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]);
                freedVariables_.clear(); // test
			    source_.push_back(newKernelCE_);
                //std::cout << "source_.size() after source_.push_back(newKernelCE_); = " << source_.size() << std::endl;
                numOfExpectationInKernel_[currentId_ - 1].push_back(0);
                newKernelCE_ = "";
                basisFunctionCE_[currentId_].push_back(basisFunctionHelper_);
                basisFunctionHelper_.clear();
                basisFunctionHelper_.push_back(0);
                resultIdCE_.emplace_back();
            }
        }
    } 
    // Check if variable id is in expectation result id
    if (!resultIdUnprocessedE_.empty()) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (std::find(resultIdUnprocessedE_.begin(), resultIdUnprocessedE_.end(), args[i]) != resultIdUnprocessedE_.end()) {
                numOfExpectationInKernel_[currentId_ - 1].push_back(expectationCount_);
                expectationCount_ = 0;
                ++expectationKernelCount_;
                source_.push_back(newKernelE_);
                //std::cout << "source_.size() after source_.push_back(newKernelE_) = " << source_.size() << std::endl;
                source_.push_back(newKernelCE_);
                numOfExpectationInKernel_[currentId_ - 1].push_back(0);
                lastResultIdCE_.push_back(nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]);
                lastResultIdCE_.push_back(nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]);
                //std::cout << "source_.size() after source_.push_back(""); = " << source_.size() << std::endl;
                newKernelCE_ = "";
                newKernelE_ = "";
                basisFunctionCE_[currentId_].push_back(basisFunctionHelper_);
                basisFunctionHelper_.clear();
                basisFunctionHelper_.push_back(0);
                resultIdCE_.emplace_back();
                resultIdCE_.emplace_back();
                // Add all unprocessed id into flat id list
                resultIdFlatE_.insert(resultIdFlatE_.end(), resultIdUnprocessedE_.begin(), resultIdUnprocessedE_.end());
                resultIdUnprocessedE_.clear();
                freedVariables_.clear();
       //         // end the current kernel, start a new kernel
       //         lastResultIdCE_.push_back(nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]);
       //         freedVariables_.clear(); // test
			    //source_.push_back(newKernelCE_);
       //         expectationCount_.push_back(0);
       //         isExpectationKernel_.back().push_back(false);
       //         newKernelCE_ = "";
			    //hasExpectation_.push_back(false);
       //         basisFunctionCE_[currentId_].push_back(basisFunctionHelper_);
       //         basisFunctionHelper_.clear();
       //         basisFunctionHelper_.push_back(0);
       //         resultIdCE_.emplace_back();
            }
        }
    } 

    // determine variable id to use for result
    std::size_t resultId;
    bool resultIdNeedsDeclaration;
    if (!freedVariables_.empty()) {
        resultId = freedVariables_.back();
        freedVariables_.pop_back();
        resultIdNeedsDeclaration = false;
    } else {
        resultId = nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1];
        ++nOperations_[currentId_ - 1];
        resultIdNeedsDeclaration = true;
    }

    std::vector<std::string> argStr(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        // arg is input variable
        if (args[i] < inputVarIsScalar_.size()) {
            argStr[i] = "input[" + std::to_string(inputVarOffset_[args[i]]) +
                        (inputVarIsScalar_[args[i]] ? "" : " + tid") + "]";
            //source_ += "printf(\"" + argStr[i] + " = %.5f\\n\", " + argStr[i] + ");";
        }
        // arg is random variable
        else if (args[i] < inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]){
            argStr[i] = "randomVariables[tid + " + std::to_string((args[i] - inputVarIsScalar_.size())  * size_[currentId_ - 1]) + "]";
        }
        // arg larger than input + random variable
        else{
            // first kernel
            if (lastResultIdCE_.empty()){
                argStr[i] = "v" + std::to_string(args[i]);
            }
            // not first kernel (i.e. have conditional expectation in previous kernel)
            else {
                // id larger than the last result id in the previous kernel, not read from values
                if (args[i] >= lastResultIdCE_.back()) {
                    argStr[i] = "v" + std::to_string(args[i]);
                }
                else {
                    // id is expectation result, read from eR
                    auto eIt = std::find(resultIdFlatE_.begin(), resultIdFlatE_.end(), args[i]);
                    if (eIt != resultIdFlatE_.end()){
                        argStr[i] = "eR[" + std::to_string(std::distance(resultIdFlatE_.begin(), eIt)) + "]";
                        switch (numOfExpectationInKernel_[currentId_ - 1].back()) {
                            case -1:
                                numOfExpectationInKernel_[currentId_ - 1].back() = -3;
                                break;
                            case -2:
                            case -3:
                                break;
                            default:
                                numOfExpectationInKernel_[currentId_ - 1].back() = -2;
                                break;
                        }
                    }
                    else {
                        // id is conditional expectation calculcated at the beginning of this kernel, not read from values
                        std::vector<size_t> secToLast;
                        //std::cout << "here source_.size() = " << source_.size() << std::endl;
                        //std::cout << "here numOfExpectationInKernel_[currentId_ - 1].size() = " << numOfExpectationInKernel_[currentId_ - 1].size() << std::endl;
                        secToLast = resultIdCE_[resultIdCE_.size() - (numOfExpectationInKernel_[currentId_ - 1][numOfExpectationInKernel_[currentId_ - 1].size()-2] > 0?3:2)];
                        if (std::find(secToLast.begin(), secToLast.end(), args[i]) != secToLast.end())
                            argStr[i] = "v" + std::to_string(args[i]);
                        else {
                            // id read from values
                            auto iter = std::find(idCopiedToValues_.begin(), idCopiedToValues_.end(), args[i]);
                            if (iter != idCopiedToValues_.end())
                                argStr[i] = "values[tid + " + std::to_string(size_[currentId_ - 1] * (std::distance(idCopiedToValues_.begin(), iter))) + "]";
                            else {
                                idCopiedToValues_.push_back(args[i]);
                                auto ub = std::distance(lastResultIdCE_.begin(), std::upper_bound(lastResultIdCE_.begin(), lastResultIdCE_.end(), args[i]));
                                if (std::find(resultIdCE_[ub].begin(), resultIdCE_[ub].end(), args[i]) == resultIdCE_[ub].end())
                                    kernelOfIdCopiedToValues_.push_back(ub);
                                else
                                    kernelOfIdCopiedToValues_.push_back(ub + 1);
                                argStr[i] = "values[tid + " + std::to_string(size_[currentId_ - 1] * (idCopiedToValues_.size())) + "]";
                            }
                        }
                    }
                }
            }
            
        }
    }
    //if (randomVariableOpCode == RandomVariableOpCode::ConditionalExpectation) {
    //    std::cout << "source_.size() =" << source_.size() << std::endl;
    //    //std::cout << "source_[0] = " << source_[0] << std::endl;
    //}
    if (randomVariableOpCode != RandomVariableOpCode::ConditionalExpectation) {

        // generate source code
        if (resultIdNeedsDeclaration)
            source_.back() += "        double v" + std::to_string(resultId) + " = ";
        else
            source_.back() += "        v" + std::to_string(resultId) + " = ";

        switch (randomVariableOpCode) {
            case RandomVariableOpCode::None: {
            }
            case RandomVariableOpCode::Add: {
                source_.back() += argStr[0];
                for (size_t i = 1;i < argStr.size(); i++) {
                    source_.back() += " + " + argStr[i];
                }
                source_.back() += ";\n";
                break;
            }
            case RandomVariableOpCode::Subtract: {
                source_.back() += argStr[0] + " - " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::Negative: {
                source_.back() += "-" + argStr[0] + ";\n";
                break;
            }
            case RandomVariableOpCode::Mult: {
                source_.back() += argStr[0] + " * " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::Div: {
                source_.back() += argStr[0] + " / " + argStr[1] + ";\n";
                break;
            }
            case RandomVariableOpCode::IndicatorEq: {
                source_.back() += "ore_indicatorEq(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::IndicatorGt: {
                source_.back() += "ore_indicatorGt(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::IndicatorGeq: {
                source_.back() += "ore_indicatorGeq(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Min: {
                source_.back() += "fmin(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Max: {
                source_.back() += "fmax(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::Abs: {
                source_.back() += "fabs(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Exp: {
                source_.back() += "exp(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Sqrt: {
                source_.back() += "sqrt(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Log: {
                source_.back() += "log(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::Pow: {
                source_.back() += "pow(" + argStr[0] + "," + argStr[1] + ");\n";
                break;
            }
            case RandomVariableOpCode::NormalCdf: {
                source_.back() += "normcdf(" + argStr[0] + ");\n";
                break;
            }
            case RandomVariableOpCode::NormalPdf: {
                source_.back() += "normpdf(" + argStr[0] + ");\n";
                break;
            }
            default: {
                QL_FAIL("CudaContext::executeKernel(): no implementation for op code "
                        << randomVariableOpCode << " (" << getRandomVariableOpLabels()[randomVariableOpCode] << ") provided.");
            }
        }
        //std::cout << "Added Normal Operation" << std::endl;
    }
    // Conditional Expectation
    else {
        //std::cout << "Start Conditional Expectation part" << std::endl;
        QL_REQUIRE(args.size() >= 2,
                   "CudaContext::applyOperation() ConditionalExpectation args.size() must be >= 2, got" << args.size());
        if (args.size() == 2){
            //std::cout << "Add Expectation" << std::endl;
            QL_REQUIRE(size_[currentId_ - 1] <= 65536,
                   "CudaContext::applyOperation() Expectation only support less than 65536 samples, got" << size_[currentId_ - 1]);
            // Expectation
            ++numOfExpectation_[currentId_ - 1];
            switch (numOfExpectationInKernel_[currentId_ - 1].back()) {
                case -2:
                    numOfExpectationInKernel_[currentId_ - 1].back() = -3;
                    break;
                case -1:
                case -3:
                    break;
                default:
                    numOfExpectationInKernel_[currentId_ - 1].back() = -1;
                    break;
            }
			source_.back() += "     if (ore_closeEnough(" + argStr[1] + ", 1.0)) partialSum[threadIdx.x] = " + argStr[0] + ";\n"
                              "     else partialSum[threadIdx.x] = 0;\n"
                              "      __syncthreads();\n"
                              "     for (int s = blockDim.x / 2; s > 0; s >>= 1) {\n"
                              "         if (threadIdx.x < s) partialSum[threadIdx.x] += partialSum[threadIdx.x + s];\n"
                              "         __syncthreads();\n"
                              "     }\n"
                              "     if (threadIdx.x == 0) e[blockIdx.x + " + std::to_string(nNUM_BLOCKS_[currentId_ - 1] * expectationCount_) + "] = partialSum[0];\n";
            newKernelE_ +=  "   if (blockIdx.x == " + std::to_string(expectationCount_) + ") {\n"
                            "       if (threadIdx.x < " + std::to_string(nNUM_BLOCKS_[currentId_ - 1]) + ") partialSum[threadIdx.x] = e[threadIdx.x + " + std::to_string(nNUM_BLOCKS_[currentId_ - 1] * expectationCount_) + "];\n"
                            "       __syncthreads();\n"
                            "       for (int s = " + std::to_string(static_cast<int>(std::pow(2, (std::ceil(std::log2(nNUM_BLOCKS_[currentId_ - 1])) - 1)))) + "; s > 0; s >>= 1) {\n"
                            "            if (threadIdx.x < s && threadIdx.x + s < " + std::to_string(nNUM_BLOCKS_[currentId_ - 1]) + ") partialSum[threadIdx.x] += partialSum[threadIdx.x + s];\n"
                            "           __syncthreads();\n"
                            "       }\n"
                            "       eR[" + std::to_string(resultIdFlatE_.size() + expectationCount_) + "] = partialSum[0] / " + std::to_string(size_[currentId_ - 1]) + ";\n"
                            "   }\n";
            //std::cout << "newKernelE_ = " << newKernelE_ << std::endl;
            ++expectationCount_;
            resultIdUnprocessedE_.push_back(resultId);
			           //                              "      __syncthreads();\n"
													 //"      if (ore_closeEnough(" + argStr[1] + ", 1.0)) atomicAdd(&partialSum, " + argStr[0] + ");\n"
              //  		                             "      __syncthreads();\n"
              //                                       "      if (threadIdx.x == 0) mean = 0;\n"
              //                                       "      if (threadIdx.x == 0) atomicAdd(&globalSum, partialSum);\n"
              //                                       "      __syncthreads();\n"
              //                                       //"      printf(\"blockIdx = %d, threadIdx = %d, partialSum = %.5f\\n\", blockIdx.x, threadIdx.x, partialSum);"
              //                                       //"      printf(\"blockIdx = %d, threadIdx = %d, globalSum = %.5f\\n\", blockIdx.x, threadIdx.x, globalSum);"  
              //                                       "      if (threadIdx.x == 0) {\n"
              //                                       "          mean = globalSum / " + std::to_string(size_[currentId_ - 1]) + ";\n"
              //                                       "          globalSum = 0.0;\n"
              //                                       "      }\n"
              //                                       "      __syncthreads();\n"
              //                                       //"      printf(\"blockIdx = %d, threadIdx = %d, mean = %.5f\\n\", blockIdx.x, threadIdx.x, mean);"
              //                                       "      " + (resultIdNeedsDeclaration?"double v":"v") + std::to_string(resultId) + " = mean;\n";
              //                                       //"      printf(\"blockIdx = %d, threadIdx = %d, v6 = %.5f\\n\", blockIdx.x, threadIdx.x, v6);";
            //std::cout << "End Expectation" << std::endl;
            //std::cout << "source_.size() =" << source_.size() << std::endl;
            //std::cout << "source_[0] = " << source_[0] << std::endl;
        } else {
            //std::cout <<"Add Conditional Expectation" << std::endl;
            // Conditional Expectation
            // calculate order
            size_t order = settings_.regressionOrder;
            size_t regressorSize = args.size() - 2;
            while (RandomVariableLsmBasisSystem::size(regressorSize, order) > static_cast<Real>(size_[currentId_ - 1]) && order > 1) {
                --order;
            }
            size_t basisFunctionSize = binom_helper(order + regressorSize, order);
            //std::cout <<"here4" << std::endl;
            // string name
            std::vector<std::string> strA(basisFunctionSize);
            std::vector<std::string> strA_unfilter(basisFunctionSize);
            for (size_t i = 0; i < basisFunctionSize; i++){
                // row-major
                //strA[i] = "A[tid * " + std::to_string(basisFunctionSize)  + " + " + std::to_string(i) + "]";
                //strA_unfilter[i] = "A_unfilter" + std::to_string(source_.size() - 1) + "[tid * " + std::to_string(basisFunctionSize)  + " + " + std::to_string(i) + "]";
                // column-major
                strA[i] = "A[tid + " + std::to_string(size_[currentId_ - 1] * (i + basisFunctionHelper_.back())) + "]";
                strA_unfilter[i] = "A_unfilter" + std::to_string(source_.size() - 1 - expectationKernelCount_) + "[tid + " + std::to_string(size_[currentId_ - 1] * (i + basisFunctionHelper_.back())) + "]";
            }
            //std::cout <<"here5" << std::endl;
            // calculate basis function, write to A_unfilter
            //degree 0
            source_.back() += "      " + strA_unfilter[0] + " = 1.0;\n";
            //std::cout <<"degree0" << std::endl;
            // degree 1
            for (size_t i = 2; i < args.size(); ++i) {
                source_.back() += "      " + strA_unfilter[i-1] + " = " + argStr[i] + ";\n";
            }
            //std::cout <<"degree1" << std::endl;
            // degree 2+
            if (order >= 2) {
                std::vector<size_t> start_point(regressorSize);
                size_t currentPosition = regressorSize + 1;
                size_t newStartPosition = 0;
                for (size_t n = 0; n < regressorSize; n++){
                    start_point[n] = n + 1;
                }
                for (size_t d = 2; d <= order; ++d) {
                    for (size_t n = 0; n < regressorSize; ++n){
                        newStartPosition = currentPosition;
                        for (size_t sp = start_point[n]; sp <= start_point.back();++sp){
                            source_.back() += "      " + strA_unfilter[currentPosition] + " = " + strA_unfilter[n + 1] + " * " + strA_unfilter[sp] + ";\n";
                            currentPosition++;
                        }
                        start_point[n] = newStartPosition;
                    }
                    //std::cout <<"degree2+" << std::endl;
                }
            }
            // apply filter, write to A
            source_.back() += "      if (ore_closeEnough(" + argStr[1] + ", 1.0)) {\n";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                source_.back() += "          " + strA[i] + " = " + strA_unfilter[i] + ";\n";
            }
            source_.back() += "      } else {\n";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                source_.back() += "          " + strA[i] + " = 0.0;\n";
            }
            source_.back() += "      }\n";
            //std::cout <<"here6" << std::endl;
            // calculate vector b
            std::string strB = "b[tid + " + std::to_string(size_[currentId_ - 1] * (basisFunctionHelper_.size() - 1)) + "]";
            source_.back() += "      if (ore_closeEnough(" + argStr[1] + ", 1.0)) {\n"
                                                     "          " + strB + " = " + argStr[0] + ";\n"
                                                     "      } else {\n"
                                                     "          " + strB + " = 0.0;\n"
                                                     "      }\n";

            //debug code
            //source_.back() += "      if (tid == 0) printf(\"A[0] = %.8f\\n\", A[0]);\n"
            //    "      if (tid == 0) printf(\"A[1] = %.8f\\n\", A[1]);\n"
            //    "      if (tid == 0) printf(\"A[2] = %.8f\\n\", A[2]);\n";

            // end the current kernel, start the next kernel
            //conditionalExpectationCount_++;
            //std::cout <<"here7" << std::endl;
            // Calculate the new y in the next kerenl
            //source_.back() += "if (tid == 0) printf(\"run output kerenl\\n\");\n";
            newKernelCE_ += "      double v" + std::to_string(resultId) + " = ";
            for (size_t i = 0; i < basisFunctionSize; ++i) {
                newKernelCE_ += strA_unfilter[i] + " * X[" + std::to_string(i + basisFunctionHelper_.back()) + "] + ";
            }
            newKernelCE_.erase(newKernelCE_.end()-3, newKernelCE_.end());
            newKernelCE_ += ";\n";
            // store result id
            basisFunctionHelper_.push_back(basisFunctionHelper_.back() + basisFunctionSize);
            resultIdCE_.back().push_back(resultId);
            //for (size_t i = 0; i < basisFunctionSize; ++i) {
            //    source_.back()+= "if (tid == 0) printf(\"X[" + std::to_string(i) + "] = %.8f\\n\", X[" + std::to_string(i) +"]);\n";
            //}
            //std::cout <<"here end" << std::endl;
        }
    }
    // update num of ops in debug info
    if (settings_.debug)
        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];

    // return result id
    return resultId;
}

void CudaContext::freeVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ == ComputeState::calc,
               "CudaContext::free(): not in state calc (" << static_cast<int>(currentState_) << ")");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::freeVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, free variable cannot be called.");

    // we do not free input variables, random numbers and any result ID assigned in the previous kernel, 
    // only variables that were added during the current kernel can be freed.
    //std::cout << "freeVariable::id = " << id << std::endl;
    //std::cout << "freeVariable::(numResultIdCE_.back() + inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]) = " << (numResultIdCE_.back() + inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]) << std::endl;
    if (id < (inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1])||(lastResultIdCE_.empty()? false: (id <= (lastResultIdCE_.back()))))
        return;

    freedVariables_.push_back(id);
}

void CudaContext::declareOutputVariable(const std::size_t id) {
    QL_REQUIRE(currentState_ != ComputeState::idle, "CudaContext::declareOutputVariable(): state is idle");
    QL_REQUIRE(currentId_ > 0, "CudaContext::declareOutputVariable(): current id not set");
    QL_REQUIRE(!hasKernel_[currentId_ - 1], "CudaContext::declareOutputVariable(): id ("
                                                << currentId_ << ") in version " << version_[currentId_ - 1]
                                                << " has a kernel already, output variables cannot be redeclared.");
    //nOutputVariables_[currentId_ - 1].push_back(id);
    outputVariables_.push_back(id);
}

void CudaContext::finalizeCalculation(std::vector<double*>& output) {
    //std::cout << "Start finalizeCalculation()" << std::endl;
    struct exitGuard {
        exitGuard() {}
        ~exitGuard() {
            *currentState = ComputeState::idle;
            for (auto& m : mem)
                cudaFree(m);
        }
        ComputeState* currentState;
        std::vector<double*> mem;
    } guard;

    guard.currentState = &currentState_;
    QL_REQUIRE(currentId_ > 0, "CudaContext::finalizeCalculation(): current id is not set");

    if (!hasKernel_[currentId_ - 1])
        nOutputVariables_[currentId_ - 1] = outputVariables_.size();

    boost::timer::cpu_timer timer;
    boost::timer::nanosecond_type timerBase;

    // allocate and copy memory for input to device

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }
    //std::cout << "inputVariables_ = " << inputVarIsScalar_.size() <<std::endl;
    //std::cout << "randomVariables_ = " << nRandomVariables_[currentId_ - 1] << std::endl;
    double* input;
    size_t inputSize = inputVarOffset_.back() * sizeof(double);
    cudaCall(cudaMalloc(&input, inputSize), "finalizeCalculation, cudaMalloc(&input)");
    //QL_REQUIRE(cudaErr == cudaSuccess,
    //           "CudaContext::finalizeCalculation(): memory allocate for input fails: " << cudaGetErrorString(cudaErr));
    double* h_input;
    cudaCall(cudaMallocHost(&h_input, inputSize), "finalizeCalculation, cudaMallocHost(&h_input)");
    //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for h_input fails: "
    //                                       << cudaGetErrorString(cudaErr));
    std::copy(inputVar_.begin(), inputVar_.end(), h_input);

    cudaCall(cudaMemcpyAsync(input, h_input, inputSize, cudaMemcpyHostToDevice, stream_), "finalizeCalculation, cudaMemcpyAsync(input, h_input)");
    //cudaErr = cudaMemcpy(input, h_input, inputSize, cudaMemcpyHostToDevice);
    //QL_REQUIRE(cudaErr == cudaSuccess,
    //           "CudaContext::finalizeCalculation(): memory copy for input fails: " << cudaGetErrorString(cudaErr));
    
    // Allocate memory for output
    if (dOutput_.count(nOutputVariables_[currentId_ - 1]) == 0) {
        double* d_output;
        cudaCall(cudaMalloc(&d_output, nOutputVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation, cudaMalloc(d_output)");
        //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for d_output fails: "
        //                                       << cudaGetErrorString(cudaErr));
        dOutput_[nOutputVariables_[currentId_ - 1]] = d_output;
    }
    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }

    // build kernel if necessary

    if (!hasKernel_[currentId_ - 1]) {
        //std::cout << "Start building kernel" << std::endl;
        std::string includeSource =
            "__device__ bool ore_closeEnough(const double x, const double y) {\n"
            "    double tol = 42.0 * 0x1.0p-52;\n"
            "    double diff = fabs(x - y);\n"
            "    if (x == 0.0 || y == 0.0)\n"
            "        return diff < tol * tol;\n"
            "    return diff <= tol * fabs(x) || diff <= tol * fabs(y);\n"
            "}\n\n"
            "__device__ double ore_indicatorEq(const double x, const double y) { return ore_closeEnough(x, y) ? 1.0 : "
            "0.0; }\n\n"
            "__device__ double ore_indicatorGt(const double x, const double y) { return x > y && !ore_closeEnough(x, "
            "y); }\n\n"
            "__device__ double ore_indicatorGeq(const double x, const double y) { return x > y || ore_closeEnough(x, "
            "y); }\n\n"
            "__device__ double normpdf(const double x) { return exp(-0.5 * x * x) / sqrt(2.0 * 3.1415926535897932384626); }\n\n";

        //std::cout << "source_.size() = " << source_.size() << std::endl;
        //std::cout << "conditionalExpectationCount_ = " << conditionalExpectationCount_ << std::endl;
        // if newKernelE_ is not empty, add a new kernel
        if (!newKernelE_.empty()) {
            //std::cout << "newKernelCE_ = " << newKernelCE_ << std::endl;
            ++expectationKernelCount_;
            source_.push_back(newKernelE_);
            //std::cout << "source_.size() after source_.push_back(newKernelE_) in finalizeCalculation = " << source_.size() << std::endl;
            numOfExpectationInKernel_[currentId_ - 1].push_back(expectationCount_);
            //lastResultIdCE_.push_back(lastResultIdCE_.back());
            //auto lastResultIdCE_ = resultIdCE_.back();
            //resultIdCE_.back().clear();
            //resultIdCE_.push_back(lastResultIdCE_);
            resultIdCE_.emplace_back();
            basisFunctionCE_[currentId_].push_back(basisFunctionHelper_);
            basisFunctionHelper_.clear();
            std::vector<std::vector<size_t>> basisFunction = basisFunctionCE_[currentId_];
            //std::cout << "newKernelE_ not empty: basisFunction.size() = " << basisFunction.size() << std::endl;
            //for (size_t ii = 0; ii < basisFunction.size(); ++ii) {
            //    for (size_t ij = 0; ij < basisFunction[ii].size(); ++ij) {
            //        std::cout << "basisFunction[" << ii << "][" << ij << "] = " << basisFunction[ii][ij] << std::endl;
            //    }
            //}
            resultIdFlatE_.insert(resultIdFlatE_.end(), resultIdUnprocessedE_.begin(), resultIdUnprocessedE_.end());
            resultIdUnprocessedE_.clear();
            //for (auto const & p: basisFunctionCE_) {
            //    std::cout << "basisFunctionCE_ key = " << p.first << std::endl;
            //    std::cout << "basisFunctionCE_ value[0].size() = " << p.second[0].size() << std::endl;
            //    for (size_t i = 0; i < p.second[0].size(); ++i)
            //        std::cout << "if !newKernelCE_.empty basisFunctionCE_[currentId_][" << i << "] = " << p.second[0][i] << std::endl;
            //}
        }
        // if newKernelCE_ is not empty, add a new kernel
        if (!newKernelCE_.empty()) {
            //std::cout << "newKernelCE_ = " << newKernelCE_ << std::endl;
            if (numOfExpectationInKernel_[currentId_ - 1].size() > 1 && numOfExpectationInKernel_[currentId_ - 1][numOfExpectationInKernel_[currentId_ - 1].size()-2] > 0)
                source_.back() +=  newKernelCE_;
            else {
                source_.push_back(newKernelCE_);
                numOfExpectationInKernel_[currentId_ - 1].push_back(0);
                basisFunctionCE_[currentId_].push_back(basisFunctionHelper_);
            }
            //std::cout << "source_.size() after source_.push_back(newKernelCE_) in finalizeCalculation = " << source_.size() << std::endl;
            //numOfExpectationInKernel_[currentId_ - 1].push_back(0);
            //lastResultIdCE_.push_back(nInputVars_ + nRandomVariables_[currentId_ - 1] + nOperations_[currentId_ - 1]);
            resultIdCE_.emplace_back();
            std::vector<std::vector<size_t>> basisFunction = basisFunctionCE_[currentId_];
            //std::cout << "newKernelCE_ not empty:basisFunction.size() = " << basisFunction.size() << std::endl;
            //for (size_t ii = 0; ii < basisFunction.size(); ++ii) {
            //    for (size_t ij = 0; ij < basisFunction[ii].size(); ++ij) {
            //        std::cout << "basisFunction[" << ii << "][" << ij << "] = " << basisFunction[ii][ij] << std::endl;
            //    }
            //}
            basisFunctionHelper_.clear();
            //for (auto const & p: basisFunctionCE_) {
            //    std::cout << "basisFunctionCE_ key = " << p.first << std::endl;
            //    std::cout << "basisFunctionCE_ value[0].size() = " << p.second[0].size() << std::endl;
            //    for (size_t i = 0; i < p.second[0].size(); ++i)
            //        std::cout << "if !newKernelCE_.empty basisFunctionCE_[currentId_][" << i << "] = " << p.second[0][i] << std::endl;
            //}
        }
        std::vector<std::string> kernelName(source_.size());
        std::string kernelSource;
        if (source_.size() == 1) {
            // expectation
            //if (hasExpectation_[0]) {
            //    includeSource += "__device__ double globalSum = 0.0;\n";
            //                     //"__device__ double mean = 0.0;\n";          
            //}
            // No conditional expectation
            kernelName[0] =
            "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]);

            kernelSource = includeSource + "extern \"C\" __global__ void " + kernelName[0] +
                                       "(const double* input, double* output, const double* randomVariables) {\n"
                                       "    int tid = blockIdx.x * blockDim.x + threadIdx.x;\n"
                                       "    if (tid < " + std::to_string(size_[currentId_ - 1]) + ") {\n";
            //for (size_t id = 0; id < nRandomVariables_[currentId_ - 1]; ++id) {
            //    // original version
            //    kernelSource += "       double v" + std::to_string(id + nInputVars_) + " = randomVariables[tid + " +
            //                   std::to_string(id * size_[currentId_ - 1]) + "];\n";
            //    //kernelSource += "printf(\"v" + std::to_string(id + nInputVars_) + " = %.5f\\n\", v" +
            //    //                std::to_string(id + nInputVars_) + ");\n";
            //    if (settings_.debug)
            //        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
            //}
            kernelSource += source_[0];

            size_t ii = 0;
            for (auto const& out : outputVariables_) {
                if (out < nInputVars_)
                    kernelSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = input[" +
                                    std::to_string(out) + (inputVarIsScalar_[out] ? "" : " + tid") + "];\n";
                else
                    kernelSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                    std::to_string(out) + ";\n";
                //kernelSource += "       printf(\"output[tid + " + std::to_string(ii) + "] = %.5f \\n\", output[tid + " +
                //                std::to_string(ii) + "]);\n";
                ++ii;
            }
            kernelSource += "   }\n"
                            "}\n";
        } else {
            // Have expectation
            // Check if any element in the vector is true
            //bool hasExpectation = std::any_of(hasExpectation_.begin(), hasExpectation_.end(), [](bool b) {
            //    return b;
            //});
            //if (hasExpectation) {
            //    includeSource += "__device__ double globalSum = 0.0;\n";
            //                     //"__device__ double mean = 0.0;\n";          
            //}
            kernelSource += includeSource;
            // Have conditional expectation
            for (int s = source_.size() - 1; s >= 0; --s) {
                //std::cout << "resultIdCE_.size() = " << resultIdCE_.size() << std::endl;
                std::string outputSource = "";
                // output (If last kernel, add output. If not, add values)
                if (s == source_.size() - 1){
                    size_t ii = 0;
                    auto secToLast = resultIdCE_[resultIdCE_.size() - (numOfExpectationInKernel_[currentId_ - 1][numOfExpectationInKernel_[currentId_ - 1].size()-2] > 0?3:2)];
                    //std::cout << "numOfExpectationInKernel_[currentId_ - 1].size() = " << numOfExpectationInKernel_[currentId_ - 1].size() << std::endl;
                    //std::cout << "numOfExpectationInKernel_[currentId_ - 1][0] = " << numOfExpectationInKernel_[currentId_ - 1][0] << std::endl;
                    //std::cout << "numOfExpectationInKernel_[currentId_ - 1][1] = " << numOfExpectationInKernel_[currentId_ - 1][1] << std::endl;
                    //std::cout << "numOfExpectationInKernel_[currentId_ - 1][2] = " << numOfExpectationInKernel_[currentId_ - 1][2] << std::endl;
                    //std::cout << "secToLast id = " << resultIdCE_.size() - (numOfExpectationInKernel_[currentId_ - 1][numOfExpectationInKernel_[currentId_ - 1].size()-2] > 0?3:2) << std::endl;
                    //std::cout << "here source_.size() = " << source_.size() << std::endl;
                    //std::cout << "here numOfExpectationInKernel_[currentId_ - 1].size() = " << numOfExpectationInKernel_[currentId_ - 1].size() << std::endl;
                    //std::cout << "secToLast.size() = " << secToLast.size() << std::endl;
                    //std::cout << "lastResultIdCE_.size() = " << lastResultIdCE_.size() << std::endl;
                    //std::cout << "lastResultIdCE_[0] = " << lastResultIdCE_[0] << std::endl;
                    //std::cout << "lastResultIdCE_[1] = " << lastResultIdCE_[1] << std::endl;
                    //std::cout << "lastResultIdCE_[2] = " << lastResultIdCE_[2] << std::endl;
                    for (auto const& out : outputVariables_) {
                        //std::cout << "out = " << out << std::endl;
                        auto expectationResult = std::find(resultIdFlatE_.begin(), resultIdFlatE_.end(), out) ;
            /*            for (auto const& id :  resultIdFlatE_)
                            std::cout << "resultIdFlatE_ = " << id << std::endl; */
          //              if (out >= inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1]) {
          //                  //std::cout << "out = " << out << std::endl;
          //                  //std::cout << "resultIdCE_.back().size() = " << resultIdCE_.back().size() << std::endl;
          ///*                  for (size_t num = 0; num < resultIdCE_.back().size(); ++num){
          //                      std::cout << "resultIdCE_.back()[" << num << "] = " << resultIdCE_.back()[num] << std::endl;
          //                  }*/
          //              }
                        if (out < inputVarIsScalar_.size()){
                            //std::cout << "out < inputVarIsScalar_.size()" << std::endl;
                            outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = input[" +
                                            std::to_string(out) + (inputVarIsScalar_[out] ? "" : " + tid") + "];\n";
                        } else if (out < inputVarIsScalar_.size() + nRandomVariables_[currentId_ - 1])
                            outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = randomVariables[tid + " +
                                            std::to_string((out - inputVarIsScalar_.size()) * size_[currentId_ - 1]) + "];\n";
                        else if (expectationResult != resultIdFlatE_.end()) {
                            //std::cout << "output from expectation" << std::endl;
                            outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = eR[" +
                                            std::to_string(std::distance(resultIdFlatE_.begin(), expectationResult)) + "];\n";
                            numOfExpectationInKernel_[currentId_ - 1].back() = -2;
                            }
                        else if (std::find(secToLast.begin(), secToLast.end(), out) != secToLast.end())
                            outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                            std::to_string(out) + ";\n";
                        else if (out < lastResultIdCE_.back()){
                            //std::cout << "out = " << out << std::endl;
                            //std::cout << "inputVarIsScalar_.size() = " << inputVarIsScalar_.size() << std::endl;
                            //std::cout << "nRandomVariables_[currentId_ - 1] = " << nRandomVariables_[currentId_ - 1] << std::endl;
                            //std::cout << "lastResultIdCE_.back() = " << lastResultIdCE_.back() << std::endl;
                            //std::cout << "lastResultIdCE_.size() = " << lastResultIdCE_.size() << std::endl;
                            auto it = std::find(idCopiedToValues_.begin(), idCopiedToValues_.end(), out);
                            if (it != idCopiedToValues_.end()){
                                //std::cout << "here true" << std::endl;
                                outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = values[tid + " +
                                            std::to_string(std::distance(idCopiedToValues_.begin(), it) * size_[currentId_ - 1]) + "];\n";
                            }else {
                                //std::cout << "here false" << std::endl;
                                idCopiedToValues_.push_back(out);
                                //std::cout << "out = " << out << std::endl;
                                //std::cout << "lastResultIdCE_.size() = " << lastResultIdCE_.size() << std::endl;
                                //for (size_t l=0; l < lastResultIdCE_.size(); ++l){
                                //    std::cout << "lastResultIdCE_[" << l << "] = " << lastResultIdCE_[l] << std::endl; 
                                //}
                                auto ub = std::distance(lastResultIdCE_.begin(), std::upper_bound(lastResultIdCE_.begin(), lastResultIdCE_.end(), out));
                                //std::cout << "ub = " << ub << std::endl; 
                                if (std::find(resultIdCE_[ub].begin(), resultIdCE_[ub].end(), out) == resultIdCE_[ub].end()){
                                    //std::cout << "not found" << std::endl; 
                                    kernelOfIdCopiedToValues_.push_back(ub);
                                }else{
                                    //std::cout << "found" << std::endl;
                                    kernelOfIdCopiedToValues_.push_back(ub + 1);
                                }
                                outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = values[tid + " + std::to_string(size_[currentId_ - 1] * (idCopiedToValues_.size() - 1)) + "];\n";
                            }
                        } else
                            outputSource += "       output[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" +
                                            std::to_string(out) + ";\n";
                        //kernelSource += "       printf(\"output[tid + " + std::to_string(ii) + "] = %.5f \\n\", output[tid + " +
                        //                std::to_string(ii) + "]);\n";
                        ++ii;
                        //std::cout << "after for loop" << std::endl;
                    }
                }
                else {
                    //if (s != 0) {
                    //    for (auto const & id: resultIdCE_[s-1]){
                    //        thisKernel += "       values[tid + " + std::to_string((id - inputVarIsScalar_.size() - nRandomVariables_[currentId_ - 1]) * size_[currentId_ - 1]) + "] = v" + 
                    //                        std::to_string(id) + ";\n";
                    //    }
                    //}
                    //for (size_t ii= (s==0?0:lastResultIdCE_[s-1]); ii < lastResultIdCE_[s]; ++ii) {
                    //    //std::cout << "ii = " << ii << std::endl;
                    //    //std::cout << "ii + nInputVars_ + nRandomVariables_[currentId_ - 1] = " << ii + nInputVars_ + nRandomVariables_[currentId_ - 1] << std::endl;
                    //    //if (s>0) std::cout << "resultIdCE_[s-1] = " << resultIdCE_[s-1] << std::endl;
                    //    //std::cout << "resultIdCE_[s] = " << resultIdCE_[s] << std::endl;
                    //    // the resultId for the last conditional expectation is assigned here, but is not needed in the global kernel
                    //    if (std::find(resultIdCE_[s].begin(), resultIdCE_[s].end(), ii + nInputVars_ + nRandomVariables_[currentId_ - 1]) == resultIdCE_[s].end()){
                    //        thisKernel += "       values[tid + " + std::to_string(ii * size_[currentId_ - 1]) + "] = v" + 
                    //                    std::to_string(ii + nInputVars_ + nRandomVariables_[currentId_ - 1]) + ";\n";
                    //    }
                    //}
                    for (size_t ii = 0; ii < kernelOfIdCopiedToValues_.size(); ++ii){
                        //std::cout << "kernelOfIdCopiedToValues_.size() = " << kernelOfIdCopiedToValues_.size() << std::endl;
                        //std::cout << "idCopiedToValues_.size() = " << idCopiedToValues_.size() << std::endl;
                        //std::cout << "kernelOfIdCopiedToValues_[" << ii << "] = " << kernelOfIdCopiedToValues_[ii] << std::endl;
                        //std::cout << "idCopiedToValues_[" << ii << "] = " << idCopiedToValues_[ii] << std::endl;
                        if (kernelOfIdCopiedToValues_[ii] == s){
                            outputSource += "       values[tid + " + std::to_string(size_[currentId_ - 1] * ii) + "] = v" + std::to_string(idCopiedToValues_[ii]) + ";\n";
                        }
                    }
                    //std::cout << thisKernel << std::endl;
                }
                //for (auto const& outt : idCopiedToValues_) {
                //    std::cout << "idCopiedToValues_ = " << outt << std::endl;
                //}
                //for (auto const& outt : kernelOfIdCopiedToValues_) {
                //    std::cout << "kernelOfIdCopiedToValues_ = " << outt << std::endl;
                //}

                //std::cout << "s = " << s << std::endl;
                kernelName[s] = "ore_kernel_" + std::to_string(currentId_) + "_" + std::to_string(version_[currentId_ - 1]) + "_" + std::to_string(s);
                std::string thisKernel;
                if (numOfExpectationInKernel_[currentId_ - 1][s] > 0) {
                    // expectation kernel
                    thisKernel = "extern \"C\" __global__ void " + kernelName[s] + "(double* e, double* eR) {\n__shared__ double partialSum[" + std::to_string(NUM_THREADS_) + "];\n";
                }
                else
                    thisKernel = "extern \"C\" __global__ void " + kernelName[s] + 
                                "(const double* input, const double* randomVariables, double* values" + 
                                ((s == source_.size() - 1)?", double* output":", double* A, double* b, double* A_unfilter" + std::to_string(s - expectationKernelCount_)) +
                                ((s == 0)?"":", const double* A_unfilter" + std::to_string(s - 1 - expectationKernelCount_) + ", const double* X") + 
                                ((numOfExpectationInKernel_[currentId_ - 1][s] == -1)?", double* e":"") +
                                ((numOfExpectationInKernel_[currentId_ - 1][s] == -2)?", double* eR":"") +
                                ((numOfExpectationInKernel_[currentId_ - 1][s] == -3)?", double* e, double* eR":"") + ") {\n" +
                                ((numOfExpectationInKernel_[currentId_ - 1][s] == -1) || (numOfExpectationInKernel_[currentId_ - 1][s] == -3)?
                                    "  __shared__ double partialSum[" + std::to_string(NUM_THREADS_)+"];\n":"") +
                                "  int tid = blockIdx.x * blockDim.x + threadIdx.x;\n"
                                "  if (tid < " + std::to_string(size_[currentId_ - 1]) + ") {\n";
                //for (size_t id = 0; id < nRandomVariables_[currentId_ - 1]; ++id) {
                //    // original version
                //    thisKernel += "       double v" + std::to_string(id + nInputVars_) + " = randomVariables[tid + " +
                //                   std::to_string(id * size_[currentId_ - 1]) + "];\n";
                //    //kernelSource += "printf(\"v" + std::to_string(id + nInputVars_) + " = %.5f\\n\", v" +
                //    //                std::to_string(id + nInputVars_) + ");\n";

                //    if (settings_.debug)
                //        debugInfo_.numberOfOperations += 1 * size_[currentId_ - 1];
                //}
                thisKernel += source_[s];
                thisKernel += outputSource;
                //std::cout << "head added" << std::endl;
                if (numOfExpectationInKernel_[currentId_ - 1][s] <= 0) thisKernel += "   }\n";
                thisKernel += "}\n";

                if (numOfExpectationInKernel_[currentId_ - 1][s] > 0)
                    --expectationKernelCount_;

                kernelSource += thisKernel;
            } // end for (int s = source_.size() - 1; s >= 0; --s) {
            //std::cout << "resultIdCE_.size() = " << resultIdCE_.size() << std::endl;
            //for (size_t ii = 0; ii < resultIdCE_.size(); ++ii) {
            //    for (size_t ij = 0; ij < resultIdCE_[ii].size(); ++ij) {
            //        std::cout << "resultIdCE_[" << ii << "][" << ij << "] = " << resultIdCE_[ii][ij] << std::endl;
            //    }
            //}
            // store the size of values to valuesSize
            valuesSize_[currentId_] = idCopiedToValues_.size();
        }

        std::cout << kernelSource << std::endl;
        //for (size_t i = 0; i < numOfExpectationInKernel_[currentId_ - 1].size(); ++i) {
        //    std::cout << numOfExpectationInKernel_[currentId_ - 1][i] << "  ";
        //}
        //std::cout << std::endl;

        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }

        // Compile source code
        nvrtcProgram nvrtcProgram;
        nvrtcCall(nvrtcCreateProgram(&nvrtcProgram, kernelSource.c_str(), "kernel.cu", 0, nullptr, nullptr), "finalizeCalculation(), nvrtcCreateProgram()");
        //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcCreateProgram(): "
        //                                          << nvrtcGetErrorString(nvrtcErr));
        //cudaDeviceProp device_prop;
        //cudaGetDeviceProperties(&device_prop, device_);
        //const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    (std::string("--gpu-architecture=") + getGPUArchitecture[device_prop.name]).c_str(), " - std = c++ 17 ", nullptr};
        //const char* compileOptions[] = {
        //    "--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
        //    "--gpu-architecture=compute_75", "-std=c++17", "-dopt=on", "--time=D:/GitHub/Engine/build/QuantExt/test/Debug/time.txt", nullptr};
        //nvrtcErr = nvrtcCompileProgram(nvrtcProgram, 5, compileOptions);
        std::string gpuArchOption = "--gpu-architecture=" + gpuArchitecture_;
        const char* compileOptions[] = {
            //"--include-path=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/include",
            gpuArchOption.c_str(),
            "--split-compile=0",
            "-std=c++17",
            //"-dopt=on",
            //"--time=time.txt",
            nullptr};
        //nvrtcCall(nvrtcCompileProgram(nvrtcProgram, 3, compileOptions), "finalizeCalculation(), nvrtcCompileProgram()");
        nvrtcCompileProgram(nvrtcProgram, 3, compileOptions);
        //std::cout << "CudaContext::finalizeCalculation(): error during nvrtcCompileProgram(): "
        //                                         << nvrtcGetErrorString(nvrtcErr) << std::endl;

        size_t logSize;
        nvrtcGetProgramLogSize(nvrtcProgram, &logSize);
        char* log = new char[logSize];
        nvrtcGetProgramLog(nvrtcProgram, log);
        std::cout << "NVRTC compilation log:\n" << log << std::endl;
        delete[] log;

        //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcCompileProgram(): "
        //                                          << nvrtcGetErrorString(nvrtcErr));

        // Retrieve the compiled PTX code
        size_t ptxSize;
        nvrtcCall(nvrtcGetPTXSize(nvrtcProgram, &ptxSize), "finalizeCalculation(), nvrtcGetPTXSize()");
        //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcGetPTXSize(): "
        //                                          << nvrtcGetErrorString(nvrtcErr));
        char* ptx = new char[ptxSize];
        nvrtcCall(nvrtcGetPTX(nvrtcProgram, ptx), "finalizeCalculation(), nvrtcGetPTX()");
        //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcGetPTXSize(): "
        //                                          << nvrtcGetErrorString(nvrtcErr));
        nvrtcCall(nvrtcDestroyProgram(&nvrtcProgram), "finalizeCalculation(), nvrtcDestroyProgram()");
        //QL_REQUIRE(nvrtcErr == NVRTC_SUCCESS, "CudaContext::finalizeCalculation(): error during nvrtcDestroyProgram(): "
        //                                          << nvrtcGetErrorString(nvrtcErr));

        cuCall(cuModuleLoadData(&module_[currentId_ - 1], ptx), "finalizeCalculation, cuModuleLoadData()");
        //if (cuErr != CUDA_SUCCESS) {
        //    const char* errStr;
        //    cuGetErrorString(cuErr, &errStr);
        //    std::cerr <<  "CudaContext::finalizeCalculation(): error during cuModuleLoadData(): " << errStr << std::endl;
        //}
        
        for (size_t i = 0; i < source_.size(); ++i) {
            CUfunction k;
            //std::cout << kernelName[i] << std::endl;
            cuCall(cuModuleGetFunction(&k, module_[currentId_ - 1], kernelName[i].c_str()), "finalizeCalculation, cuModuleGetFunction(&k)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::finalizeCalculation(): error during cuModuleGetFunction(): " << errStr << std::endl;
            //}
            kernel_[currentId_-1].push_back(k);
        }
        delete[] ptx;

        hasKernel_[currentId_ - 1] = true;
		source_.clear();
        
        if (settings_.debug) {
            debugInfo_.nanoSecondsProgramBuild += timer.elapsed().wall - timerBase;
        }
    }

    // Allocate memory for values (conditional expectation)
    double* values;
    if (!basisFunctionCE_[currentId_].empty()) {
        cudaCall(cudaMalloc(&values, sizeof(double) * size_[currentId_ - 1] * valuesSize_[currentId_]), "finalizeCalculation, cudaMalloc(&values)");
        //std::cout << "valuesSize_[currentId_].size()) = " << valuesSize_[currentId_] << std::endl;
        //QL_REQUIRE(cudaErr == cudaSuccess,
        //           "CudaContext::finalizeCalculation(): memory allocate for values fails: " << cudaGetErrorString(cudaErr));
    }

    // Allocate memory for eR (expectation results)
    double* eR;
    if (numOfExpectation_[currentId_ - 1] > 0)
        cudaCall(cudaMalloc(&eR, sizeof(double) * numOfExpectation_[currentId_ - 1]), "finalizeCalculation, cudaMalloc(&eR)");

    //std::cout << "Before kernel execution" << std::endl;

    if (kernel_[currentId_ - 1].size() == 1){

        // no conditional expectation

        // set kernel args
        void* args[] = {&input,
                        &dOutput_[nOutputVariables_[currentId_ - 1]],
                        &d_randomVariables_};

        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuCall(cuLaunchKernel(kernel_[currentId_ - 1][0], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args,
                               nullptr), "finalizeCalculation, cuLaunchKernel(no conditional expectation)");
        //if (cuErr != CUDA_SUCCESS) {
        //    const char* errStr;
        //    cuGetErrorString(cuErr, &errStr);
        //    std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        //}
    
    } else{
        // more than one kernel => have expectation and/or conditional expectation
        size_t unfilterCount_ = 0;
        bool haveExpectation = false;
        bool expectationInitialized = false;
        bool haveConditionalExpectation = false;
        bool previousKernelHaveConditionalExpectation = false;
        std::vector<std::vector<size_t>> basisFunction = basisFunctionCE_[currentId_];
        //std::cout << "basisFunction.size() = " << basisFunction.size() << std::endl;
        //for (size_t ii = 0; ii < basisFunction.size(); ++ii) {
        //    for (size_t ij = 0; ij < basisFunction[ii].size(); ++ij) {
        //        std::cout << "basisFunction[" << ii << "][" << ij << "] = " << basisFunction[ii][ij] << std::endl;
        //    }
        //}
        //cusolverStatus_t cusolverErr;
        cusolverDnHandle_t cusolverHandle;
        cusolverDnIRSParams_t gels_params;
        cusolverDnIRSInfos_t gels_infos;
        bool cusolverInitiated = false;
        double* A;
        double* b;
        double* X;
        int* d_info;
        double* d_work;
        size_t d_workspace;
        //cudaCall(cudaMalloc((void**)&d_info, sizeof(size_t)), "finalizeCalculation(), cudaMalloc(&d_info)");
        std::vector<double*> A_unfilter(kernel_[currentId_ - 1].size() - 1 - std::count_if(numOfExpectationInKernel_[currentId_ -1].begin(), numOfExpectationInKernel_[currentId_ -1].end(), [](int c){return c > 0;}));
        // initiate cusolver if have conditional expectation
        if (basisFunction[0].size() > 0) {
            haveConditionalExpectation = true;
            previousKernelHaveConditionalExpectation = true;
            cusolverCall(cusolverDnCreate(&cusolverHandle), "finalizeCalculation, cusolverDnCreate()");
            cusolverCall(cusolverDnSetStream(cusolverHandle, stream_), "finalizeCalculation, cusolverDnSetStream()");
            cusolverCall(cusolverDnIRSParamsCreate(&gels_params), "finalizeCalculation, cusolverDnIRSParamsCreate()");
            cusolverCall(cusolverDnIRSInfosCreate(&gels_infos), "finalizeCalculation, cusolverDnIRSInfosCreate()");

            cusolverCall(cusolverDnIRSParamsSetSolverPrecisions(gels_params, CUSOLVER_R_64F, CUSOLVER_R_64F), "finalizeCalculation, cusolverDnIRSParamsSetSolverPrecisions()");
            cusolverCall(cusolverDnIRSParamsSetRefinementSolver(gels_params, CUSOLVER_IRS_REFINE_NONE), "finalizeCalculation, cusolverDnIRSParamsSetRefinementSolver()");//CUSOLVER_IRS_REFINE_CLASSICAL, CUSOLVER_IRS_REFINE_NONE
            cusolverCall(cusolverDnIRSParamsSetMaxIters(gels_params, 1), "finalizeCalculation, cusolverDnIRSParamsSetMaxIters()");
            cusolverCall(cusolverDnIRSParamsEnableFallback(gels_params), "finalizeCalculation, cusolverDnIRSParamsEnableFallback()");
            cudaCall(cudaMalloc((void**)&d_info, sizeof(size_t)), "finalizeCalculation(), cudaMalloc(&d_info)");
            cusolverInitiated = true;
            cudaCall(cudaMalloc(&A, basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A)");
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
            //                                    << cudaGetErrorString(cudaErr));
            cudaCall(cudaMalloc(&b, (basisFunction[0].size() - 1) * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&B)");
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
            //                                    << cudaGetErrorString(cudaErr));
            //std::cout << "A_unfilter.size() = " << A_unfilter.size() << std::endl;
            cudaCall(cudaMalloc(&A_unfilter[0], basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A_unfilter[0])");
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_filter[0] fails: "
            //                                    << cudaGetErrorString(cudaErr));
        }

        double* e;
        if (numOfExpectationInKernel_[currentId_ - 1][0] == -1) {
            haveExpectation = true;
            // find the next positive number in numOfExpectationInKernel_, which determines the size of e
            int eSize;
            for (int n: numOfExpectationInKernel_[currentId_ - 1]){
                if (n > 0) {
                    eSize = n;
                    break;
                }
            }
            cudaCall(cudaMalloc(&e, eSize * nNUM_BLOCKS_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&e)");
            expectationInitialized = true;
        }
  
        //std::cout << "First kernel" << std::endl;
        // first kernel
        // Allocate memory for A, b, A_filter
        //std::cout << "currentId_ = " << currentId_ << std::endl;
        //
        //for (auto const& pair: basisFunctionCE_){
        //    std::cout << "key = " << pair.first << std::endl;
        //    for (auto const& n : pair.second) {
        //        std::cout << "value = " << n << std::endl;
        //    }
        //}
        //std::cout << "basisFunctionCE_[currentId_][0] = " << basisFunctionCE_[currentId_][0] << std::endl;
        //double* A;
        //cudaCall(cudaMalloc(&A, basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A)");
        //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
        //                                    << cudaGetErrorString(cudaErr));
        //double* b;
        //cudaCall(cudaMalloc(&b, (basisFunction[0].size() - 1) * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&B)");
        //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
        //                                    << cudaGetErrorString(cudaErr));
        //std::vector<double*> A_unfilter(kernel_[currentId_ - 1].size() - 1);
        //std::cout << "A_unfilter.size() = " << A_unfilter.size() << std::endl;
        //cudaCall(cudaMalloc(&A_unfilter[0], basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A_unfilter[0])");
        //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_filter[0] fails: "
        //                                    << cudaGetErrorString(cudaErr));
        // set kernel args
        //void* args0[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[0], &e};
        void* args0[] = {&input, &d_randomVariables_, &values, nullptr, nullptr, nullptr, nullptr};
        if (haveExpectation && haveConditionalExpectation){
            args0[3] = &A;
            args0[4] = &b;
            args0[5] = &A_unfilter[0];
            args0[6] = &e;
        } else {
            if (haveExpectation)
                args0[3] = &e;
            else if (haveConditionalExpectation) {
                args0[3] = &A;
                args0[4] = &b;
                args0[5] = &A_unfilter[0];
            }
        }

        //std::cout << "Start execute kernel" << std::endl;
        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuCall(cuLaunchKernel(kernel_[currentId_ - 1][0], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args0,
                                nullptr), "finalizeCalculation, cuLaunchKernel(first kernel for trades with conditional expectation)");
        //if (cuErr != CUDA_SUCCESS) {
        //    const char* errStr;
        //    cuGetErrorString(cuErr, &errStr);
        //    std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        //}

        ////temp check result
        //cudaStreamSynchronize(stream_);
        //double* hA;
        //////cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        //cudaErr = cudaMallocHost(&hA, basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double));
        ////std::cout << "cudaErr = " << cudaErr << std::endl;
        //cudaErr = cudaMemcpy(hA, A, basisFunction[0].back() * size_[currentId_ - 1] * sizeof(double), cudaMemcpyDeviceToHost);
        ////std::cout << "cudaErr = " << cudaErr << std::endl;
        //for (size_t i = 0; i < basisFunction[0].back() * size_[currentId_ - 1]; ++i) {
        //    std::cout << "cpu:A["<< i <<"] = " << hA[i] << std::endl;
        //}
        //double* hB;
        //cudaErr = cudaMallocHost(&hB, (basisFunction[0].size() - 1) * size_[currentId_ - 1] * sizeof(double));
        ////std::cout << "cudaErr = " << cudaErr << std::endl;
        //cudaErr = cudaMemcpy(hB, b, (basisFunction[0].size() - 1) * size_[currentId_ - 1] * sizeof(double), cudaMemcpyDeviceToHost);
        ////std::cout << "cudaErr = " << cudaErr << std::endl;
        //std::cout << "basisFunction[0].size() = " << basisFunction[0].size() << std::endl;
        //std::cout << "size_[currentId_ - 1] = " << size_[currentId_ - 1] << std::endl;
        //for (size_t i = 0; i < (basisFunction[0].size() - 1) * size_[currentId_ - 1]; ++i) {
        //    std::cout << "cpu:b["<< i <<"] = " << hB[i] << std::endl;
        //}

        // linear regression
        if (haveConditionalExpectation) {
            cudaCall(cudaMalloc(&X, basisFunction[0].back() * sizeof(double)), "finalizeCalculation(), cudaMalloc(&X)");
            //std::cout << "basisFunction[0].back() = " << basisFunction[0].back() << std::endl;
            //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for X[0] fails: "
            //                                    << cudaGetErrorString(cudaErr));

            timerBase = timer.elapsed().wall;

            /*
            std::vector<cusolverDnHandle_t> handles;
            std::vector<cudaStream_t> streams;
            std::vector<int*> d_infos;
            std::vector<double*> d_works;
            std::vector<size_t> d_workspaces;
            std::vector<int> niters;
            std::vector<cusolverDnIRSParams_t> gels_params_vec;
            std::vector<cusolverDnIRSInfos_t> gels_infos_vec;

            size_t nStream = 2;

            for (size_t i = 0; i < nStream; ++i) {
                cusolverDnHandle_t handle;
                handles.push_back(handle);
                cusolverErr = cusolverDnCreate(&handles[i]);
                cudaStream_t tmpStream;
                streams.push_back(tmpStream);
                cudaStreamCreate(&streams[i]);
                cusolverDnSetStream(handles[i], streams[i]);
                int* d_info;
                d_infos.push_back(d_info);
                cudaErr = cudaMalloc((void**)&d_infos[i], sizeof(size_t));
                niters.push_back(0);
                double* d_work;
                size_t d_workspace;
                d_works.push_back(d_work);
                d_workspaces.push_back(d_workspace);

                cusolverDnIRSParams_t gels_params;
                gels_params_vec.push_back(gels_params);
                cusolverDnIRSParamsCreate(&gels_params_vec[i]);
                cusolverDnIRSInfos_t gels_infos;
                gels_infos_vec.push_back(gels_infos);
                cusolverDnIRSInfosCreate(&gels_infos_vec[i]);

                cusolverDnIRSParamsSetSolverPrecisions(gels_params_vec[i], CUSOLVER_R_64F, CUSOLVER_R_32F);
                cusolverDnIRSParamsSetRefinementSolver(gels_params_vec[i], CUSOLVER_IRS_REFINE_CLASSICAL);//CUSOLVER_IRS_REFINE_CLASSICAL, CUSOLVER_IRS_REFINE_NONE
                cusolverDnIRSParamsSetMaxIters(gels_params_vec[i], 1);
                cusolverDnIRSParamsEnableFallback(gels_params_vec[i]);
            } */

            //    //int* d_info;
            //    //cudaErr = cudaMalloc((void**)&d_info, sizeof(size_t));
            //    //d_infos.push_back(d_info);

            //cusolverDnIRSParams_t gels_params;
            //cusolverCall(cusolverDnIRSParamsCreate(&gels_params), "finalizeCalculation, cusolverDnIRSParamsCreate()");
            //cusolverDnIRSInfos_t gels_infos;
            //cusolverCall(cusolverDnIRSInfosCreate(&gels_infos), "finalizeCalculation, cusolverDnIRSInfosCreate()");

            //cusolverCall(cusolverDnIRSParamsSetSolverPrecisions(gels_params, CUSOLVER_R_64F, CUSOLVER_R_64F), "finalizeCalculation, cusolverDnIRSParamsSetSolverPrecisions()");
            //cusolverCall(cusolverDnIRSParamsSetRefinementSolver(gels_params, CUSOLVER_IRS_REFINE_NONE), "finalizeCalculation, cusolverDnIRSParamsSetRefinementSolver()");//CUSOLVER_IRS_REFINE_CLASSICAL, CUSOLVER_IRS_REFINE_NONE
            //cusolverCall(cusolverDnIRSParamsSetMaxIters(gels_params, 1), "finalizeCalculation, cusolverDnIRSParamsSetMaxIters()");
            //cusolverCall(cusolverDnIRSParamsEnableFallback(gels_params), "finalizeCalculation, cusolverDnIRSParamsEnableFallback()");

            //int* d_info;
            //double* d_work;
            //size_t d_workspace;
            //cudaCall(cudaMalloc((void**)&d_info, sizeof(size_t)), "finalizeCalculation(), cudaMalloc(&d_info)");

            //}
            //std::cout << "handle 1" << std::endl;
            size_t numBF = 0;
            for (size_t i = 0; i < basisFunction[0].size() - 1; ++i) {
                //cusolverDnSetStream(handles[i%nStream], streams[i%nStream]);
                //std::cout << "steams " << i%nStream << std::endl;
                //int* d_info;
                //double* d_work;
                //size_t d_workspace;
                //cusolverErr = cusolverDnSetStream(handles[i%nStream], streams[i%nStream]);
                //std::cout << "cusolverErr cusolverDnDDgels() = " << cusolverErr << std::endl;
                numBF = basisFunction[0][i+1] - basisFunction[0][i];
                if (i == 0) {
                    cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(first kernel)");
                    //cusolverErr = cusolverDnDSgels_bufferSize(handles[i%nStream], size_[currentId_ - 1], numBF, 1, A_temp, size_[currentId_ - 1], b_temp, size_[currentId_ - 1], X_temp, numBF, NULL, &d_workspace);
                    //std::cout << "cusolverErr cusolverDnDSgels_bufferSize() = " << cusolverErr << std::endl;
                    cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                }
                else if (numBF != basisFunction[0][i] - basisFunction[0][i-1]) {
                    releaseMem(d_work, "finalizeCalculation() first kernel");
                    cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(first kernel)");
                    //cusolverErr = cusolverDnDSgels_bufferSize(handles[i%nStream], size_[currentId_ - 1], numBF, 1, A_temp, size_[currentId_ - 1], b_temp, size_[currentId_ - 1], X_temp, numBF, NULL, &d_workspace);
                    //std::cout << "cusolverErr cusolverDnDSgels_bufferSize() = " << cusolverErr << std::endl;
                    cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                }
            
                //double* A_temp;
                //cudaMalloc((void**)&A_temp, sizeof(double) * size_[currentId_ - 1] * numBF);
                //cudaMemcpyAsync(A_temp, A + size_[currentId_ - 1] * basisFunction[0][i], sizeof(double) * size_[currentId_ - 1] * numBF, cudaMemcpyDeviceToDevice, stream_);
                //double* b_temp;
                //cudaMalloc((void**)&b_temp, sizeof(double) * size_[currentId_ - 1]);
                //cudaMemcpyAsync(b_temp, b + size_[currentId_ - 1] * i, sizeof(double) * size_[currentId_ - 1] , cudaMemcpyDeviceToDevice, stream_);
                //double* X_temp;
                //cudaMalloc((void**)&X_temp, sizeof(double) * numBF);
         
                //cusolverErr = cusolverDnIRSXgels_bufferSize(handles[i%nStream], gels_params_vec[i%nStream], size_[currentId_ - 1], numBF, 1, &d_workspaces[i%nStream]);
                //std::cout << "cusolverErr cusolverDnDSgels_bufferSize() = " << cusolverErr << std::endl;
                //cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(first kernel)");
                //cusolverErr = cusolverDnDSgels_bufferSize(handles[i%nStream], size_[currentId_ - 1], numBF, 1, A_temp, size_[currentId_ - 1], b_temp, size_[currentId_ - 1], X_temp, numBF, NULL, &d_workspace);
                //std::cout << "cusolverErr cusolverDnDSgels_bufferSize() = " << cusolverErr << std::endl;
                //cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                //cusolverErr = cusolverDnIRSXgels(handles[i%nStream], gels_params_vec[i%nStream], gels_infos_vec[i%nStream], size_[currentId_ - 1], numBF, 1, A + size_[currentId_ - 1] * basisFunction[0][i], size_[currentId_ - 1], b + size_[currentId_ - 1] * i, size_[currentId_ - 1], X + basisFunction[0][i], numBF, d_works[i%nStream], d_workspaces[i%nStream], &niters[i%nStream], d_infos[i%nStream]);
                //std::cout << "cusolverErr cusolverDnDSgels() = " << cusolverErr << std::endl;
                int niter = 0;
                cusolverCall(cusolverDnIRSXgels(cusolverHandle, gels_params, gels_infos, size_[currentId_ - 1], numBF, 1, A + size_[currentId_ - 1] * basisFunction[0][i], size_[currentId_ - 1], b + size_[currentId_ - 1] * i, size_[currentId_ - 1], X + basisFunction[0][i], numBF, d_work, d_workspace, &niter, d_info), "finalizeCalculation, cusolverDnIRSXgels(first kernel)");
                //cusolverCall(cusolverDnIRSXgels(cusolverHandle, gels_params, gels_infos, size_[currentId_ - 1], numBF, 1, A_temp, size_[currentId_ - 1], b_temp, size_[currentId_ - 1], X_temp, numBF, d_work, d_workspace, &niters, d_info);
                //cusolverErr = cusolverDnDSgels(handles[i%nStream], size_[currentId_ - 1], numBF, 1, A_temp, size_[currentId_ - 1], b_temp, size_[currentId_ - 1], X_temp, numBF, d_work, d_workspace, &niters, d_info);
                //std::cout << "cusolverErr cusolverDnDDgels() = " << cusolverErr << std::endl;
                //cudaStreamSynchronize(streams[i%nStream]);
                //std::cout << "niters = " << niters[i%nStream] << std::endl;
                //cudaMemcpyAsync(X + basisFunction[0][i], X_temp, sizeof(double) * numBF , cudaMemcpyDeviceToDevice, stream_);
                //int h_info;
                //cudaMemcpyAsync(&h_info, d_infos[i%nStream], sizeof(int), cudaMemcpyDeviceToHost, streams[i%nStream]);
                //cudaStreamSynchronize(streams[i%nStream]);
                //std::cout << "h_info = " << h_info << std::endl;
                //releaseMem(d_work, "finalizeCalculation() first kernel");
                //cudaFree(d_info);
                //releaseMem(A_temp, "finalizeCalculation() first kernel");
                //releaseMem(b_temp, "finalizeCalculation() first kernel");
                //releaseMem(X_temp, "finalizeCalculation() first kernel");
            }
            releaseMem(d_work, "finalizeCalculation() first kernel");
            //std::cout << "handle 2" << std::endl;
            //for (size_t i = 0; i < nStream; ++i) {
            //    //cudaFree(d_infos[i]);
            //    cusolverErr = cusolverDnDestroy(handles[i%nStream]);
            //    //std::cout << "cusolverErr cusolverDnDestroy() = " << cusolverErr << std::endl;
            //    cudaStreamSynchronize(streams[i]);
            //    cudaStreamDestroy(streams[i]);
            //}

            //std::cout << "time used for cuSolver: " << (timer.elapsed().wall - timerBase)/1000000 << "ms" << std::endl;

            ////temp check result
            //int h_info = 0;
            ////cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
            //cudaMemcpy(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost);
            //std::cout << "cusolver info: " << h_info << std::endl;

            ////temp check result
            //cudaStreamSynchronize(stream_);
            //double* hX;
            ////////cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
            //cudaMallocHost(&hX, basisFunction[0].back() * sizeof(double));
            //////std::cout << "cudaErr = " << cudaErr << std::endl;
            //cudaMemcpy(hX, X, basisFunction[0].back() * sizeof(double), cudaMemcpyDeviceToHost);
            //////std::cout << "cudaErr = " << cudaErr << std::endl;
            //for (size_t i = 0; i < basisFunction[0].back(); ++i) {
            //    std::cout << "X["<< i % 5 <<"] = " << hX[i] << std::endl;
            //}

            //cudaStreamSynchronize(stream_);
            // clear memory
            releaseMem(A, "CudaContext::finalizeCalculation::A");
            releaseMem(b, "CudaContext::finalizeCalculation::b");
            ++unfilterCount_;
        }
        //std::cout << "First kernel end " << std::endl;
        // second to penultimate kernel
        for (size_t k = 1; k < kernel_[currentId_ - 1].size() - 1; ++k) {
            //std::cout << "for loop called " << k << std::endl;
            //std::cout << "basisFunction[" << k <<"].size() = " << basisFunction[k].size() << std::endl;
            // Expectation
            if (numOfExpectationInKernel_[currentId_ - 1][k] > 0) {
                // Expectation kernel
                void* args[] = {&e, &eR};
                cuCall(cuLaunchKernel(kernel_[currentId_ - 1][k], numOfExpectationInKernel_[currentId_ - 1][k], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args,
                                   nullptr), "finalizeCalculation, cuLaunchKernel(middle kernels, expectation)");
                releaseMem(e, "CudaContext::finalizeCalculation::e");
                expectationInitialized = false;
                continue;
            } else if (numOfExpectationInKernel_[currentId_ - 1][k] == -1 || numOfExpectationInKernel_[currentId_ - 1][k] == -3){
                haveExpectation = true;
                if (expectationInitialized = false) {
                    // find the next positive number in numOfExpectationInKernel_, which determines the size of e
                    int eSize;
                    for (int n: numOfExpectationInKernel_[currentId_ - 1]){
                        if (n > 0) {
                            eSize = n;
                            break;
                        }
                    }
                    cudaCall(cudaMalloc(&e, eSize * nNUM_BLOCKS_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&e)");
                    expectationInitialized = true;
                }
            } else if (numOfExpectationInKernel_[currentId_ - 1][k] == 0)
                haveExpectation = false;

            if (basisFunction[k].size() > 0){
                haveConditionalExpectation = true;
                if (!cusolverInitiated){
                    cusolverCall(cusolverDnCreate(&cusolverHandle), "finalizeCalculation, cusolverDnCreate()");
                    cusolverCall(cusolverDnSetStream(cusolverHandle, stream_), "finalizeCalculation, cusolverDnSetStream()");
                    cusolverCall(cusolverDnIRSParamsCreate(&gels_params), "finalizeCalculation, cusolverDnIRSParamsCreate()");
                    cusolverCall(cusolverDnIRSInfosCreate(&gels_infos), "finalizeCalculation, cusolverDnIRSInfosCreate()");

                    cusolverCall(cusolverDnIRSParamsSetSolverPrecisions(gels_params, CUSOLVER_R_64F, CUSOLVER_R_64F), "finalizeCalculation, cusolverDnIRSParamsSetSolverPrecisions()");
                    cusolverCall(cusolverDnIRSParamsSetRefinementSolver(gels_params, CUSOLVER_IRS_REFINE_NONE), "finalizeCalculation, cusolverDnIRSParamsSetRefinementSolver()");//CUSOLVER_IRS_REFINE_CLASSICAL, CUSOLVER_IRS_REFINE_NONE
                    cusolverCall(cusolverDnIRSParamsSetMaxIters(gels_params, 1), "finalizeCalculation, cusolverDnIRSParamsSetMaxIters()");
                    cusolverCall(cusolverDnIRSParamsEnableFallback(gels_params), "finalizeCalculation, cusolverDnIRSParamsEnableFallback()");
                    cudaCall(cudaMalloc((void**)&d_info, sizeof(size_t)), "finalizeCalculation(), cudaMalloc(&d_info)");
                    cusolverInitiated = true;
                }
                // Allocate memory for A, b, A_filter
                cudaCall(cudaMalloc(&A, basisFunction[k].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A)");
                //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
                //                                    << cudaGetErrorString(cudaErr));
                cudaCall(cudaMalloc(&b, (basisFunction[k].size() - 1) * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&b)");
                //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
                //                                    << cudaGetErrorString(cudaErr));
                cudaCall(cudaMalloc(&A_unfilter[unfilterCount_], basisFunction[k].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A_unfilter[unfilterCount_])");
                //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_unfilter[k] fails: "
                //                                    << cudaGetErrorString(cudaErr));
            } else
                haveConditionalExpectation = false;

            //// Allocate memory for A, b, A_filter
            //cudaCall(cudaMalloc(&A, basisFunction[k].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A)");
            ////QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A fails: "
            ////                                    << cudaGetErrorString(cudaErr));
            //cudaCall(cudaMalloc(&b, (basisFunction[k].size() - 1) * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&b)");
            ////QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for b fails: "
            ////                                    << cudaGetErrorString(cudaErr));
            //cudaCall(cudaMalloc(&A_unfilter[k], basisFunction[k].back() * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMalloc(&A_unfilter[k])");
            ////QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for A_unfilter[k] fails: "
            ////                                    << cudaGetErrorString(cudaErr));
            
            // set kernel args
            std::vector<double*> argVec;
            if (haveConditionalExpectation) {
                //std::cout << "unfilterCount_ = " << unfilterCount_ << std::endl;
                argVec.push_back(A);
                argVec.push_back(b);
                argVec.push_back(A_unfilter[unfilterCount_]);
            }
            if (previousKernelHaveConditionalExpectation){
                //std::cout << "previousKernelHaveConditionalExpectation" << std::endl;
                argVec.push_back(A_unfilter[unfilterCount_-1]);
                argVec.push_back(X);
            }
            if (haveExpectation)
                argVec.push_back(e);
            if (numOfExpectationInKernel_[currentId_-1][k] == -2 || numOfExpectationInKernel_[currentId_-1][k] == -3)
                argVec.push_back(eR);
            previousKernelHaveConditionalExpectation = haveConditionalExpectation;
            void* args[] = {&input, &d_randomVariables_, &values, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
            int nArgs = 3;
            for (auto& ptr : argVec) {
                args[nArgs] = &ptr;
                ++nArgs;
            }

            //void* args[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[unfilterCount_], &A_unfilter[unfilterCount_-1], &X};

            // execute kernel
            if (settings_.debug) {
                timerBase = timer.elapsed().wall;
            }
            cuCall(cuLaunchKernel(kernel_[currentId_ - 1][k], nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, args,
                                   nullptr), "finalizeCalculation, cuLaunchKernel(middle kernels)");
            //if (cuErr != CUDA_SUCCESS) {
            //    const char* errStr;
            //    cuGetErrorString(cuErr, &errStr);
            //    std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
            //}

            //cudaStreamSynchronize(stream_);
            if (haveConditionalExpectation) {
                // clear memory
                releaseMem(A_unfilter[unfilterCount_-1], "CudaContext::finalizeCalculation::A_unfilter[k-1]");
                releaseMem(X, "CudaContext::finalizeCalculation::X");

                // linear regression
                cudaCall(cudaMalloc(&X, basisFunction[k].back() * sizeof(double)), "finalizeCalculation(), cudaMalloc(&X)");
                //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for X[k] fails: "
                //                                    << cudaGetErrorString(cudaErr));

                for (size_t i = 0; i < basisFunction[k].size()-1; ++i) {
                    //int* d_info;
                    //double* d_work;
                    //size_t d_workspace;
                    size_t numBF;

                    numBF = basisFunction[k][i+1] - basisFunction[k][i];
                    if (i == 0) {
                        cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(middle kernels)");
                        //std::cout << "cusolverErr = " << cusolverErr << std::endl;
                        cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                    }
                    else if (numBF != basisFunction[k][i] - basisFunction[k][i-1]){
                        releaseMem(d_work, "CUDAContext::finalizeCalculation() middle kernels");
                        cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(middle kernels)");
                        //std::cout << "cusolverErr = " << cusolverErr << std::endl;
                        cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                    }
                    //cusolverCall(cusolverDnIRSXgels_bufferSize(cusolverHandle, gels_params, size_[currentId_ - 1], numBF, 1, &d_workspace), "finalizeCalculation, cusolverDnIRSXgels_bufferSize(middle kernels)");
                    //std::cout << "cusolverErr = " << cusolverErr << std::endl;
                    //cudaCall(cudaMalloc((void**)&d_work, sizeof(double) * d_workspace), "finalizeCalculation(), cudaMalloc(&d_work)");
                    //cudaErr = cudaMalloc((void**)&d_infos[i%nStream], sizeof(size_t));
                    int niter = 0;
                    cusolverCall(cusolverDnIRSXgels(cusolverHandle, gels_params, gels_infos, size_[currentId_ - 1], numBF, 1, A + size_[currentId_ - 1] * basisFunction[k][i], size_[currentId_ - 1], b + size_[currentId_ - 1] * i, size_[currentId_ - 1], X + basisFunction[k][i], numBF, d_work, d_workspace, &niter, d_info), "finalizeCalculation, cusolverDnIRSXgels(middle kernels)");
                    //std::cout << "cusolverErr = " << cusolverErr << std::endl;
                    //releaseMem(d_work, "CUDAContext::finalizeCalculation() middle kernels");
                    //cudaFree(d_info);
                }
                releaseMem(d_work, "CUDAContext::finalizeCalculation() middle kernels");
                //temp check result
                //int h_info = 0;
                //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
                //cudaMemcpy(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost);
                //std::cout << "cusolver info: " << h_info << std::endl;

                // temp output
                //double* hX2;
                //////cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
                //cudaMallocHost(&hX2, basisFunctionCE_[currentId_][k].back() * sizeof(double));
                ////std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
                //cudaMemcpy(hX2, X, basisFunctionCE_[currentId_][k].back() * sizeof(double), cudaMemcpyDeviceToHost);
                ////std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
                //for (size_t i = 0; i < 5; ++i) {
                //    std::cout << "X2["<< i <<"] = " << hX2[i] << std::endl;
                //}

                //cudaStreamSynchronize(stream_);
                // clear memory
                releaseMem(A, "CudaContext::finalizeCalculation::A");
                releaseMem(b, "CudaContext::finalizeCalculation::b");
                ++unfilterCount_;
            }
        }
        //std::cout << "A_unfilter.size() = " << A_unfilter.size() << std::endl;
        //cudaStreamSynchronize(stream_);
        //double* hX1;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        //cudaErr = cudaMallocHost(&hX1, basisFunctionCE_[currentId_].back() * sizeof(double));
        //std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        //cudaErr = cudaMemcpy(hX1, X, basisFunctionCE_[currentId_].back() * sizeof(double), cudaMemcpyDeviceToHost);
        //std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        //for (size_t i = 0; i < 5; ++i) {
        //    std::cout << "hX1["<< i <<"]: " << hX1[i] << std::endl;
        //}
        // last kernel
        // set kernel args
        //void* argsFinal[] = {&input, &d_randomVariables_, &values, &dOutput_[nOutputVariables_[currentId_ - 1]], &A_unfilter.back(), &X, &eR};
        std::vector<double*> argVec;
        void* argsFinal[] = {&input, &d_randomVariables_, &values, &dOutput_[nOutputVariables_[currentId_ - 1]], nullptr, nullptr, nullptr};
        if (previousKernelHaveConditionalExpectation){
            argVec.push_back(A_unfilter.back());
            argVec.push_back(X);
        }
        if (numOfExpectationInKernel_[currentId_-1].back() == -2)
            argVec.push_back(eR);
        int nArgs = 4;
        for (auto& ptr : argVec) {
            argsFinal[nArgs] = &ptr;
            ++nArgs;
        }

        //void* argsTmp[] = {&input, &d_randomVariables_, &values, &output, &A_unfilter.back(), &X};
        //void* args0[] = {&input, &d_randomVariables_, &values, &A, &b, &A_unfilter[0]};
        // execute kernel
        if (settings_.debug) {
            timerBase = timer.elapsed().wall;
        }
        cuCall(cuLaunchKernel(kernel_[currentId_ - 1].back(), nNUM_BLOCKS_[currentId_ - 1], 1, 1, NUM_THREADS_, 1, 1, 0, stream_, argsFinal,
                                nullptr), "finalizeCalculation, cuLaunchKernel(last kernel)");
        //const char* errStr;
        //    cuGetErrorString(cuErr, &errStr);
        //std::cout << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        //if (cuErr != CUDA_SUCCESS) {
        //    const char* errStr;
        //    cuGetErrorString(cuErr, &errStr);
        //    std::cerr << "CudaContext::finalizeCalculation(): error during cuLaunchKernel(): " << errStr << std::endl;
        //}
        //double* hX3;
        //cudaMemcpyAsync(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost, stream_);
        //cudaErr = cudaMallocHost(&hX3, basisFunctionCE_[currentId_].back() * sizeof(double));
        //std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        //cudaErr = cudaMemcpy(hX3, X, basisFunctionCE_[currentId_].back() * sizeof(double), cudaMemcpyDeviceToHost);
        //std::cout << "cudaErr = " << cudaGetErrorString(cudaErr) << std::endl;
        //for (size_t i = 0; i < 5; ++i) {
        //    std::cout << "hX3["<< i <<"]: " << hX3[i] << std::endl;
        //}
        //cudaStreamSynchronize(stream_);

        //for (size_t i = 0; i < nStream; ++i) {
        //    cusolverErr = cusolverDnIRSParamsDestroy(gels_params_vec[i]);
        //    cusolverErr = cusolverDnIRSInfosDestroy(gels_infos_vec[i]);
        //    cusolverErr = cusolverDnDestroy(handles[i]);
        //    cudaErr = cudaStreamDestroy(streams[i]);
        //}

  //      // clear memory
		cusolverCall(cusolverDnIRSParamsDestroy(gels_params), "finalizeCalculation, cusolverDnIRSParamsDestroy()");
        cusolverCall(cusolverDnIRSInfosDestroy(gels_infos), "finalizeCalculation, cusolverDnIRSInfosDestroy()");
        cusolverCall(cusolverDnDestroy(cusolverHandle), "finalizeCalculation, cusolverDnDestroy()");
        releaseMem(d_info, "finalizeCalculation()::d_info");
        releaseMem(A_unfilter.back(), "CudaContext::finalizeCalculation::A_unfilter.back()");
        releaseMem(X, "CudaContext::finalizeCalculation::X");
        releaseMem(values, "CudaContext::finalizeCalculation::values");
    }

     if (settings_.debug) {
        debugInfo_.nanoSecondsCalculation += timer.elapsed().wall - timerBase;
    }

    // copy the results

    if (settings_.debug) {
        timerBase = timer.elapsed().wall;
    }
    double* h_output;
    cudaCall(cudaMallocHost(&h_output, nOutputVariables_[currentId_ - 1] * size_[currentId_ - 1] * sizeof(double)), "finalizeCalculation(), cudaMallocHost(&h_output)");
    //QL_REQUIRE(cudaErr == cudaSuccess, "CudaContext::finalizeCalculation(): memory allocate for h_output fails: "
    //                                       << cudaGetErrorString(cudaErr));
    //cudaErr = cudaMemcpyAsync(h_output, dOutput_[nOutputVariables_[currentId_ - 1]],
    //                          sizeof(double) * size_[currentId_ - 1] * nOutputVariables_[currentId_ - 1],
    //                          cudaMemcpyDeviceToHost, stream_);
    cudaCall(cudaMemcpyAsync(h_output, dOutput_[nOutputVariables_[currentId_ - 1]],
                              sizeof(double) * size_[currentId_ - 1] * nOutputVariables_[currentId_ - 1],
                              cudaMemcpyDeviceToHost, stream_), "finalizeCalculation(), cudaMemcpyAsync(h_output, dOutput_[nOutputVariables_[currentId_ - 1]])");
    //QL_REQUIRE(cudaErr == cudaSuccess,
    //           "CudaContext::finalizeCalculation(): memory copy from device to host for h_output fails: " << cudaGetErrorString(cudaErr));
    cudaCall(cudaStreamSynchronize(stream_), "finalizeCalculation(), cudaStreamSynchronize()");
    for (size_t i = 0; i < nOutputVariables_[currentId_ - 1]; ++i) {
        std::copy(h_output + i * size_[currentId_ - 1],
                  h_output + (i + 1) * size_[currentId_ - 1], output[i]);
    }
    //for (size_t i = 100000; i < 100100; ++i) {
    //    std::cout << "h_output[" << i << "] = " << h_output[i] << std::endl;
    //}

    // clear memory
    releaseMem(input, "finalizeCalculation()");
    //delete[] h_output;
    cudaCall(cudaFreeHost(h_input), "finalizeCalculation(), cudaFreeHost(h_input)");
    cudaCall(cudaFreeHost(h_output), "finalizeCalculation(), cudaFreeHost(h_output)");
    //std::cout << "finalizeCalculation() done" << std::endl;
    if (settings_.debug) {
        debugInfo_.nanoSecondsDataCopy += timer.elapsed().wall - timerBase;
    }
    //// force stop
    //QL_REQUIRE(1 == 0, "Force exit for testing");
    //std::cout << "================================================" << std::endl;
}

const ComputeContext::DebugInfo& CudaContext::debugInfo() const { return debugInfo_; }
std::vector<std::pair<std::string, std::string>> CudaContext::deviceInfo() const { return deviceInfo_; }
bool CudaContext::supportsDoublePrecision() const { return supportsDoublePrecision_; }

size_t CudaContext::binom_helper(size_t n, size_t k) {
    return (k==1||k==n-1)?n:(k+k<n)?(binom_helper(n-1,k-1) * n)/k:(binom_helper(n-1,k)*n)/(n-k);
}

#endif

#ifndef ORE_ENABLE_CUDA
CudaFramework::CudaFramework() {}
CudaFramework::~CudaFramework() {}
#endif

std::set<std::string> CudaFramework::getAvailableDevices() const {
    std::set<std::string> tmp;
    for (auto const& [name, _] : contexts_)
        tmp.insert(name);
    return tmp;
}

ComputeContext* CudaFramework::getContext(const std::string& deviceName) {
    auto c = contexts_.find(deviceName);
    if (c != contexts_.end()) {
        return c->second;
    }
    QL_FAIL("CudaFrameWork::getContext(): device '"
            << deviceName << "' not found. Available devices: " << boost::join(getAvailableDevices(), ","));
}
}; // namespace QuantExt
