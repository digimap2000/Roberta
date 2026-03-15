#include <roberta/core/runtime.hpp>

#include <iostream>

int main()
{
    const auto runtime = roberta::core::query_runtime();
    std::cout << "roberta_embedded_linux_app placeholder on " << runtime.platform_name << '\n';
    return 0;
}
