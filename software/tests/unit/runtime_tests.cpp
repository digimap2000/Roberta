#include <roberta/core/registers.hpp>
#include <roberta/core/runtime.hpp>

#include <gtest/gtest.h>

namespace {

TEST(RuntimeTests, QueryRuntimeReturnsBasicMetadata)
{
    const auto runtime = roberta::core::query_runtime();

    EXPECT_FALSE(runtime.application_name.empty());
    EXPECT_FALSE(runtime.application_version.empty());
    EXPECT_FALSE(runtime.dts_version.empty());
    EXPECT_FALSE(runtime.platform_name.empty());
}

TEST(RegisterTests, ControlAndStatusRegistersMatchFirmwareContract)
{
    using roberta::core::AxisRegister;
    using roberta::core::to_underlying;

    EXPECT_EQ(to_underlying(AxisRegister::control_word), 0x0010);
    EXPECT_EQ(to_underlying(AxisRegister::status_word), 0x0011);
    EXPECT_EQ(to_underlying(AxisRegister::fault_word), 0x0012);
    EXPECT_EQ(to_underlying(AxisRegister::staged_target_position_steps), 0x0020);
    EXPECT_EQ(to_underlying(AxisRegister::staged_relative_move_steps), 0x002B);
}

} // namespace
