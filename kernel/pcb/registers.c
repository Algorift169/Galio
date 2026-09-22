#include "process/pcb.h"

const register_state_t *pcb_get_registers(const pcb_t *pcb) {
    return pcb ? &pcb->regs : NULL;
}

register_state_t *pcb_get_registers_mutable(pcb_t *pcb) {
    return pcb ? &pcb->regs : NULL;
}