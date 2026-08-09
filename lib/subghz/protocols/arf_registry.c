#include "arf_automotive.h"
#include "protocol.h"

/**
 * ARF Automotive Decoder Registry
 * Minimal, decoder-only protocol registration for Momentum
 */

void subghz_protocol_arf_register_automotive_decoders(SubGhzProtocol** registry, size_t* count) {
    if(!registry || !count) return;
    
    /* Batch 1: Japanese manufacturers */
    registry[(*count)++] = (SubGhzProtocol*)&subghz_protocol_suzuki;
    
    /* TODO Batch 2: European OEM (Subaru EU, VAG, PSA, Clemsa) */
    /* TODO Batch 3: Rolling code & advanced (Nero Sketch, Jarolift, Magellan) */
}
