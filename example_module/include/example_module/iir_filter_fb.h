#pragma once
#include <example_module/common.h>
#include <opendaq/function_block_impl.h>
#include <opendaq/opendaq.h>
#include <opendaq/stream_reader_ptr.h>

BEGIN_NAMESPACE_EXAMPLE_MODULE

class IIRFilterFBImpl final : public FunctionBlock
{
public:
    explicit IIRFilterFBImpl(const ContextPtr& ctx, const ComponentPtr& parent, const StringPtr& localId);
    static FunctionBlockTypePtr CreateType();

private:
    InputPortPtr inputPort;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;
    StreamReaderPtr reader;

    std::vector<double> inputData;
    std::vector<double> inputDomainData;

    double prevInput = 0.0;
    double prevOutput = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double b1 = 0.0;
    double cutoffFreq;

    bool configValid = false;
    SizeT sampleRate = 0;
    SampleType inputSampleType;

    DataDescriptorPtr inputDataDescriptor;
    DataDescriptorPtr inputDomainDataDescriptor;

    void createInputPorts();
    void createSignals();
    void initProperties();
    void readProperties();
    void propertyChanged(bool configure);
    void configure();
    void resetFilterState();

    void calculate();
    void processData(SizeT readAmount, SizeT packetOffset);
    void processEventPacket(const EventPacketPtr& packet);
    void processSignalDescriptorChanged(const DataDescriptorPtr& dataDesc, const DataDescriptorPtr& domainDesc);

    void calculateFilterCoefficients(double sampleRate);
    void validateCutoffFrequency(double cutoffFreq, double sampleRate) const;
};

END_NAMESPACE_EXAMPLE_MODULE
