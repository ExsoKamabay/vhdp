/* Compiling this file proves include/vhdp/vhdp.h is self-contained and valid C17. */
#include "vhdp/vhdp.h"

uint32_t vhdp_test_header_c_abi(void);
uint32_t vhdp_test_header_c_abi(void) {
    return (uint32_t)VHDP_ABI_VERSION + (uint32_t)sizeof(vhdp_event) +
           (uint32_t)sizeof(vhdp_exit_info) + (uint32_t)VHDP_OK + (uint32_t)VHDP_ENGINE_AUTO;
}
