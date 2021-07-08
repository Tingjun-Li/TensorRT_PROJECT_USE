#include "argsParser.h"
#include "buffers.h"
#include "common.h"
#include "logger.h"
#include "parserOnnxConfig.h"
#include "NvInfer.h"


#include "NvInfer.h"
#include <cuda_runtime_api.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

const std::string gSampleName = "TensorRT.sample_onnx_mnist";

//! \brief  The SampleOnnxMNIST class implements the ONNX MNIST sample
//!
//! \details It creates the network using an ONNX model
//!
class SampleOnnxMNIST
{
    template <typename T>
    using SampleUniquePtr = std::unique_ptr<T, samplesCommon::InferDeleter>;

public:
    SampleOnnxMNIST(const samplesCommon::OnnxSampleParams& params)
        : mParams(params)
        , mEngine(nullptr)
    {
    }

    //!
    //! \brief Function builds the network engine
    //!
    bool build();

    //!
    //! \brief Runs the TensorRT inference engine for this sample
    //!
    bool infer();


    bool serialize();

private:
    samplesCommon::OnnxSampleParams mParams; //!< The parameters for the sample.

    nvinfer1::Dims mInputDims;  //!< The dimensions of the input to the network.
    nvinfer1::Dims mOutputDims; //!< The dimensions of the output to the network.
    int mNumber{0};             //!< The number to classify

    std::shared_ptr<nvinfer1::ICudaEngine> mEngine; //!< The TensorRT engine used to run the network

    //!
    //! \brief Parses an ONNX model for MNIST and creates a TensorRT network
    //!
    bool constructNetwork(SampleUniquePtr<nvinfer1::IBuilder>& builder,
        SampleUniquePtr<nvinfer1::INetworkDefinition>& network, SampleUniquePtr<nvinfer1::IBuilderConfig>& config,
        SampleUniquePtr<nvonnxparser::IParser>& parser);

    //!
    //! \brief Reads the input  and stores the result in a managed buffer
    //!
    bool processInput(const samplesCommon::BufferManager& buffers);

    //!
    //! \brief Classifies digits and verify result
    //!
    bool verifyOutput(const samplesCommon::BufferManager& buffers);
};

//!
//! \brief Creates the network, configures the builder and creates the network engine
//!
//! \details This function creates the Onnx MNIST network by parsing the Onnx model and builds
//!          the engine that will be used to run MNIST (mEngine)
//!
//! \return Returns true if the engine was created successfully and false otherwise
//!
bool SampleOnnxMNIST::build()
{
    /// REMARK: to load a new onnx model, uncommon the following lines until the next <REMARK:> script
    // -----------------------------------------------------------------------------------------------------------------------
    // auto builder = SampleUniquePtr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(gLogger.getTRTLogger()));
    // if (!builder)
    // {
    //     return false;
    // }

    // const auto explicitBatch = 1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);     
    // auto network = SampleUniquePtr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicitBatch));
    // if (!network)
    // {
    //     return false;
    // }

    // auto config = SampleUniquePtr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
    // if (!config)
    // {
    //     return false;
    // }

    // auto parser = SampleUniquePtr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, gLogger.getTRTLogger()));
    // if (!parser)
    // {
    //     return false;
    // }

    // auto constructed = constructNetwork(builder, network, config, parser);
    // if (!constructed)
    // {
    //     return false;
    // }

    // mEngine = std::shared_ptr<nvinfer1::ICudaEngine>(
    //     builder->buildEngineWithConfig(*network, *config), samplesCommon::InferDeleter());


    /// REMARK: we can deserialize a serialized engine if we have one:
    // -----------------------------------------------------------------------------------------------------------------------
    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(gLogger);
    std::string cached_path = "/home/tingjun/Desktop/TensorRT_PROJECT_USE/engines/0616_2blocks_best_val_loss.trt";
    std::ifstream fin(cached_path);
    std::string cached_engine = "";
    while (fin.peek() != EOF) {
        std::stringstream buffer;
        buffer << fin.rdbuf();
        cached_engine.append(buffer.str());
    }
    fin.close();
    mEngine = std::shared_ptr<nvinfer1::ICudaEngine> (
                                    runtime->deserializeCudaEngine(cached_engine.data(), cached_engine.size(), nullptr),
                                    samplesCommon::InferDeleter());
    if (!mEngine)
    {
        return false;
    }

    /// REMARK: the following can be used to find input/output dimension
    // -----------------------------------------------------------------------------------------------------------------------
    // assert(network->getNbInputs() == 1);
    // mInputDims = network->getInput(0)->getDimensions();
    // assert(mInputDims.nbDims == 3);

    // assert(network->getNbOutputs() == 1);
    // mOutputDims = network->getOutput(0)->getDimensions();
    // assert(mOutputDims.nbDims == 2);
    // -----------------------------------------------------------------------------------------------------------------------

    std::cout << "Successfully built the engine" << std::endl;

    return true;
}

//!
//! \brief Uses a ONNX parser to create the Onnx MNIST Network and marks the
//!        output layers
//!
//! \param network Pointer to the network that will be populated with the Onnx MNIST network
//!
//! \param builder Pointer to the engine builder
//!
bool SampleOnnxMNIST::constructNetwork(SampleUniquePtr<nvinfer1::IBuilder>& builder,
    SampleUniquePtr<nvinfer1::INetworkDefinition>& network, SampleUniquePtr<nvinfer1::IBuilderConfig>& config,
    SampleUniquePtr<nvonnxparser::IParser>& parser)
{
    auto parsed = parser->parseFromFile(
        locateFile(mParams.onnxFileName, mParams.dataDirs).c_str(), static_cast<int>(gLogger.getReportableSeverity()));
    if (!parsed)
    {
        return false;
    }

    builder->setMaxBatchSize(mParams.batchSize);
    config->setMaxWorkspaceSize(16_MiB);
    if (mParams.fp16)
    {
        config->setFlag(BuilderFlag::kFP16);
    }
    // if (mParams.int8)
    // {
    //     config->setFlag(BuilderFlag::kINT8);
    //     samplesCommon::setAllTensorScales(network.get(), 127.0f, 127.0f);
    // }

    samplesCommon::enableDLA(builder.get(), config.get(), mParams.dlaCore);

    return true;
}

//!
//! \brief Runs the TensorRT inference engine for this sample
//!
//! \details This function is the main execution function of the sample. It allocates the buffer,
//!          sets inputs and executes the engine.
//!
bool SampleOnnxMNIST::infer()
{
    // Create RAII buffer manager object
    if (!mEngine) {
        std::cerr << "Failed to load mEngine" << std::endl;
    }
    samplesCommon::BufferManager buffers(mEngine, mParams.batchSize);
    std::cout << "Successfuly built the buffer" << std::endl;
    auto context = SampleUniquePtr<nvinfer1::IExecutionContext>(mEngine->createExecutionContext());
    std::cout << "Successfully build an execution context" << std::endl;
    if (!context)
    {
        return false;
    }

    // Read the input data into the managed buffers
    assert(mParams.inputTensorNames.size() == 1);
    if (!processInput(buffers))
    {
        std::cerr << "Failed in reading input" << std::endl;
        return false;
    }

    // Memcpy from host input buffers to device input buffers
    buffers.copyInputToDevice();

    bool status = context->executeV2(buffers.getDeviceBindings().data());
    if (!status)
    {
        std::cerr << "Failed in making execution" << std::endl;
        return false;
    }

    // Memcpy from device output buffers to host output buffers
    buffers.copyOutputToHost();

    // Verify results
    if (!verifyOutput(buffers))
    {
        return false;
    }

    return true;
}

bool SampleOnnxMNIST::serialize() {
    nvinfer1::IHostMemory *serializedModel = mEngine->serialize();
    std::string serialize_str;
    std::ofstream serialize_output_stream;
    serialize_str.resize(serializedModel->size());
    memcpy((void*)serialize_str.data(), serializedModel->data(), serializedModel->size());
    serialize_output_stream.open("/home/tingjun/Desktop/TensorRT_PROJECT_USE/engines/0616_2blocks_best_val_loss.trt");
    
    serialize_output_stream << serialize_str;
    serialize_output_stream.close();
    serializedModel->destroy();
    
    std::cout << "Successfully serialized the engine" << std::endl;
    return true;
}

//!
//! \brief Reads the input and stores the result in a managed buffer
//!
bool SampleOnnxMNIST::processInput(const samplesCommon::BufferManager& buffers)
{   
    // const int inputH = mInputDims.d[1];
    // std::cout << "inputH is: " << inputH << std::endl;

    // const int inputW = mInputDims.d[2];
    // std::cout << "inputW is: " << inputW << std::endl;

    const int inputH = 150;
    const int inputW = 54;
    
    std::vector<uint8_t> fileData(inputH * inputW);
    std::ifstream data_file;
    data_file.open((locateFile("input_matrix.bin", mParams.dataDirs)), std::ios::in | std::ios::binary);
    float* hostDataBuffer = static_cast<float*>(buffers.getHostBuffer(mParams.inputTensorNames[0]));
    int number_of_items = 150 * 54;
    // hostDataBuffer.resize(number_of_items);
    data_file.read(reinterpret_cast<char*>(&hostDataBuffer[0]), number_of_items * sizeof(float));
    std::cout << hostDataBuffer[0] << std::endl;
    std::cout << hostDataBuffer[5 * 54 + 5] << std::endl;
    std::cout << hostDataBuffer[10 * 54 + 10] << std::endl;
    std::cout << hostDataBuffer[15 * 54 + 15] << std::endl;
    data_file.close(); 


    return true;
}

//!
//! \brief Classifies digits and verify result
//!
//! \return whether the classification output matches expectations
//!
bool SampleOnnxMNIST::verifyOutput(const samplesCommon::BufferManager& buffers)
{
    // const int outputSize = mOutputDims.d[1];
    // std::cout << "outputSize is " << outputSize << std::endl;

    const int outputSize = 16;
    
    float* output = static_cast<float*>(buffers.getHostBuffer(mParams.outputTensorNames[0]));
    float val{0.0f};
    int idx{0};

    gLogInfo << "Output: " << std::endl;
    float current_max = output[0];
    int output_idx = 0;
    for (int i = 0; i < outputSize; i++) {
        gLogInfo << "Probability of leg status " << i << " before normalization is: " << output[i] << std::endl;
        if (output[i] > current_max) {
            current_max = output[i];
            output_idx = i;
        }
    }
    std::cout << "OUTPUT: " << output_idx << std::endl;
    return true;
}

//!
//! \brief Initializes members of the params struct using the command line args
//!
samplesCommon::OnnxSampleParams initializeSampleParams(const samplesCommon::Args& args)
{
    samplesCommon::OnnxSampleParams params;
    if (args.dataDirs.empty()) //!< Use default directories if user hasn't provided directory paths
    {
        std::cout << "Using default directory" << endl;
        params.dataDirs.push_back("weights/");
        params.dataDirs.push_back("data/");
        // params.dataDirs.push_back("data/samples/mnist/");
    }
    else //!< Use the data directory provided by the user
    {
        std::cout << "Using directory provided by the user" << endl;
        params.dataDirs = args.dataDirs;
    }
    params.onnxFileName = "0616_2blocks_best_val_loss.onnx";
    params.inputTensorNames.push_back("input");
    params.batchSize = 1;
    params.outputTensorNames.push_back("output");
    params.dlaCore = args.useDLACore;
    params.int8 = args.runInInt8;
    params.fp16 = args.runInFp16;

    return params;
}

//!
//! \brief Prints the help information for running this sample
//!
void printHelpInfo()
{
    std::cout
        << "Usage: ./sample_onnx_mnist [-h or --help] [-d or --datadir=<path to data directory>] [--useDLACore=<int>]"
        << std::endl;
    std::cout << "--help          Display help information" << std::endl;
    std::cout << "--datadir       Specify path to a data directory, overriding the default. This option can be used "
                 "multiple times to add multiple directories. If no data directories are given, the default is to use "
                 "(data/samples/mnist/, data/mnist/)"
              << std::endl;
    std::cout << "--useDLACore=N  Specify a DLA engine for layers that support DLA. Value can range from 0 to n-1, "
                 "where n is the number of DLA engines on the platform."
              << std::endl;
    std::cout << "--int8          Run in Int8 mode." << std::endl;
    std::cout << "--fp16          Run in FP16 mode." << std::endl;
}

int main(int argc, char** argv) 
// argc is 1 + the number of arguments
// *argv is the execution line without arguments
{
    samplesCommon::Args args;
    bool argsOK = samplesCommon::parseArgs(args, argc, argv);
    std::cout << "What is argc?  " << argc << std::endl;
    std::cout << "What is *argv?  " << *argv << std::endl;
    
    if (!argsOK)
    {
        gLogError << "Invalid arguments" << std::endl;
        printHelpInfo();
        return EXIT_FAILURE;
    }
    if (args.help)
    {
        printHelpInfo();
        return EXIT_SUCCESS;
    }

    auto sampleTest = gLogger.defineTest(gSampleName, argc, argv);

    gLogger.reportTestStart(sampleTest);

    SampleOnnxMNIST sample(initializeSampleParams(args));

    gLogInfo << "Building and running a GPU inference engine for Onnx MNIST" << std::endl;

    if (!sample.build())
    {
        return gLogger.reportFail(sampleTest);
    }
    if (!sample.infer())
    {
        return gLogger.reportFail(sampleTest);
    }

    /// REMARK: if you want to serialize the engine, please uncommon the following line:
    // if (!sample.serialize())
    // {
    //     std::cerr << "Failed to serialize" << std::endl;
    // }

    return gLogger.reportPass(sampleTest);
}
