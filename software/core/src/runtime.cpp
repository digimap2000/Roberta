#include <roberta/core/runtime.hpp>

#include <dts/dts.hpp>

namespace roberta::core {

std::string_view application_name() noexcept
{
    return "roberta";
}

std::string_view application_version() noexcept
{
    return "0.1.0";
}

RuntimeInfo query_runtime()
{
    const auto platform = dts::current_platform();

    return RuntimeInfo{
        .application_name = std::string{application_name()},
        .application_version = std::string{application_version()},
        .dts_version = std::string{dts::version()},
        .platform_name = std::string{platform.name},
        .platform_supported = platform.supported,
    };
}

} // namespace roberta::core
