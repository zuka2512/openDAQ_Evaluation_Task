#include <coretypes/version_info_factory.h>
#include <example_module/example_fb.h>
#include <example_module/example_module.h>
#include <example_module/version.h>
#include <opendaq/custom_log.h>
#include <example_module/iir_filter_fb.h>

BEGIN_NAMESPACE_EXAMPLE_MODULE

ExampleModule::ExampleModule(ContextPtr context)
    : Module("ReferenceFunctionBlockModule",
             VersionInfo(EXAMPLE_MODULE_MAJOR_VERSION, EXAMPLE_MODULE_MINOR_VERSION, EXAMPLE_MODULE_PATCH_VERSION),
             std::move(context),
             "ReferenceFunctionBlockModule")
{
}

DictPtr<IString, IFunctionBlockType> ExampleModule::onGetAvailableFunctionBlockTypes()
{
    auto types = Dict<IString, IFunctionBlockType>();

    const auto typeScaling = ExampleFBImpl::CreateType();
    types.set(typeScaling.getId(), typeScaling);

    const auto typeIIR = IIRFilterFBImpl::CreateType();
    types.set(typeIIR.getId(), typeIIR);

    return types;
}

FunctionBlockPtr ExampleModule::onCreateFunctionBlock(const StringPtr& id,
                                                      const ComponentPtr& parent,
                                                      const StringPtr& localId,
                                                      const PropertyObjectPtr& config)
{
    if (id == ExampleFBImpl::CreateType().getId())
    {
        FunctionBlockPtr fb = createWithImplementation<IFunctionBlock, ExampleFBImpl>(context, parent, localId);
        return fb;
    }

    if (id == IIRFilterFBImpl::CreateType().getId())
    {
        FunctionBlockPtr fb = createWithImplementation<IFunctionBlock, IIRFilterFBImpl>(context, parent, localId);
        return fb;
    }

    LOG_W("Function block \"{}\" not found", id);
    throw NotFoundException("Function block not found");
}

END_NAMESPACE_EXAMPLE_MODULE
