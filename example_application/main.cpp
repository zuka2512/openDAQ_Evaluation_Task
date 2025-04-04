/**
 * ExampleIIRFilter demo application
 */

#include <opendaq/opendaq.h>
#include <iostream>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    const auto instance = Instance();
    const auto referenceDevice = instance.addDevice("daqref://device0");
    const auto iirFilter = instance.addFunctionBlock("ExampleIIRFilter");
    std::cout << "ExampleIIRFilter successfully added!";

    iirFilter.getInputPorts()[0].connect(referenceDevice.getSignalsRecursive()[0]);

    const auto renderer = instance.addFunctionBlock("RefFBModuleRenderer");
    renderer.getInputPorts()[0].connect(iirFilter.getSignals()[0]);
    renderer.getInputPorts()[1].connect(referenceDevice.getSignalsRecursive()[0]);

    std::cout << "ExampleIIRFilter is running.\n";
    std::cout << "Press ENTER to exit the application..." << std::endl;
    std::cin.get();
}
