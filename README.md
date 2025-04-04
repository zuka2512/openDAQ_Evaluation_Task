# Example function block module

Simple example that builds an openDAQ module giving access to an example function block. Said function block scales an input signal with a provided scale, and offsets it by a provided offset.

## Testing the module

To test the module, enable the `OPENDAQ_FB_EXAMPLE_ENABLE_APP` cmake flag. Doing so will add the openDAQ reference device and function block modules to your project. Those are used to create a simulator device via the "daqref://device0" connection string, as well as a renderer via the "RefFBModuleRenderer" function block ID. The main application connects a reference device signal into both the example scaler and renderer. Additionally, it connects the scaler output into the renderer.

To add additional tests, enable the `EXAMPLE_MODULE_ENABLE_TESTS` cmake flag. Doing so will create a new test target configured for use with the GTest framework.

---

## ExampleIIRFilter

This project also includes an `ExampleIIRFilter` function block, which implements a simple first-order Butterworth IIR low-pass filter. The filter allows configuration of the cutoff frequency via the `CutoffFrequency` property (default: 5 Hz).

### Running the example application

The main application demonstrates the usage of `ExampleIIRFilter` by:

1. Adding the reference device (`daqref://device0`)
2. Adding the `ExampleIIRFilter` block
3. Connecting the reference signal to the filter input
4. Connecting the filter output to the built-in renderer (`RefFBModuleRenderer`)

To run the example:

- Enable the `OPENDAQ_FB_EXAMPLE_ENABLE_APP` flag
- Build the project (e.g. with Visual Studio or CMake)
- Run the generated `example_application.exe`

The console output will confirm if the filter was successfully added and connected.

Example output:
```text
[+] ExampleIIRFilter successfully added!
