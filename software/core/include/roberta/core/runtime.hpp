#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace roberta::core {

struct RuntimeInfo final {
    std::string application_name;
    std::string application_version;
    std::string dts_version;
    std::string platform_name;
    bool platform_supported{false};
};

[[nodiscard]] std::string_view application_name() noexcept;
[[nodiscard]] std::string_view application_version() noexcept;
[[nodiscard]] RuntimeInfo query_runtime();

} // namespace roberta::core
