#include <gmock/gmock.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/opendaq.h>
#include <testutils/testutils.h>
#include <thread>

using namespace daq;
using ExampleModuleTest = testing::Test;
using ExampleIIRFilterTest = testing::Test;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST_F(ExampleModuleTest, TestAdd)
{
    const auto instance = Instance();
    ASSERT_TRUE(instance.addFunctionBlock("ExampleScalingModule").assigned());
}

TEST_F(ExampleModuleTest, TestPropCount)
{
    const auto instance = Instance();
    auto fb = instance.addFunctionBlock("ExampleScalingModule");
    ASSERT_EQ(fb.getAllProperties().getCount(), 7);
}

TEST_F(ExampleModuleTest, TestDataScaling)
{
    const auto instance = Instance();
    auto fb = instance.addFunctionBlock("ExampleScalingModule");
    fb.setPropertyValue("Scale", 2);

    auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float32).setValueRange(Range(-10, 10)).build();
    auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "Data");

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::Int64)
                                      .setUnit(Unit("s", -1, "seconds", "time"))
                                      .setTickResolution(Ratio(1, 1000))
                                      .setRule(LinearDataRule(1, 0))
                                      .setOrigin("1970-01-01T01:00:00+00:00")
                                      .build();
    auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "DomainData");
    signal.setDomainSignal(domainSignal);

    fb.getInputPorts()[0].connect(signal);
    auto streamReader = StreamReader(fb.getSignals()[0], SampleType::Float32, SampleType::Int64);

    auto domainPacket = DataPacket(domainDescriptor, 10, 0);
    auto packet = DataPacketWithDomain(domainPacket, dataDescriptor, 10);
    float* data = static_cast<float*>(packet.getRawData());
    for (auto i = 0; i < 10; i++)
        data[i] = static_cast<float>(i);

    signal.sendPacket(packet);
    domainSignal.sendPacket(domainPacket);

    SizeT count = 10;
    std::vector<float> readData;
    readData.resize(count);

    auto status = streamReader.read(readData.data(), &count);
    ASSERT_EQ(status.getReadStatus(), ReadStatus::Event);
    ASSERT_EQ(count, 0);

    while (count < 10)
    {
        using namespace std::chrono_literals;
        count = streamReader.getAvailableCount();
        std::this_thread::sleep_for(100ms);
    }

    ASSERT_EQ(count, 10);

    status = streamReader.read(readData.data(), &count);
    ASSERT_EQ(count, 10);
    ASSERT_EQ(status.getReadStatus(), ReadStatus::Ok);

    for (int i = 0; i < 10; i++)
        ASSERT_EQ(static_cast<int>(readData[i]), i * 2);
}

// Test 1: Adding function block
TEST_F(ExampleIIRFilterTest, CanAddFilter)
{
    const auto instance = Instance();
    ASSERT_TRUE(instance.addFunctionBlock("ExampleIIRFilter").assigned());
}

// Test 2: Response of filter to step values 0 then 1
TEST_F(ExampleIIRFilterTest, StepResponse)
{
    const auto instance = Instance();
    const auto fb = instance.addFunctionBlock("ExampleIIRFilter");
    fb.setPropertyValue("CutoffFrequency", 5);

    const SizeT sampleCount = 200;

    const auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float64).setValueRange(Range(0.0, 1.0)).build();

    const auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "StepInput");

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::UInt64)
                                      .setTickResolution(Ratio(1, 1000))
                                      .setRule(LinearDataRule(1, 0))
                                      .setOrigin("1970-01-01T01:00:00+00:00")
                                      .build();

    auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "Domain");
    signal.setDomainSignal(domainSignal);

    fb.getInputPorts()[0].connect(signal);

    auto reader = StreamReader(fb.getSignals()[0], SampleType::Float64, SampleType::UInt64);

    // Preparing data – 50 zero, 150 one
    SizeT count = sampleCount;
    auto domainPacket = DataPacket(domainDescriptor, count, 0);
    auto dataPacket = DataPacketWithDomain(domainPacket, dataDescriptor, count);

    double* raw = static_cast<double*>(dataPacket.getRawData());
    for (auto i = 0; i < sampleCount; ++i)
        raw[i] = i < 50 ? 0.0 : 1.0;

    signal.sendPacket(dataPacket);
    domainSignal.sendPacket(domainPacket);
    std::vector<double> dummyReadData;
    dummyReadData.resize(count);

    auto status = reader.read(dummyReadData.data(), &count);
    ASSERT_EQ(status.getReadStatus(), ReadStatus::Event);

    int retries = 20;
    SizeT availableCount = 0;
    while (availableCount < sampleCount && retries-- > 0)
    {
        using namespace std::chrono_literals;
        availableCount = reader.getAvailableCount();
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_GT(availableCount, 0);

    std::vector<double> output(sampleCount);
    std::vector<uint64_t> domain(sampleCount);
    SizeT read = sampleCount;
    status = reader.readWithDomain(output.data(), domain.data(), &read);

    ASSERT_EQ(status.getReadStatus(), ReadStatus::Ok);
    ASSERT_EQ(read, sampleCount);
    ASSERT_NEAR(output.back(), 1.0, 0.05);

    for (SizeT i = 51; i < read; ++i)
        ASSERT_GE(output[i], output[i - 1]) << "Output is not monotonically increasing: y[" << i - 1 << "]=" << output[i - 1] << ", y[" << i
                                            << "]=" << output[i];
}

// Test 3: Testing attenuation of high frequencies
TEST_F(ExampleIIRFilterTest, AttenuatesHighFrequencies)
{
    const auto instance = Instance();
    const auto fb = instance.addFunctionBlock("ExampleIIRFilter");
    fb.setPropertyValue("CutoffFrequency", 5);

    const SizeT sampleCount = 200;
    const double samplingRate = 1000.0;
    const double signalFrequency = 100.0;

    const auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float64).setValueRange(Range(-10.0, 10.0)).build();

    const auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "HighFreqInput");

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::UInt64)
                                      .setTickResolution(Ratio(1, static_cast<Int>(samplingRate)))
                                      .setRule(LinearDataRule(1, 0))
                                      .setOrigin("1970-01-01T01:00:00+00:00")
                                      .build();

    auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "Domain");
    signal.setDomainSignal(domainSignal);

    fb.getInputPorts()[0].connect(signal);

    auto reader = StreamReader(fb.getSignals()[0], SampleType::Float64, SampleType::UInt64);

    auto domainPacket = DataPacket(domainDescriptor, sampleCount, 0);
    auto dataPacket = DataPacketWithDomain(domainPacket, dataDescriptor, sampleCount);
    double* raw = static_cast<double*>(dataPacket.getRawData());
    const int amplitude = 10;
    double inputEnergy = 0;

    for (SizeT i = 0; i < sampleCount; ++i)
    {
        double timeInSeconds = static_cast<double>(i) / samplingRate;
        double inputSample = amplitude * std::sin(2 * M_PI * signalFrequency * timeInSeconds);
        raw[i] = inputSample;
        inputEnergy += inputSample * inputSample;
    }

    signal.sendPacket(dataPacket);
    domainSignal.sendPacket(domainPacket);

    std::vector<double> dummyReadData(sampleCount);
    SizeT dummyCount = sampleCount;
    auto status = reader.read(dummyReadData.data(), &dummyCount);
    ASSERT_EQ(status.getReadStatus(), ReadStatus::Event);

    int retries = 20;
    SizeT availableCount = 0;
    while (availableCount < sampleCount && retries-- > 0)
    {
        using namespace std::chrono_literals;
        availableCount = reader.getAvailableCount();
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_GT(availableCount, 0);

    std::vector<double> output(sampleCount);
    std::vector<uint64_t> domain(sampleCount);
    SizeT read = sampleCount;
    status = reader.readWithDomain(output.data(), domain.data(), &read);

    ASSERT_EQ(status.getReadStatus(), ReadStatus::Ok);
    ASSERT_EQ(read, sampleCount);

    double outputEnergy = 0;
    for (SizeT i = 0; i < read; ++i)
        outputEnergy += output[i] * output[i];

    const SizeT maxExpectedTotalOutputEnergy = 28;
    ASSERT_LT(outputEnergy, maxExpectedTotalOutputEnergy);
    ASSERT_LT(outputEnergy, inputEnergy * 0.1);
}

// Test 4: Testing attenuation of low frequencies
TEST_F(ExampleIIRFilterTest, PassesLowFrequencies)
{
    const auto instance = Instance();
    const auto fb = instance.addFunctionBlock("ExampleIIRFilter");
    fb.setPropertyValue("CutoffFrequency", 50);

    const SizeT sampleCount = 100;
    const double samplingRate = 1000.0;
    const double signalFrequency = 0.1;

    const auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float64).setValueRange(Range(-10.0, 10.0)).build();

    const auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "LowFreqInput");

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::UInt64)
                                      .setTickResolution(Ratio(1, static_cast<Int>(samplingRate)))
                                      .setRule(LinearDataRule(1, 0))
                                      .setOrigin("1970-01-01T01:00:00+00:00")
                                      .build();

    auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "Domain");
    signal.setDomainSignal(domainSignal);

    fb.getInputPorts()[0].connect(signal);

    auto reader = StreamReader(fb.getSignals()[0], SampleType::Float64, SampleType::UInt64);

    auto domainPacket = DataPacket(domainDescriptor, sampleCount, 0);
    auto dataPacket = DataPacketWithDomain(domainPacket, dataDescriptor, sampleCount);
    double* raw = static_cast<double*>(dataPacket.getRawData());
    const int amplitude = 10;
    double inputEnergy = 0;

    for (SizeT i = 0; i < sampleCount; ++i)
    {
        double timeInSeconds = static_cast<double>(i) / samplingRate;
        double inputSample = amplitude * std::sin(2 * M_PI * signalFrequency * timeInSeconds);
        raw[i] = inputSample;
        inputEnergy += inputSample * inputSample;
    }

    signal.sendPacket(dataPacket);
    domainSignal.sendPacket(domainPacket);

    std::vector<double> dummyReadData(sampleCount);
    SizeT dummyCount = sampleCount;
    auto status = reader.read(dummyReadData.data(), &dummyCount);
    ASSERT_EQ(status.getReadStatus(), ReadStatus::Event);

    int retries = 20;
    SizeT availableCount = 0;
    while (availableCount < sampleCount && retries-- > 0)
    {
        using namespace std::chrono_literals;
        availableCount = reader.getAvailableCount();
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_GT(availableCount, 0);

    std::vector<double> output(sampleCount);
    std::vector<uint64_t> domain(sampleCount);
    SizeT read = sampleCount;
    status = reader.readWithDomain(output.data(), domain.data(), &read);

    ASSERT_EQ(status.getReadStatus(), ReadStatus::Ok);
    ASSERT_EQ(read, sampleCount);

    double outputEnergy = 0;
    for (SizeT i = 0; i < read; ++i)
        outputEnergy += output[i] * output[i];

    ASSERT_GT(outputEnergy, inputEnergy * 0.9);
}

// Test 5: Testing valid range of cutoff frequency
TEST_F(ExampleIIRFilterTest, CutoffFrequencyInValidRange)
{
    const auto instance = Instance();
    const auto fb = instance.addFunctionBlock("ExampleIIRFilter");

    const double sampleRate = 1000.0;
    const auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float64).setValueRange(Range(0.0, 1.0)).build();

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::UInt64)
                                      .setTickResolution(Ratio(1, static_cast<Int>(sampleRate)))
                                      .setRule(LinearDataRule(1, 0))
                                      .build();

    const auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "Input");
    const auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "Domain");

    signal.setDomainSignal(domainSignal);
    fb.getInputPorts()[0].connect(signal);

    const Int defaultCutoff = fb.getPropertyValue("CutoffFrequency");

    ASSERT_GT(defaultCutoff, 0);
    const Int maxAllowed = static_cast<Int>(sampleRate / 2) - 1;
    ASSERT_LT(defaultCutoff, maxAllowed);

    const std::vector<Int> validValues = {1, 10, 100, 400, maxAllowed};
    for (const auto& val : validValues)
    {
        fb.setPropertyValue("CutoffFrequency", val);
        const Int readBack = fb.getPropertyValue("CutoffFrequency");
        ASSERT_EQ(readBack, val) << "The set value was not stored correctly:: " << val;
    }
}

// Test 6: Testing invalid values for cutoff frequency
TEST_F(ExampleIIRFilterTest, InvalidCutoffFrequencyThrows)
{
    const auto instance = Instance();
    const auto fb = instance.addFunctionBlock("ExampleIIRFilter");

    // Konfiguracija za sampleRate
    const double sampleRate = 1000.0;
    const auto dataDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float64).setValueRange(Range(0.0, 1.0)).build();

    const auto domainDescriptor = DataDescriptorBuilder()
                                      .setSampleType(SampleType::UInt64)
                                      .setTickResolution(Ratio(1, static_cast<Int>(sampleRate)))
                                      .setRule(LinearDataRule(1, 0))
                                      .build();

    const auto signal = SignalWithDescriptor(instance.getContext(), dataDescriptor, nullptr, "Input");
    const auto domainSignal = SignalWithDescriptor(instance.getContext(), domainDescriptor, nullptr, "Domain");
    signal.setDomainSignal(domainSignal);
    fb.getInputPorts()[0].connect(signal);

    const Int nyquistLimit = static_cast<Int>(sampleRate / 2);
    const Int maxValid = nyquistLimit - 1;

    EXPECT_THROW(fb.setPropertyValue("CutoffFrequency", -1), daq::GeneralErrorException);
    EXPECT_THROW(fb.setPropertyValue("CutoffFrequency", 0), daq::GeneralErrorException);
    EXPECT_THROW(fb.setPropertyValue("CutoffFrequency", nyquistLimit), daq::GeneralErrorException);

    EXPECT_NO_THROW(fb.setPropertyValue("CutoffFrequency", 1));
    EXPECT_NO_THROW(fb.setPropertyValue("CutoffFrequency", maxValid));
}
