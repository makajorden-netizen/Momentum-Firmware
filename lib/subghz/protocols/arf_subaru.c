#include "arf_subaru.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "ARFSubaruProtocol"

static const SubGhzBlockConst subghz_protocol_subaru_const = {
    .te_short = 800,
    .te_long = 1600,
    .te_delta = 250,
    .min_count_bit_for_found = 64,
};

typedef struct SubGhzProtocolDecoderSubaru {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
} SubGhzProtocolDecoderSubaru;

typedef enum {
    SubaruDecoderStepReset = 0,
    SubaruDecoderStepCheckPreamble,
    SubaruDecoderStepFoundGap,
    SubaruDecoderStepFoundSync,
    SubaruDecoderStepSaveDuration,
    SubaruDecoderStepCheckDuration,
} SubaruDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_subaru_decoder = {
    .alloc = subghz_protocol_decoder_subaru_alloc,
    .free = subghz_protocol_decoder_subaru_free,
    .feed = subghz_protocol_decoder_subaru_feed,
    .reset = subghz_protocol_decoder_subaru_reset,
    .get_hash_data = subghz_protocol_decoder_subaru_get_hash_data,
    .serialize = subghz_protocol_decoder_subaru_serialize,
    .deserialize = subghz_protocol_decoder_subaru_deserialize,
    .get_string = subghz_protocol_decoder_subaru_get_string,
};

const SubGhzProtocol subghz_protocol_subaru = {
    .name = SUBGHZ_PROTOCOL_SUBARU_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | 
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_subaru_decoder,
    .encoder = NULL,
};

void* subghz_protocol_decoder_subaru_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderSubaru* instance = malloc(sizeof(SubGhzProtocolDecoderSubaru));
    furi_check(instance);
    instance->base.protocol = &subghz_protocol_subaru;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_subaru_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    free(instance);
}

void subghz_protocol_decoder_subaru_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    instance->decoder.parser_step = SubaruDecoderStepReset;
}

void subghz_protocol_decoder_subaru_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;

    switch(instance->decoder.parser_step) {
    case SubaruDecoderStepReset:
        if(!level) return;
        if(DURATION_DIFF(duration, subghz_protocol_subaru_const.te_short) > subghz_protocol_subaru_const.te_delta)
            return;
        instance->decoder.decode_data = 0;
        instance->decoder.decode_count_bit = 0;
        instance->decoder.parser_step = SubaruDecoderStepCheckPreamble;
        instance->header_count = 0;
        break;

    case SubaruDecoderStepCheckPreamble:
        if(level) {
            if(DURATION_DIFF(duration, subghz_protocol_subaru_const.te_short) <= subghz_protocol_subaru_const.te_delta) {
                instance->header_count++;
            } else {
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }
        } else {
            if(instance->header_count > 10) {
                instance->decoder.parser_step = SubaruDecoderStepFoundGap;
            } else {
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }
        }
        break;

    case SubaruDecoderStepFoundGap:
        if(level) {
            instance->decoder.parser_step = SubaruDecoderStepFoundSync;
        } else {
            instance->decoder.parser_step = SubaruDecoderStepReset;
        }
        break;

    case SubaruDecoderStepFoundSync:
        if(!level) {
            if(DURATION_DIFF(duration, subghz_protocol_subaru_const.te_short) <= subghz_protocol_subaru_const.te_delta) {
                instance->decoder.parser_step = SubaruDecoderStepSaveDuration;
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }
        }
        break;

    case SubaruDecoderStepSaveDuration:
        if(level) {
            if(DURATION_DIFF(duration, instance->decoder.te_last) <= subghz_protocol_subaru_const.te_delta) {
                instance->decoder.parser_step = SubaruDecoderStepCheckDuration;
            } else {
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }
        }
        break;

    case SubaruDecoderStepCheckDuration:
        if(!level) {
            if(DURATION_DIFF(duration, subghz_protocol_subaru_const.te_short) <= subghz_protocol_subaru_const.te_delta) {
                instance->decoder.decode_data = (instance->decoder.decode_data << 1) | 0;
                instance->decoder.decode_count_bit++;
            } else if(DURATION_DIFF(duration, subghz_protocol_subaru_const.te_long) <= subghz_protocol_subaru_const.te_delta) {
                instance->decoder.decode_data = (instance->decoder.decode_data << 1) | 1;
                instance->decoder.decode_count_bit++;
            } else {
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }

            if(instance->decoder.decode_count_bit == 64) {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = 64;
                instance->generic.serial = (instance->generic.data >> 16) & 0xFFFFFF;
                instance->generic.btn = (instance->generic.data >> 8) & 0xFF;
                instance->generic.cnt = instance->generic.data & 0xFFFF;

                if(instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = SubaruDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = SubaruDecoderStepReset;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_subaru_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    return subghz_protocol_blocks_get_hash_data(&instance->decoder, (instance->generic.data_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_subaru_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_subaru_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_subaru_const.min_count_bit_for_found);
    if(ret == SubGhzProtocolStatusOk) {
        instance->generic.serial = (instance->generic.data >> 16) & 0xFFFFFF;
        instance->generic.btn = (instance->generic.data >> 8) & 0xFF;
        instance->generic.cnt = instance->generic.data & 0xFFFF;
    }
    return ret;
}

static const char* subaru_get_button_name(uint8_t btn) {
    switch(btn) {
    case 0: return "Lock";
    case 1: return "Unlock";
    case 2: return "Trunk";
    case 3: return "Panic";
    default: return "Unknown";
    }
}

void subghz_protocol_decoder_subaru_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderSubaru* instance = context;
    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Sn:%06lX Btn:%01X [%s]\r\n"
        "Cnt:%04lX",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->generic.data,
        instance->generic.serial,
        instance->generic.btn,
        subaru_get_button_name(instance->generic.btn),
        instance->generic.cnt);
}
