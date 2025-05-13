#pragma once

#define RTE_LIBRTE_QDMA_PMD 1

static inline void do_sanity_checks(void)
{
    #if (!defined(RTE_LIBRTE_QDMA_PMD))
        rte_exit(EXIT_FAILURE, "CONFIG_RTE_LIBRTE_QDMA_PMD must be set "
                "to 'Y' in the .config file\n");
    #endif /* RTE_LIBRTE_XDMA_PMD */
}
