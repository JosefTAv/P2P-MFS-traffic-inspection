#pragma once

#define RTE_LIBRTE_QDMA_PMD 1

#include <array>
#include <string>
#include <sstream>
#include <iostream>

static inline void do_sanity_checks(void)
{
    #if (!defined(RTE_LIBRTE_QDMA_PMD))
        rte_exit(EXIT_FAILURE, "CONFIG_RTE_LIBRTE_QDMA_PMD must be set "
                "to 'Y' in the .config file\n");
    #endif /* RTE_LIBRTE_XDMA_PMD */
}

template<typename T, std::size_t N>
std::string to_string(const std::array<T, N>& arr) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < N; ++i) {
        oss << std::hex << arr[i];
        if (i != N - 1)
            oss << ".";
    }
    return oss.str();
}
