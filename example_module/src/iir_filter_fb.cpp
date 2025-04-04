#include <example_module/iir_filter_fb.h>
#include <opendaq/data_descriptor_ptr.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/signal_factory.h>
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BEGIN_NAMESPACE_EXAMPLE_MODULE

IIRFilterFBImpl::IIRFilterFBImpl(const ContextPtr& context, const ComponentPtr& parent, const StringPtr& localId)
    : FunctionBlock(CreateType(), context, parent, localId)
{
    initComponentStatus();
    createInputPorts();
    createSignals();
    initProperties();
}

FunctionBlockTypePtr IIRFilterFBImpl::CreateType()
{
    return FunctionBlockType("ExampleIIRFilter", "IIR Filter", "Simple first-order IIR filter");
}

void IIRFilterFBImpl::createInputPorts()
{
    inputPort = createAndAddInputPort("Input", PacketReadyNotification::Scheduler);
    reader = StreamReaderFromPort(inputPort, SampleType::Float64, SampleType::UInt64);
    reader.setOnDataAvailable([this] { calculate(); });
}

void IIRFilterFBImpl::calculateFilterCoefficients(double sampleRate)
{
    double wc = std::tan(static_cast<double>(M_PI) * cutoffFreq / sampleRate);
    double norm = 1.0f / (1.0f + wc);
    a0 = wc * norm;
    a1 = a0;
    b1 = (1.0f - wc) * norm;
}

void IIRFilterFBImpl::configure()
{
    resetFilterState();
    inputData.clear();
    inputDomainData.clear();

    try
    {
        if (!inputDomainDataDescriptor.assigned() || inputDomainDataDescriptor == NullDataDescriptor())
        {
            throw std::runtime_error("No domain input");
        }

        if (!inputDataDescriptor.assigned() || inputDataDescriptor == NullDataDescriptor())
        {
            throw std::runtime_error("No value input");
        }

        inputSampleType = inputDataDescriptor.getSampleType();
        if (inputSampleType != SampleType::Float64 && 
            inputSampleType != SampleType::Float32 && 
            inputSampleType != SampleType::Int8 &&
            inputSampleType != SampleType::Int16 && 
            inputSampleType != SampleType::Int32 && 
            inputSampleType != SampleType::Int64 &&
            inputSampleType != SampleType::UInt8 && 
            inputSampleType != SampleType::UInt16 && 
            inputSampleType != SampleType::UInt32 &&
            inputSampleType != SampleType::UInt64)
        {
            throw std::runtime_error("Invalid sample type");
        }

        // Accept only synchronous (linear implicit) domain signals
        if (inputDomainDataDescriptor.getSampleType() != SampleType::Int64 &&
            inputDomainDataDescriptor.getSampleType() != SampleType::UInt64)
        {
            throw std::runtime_error("Incompatible domain data sample type");
        }

        const auto domainRule = inputDomainDataDescriptor.getRule();
        if (!domainRule.assigned() || domainRule.getType() != DataRuleType::Linear)
            throw std::runtime_error("Domain must have linear rule");

        double sampleRate = static_cast<double>(reader::getSampleRate(inputDomainDataDescriptor));
        if (sampleRate <= 0.0f)
        {
            throw std::runtime_error("Invalid sampleRate: " + std::to_string(sampleRate) + "\n");
        }

        calculateFilterCoefficients(sampleRate);
        validateCutoffFrequency(cutoffFreq, sampleRate);

        outputSignal.setDescriptor(inputDataDescriptor);
        outputDomainSignal.setDescriptor(inputDomainDataDescriptor);

        inputData.resize(sampleRate);
        inputDomainData.resize(sampleRate);

        setComponentStatus(ComponentStatus::Ok);
        configValid = true;
    }
    catch (const std::exception& e)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, e.what());
        outputSignal.setDescriptor(nullptr);
        configValid = false;
        throw e;
    }
}

void IIRFilterFBImpl::processSignalDescriptorChanged(const DataDescriptorPtr& dataDescriptor, const DataDescriptorPtr& domainDescriptor)
{
    if (dataDescriptor.assigned())
        this->inputDataDescriptor = dataDescriptor;
    if (domainDescriptor.assigned())
        this->inputDomainDataDescriptor = domainDescriptor;

    configure();
}

void IIRFilterFBImpl::calculate()
{
    auto lock = this->getAcquisitionLock();

    while (!reader.getEmpty())
    {
        SizeT readAmount = std::min(reader.getAvailableCount(), static_cast<SizeT>(1024));
        const auto status = reader.readWithDomain(inputData.data(), inputDomainData.data(), &readAmount);

        if (configValid)
            processData(readAmount, status.getOffset());

        if (status.getReadStatus() == ReadStatus::Event)
        {
            const auto eventPacket = status.getEventPacket();
            if (eventPacket.assigned())
                processEventPacket(eventPacket);
            return;
        }
    }
}

void IIRFilterFBImpl::processEventPacket(const EventPacketPtr& packet)
{
    if (packet.getEventId() == event_packet_id::DATA_DESCRIPTOR_CHANGED)
    {
        DataDescriptorPtr dataDesc = packet.getParameters().get(event_packet_param::DATA_DESCRIPTOR);
        DataDescriptorPtr domainDesc = packet.getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR);
        processSignalDescriptorChanged(dataDesc, domainDesc);
    }
}

void IIRFilterFBImpl::processData(SizeT readAmount, SizeT packetOffset)
{
    if (readAmount == 0)
        return;

    const auto outputDomainPacket = DataPacket(inputDomainDataDescriptor, readAmount, packetOffset);
    const auto outputPacket = DataPacketWithDomain(outputDomainPacket, inputDataDescriptor, readAmount);
    auto outputData = static_cast<double*>(outputPacket.getRawData());

    for (SizeT i = 0; i < readAmount; ++i)
    {
        const double x = inputData[i];
        const double y = a0 * x + a1 * prevInput + b1 * prevOutput;

        prevInput = x;
        prevOutput = y;

        *outputData++ = y;
    }

    outputSignal.sendPacket(outputPacket);
    outputDomainSignal.sendPacket(outputDomainPacket);
}

void IIRFilterFBImpl::createSignals()
{
    outputSignal = createAndAddSignal("Filtered");
    outputDomainSignal = createAndAddSignal("FilteredTime", nullptr, false);
    outputSignal.setDomainSignal(outputDomainSignal);
    outputSignal.setName("Filtered");
}

void IIRFilterFBImpl::initProperties()
{
    const auto cutoffProp = IntProperty("CutoffFrequency", 5);
    objPtr.addProperty(cutoffProp);
    objPtr.getOnPropertyValueWrite("CutoffFrequency") += [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(true); };
    readProperties();
}

void IIRFilterFBImpl::propertyChanged(bool configure)
{
    readProperties();
    if (configure && inputDomainDataDescriptor.assigned())
        this->configure();
}

void IIRFilterFBImpl::readProperties()
{
    cutoffFreq = static_cast<double>(objPtr.getPropertyValue("CutoffFrequency"));
}

void IIRFilterFBImpl::validateCutoffFrequency(double cutoffFreq, const double sampleRate) const
{
    const Int maxCutoff = static_cast<Int>(sampleRate / 2) - 1;
    if (cutoffFreq < 1 || cutoffFreq > maxCutoff)
    {
        std::stringstream ss;
        ss << "CutoffFrequency " << cutoffFreq << " is in invalid range (1 - " << maxCutoff << " Hz)";
        throw std::invalid_argument(ss.str());
    }
}

void IIRFilterFBImpl::resetFilterState()
{
    prevInput = 0.0;
    prevOutput = 0.0;
}

END_NAMESPACE_EXAMPLE_MODULE
