#include "arf_automotive.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"
#include "../blocks/custom_btn_i.h"

#define TAG "ARFSuzukiProtocol"

static const SubGhzBlockConst subghz_protocol_suzuki_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 99,
    .min_count_bit_for_found = 64,
};

#define SUZUKI_GAP_TIME 2000
#define SUZUKI_GAP_DELTA 399
#define SUZUKI_MIN_PREAMBLE_COUNT 200

typedef struct SubGhzProtocolDecoderSuzuki {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
} SubGhzProtocolDecoderSuzuki;

typedef enum {
    SuzukiDecoderStepReset = 0,
    SuzukiDecoderStepCountPreamble = 1,
    SuzukiDecoderStepDecodeData = 2,
} SuzukiDecoderStep;

static void suzuki_add_bit(SubGhzProtocolDecoderSuzuki* instance, uint8_t bit) {
    instance->decoder.decode_data = (instance->decoder.decode_data << 1) | bit;
    instance->decoder.decode_count_bit++;
}

static uint8_t suzuki_crc8(uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t j = 0; j < 8; j++) {
            if((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x7F);
            else
                crc <<= 1;
        }
    }
    return crc;
}

static uint8_t suzuki_calculate_crc(uint64_t data) {
    uint8_t crc_data[6];
    crc_data[0] = (data >> 52) & 0xFF;
    crc_data[1] = (data >> 44) & 0xFF;
    crc_data[2] = (data >> 36) & 0xFF;
    crc_data[3] = (data >> 28) & 0xFF;
    crc_data[4] = (data >> 20) & 0xFF;
    crc_data[5] = (data >> 12) & 0xFF;
    return suzuki_crc8(crc_data, 6);
}

static bool suzuki_verify_crc(uint64_t data) {
    uint8_t received_crc = (data >> 4) & 0xFF;
    uint8_t calculated_crc = suzuki_calculate_crc(data);
    return (received_crc == calculated_crc);
}

const SubGhzProtocolDecoder subghz_protocol_suzuki_decoder = {
    .alloc = subghz_protocol_decoder_suzuki_alloc,
    .free = subghz_protocol_decoder_suzuki_free,
    .feed = subghz_protocol_decoder_suzuki_feed,
    .reset = subghz_protocol_decoder_suzuki_reset,
    .get_hash_data = subghz_protocol_decoder_suzuki_get_hash_data,
    .serialize = subghz_protocol_decoder_suzuki_serialize,
    .deserialize = subghz_protocol_decoder_suzuki_deserialize,
    .get_string = subghz_protocol_decoder_suzuki_get_string,
};

const SubGhzProtocol subghz_protocol_suzuki = {
    .name = SUBGHZ_PROTOCOL_SUZUKI_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | 
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_suzuki_decoder,
    .encoder = NULL,
};

void* subghz_protocol_decoder_suzuki_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderSuzuki* instance = malloc(sizeof(SubGhzProtocolDecoderSuzuki));
    furi_check(instance);
    instance->base.protocol = &subghz_protocol_suzuki;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_suzuki_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    free(instance);
}

void subghz_protocol_decoder_suzuki_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    instance->decoder.parser_step = SuzukiDecoderStepReset;
}

void subghz_protocol_decoder_suzuki_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;

    switch(instance->decoder.parser_step) {
    case SuzukiDecoderStepReset:
        if(!level) return;
        if(DURATION_DIFF(duration, subghz_protocol_suzuki_const.te_short) > subghz_protocol_suzuki_const.te_delta)
            return;
        
        instance->decoder.decode_data = 0;
        instance->decoder.decode_count_bit = 0;
        instance->decoder.parser_step = SuzukiDecoderStepCountPreamble;
        instance->header_count = 0;
        break;

    case SuzukiDecoderStepCountPreamble:
        if(level) {
            if(instance->header_count >= SUZUKI_MIN_PREAMBLE_COUNT) {
                if(DURATION_DIFF(duration, subghz_protocol_suzuki_const.te_long) <= subghz_protocol_suzuki_const.te_delta) {
                    instance->decoder.parser_step = SuzukiDecoderStepDecodeData;
                    suzuki_add_bit(instance, 1);
                }
            }
        } else {
            if(DURATION_DIFF(duration, subghz_protocol_suzuki_const.te_short) <= subghz_protocol_suzuki_const.te_delta) {
                instance->decoder.te_last = duration;
                instance->header_count++;
            } else {
                instance->decoder.parser_step = SuzukiDecoderStepReset;
            }
        }
        break;

    case SuzukiDecoderStepDecodeData:
        if(level) {
            if(duration < subghz_protocol_suzuki_const.te_long) {
                uint32_t diff_long = 500 - duration;
                if(diff_long > 99) {
                    uint32_t diff_short = (duration < 250) ? (250 - duration) : (duration - 250);
                    if(diff_short <= 99) suzuki_add_bit(instance, 0);
                } else {
                    suzuki_add_bit(instance, 1);
                }
            } else {
                uint32_t diff_long = duration - 500;
                if(diff_long <= 99) suzuki_add_bit(instance, 1);
            }
        } else {
            uint32_t diff_gap = (duration < SUZUKI_GAP_TIME) ? (SUZUKI_GAP_TIME - duration) : (duration - SUZUKI_GAP_TIME);
            
            if(diff_gap <= SUZUKI_GAP_DELTA) {
                if(instance->decoder.decode_count_bit == 64) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = 64;

                    if(suzuki_verify_crc(instance->generic.data)) {
                        uint64_t data = instance->generic.data;
                        uint32_t data_high = (uint32_t)(data >> 32);
                        uint32_t data_low = (uint32_t)data;
                        
                        instance->generic.serial = ((data_high & 0xFFF) << 16) | (data_low >> 16);
                        instance->generic.btn = (data_low >> 12) & 0xF;
                        instance->generic.cnt = (data_high << 4) >> 16;

                        if(instance->base.callback) {
                            instance->base.callback(&instance->base, instance->base.context);
                        }
                    }
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = SuzukiDecoderStepReset;
            }
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_suzuki_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    return subghz_protocol_blocks_get_hash_data(&instance->decoder, (instance->generic.data_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_suzuki_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_suzuki_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    
    SubGhzProtocolStatus ret = subghz_block_generic_deserialize(&instance->generic, flipper_format);
    
    if(ret == SubGhzProtocolStatusOk) {
        uint64_t data = instance->generic.data;
        instance->generic.cnt = (uint32_t)((data >> 44) & 0xFFFFF);
        instance->generic.serial = (uint32_t)((data >> 16) & 0x0FFFFFFF);
        instance->generic.btn = (uint8_t)((data >> 12) & 0xF);
    }
    
    return ret;
}

static const char* suzuki_get_button_name(uint8_t btn) {
    switch(btn) {
    case 1: return "Panic";
    case 2: return "Trunk";
    case 3: return "Lock";
    case 4: return "Unlock";
    default: return "Unknown";
    }
}

void subghz_protocol_decoder_suzuki_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderSuzuki* instance = context;
    
    uint64_t data = instance->generic.data;
    uint32_t key_high = (data >> 32) & 0xFFFFFFFF;
    uint32_t key_low = data & 0xFFFFFFFF;
    uint8_t received_crc = (data >> 4) & 0xFF;
    uint8_t calculated_crc = suzuki_calculate_crc(data);
    bool crc_valid = (received_crc == calculated_crc);
    
    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%07lX Cnt:%04lX\r\n"
        "Btn:%02X:[%s]\r\n"
        "CRC:%02X %s",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        key_high,
        key_low,
        instance->generic.serial,
        instance->generic.cnt,
        instance->generic.btn,
        suzuki_get_button_name(instance->generic.btn),
        received_crc,
        crc_valid ? "(OK)" : "(FAIL)");
}
