#pragma once

/**
 * ARF Automotive Protocol Integration Header
 * Provides decoder-only access to ARF automotive SubGhz protocols
 * Encoder interfaces are disabled to maintain clean Momentum separation
 */

#include "base.h"
#include "../blocks/math.h"

/* Automotive Protocols from ARF */
#define SUBGHZ_PROTOCOL_SUZUKI_NAME "SUZUKI"
#define SUBGHZ_PROTOCOL_SUBARU_NAME "SUBARU"
#define SUBGHZ_PROTOCOL_VAG_NAME "VAG GROUP"
#define SUBGHZ_PROTOCOL_PSA_NAME "PSA GROUP"
#define SUBGHZ_PROTOCOL_CLEMSA_NAME "Clemsa"
#define SUBGHZ_PROTOCOL_NERO_SKETCH_NAME "Nero Sketch"
#define SUBGHZ_PROTOCOL_JAROLIFT_NAME "Jarolift"
#define SUBGHZ_PROTOCOL_MAGELLAN_NAME "Magellan"

/* Suzuki Decoder */
extern const SubGhzProtocol subghz_protocol_suzuki;

void* subghz_protocol_decoder_suzuki_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_suzuki_free(void* context);
void subghz_protocol_decoder_suzuki_reset(void* context);
void subghz_protocol_decoder_suzuki_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_suzuki_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_suzuki_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus subghz_protocol_decoder_suzuki_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_suzuki_get_string(void* context, FuriString* output);
