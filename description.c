/*****************************************************************************
 * description.c:
 *****************************************************************************
 * Copyright (C) 2012 L-SMASH project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

/* This file is available under an ISC license. */

#include "internal.h"   /* must be placed first */

#include <stdlib.h>
#include <string.h>

#include "box.h"
#include "mp4a.h"
#include "mp4sys.h"
#include "description.h"

static void global_destruct_specific_data( void *data )
{
    if( !data )
        return;
    lsmash_codec_global_header_t *global = (lsmash_codec_global_header_t *)data;
    if( global->header_data )
        free( global->header_data );
    free( global );
}

static int isom_is_qt_video( uint32_t type )
{
    return type == QT_CODEC_TYPE_APCH_VIDEO
        || type == QT_CODEC_TYPE_APCN_VIDEO
        || type == QT_CODEC_TYPE_APCS_VIDEO
        || type == QT_CODEC_TYPE_APCO_VIDEO
        || type == QT_CODEC_TYPE_AP4H_VIDEO
        || type == QT_CODEC_TYPE_DVC_VIDEO
        || type == QT_CODEC_TYPE_DVCP_VIDEO
        || type == QT_CODEC_TYPE_DVPP_VIDEO
        || type == QT_CODEC_TYPE_DV5N_VIDEO
        || type == QT_CODEC_TYPE_DV5P_VIDEO
        || type == QT_CODEC_TYPE_DVH2_VIDEO
        || type == QT_CODEC_TYPE_DVH3_VIDEO
        || type == QT_CODEC_TYPE_DVH5_VIDEO
        || type == QT_CODEC_TYPE_DVH6_VIDEO
        || type == QT_CODEC_TYPE_DVHP_VIDEO
        || type == QT_CODEC_TYPE_DVHQ_VIDEO
        || type == QT_CODEC_TYPE_ULRA_VIDEO
        || type == QT_CODEC_TYPE_ULRG_VIDEO
        || type == QT_CODEC_TYPE_ULY2_VIDEO
        || type == QT_CODEC_TYPE_ULY0_VIDEO
        || type == QT_CODEC_TYPE_V210_VIDEO
        || type == QT_CODEC_TYPE_V216_VIDEO
        || type == QT_CODEC_TYPE_V308_VIDEO
        || type == QT_CODEC_TYPE_V408_VIDEO
        || type == QT_CODEC_TYPE_V410_VIDEO
        || type == QT_CODEC_TYPE_YUV2_VIDEO;
}

static int isom_is_qt_audio( uint32_t type )
{
    return type == QT_CODEC_TYPE_23NI_AUDIO
        || type == QT_CODEC_TYPE_MAC3_AUDIO
        || type == QT_CODEC_TYPE_MAC6_AUDIO
        || type == QT_CODEC_TYPE_NONE_AUDIO
        || type == QT_CODEC_TYPE_QDM2_AUDIO
        || type == QT_CODEC_TYPE_QDMC_AUDIO
        || type == QT_CODEC_TYPE_QCLP_AUDIO
        || type == QT_CODEC_TYPE_AC_3_AUDIO
        || type == QT_CODEC_TYPE_AGSM_AUDIO
        || type == QT_CODEC_TYPE_ALAC_AUDIO
        || type == QT_CODEC_TYPE_ALAW_AUDIO
        || type == QT_CODEC_TYPE_CDX2_AUDIO
        || type == QT_CODEC_TYPE_CDX4_AUDIO
        || type == QT_CODEC_TYPE_DVCA_AUDIO
        || type == QT_CODEC_TYPE_DVI_AUDIO
        || type == QT_CODEC_TYPE_FL32_AUDIO
        || type == QT_CODEC_TYPE_FL64_AUDIO
        || type == QT_CODEC_TYPE_IMA4_AUDIO
        || type == QT_CODEC_TYPE_IN24_AUDIO
        || type == QT_CODEC_TYPE_IN32_AUDIO
        || type == QT_CODEC_TYPE_LPCM_AUDIO
        || type == QT_CODEC_TYPE_MP4A_AUDIO
        || type == QT_CODEC_TYPE_RAW_AUDIO
        || type == QT_CODEC_TYPE_SOWT_AUDIO
        || type == QT_CODEC_TYPE_TWOS_AUDIO
        || type == QT_CODEC_TYPE_ULAW_AUDIO
        || type == QT_CODEC_TYPE_VDVA_AUDIO
        || type == QT_CODEC_TYPE_FULLMP3_AUDIO
        || type == QT_CODEC_TYPE_MP3_AUDIO
        || type == QT_CODEC_TYPE_ADPCM2_AUDIO
        || type == QT_CODEC_TYPE_ADPCM17_AUDIO
        || type == QT_CODEC_TYPE_GSM49_AUDIO
        || type == QT_CODEC_TYPE_NOT_SPECIFIED;
}

static int isom_is_avc( uint32_t type )
{
    return type == ISOM_CODEC_TYPE_AVC1_VIDEO
        || type == ISOM_CODEC_TYPE_AVC2_VIDEO
        || type == ISOM_CODEC_TYPE_AVCP_VIDEO;
}

int lsmash_convert_crop_into_clap( lsmash_crop_t crop, uint32_t width, uint32_t height, lsmash_clap_t *clap )
{
    if( !clap || crop.top.d == 0 || crop.bottom.d == 0 || crop.left.d == 0 ||  crop.right.d == 0 )
        return -1;
    uint64_t vertical_crop_lcm   = lsmash_get_lcm( crop.top.d,  crop.bottom.d );
    uint64_t horizontal_crop_lcm = lsmash_get_lcm( crop.left.d, crop.right.d  );
    lsmash_rational_u64_t clap_height;
    lsmash_rational_u64_t clap_width;
    lsmash_rational_s64_t clap_horizontal_offset;
    lsmash_rational_s64_t clap_vertical_offset;
    clap_height.d            = vertical_crop_lcm;
    clap_width.d             = horizontal_crop_lcm;
    clap_horizontal_offset.d = 2 * vertical_crop_lcm;
    clap_vertical_offset.d   = 2 * horizontal_crop_lcm;
    clap_height.n = height * vertical_crop_lcm
                  - (crop.top.n * (vertical_crop_lcm / crop.top.d) + crop.bottom.n * (vertical_crop_lcm / crop.bottom.d));
    clap_width.n  = width * horizontal_crop_lcm
                  - (crop.left.n * (horizontal_crop_lcm / crop.left.d) + crop.right.n * (horizontal_crop_lcm / crop.right.d));
    clap_horizontal_offset.n = (int64_t)(crop.left.n * (horizontal_crop_lcm / crop.left.d))
                             - crop.right.n * (horizontal_crop_lcm / crop.right.d);
    clap_vertical_offset.n   = (int64_t)(crop.top.n * (vertical_crop_lcm / crop.top.d))
                             - crop.bottom.n * (vertical_crop_lcm / crop.bottom.d);
    lsmash_reduce_fraction( &clap_height.n, &clap_height.d );
    lsmash_reduce_fraction( &clap_width.n,  &clap_width.d  );
    lsmash_reduce_fraction_su( &clap_vertical_offset.n,   &clap_vertical_offset.d   );
    lsmash_reduce_fraction_su( &clap_horizontal_offset.n, &clap_horizontal_offset.d );
    clap->height            = (lsmash_rational_u32_t){ clap_height.n,            clap_height.d            };
    clap->width             = (lsmash_rational_u32_t){ clap_width.n,             clap_width.d             };
    clap->vertical_offset   = (lsmash_rational_s32_t){ clap_vertical_offset.n,   clap_vertical_offset.d   };
    clap->horizontal_offset = (lsmash_rational_s32_t){ clap_horizontal_offset.n, clap_horizontal_offset.d };
    return 0;
}

int lsmash_convert_clap_into_crop( lsmash_clap_t clap, uint32_t width, uint32_t height, lsmash_crop_t *crop )
{
    if( !crop || clap.height.d == 0 || clap.vertical_offset.d == 0 || clap.width.d == 0 || clap.horizontal_offset.d == 0 )
        return -1;
    uint64_t clap_vertical_lcm   = lsmash_get_lcm( clap.height.d, clap.vertical_offset.d   );
    uint64_t clap_horizontal_lcm = lsmash_get_lcm( clap.width.d,  clap.horizontal_offset.d );
    lsmash_rational_u64_t crop_top;
    lsmash_rational_u64_t crop_bottom;
    lsmash_rational_u64_t crop_left;
    lsmash_rational_u64_t crop_right;
    crop_top.d    = 2 * clap_vertical_lcm;
    crop_bottom.d = 2 * clap_vertical_lcm;
    crop_left.d   = 2 * clap_horizontal_lcm;
    crop_right.d  = 2 * clap_horizontal_lcm;
    crop_top.n    = (height * crop_top.d - clap.height.n * (crop_top.d / clap.height.d)) / 2
                  + clap.vertical_offset.n * (crop_top.d / clap.vertical_offset.d);
    crop_bottom.n = (height * crop_bottom.d - clap.height.n * (crop_bottom.d / clap.height.d)) / 2
                  - clap.vertical_offset.n * (crop_bottom.d / clap.vertical_offset.d);
    crop_left.n   = (width * crop_left.d - clap.width.n * (crop_left.d / clap.width.d)) / 2
                  + clap.horizontal_offset.n * (crop_left.d / clap.horizontal_offset.d);
    crop_right.n  = (width * crop_right.d - clap.width.n * (crop_right.d / clap.width.d)) / 2
                  - clap.horizontal_offset.n * (crop_right.d / clap.horizontal_offset.d);
    lsmash_reduce_fraction( &crop_top.n,    &crop_top.d    );
    lsmash_reduce_fraction( &crop_bottom.n, &crop_bottom.d );
    lsmash_reduce_fraction( &crop_left.n,   &crop_left.d   );
    lsmash_reduce_fraction( &crop_right.n,  &crop_right.d  );
    crop->top    = (lsmash_rational_u32_t){ crop_top.n,    crop_top.d    };
    crop->bottom = (lsmash_rational_u32_t){ crop_bottom.n, crop_bottom.d };
    crop->left   = (lsmash_rational_u32_t){ crop_left.n,   crop_left.d   };
    crop->right  = (lsmash_rational_u32_t){ crop_right.n,  crop_right.d  };
    return 0;
}

static void isom_destruct_nothing( void *data )
{
    /* Do nothing. */;
}

static int isom_initialize_structured_codec_specific_data( lsmash_codec_specific_t *specific )
{
    extern void mp4sys_destruct_decoder_config( void * );
    extern void h264_destruct_specific_data( void * );
    extern void vc1_destruct_specific_data( void * );
    extern void dts_destruct_specific_data( void * );
    switch( specific->type )
    {
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG :
            specific->size     = sizeof(lsmash_mp4sys_decoder_parameters_t);
            specific->destruct = mp4sys_destruct_decoder_config;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264 :
            specific->size     = sizeof(lsmash_h264_specific_parameters_t);
            specific->destruct = h264_destruct_specific_data;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1 :
            specific->size     = sizeof(lsmash_vc1_specific_parameters_t);
            specific->destruct = vc1_destruct_specific_data;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3 :
            specific->size     = sizeof(lsmash_ac3_specific_parameters_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3 :
            specific->size     = sizeof(lsmash_eac3_specific_parameters_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS :
            specific->size     = sizeof(lsmash_dts_specific_parameters_t);
            specific->destruct = dts_destruct_specific_data;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_SAMPLE_SCALE :
            specific->size     = sizeof(lsmash_isom_sample_scale_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE :
            specific->size     = sizeof(lsmash_h264_bitrate_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_COMMON :
            specific->size     = sizeof(lsmash_qt_video_common_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_COMMON :
            specific->size     = sizeof(lsmash_qt_audio_common_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS :
            specific->size     = sizeof(lsmash_qt_audio_format_specific_flags_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER :
            specific->size     = sizeof(lsmash_codec_global_header_t);
            specific->destruct = global_destruct_specific_data;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO :
            specific->size     = sizeof(lsmash_qt_field_info_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_PIXEL_FORMAT :
            specific->size     = sizeof(lsmash_qt_pixel_format_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS :
            specific->size     = sizeof(lsmash_qt_significant_bits_t);
            specific->destruct = free;
            break;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT :
            specific->size     = sizeof(lsmash_qt_audio_channel_layout_t);
            specific->destruct = free;
            break;
        default :
            specific->size     = 0;
            specific->destruct = isom_destruct_nothing;
            return 0;
    }
    specific->data.structured = lsmash_malloc_zero( specific->size );
    if( !specific->data.structured )
    {
        specific->size     = 0;
        specific->destruct = NULL;
        return -1;
    }
    return 0;
}

static inline int isom_initialize_codec_specific_data( lsmash_codec_specific_t *specific,
                                                       lsmash_codec_specific_data_type type,
                                                       lsmash_codec_specific_format format )
{
    specific->type   = type;
    specific->format = format;
    if( format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
    {
        if( isom_initialize_structured_codec_specific_data( specific ) )
            return -1;
    }
    else
    {
        specific->data.unstructured = NULL;
        specific->size              = 0;
        specific->destruct          = (lsmash_codec_specific_destructor_t)free;
    }
    return 0;
}

void lsmash_destroy_codec_specific_data( lsmash_codec_specific_t *specific )
{
    if( !specific )
        return;
    if( specific->destruct )
    {
        if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
        {
            if( specific->data.structured )
                specific->destruct( specific->data.structured );
        }
        else
        {
            if( specific->data.unstructured )
                specific->destruct( specific->data.unstructured );
        }
    }
    free( specific );
}

lsmash_codec_specific_t *lsmash_create_codec_specific_data( lsmash_codec_specific_data_type type, lsmash_codec_specific_format format )
{
    lsmash_codec_specific_t *specific = malloc( sizeof(lsmash_codec_specific_t) );
    if( !specific )
        return NULL;
    if( isom_initialize_codec_specific_data( specific, type, format  ) )
    {
        lsmash_destroy_codec_specific_data( specific );
        return NULL;
    }
    return specific;
}

static int isom_duplicate_structured_specific_data( lsmash_codec_specific_t *dst, lsmash_codec_specific_t *src )
{
    extern int mp4sys_copy_decoder_config( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
    extern int h264_copy_codec_specific( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
    extern int vc1_copy_codec_specific( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
    extern int dts_copy_codec_specific( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
    void *src_data = src->data.structured;
    void *dst_data = dst->data.structured;
    switch( src->type )
    {
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG :
            return mp4sys_copy_decoder_config( dst, src );
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264 :
            return h264_copy_codec_specific( dst, src );
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1 :
            return vc1_copy_codec_specific( dst, src );
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3 :
            *(lsmash_ac3_specific_parameters_t *)dst_data = *(lsmash_ac3_specific_parameters_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3 :
            *(lsmash_eac3_specific_parameters_t *)dst_data = *(lsmash_eac3_specific_parameters_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS :
            return dts_copy_codec_specific( dst, src );
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_SAMPLE_SCALE :
            *(lsmash_isom_sample_scale_t *)dst_data = *(lsmash_isom_sample_scale_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE :
            *(lsmash_h264_bitrate_t *)dst_data = *(lsmash_h264_bitrate_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_COMMON :
            *(lsmash_qt_video_common_t *)dst_data = *(lsmash_qt_video_common_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_COMMON :
            *(lsmash_qt_audio_common_t *)dst_data = *(lsmash_qt_audio_common_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS :
            *(lsmash_qt_audio_format_specific_flags_t *)dst_data = *(lsmash_qt_audio_format_specific_flags_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER :
        {
            lsmash_codec_global_header_t *src_global = (lsmash_codec_global_header_t *)src_data;
            if( src_global->header_data && src_global->header_size )
            {
                lsmash_codec_global_header_t *dst_global = (lsmash_codec_global_header_t *)dst_data;
                dst_global->header_data = lsmash_memdup( src_global->header_data, src_global->header_size );
                if( !dst_global->header_data )
                    return -1;
                dst_global->header_size = src_global->header_size;
            }
            return 0;
        }
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO :
            *(lsmash_qt_field_info_t *)dst_data = *(lsmash_qt_field_info_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_PIXEL_FORMAT :
            *(lsmash_qt_pixel_format_t *)dst_data = *(lsmash_qt_pixel_format_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS :
            *(lsmash_qt_significant_bits_t *)dst_data = *(lsmash_qt_significant_bits_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_GAMMA_LEVEL :
            *(lsmash_qt_gamma_t *)dst_data = *(lsmash_qt_gamma_t *)src_data;
            return 0;
        case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT :
            *(lsmash_qt_audio_channel_layout_t *)dst_data = *(lsmash_qt_audio_channel_layout_t *)src_data;
            return 0;
        default :
            return -1;
    }
}

lsmash_codec_specific_t *isom_duplicate_codec_specific_data( lsmash_codec_specific_t *specific )
{
    if( !specific )
        return NULL;
    lsmash_codec_specific_t *dup = lsmash_create_codec_specific_data( specific->type, specific->format );
    if( !dup )
        return NULL;
    if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
    {
        if( isom_duplicate_structured_specific_data( dup, specific ) )
        {
            lsmash_destroy_codec_specific_data( dup );
            return NULL;
        }
    }
    else
    {
        dup->data.unstructured = lsmash_memdup( specific->data.unstructured, specific->size );
        if( !dup->data.unstructured )
        {
            lsmash_destroy_codec_specific_data( dup );
            return NULL;
        }
    }
    dup->size = specific->size;
    return dup;
}

static uint32_t isom_description_read_box_common( uint8_t **p_data, uint64_t *size, uint32_t *type )
{
    uint8_t *orig = *p_data;
    uint8_t *data = *p_data;
    *size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    *type = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    data += ISOM_BASEBOX_COMMON_SIZE;
    if( *size == 1 )
    {
        *size = ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32)
              | ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) | ((uint64_t)data[6] <<  8) |  (uint64_t)data[7];
        data += 8;
    }
    *p_data = data;
    return data - orig;
}

uint8_t *isom_get_child_box_position( uint8_t *parent_data, uint32_t parent_size, uint32_t child_type, uint32_t *child_size )
{
    if( !parent_data || !child_size || parent_size < ISOM_BASEBOX_COMMON_SIZE )
        return NULL;
    uint8_t *data = parent_data;
    uint64_t size;
    uint32_t type;
    uint32_t offset = isom_description_read_box_common( &data, &size, &type );
    if( size != parent_size )
        return NULL;
    uint8_t *end = parent_data + parent_size;
    for( uint8_t *pos = data; pos + ISOM_BASEBOX_COMMON_SIZE <= end; )
    {
        offset = isom_description_read_box_common( &pos, &size, &type );
        if( type == child_type )
        {
            *child_size = size;
            return pos - offset;
        }
        pos += size - offset;   /* Move to the next box. */
    }
    return NULL;
}

static int isom_construct_global_specific_header( lsmash_codec_specific_t *dst, lsmash_codec_specific_t *src )
{
    if( src->size < ISOM_BASEBOX_COMMON_SIZE )
        return -1;
    lsmash_codec_global_header_t *global = (lsmash_codec_global_header_t *)dst->data.structured;
    uint8_t *data = src->data.unstructured;
    uint64_t size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    data += ISOM_BASEBOX_COMMON_SIZE;
    global->header_size = size - ISOM_BASEBOX_COMMON_SIZE;
    if( size == 1 )
    {
        size = ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) | ((uint64_t)data[3] << 32)
             | ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) | ((uint64_t)data[6] <<  8) |  (uint64_t)data[7];
        data += 8;
        global->header_size -= 8;
    }
    if( size != src->size )
        return -1;
    if( global->header_size )
    {
        global->header_data = lsmash_memdup( data, global->header_size );
        if( !global->header_data )
            return -1;
    }
    return 0;
}

#if 0
static int codec_construct_qt_audio_decompression_info( lsmash_codec_specific_t *dst, lsmash_codec_specific_t *src )
{
    if( src->size < ISOM_BASEBOX_COMMON_SIZE )
        return -1;
    uint8_t *data = src->data.unstructured;
    uint64_t size;
    uint32_t type;
    uint32_t offset = isom_description_read_box_common( &data, &size, &type );
    if( size != src->size )
        return -1;
    uint8_t *end = src->data.unstructured + src->size;
    isom_wave_t *wave = lsmash_malloc_zero( sizeof(isom_wave_t) );
    if( !wave )
        return -1;
    wave->type = QT_BOX_TYPE_WAVE;
    for( uint8_t *pos = data; pos + ISOM_BASEBOX_COMMON_SIZE <= end; )
    {
        offset = isom_description_read_box_common( &pos, &size, &type );
        switch( type )
        {
            case QT_BOX_TYPE_FRMA :
            {
                if( pos + 4 > end )
                    return -1;
                isom_frma_t *frma = lsmash_malloc_zero( sizeof(isom_frma_t) );
                if( !frma )
                    return -1;
                isom_init_box_common( frma, wave, QT_BOX_TYPE_FRMA );
                frma->data_format = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
                pos += 4;
                wave->frma = frma;
                break;
            }
            case QT_BOX_TYPE_ENDA :
            {
                if( pos + 2 > end )
                    return -1;
                isom_enda_t *enda = lsmash_malloc_zero( sizeof(isom_enda_t) );
                if( !enda )
                    return -1;
                isom_init_box_common( enda, wave, QT_BOX_TYPE_ENDA );
                enda->littleEndian = (pos[0] << 8) | pos[1];
                pos += 2;
                wave->enda = enda;
                break;
            }
            case QT_BOX_TYPE_MP4A :
            {
                if( pos + 4 > end )
                    return -1;
                isom_mp4a_t *mp4a = lsmash_malloc_zero( sizeof(isom_mp4a_t) );
                if( !mp4a )
                    return -1;
                isom_init_box_common( mp4a, wave, QT_BOX_TYPE_MP4A );
                mp4a->unknown = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
                pos += 4;
                wave->mp4a = mp4a;
                break;
            }
            case QT_BOX_TYPE_TERMINATOR :
            {
                isom_terminator_t *terminator = lsmash_malloc_zero( sizeof(isom_terminator_t) );
                if( !terminator )
                    return -1;
                isom_init_box_common( terminator, wave, QT_BOX_TYPE_TERMINATOR );
                wave->terminator = terminator;
                break;
            }
            default :
            {
                isom_unknown_box_t *box = lsmash_malloc_zero( sizeof(isom_unknown_box_t) );
                if( !box )
                    return -1;
                isom_init_box_common( box, wave, type );
                box->unknown_size  = size - offset;
                box->unknown_field = lsmash_memdup( pos, box->unknown_size );
                if( !box->unknown_field )
                {
                    free( box );
                    return -1;
                }
                if( isom_add_extension_box( &wave->extensions, box, isom_remove_unknown_box ) )
                {
                    isom_remove_unknown_box( box );
                    return -1;
                }
                pos += box->unknown_size;
                break;
            }
        }
    }
    return 0;
}
#endif

/* structured <-> unstructured conversion might be irreversible by CODEC
 * since structured formats we defined don't always have all contents included in unstructured data. */
lsmash_codec_specific_t *lsmash_convert_codec_specific_format( lsmash_codec_specific_t *specific, lsmash_codec_specific_format format )
{
    if( !specific )
        return NULL;
    if( format == specific->format )
        return isom_duplicate_codec_specific_data( specific );
    lsmash_codec_specific_t *dst = lsmash_create_codec_specific_data( specific->type, format );
    if( format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
        /* structured -> unstructured */
        switch( specific->type )
        {
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG :
                dst->data.unstructured = lsmash_create_mp4sys_decoder_config( (lsmash_mp4sys_decoder_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264 :
                dst->data.unstructured = lsmash_create_h264_specific_info( (lsmash_h264_specific_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1 :
                dst->data.unstructured = lsmash_create_vc1_specific_info( (lsmash_vc1_specific_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3 :
                dst->data.unstructured = lsmash_create_ac3_specific_info( (lsmash_ac3_specific_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3 :
                dst->data.unstructured = lsmash_create_eac3_specific_info( (lsmash_eac3_specific_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS :
                dst->data.unstructured = lsmash_create_dts_specific_info( (lsmash_dts_specific_parameters_t *)specific->data.structured, &dst->size );
                if( !dst->data.unstructured )
                    goto fail;
                return dst;
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER :
            {
                lsmash_bs_t *bs = lsmash_bs_create( NULL );
                if( !bs )
                    goto fail;
                lsmash_codec_global_header_t *global = specific->data.structured;
                lsmash_bs_put_be32( bs, ISOM_BASEBOX_COMMON_SIZE + global->header_size );
                lsmash_bs_put_be32( bs, QT_BOX_TYPE_GLBL );
                lsmash_bs_put_bytes( bs, global->header_size, global->header_data );
                dst->data.unstructured = lsmash_bs_export_data( bs, &dst->size );
                lsmash_bs_cleanup( bs );
                if( !dst->data.unstructured || dst->size != (ISOM_BASEBOX_COMMON_SIZE + global->header_size) )
                    goto fail;
                return dst;
            }
            default :
                break;
        }
    else
    {
        /* unstructured -> structured */
        extern int mp4sys_construct_decoder_config( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        extern int h264_construct_specific_parameters( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        extern int vc1_construct_specific_parameters( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        extern int ac3_construct_specific_parameters( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        extern int eac3_construct_specific_parameters( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        extern int dts_construct_specific_parameters( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        static const struct
        {
            lsmash_codec_specific_data_type data_type;
            int (*constructor)( lsmash_codec_specific_t *, lsmash_codec_specific_t * );
        } codec_specific_format_constructor_table[] =
            {
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG, mp4sys_construct_decoder_config },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264,       h264_construct_specific_parameters },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1,       vc1_construct_specific_parameters },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3,       ac3_construct_specific_parameters },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3,       eac3_construct_specific_parameters },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS,        dts_construct_specific_parameters },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER,   isom_construct_global_specific_header },
                { LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN,               NULL }
            };
        int (*constructor)( lsmash_codec_specific_t *, lsmash_codec_specific_t * ) = NULL;
        for( int i = 0; codec_specific_format_constructor_table[i].constructor; i++ )
            if( specific->type == codec_specific_format_constructor_table[i].data_type )
            {
                constructor = codec_specific_format_constructor_table[i].constructor;
                break;
            }
        if( constructor && !constructor( dst, specific ) )
            return dst;
    }
fail:
    lsmash_destroy_codec_specific_data( dst );
    return NULL;
}

void isom_remove_sample_description_extension( isom_extension_box_t *ext )
{
    if( !ext )
        return;
    if( ext->destruct )
    {
        if( ext->format == EXTENSION_FORMAT_BINARY )
        {
            if( ext->form.binary )
                ext->destruct( ext->form.binary );
        }
        else
        {
            if( ext->form.box )
                ext->destruct( ext->form.box );
        }
    }
    free( ext );
}

void isom_remove_sample_description_extensions( lsmash_entry_list_t *extensions )
{
    lsmash_remove_entries( extensions, isom_remove_sample_description_extension );
}

static inline void isom_set_default_compressorname( char *compressorname, lsmash_codec_type sample_type )
{
    static const struct
    {
        uint32_t type;
        char    *name;
    } compressorname_table[] =
        {
            { ISOM_CODEC_TYPE_AVC1_VIDEO, "\012AVC Coding" },
            { ISOM_CODEC_TYPE_AVC2_VIDEO, "\012AVC Coding" },
            { ISOM_CODEC_TYPE_AVCP_VIDEO, "\016AVC Parameters" },
            { ISOM_CODEC_TYPE_SVC1_VIDEO, "\012SVC Coding" },
            { ISOM_CODEC_TYPE_MVC1_VIDEO, "\012MVC Coding" },
            { ISOM_CODEC_TYPE_MVC2_VIDEO, "\012MVC Coding" },
            { QT_CODEC_TYPE_APCH_VIDEO,   "\023Apple ProRes 422 (HQ)" },
            { QT_CODEC_TYPE_APCN_VIDEO,   "\023Apple ProRes 422 (SD)" },
            { QT_CODEC_TYPE_APCS_VIDEO,   "\023Apple ProRes 422 (LT)" },
            { QT_CODEC_TYPE_APCO_VIDEO,   "\026Apple ProRes 422 (Proxy)" },
            { QT_CODEC_TYPE_AP4H_VIDEO,   "\019Apple ProRes 4444" },
            { QT_CODEC_TYPE_DVPP_VIDEO,   "\014DVCPRO - PAL" },
            { QT_CODEC_TYPE_DV5N_VIDEO,   "\017DVCPRO50 - NTSC" },
            { QT_CODEC_TYPE_DV5P_VIDEO,   "\016DVCPRO50 - PAL" },
            { QT_CODEC_TYPE_DVH2_VIDEO,   "\019DVCPRO HD 1080p25" },
            { QT_CODEC_TYPE_DVH3_VIDEO,   "\019DVCPRO HD 1080p30" },
            { QT_CODEC_TYPE_DVH5_VIDEO,   "\019DVCPRO HD 1080i50" },
            { QT_CODEC_TYPE_DVH6_VIDEO,   "\019DVCPRO HD 1080i60" },
            { QT_CODEC_TYPE_DVHP_VIDEO,   "\018DVCPRO HD 720p60" },
            { QT_CODEC_TYPE_DVHQ_VIDEO,   "\018DVCPRO HD 720p50" },
            { QT_CODEC_TYPE_ULRA_VIDEO,   "\017Ut Video (ULRA)" },
            { QT_CODEC_TYPE_ULRG_VIDEO,   "\017Ut Video (ULRG)" },
            { QT_CODEC_TYPE_ULY0_VIDEO,   "\017Ut Video (ULY0)" },
            { QT_CODEC_TYPE_ULY2_VIDEO,   "\017Ut Video (ULY2)" },
            { 0,                          NULL },
        };
    for( int i = 0; compressorname_table[i].name; i++ )
        if( sample_type == compressorname_table[i].type )
            strcpy( compressorname, compressorname_table[i].name );
}

int isom_add_extension_box( lsmash_entry_list_t *extensions, void *box, void *eliminator )
{
    if( !box )
        return -1;
    isom_extension_box_t *ext = lsmash_malloc_zero( sizeof(isom_extension_box_t) );
    if( !ext )
        return -1;
    ext->type     = ((isom_box_t *)box)->type;
    ext->format   = EXTENSION_FORMAT_BOX;
    ext->form.box = box;
    ext->destruct = eliminator ? eliminator : free;
    if( lsmash_add_entry( extensions, ext ) )
    {
        ext->destruct( ext );
        return -1;
    }
    return 0;
}

lsmash_codec_specific_t *isom_get_codec_specific( lsmash_codec_specific_list_t *opaque, lsmash_codec_specific_data_type type )
{
    for( lsmash_entry_t *entry = opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific || specific->type != type )
            continue;
        return specific;
    }
    return NULL;
}

lsmash_codec_specific_t *isom_get_codec_specific_by_box_type( lsmash_codec_specific_list_t *opaque, uint32_t box_type )
{
    for( lsmash_entry_t *entry = opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific )
            continue;
        if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
        {
            if( specific->size < ISOM_BASEBOX_COMMON_SIZE )
                continue;
            uint8_t *data = (uint8_t *)specific->data.unstructured;
            if( box_type == LSMASH_4CC( data[4], data[5], data[6], data[7] ) )
                return specific;
        }
        else
        {
            isom_box_t *box = (isom_box_t *)specific->data.structured;
            if( box->type == box_type )
                return specific;
        }
    }
    return NULL;
}

static int isom_check_valid_summary( lsmash_summary_t *summary )
{
    if( !summary )
        return -1;
    isom_box_t temp_box;
    temp_box.type    = summary->sample_type;
    temp_box.manager = summary->summary_type == LSMASH_SUMMARY_TYPE_AUDIO ? LSMASH_AUDIO_DESCRIPTION: 0;
    if( isom_is_lpcm_audio( &temp_box ) )
    {
        if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS ) )
            return 0;
        return -1;
    }
    if( isom_is_uncompressed_ycbcr( summary->sample_type ) )
    {
        if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO ) )
        {
            if( summary->sample_type != QT_CODEC_TYPE_V216_VIDEO )
                return 0;
        }
        else
            return -1;
    }
    switch( summary->sample_type )
    {
        case ISOM_CODEC_TYPE_AVC1_VIDEO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264 ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_VC_1_VIDEO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1 ) )
                return 0;
            break;
        case QT_CODEC_TYPE_ULRA_VIDEO :
        case QT_CODEC_TYPE_ULRG_VIDEO :
        case QT_CODEC_TYPE_ULY0_VIDEO :
        case QT_CODEC_TYPE_ULY2_VIDEO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER ) )
                return 0;
            break;
        case QT_CODEC_TYPE_V216_VIDEO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_MP4V_VIDEO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_MP4A_AUDIO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG ) )
                return 0;
            lsmash_codec_specific_t *specific = isom_get_codec_specific_by_box_type( summary->opaque, QT_BOX_TYPE_WAVE );
            if( !specific )
                return -1;
            if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
            {
                uint32_t dummy;
                if( isom_get_child_box_position( specific->data.unstructured, specific->size, ISOM_BOX_TYPE_ESDS, &dummy ) )
                    return 0;   /* Found ES Descriptor Box inside 'wave' extension. */
            }
            else
            {
                isom_wave_t *wave = (isom_wave_t *)specific->data.structured;
                if( isom_get_sample_description_extension( &wave->extensions, ISOM_BOX_TYPE_ESDS ) )
                    return 0;   /* Found ES Descriptor Box inside 'wave' extension. */
            }
            break;
        case ISOM_CODEC_TYPE_AC_3_AUDIO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3 ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_EC_3_AUDIO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3 ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_DTSC_AUDIO :
        case ISOM_CODEC_TYPE_DTSE_AUDIO :
        case ISOM_CODEC_TYPE_DTSH_AUDIO :
        case ISOM_CODEC_TYPE_DTSL_AUDIO :
            if( isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS ) )
                return 0;
            break;
        case ISOM_CODEC_TYPE_SAMR_AUDIO :
        case ISOM_CODEC_TYPE_SAWB_AUDIO :
        default :
            return 0;
    }
    return -1;
}

int isom_setup_visual_description( isom_stsd_t *stsd, uint32_t sample_type, lsmash_video_summary_t *summary )
{
    if( !summary || !stsd || !stsd->list || !stsd->parent || !stsd->parent->parent
     || !stsd->parent->parent->parent || !stsd->parent->parent->parent->parent )
        return -1;
    if( isom_check_valid_summary( (lsmash_summary_t *)summary ) )
        return -1;
    lsmash_entry_list_t *list = stsd->list;
    isom_visual_entry_t *visual = lsmash_malloc_zero( sizeof(isom_visual_entry_t) );
    if( !visual )
        return -1;
    isom_init_box_common( visual, stsd, sample_type );
    visual->manager             |= LSMASH_VIDEO_DESCRIPTION;
    visual->data_reference_index = 1;
    visual->version              = 0;
    visual->revision_level       = 0;
    visual->vendor               = 0;
    visual->temporalQuality      = 0;
    visual->spatialQuality       = 0;
    visual->width                = (uint16_t)summary->width;
    visual->height               = (uint16_t)summary->height;
    visual->horizresolution      = 0x00480000;
    visual->vertresolution       = 0x00480000;
    visual->dataSize             = 0;
    visual->frame_count          = 1;
    visual->depth                = isom_is_qt_video( summary->sample_type ) || isom_is_avc( summary->sample_type ) ? summary->depth : 0x0018;
    visual->color_table_ID       = -1;
    if( summary->compressorname[0] == '\0' )
        isom_set_default_compressorname( visual->compressorname, sample_type );
    else
    {
        memcpy( visual->compressorname, summary->compressorname, 32 );
        visual->compressorname[32] = '\0';
    }
    for( lsmash_entry_t *entry = summary->opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific )
            goto fail;
        if( specific->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN
         && specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
            continue;   /* LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN + LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED is not supported. */
        switch( specific->type )
        {
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_COMMON :
            {
                if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
                    continue;
                lsmash_qt_video_common_t *data = (lsmash_qt_video_common_t *)specific->data.structured;
                visual->revision_level  = data->revision_level;
                visual->vendor          = data->vendor;
                visual->temporalQuality = data->temporalQuality;
                visual->spatialQuality  = data->spatialQuality;
                visual->horizresolution = data->horizontal_resolution;
                visual->vertresolution  = data->vertical_resolution;
                visual->dataSize        = data->dataSize;
                visual->frame_count     = data->frame_count;
                visual->color_table_ID  = data->color_table_ID;
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_SAMPLE_SCALE :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_isom_sample_scale_t *data = (lsmash_isom_sample_scale_t *)cs->data.structured;
                isom_stsl_t *box = lsmash_malloc_zero( sizeof(isom_stsl_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, ISOM_BOX_TYPE_STSL );
                box->constraint_flag  = data->constraint_flag;
                box->scale_method     = data->scale_method;
                box->display_center_x = data->display_center_x;
                box->display_center_y = data->display_center_y;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_stsl ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_h264_bitrate_t *data = (lsmash_h264_bitrate_t *)cs->data.structured;
                isom_btrt_t *box = lsmash_malloc_zero( sizeof(isom_btrt_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, ISOM_BOX_TYPE_BTRT );
                box->bufferSizeDB = data->bufferSizeDB;
                box->maxBitrate   = data->maxBitrate;
                box->avgBitrate   = data->avgBitrate;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_btrt ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_qt_field_info_t *data = (lsmash_qt_field_info_t *)cs->data.structured;
                isom_fiel_t *box = lsmash_malloc_zero( sizeof(isom_fiel_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, QT_BOX_TYPE_FIEL );
                box->fields = data->fields;
                box->detail = data->detail;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_fiel ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_PIXEL_FORMAT :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_qt_pixel_format_t *data = (lsmash_qt_pixel_format_t *)cs->data.structured;
                isom_cspc_t *box = lsmash_malloc_zero( sizeof(isom_cspc_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, QT_BOX_TYPE_CSPC );
                box->pixel_format = data->pixel_format;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_cspc ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_qt_significant_bits_t *data = (lsmash_qt_significant_bits_t *)cs->data.structured;
                isom_sgbt_t *box = lsmash_malloc_zero( sizeof(isom_sgbt_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, QT_BOX_TYPE_SGBT );
                box->significantBits = data->significantBits;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_sgbt ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_GAMMA_LEVEL :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_qt_gamma_t *data = (lsmash_qt_gamma_t *)cs->data.structured;
                isom_gama_t *box = lsmash_malloc_zero( sizeof(isom_gama_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, QT_BOX_TYPE_GAMA );
                box->level = data->level;
                lsmash_destroy_codec_specific_data( cs );
                if( isom_add_extension_box( &visual->extensions, box, isom_remove_gama ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_codec_global_header_t *data = (lsmash_codec_global_header_t *)cs->data.structured;
                isom_glbl_t *box = lsmash_malloc_zero( sizeof(isom_glbl_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, visual, QT_BOX_TYPE_GLBL );
                box->header_size = data->header_size;
                box->header_data = lsmash_memdup( data->header_data, data->header_size );
                lsmash_destroy_codec_specific_data( cs );
                if( !box->header_data
                 || isom_add_extension_box( &visual->extensions, box, isom_remove_glbl ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            default :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
                if( !cs || cs->size < ISOM_BASEBOX_COMMON_SIZE )
                    goto fail;
                isom_extension_box_t *extension = malloc( sizeof(isom_extension_box_t) );
                if( !extension )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                uint8_t *data = cs->data.unstructured;
                extension->size        = cs->size;
                extension->type        = LSMASH_4CC( data[4], data[5], data[6], data[7] );
                extension->format      = EXTENSION_FORMAT_BINARY;
                extension->form.binary = data;
                extension->destruct    = free;
                cs->data.unstructured = NULL;   /* Avoid freeing the binary data of the extension. */
                lsmash_destroy_codec_specific_data( cs );
                if( lsmash_add_entry( &visual->extensions, extension ) )
                {
                    extension->destruct( extension );
                    goto fail;
                }
                break;
            }
        }
    }
    isom_trak_entry_t *trak = (isom_trak_entry_t *)visual->parent->parent->parent->parent->parent;
    int qt_compatible = trak->root->qt_compatible;
    isom_tapt_t *tapt = trak->tapt;
    isom_stsl_t *stsl = (isom_stsl_t *)isom_get_extension_box( &visual->extensions, ISOM_BOX_TYPE_STSL );
    int set_aperture_modes = qt_compatible                      /* Track Aperture Modes is only available under QuickTime file format. */
        && (!stsl || stsl->scale_method == 0)                   /* Sample scaling method might conflict with this feature. */
        && tapt && tapt->clef && tapt->prof && tapt->enof       /* Check if required boxes exist. */
        && !((isom_stsd_t *)visual->parent)->list->entry_count; /* Multiple sample description might conflict with this, so in that case, disable this feature.
                                                                 * Note: this sample description isn't added yet here. */
    if( !set_aperture_modes )
        isom_remove_tapt( trak->tapt );
    int uncompressed_ycbcr = qt_compatible && isom_is_uncompressed_ycbcr( visual->type );
    /* Set up Clean Aperture. */
    if( set_aperture_modes || uncompressed_ycbcr
     || (summary->clap.width.d && summary->clap.height.d && summary->clap.horizontal_offset.d && summary->clap.vertical_offset.d) )
    {
        isom_clap_t *box = lsmash_malloc_zero( sizeof(isom_clap_t) );
        if( !box )
            goto fail;
        isom_init_box_common( box, visual, ISOM_BOX_TYPE_CLAP );
        if( summary->clap.width.d && summary->clap.height.d && summary->clap.horizontal_offset.d && summary->clap.vertical_offset.d )
        {
            box->cleanApertureWidthN  = summary->clap.width.n;
            box->cleanApertureWidthD  = summary->clap.width.d;
            box->cleanApertureHeightN = summary->clap.height.n;
            box->cleanApertureHeightD = summary->clap.height.d;
            box->horizOffN            = summary->clap.horizontal_offset.n;
            box->horizOffD            = summary->clap.horizontal_offset.d;
            box->vertOffN             = summary->clap.vertical_offset.n;
            box->vertOffD             = summary->clap.vertical_offset.d;
        }
        else
        {
            box->cleanApertureWidthN  = summary->width;
            box->cleanApertureWidthD  = 1;
            box->cleanApertureHeightN = summary->height;
            box->cleanApertureHeightD = 1;
            box->horizOffN            = 0;
            box->horizOffD            = 1;
            box->vertOffN             = 0;
            box->vertOffD             = 1;
        }
        if( isom_add_extension_box( &visual->extensions, box, isom_remove_clap ) )
        {
            free( box );
            goto fail;
        }
    }
    /* Set up Pixel Aspect Ratio. */
    if( set_aperture_modes || (summary->par_h && summary->par_v) )
    {
        isom_pasp_t *box = lsmash_malloc_zero( sizeof(isom_pasp_t) );
        if( !box )
            goto fail;
        isom_init_box_common( box, visual, ISOM_BOX_TYPE_PASP );
        box->hSpacing = LSMASH_MAX( summary->par_h, 1 );
        box->vSpacing = LSMASH_MAX( summary->par_v, 1 );
        if( isom_add_extension_box( &visual->extensions, box, isom_remove_pasp ) )
        {
            free( box );
            goto fail;
        }
    }
    /* Set up Color Parameter. */
    if( qt_compatible
     && (uncompressed_ycbcr
      || summary->color.primaries_index
      || summary->color.transfer_index
      || summary->color.matrix_index) )
    {
        isom_colr_t *box = lsmash_malloc_zero( sizeof(isom_colr_t) );
        if( !box )
            goto fail;
        isom_init_box_common( box, visual, QT_BOX_TYPE_COLR );
        /* Set 'nclc' to parameter type, we don't support 'prof'. */
        uint16_t primaries = summary->color.primaries_index;
        uint16_t transfer  = summary->color.transfer_index;
        uint16_t matrix    = summary->color.matrix_index;
        box->color_parameter_type    = QT_COLOR_PARAMETER_TYPE_NCLC;
        box->primaries_index         = (primaries == 1 || primaries == 5 || primaries == 6) ? primaries : QT_PRIMARIES_INDEX_UNSPECIFIED;
        box->transfer_function_index = (transfer == 1 || transfer == 7)                     ? transfer  : QT_TRANSFER_INDEX_UNSPECIFIED;
        box->matrix_index            = (matrix == 1 || matrix == 6 || matrix == 7)          ? matrix    : QT_MATRIX_INDEX_UNSPECIFIED;
        if( isom_add_extension_box( &visual->extensions, box, isom_remove_colr ) )
        {
            free( box );
            goto fail;
        }
    }
    /* Set up Track Apeture Modes. */
    if( set_aperture_modes )
    {
        uint32_t width  = visual->width  << 16;
        uint32_t height = visual->height << 16;
        isom_clap_t *clap = (isom_clap_t *)isom_get_extension_box( &visual->extensions, ISOM_BOX_TYPE_CLAP );
        isom_pasp_t *pasp = (isom_pasp_t *)isom_get_extension_box( &visual->extensions, ISOM_BOX_TYPE_PASP );
        double clap_width  = ((double)clap->cleanApertureWidthN  / clap->cleanApertureWidthD)  * (1<<16);
        double clap_height = ((double)clap->cleanApertureHeightN / clap->cleanApertureHeightD) * (1<<16);
        double par = (double)pasp->hSpacing / pasp->vSpacing;
        if( par >= 1.0 )
        {
            tapt->clef->width  = clap_width * par;
            tapt->clef->height = clap_height;
            tapt->prof->width  = width * par;
            tapt->prof->height = height;
        }
        else
        {
            tapt->clef->width  = clap_width;
            tapt->clef->height = clap_height / par;
            tapt->prof->width  = width;
            tapt->prof->height = height / par;
        }
        tapt->enof->width  = width;
        tapt->enof->height = height;
    }
    if( !lsmash_add_entry( list, visual ) )
        return 0;   /* successed */
fail:
    isom_remove_sample_description_extensions( &visual->extensions );
    free( visual );
    return -1;
}

static int isom_append_audio_es_descriptor_extension( isom_box_t *box, lsmash_audio_summary_t *summary )
{
    uint32_t esds_size = 0;
    uint8_t *esds_data = NULL;
    lsmash_codec_specific_t *specific = isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG );
    if( specific )
    {
        if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
        {
            esds_size = specific->size;
            esds_data = lsmash_memdup( specific->data.unstructured, specific->size );
            if( !esds_data )
                return -1;
        }
        else
        {
            esds_data = lsmash_create_mp4sys_decoder_config( (lsmash_mp4sys_decoder_parameters_t *)specific->data.structured, &esds_size );
            if( !esds_data )
                return -1;
        }
    }
    else
    {
        /* Try to find inside 'wave' extension. */
        specific = isom_get_codec_specific_by_box_type( summary->opaque, QT_BOX_TYPE_WAVE );
        if( !specific )
            return -1;
        if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
        {
            esds_data = isom_get_child_box_position( specific->data.unstructured, specific->size, ISOM_BOX_TYPE_ESDS, &esds_size );
            if( !esds_data )
                return -1;
        }
        else
        {
            isom_extension_box_t *ext = isom_get_sample_description_extension( &((isom_box_t *)specific->data.structured)->extensions, ISOM_BOX_TYPE_ESDS );
            if( !ext )
                return -1;
            if( ext->format == EXTENSION_FORMAT_BINARY )
            {
                esds_size = ext->size;
                esds_data = lsmash_memdup( ext->form.binary, ext->size );
                if( !esds_data )
                    return -1;
            }
            else
            {
                isom_esds_t *esds = (isom_esds_t *)ext->form.box;
                if( !esds )
                    return -1;
                lsmash_bs_t *bs = lsmash_bs_create( NULL );
                if( !bs )
                    return -1;
                lsmash_bs_put_be32( bs, 0 );
                lsmash_bs_put_be32( bs, ISOM_BOX_TYPE_ESDS );
                lsmash_bs_put_be32( bs, 0 );
                mp4sys_put_ES_Descriptor( bs, esds->ES );
                esds_data = lsmash_bs_export_data( bs, &esds_size );
                lsmash_bs_cleanup( bs );
                if( !esds_data )
                    return -1;
                /* Update box size. */
                esds_data[0] = ((esds_size) >> 24) & 0xff;
                esds_data[1] = ((esds_size) >> 16) & 0xff;
                esds_data[2] = ((esds_size) >>  8) & 0xff;
                esds_data[3] =  (esds_size)        & 0xff;
            }
        }
    }
    isom_esds_t *esds = lsmash_malloc_zero( sizeof(isom_esds_t) );
    if( !esds )
    {
        free( esds_data );
        return -1;
    }
    isom_init_box_common( esds, box, ISOM_BOX_TYPE_ESDS );
    uint8_t buffer[ esds_size ];
    lsmash_bs_t bs = { 0 };
    bs.data  = buffer;
    bs.alloc = esds_size;
    bs.store = esds_size - ISOM_FULLBOX_COMMON_SIZE;
    memcpy( bs.data, esds_data + ISOM_FULLBOX_COMMON_SIZE, bs.store );
    esds->ES = mp4sys_get_ES_Descriptor( &bs );
    if( !esds->ES )
    {
        free( esds );
        return -1;
    }
    if( isom_add_extension_box( &box->extensions, esds, isom_remove_esds ) )
    {
        isom_remove_esds( esds );
        return -1;
    }
    return 0;
}

static int isom_set_qtff_mp4a_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    isom_wave_t *wave = lsmash_malloc_zero( sizeof(isom_wave_t) );
    if( !wave )
        return -1;
    isom_init_box_common( wave, audio, QT_BOX_TYPE_WAVE );
    if( isom_add_frma( wave )
     || isom_add_mp4a( wave )
     || isom_add_terminator( wave )
     || isom_add_extension_box( &audio->extensions, wave, isom_remove_wave ) )
    {
        isom_remove_wave( wave );
        return -1;
    }
    wave->frma->data_format = audio->type;
    /* Add ES Descriptor Box. */
    if( isom_append_audio_es_descriptor_extension( (isom_box_t *)wave, summary ) )
        return -1;
    /* */
    audio->version              = (summary->channels > 2 || summary->frequency > UINT16_MAX) ? 2 : 1;
    audio->channelcount         = audio->version == 2 ? 3 : LSMASH_MIN( summary->channels, 2 );
    audio->samplesize           = 16;
    audio->compression_ID       = QT_AUDIO_COMPRESSION_ID_VARIABLE_COMPRESSION;
    audio->packet_size          = 0;
    if( audio->version == 1 )
    {
        audio->samplerate       = summary->frequency << 16;
        audio->samplesPerPacket = summary->samples_in_frame;
        audio->bytesPerPacket   = 1;    /* Apparently, this field is set to 1. */
        audio->bytesPerFrame    = audio->bytesPerPacket * summary->channels;
        audio->bytesPerSample   = 2;
    }
    else    /* audio->version == 2 */
    {
        audio->samplerate                    = 0x00010000;
        audio->sizeOfStructOnly              = 72;
        audio->audioSampleRate               = (union {double d; uint64_t i;}){summary->frequency}.i;
        audio->numAudioChannels              = summary->channels;
        audio->always7F000000                = 0x7F000000;
        audio->constBitsPerChannel           = 0;   /* compressed audio */
        audio->formatSpecificFlags           = 0;
        audio->constBytesPerAudioPacket      = 0;   /* variable */
        audio->constLPCMFramesPerAudioPacket = summary->samples_in_frame;
    }
    return 0;
}

static int isom_set_isom_mp4a_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    if( summary->summary_type != LSMASH_SUMMARY_TYPE_AUDIO )
        return -1;
    /* Check objectTypeIndication. */
    lsmash_codec_specific_t *src = isom_get_codec_specific( summary->opaque, LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG );
    if( !src )
        return -1;
    lsmash_mp4sys_object_type_indication objectTypeIndication;
    if( src->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
        objectTypeIndication = ((lsmash_mp4sys_decoder_parameters_t *)src->data.structured)->objectTypeIndication;
    else
    {
        lsmash_codec_specific_t *dst = lsmash_convert_codec_specific_format( src, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
        if( !dst )
            return -1;
        objectTypeIndication = ((lsmash_mp4sys_decoder_parameters_t *)dst->data.structured)->objectTypeIndication;
        lsmash_destroy_codec_specific_data( dst );
    }
    switch( objectTypeIndication )
    {
        case MP4SYS_OBJECT_TYPE_Audio_ISO_14496_3:
        case MP4SYS_OBJECT_TYPE_Audio_ISO_13818_7_Main_Profile:
        case MP4SYS_OBJECT_TYPE_Audio_ISO_13818_7_LC_Profile:
        case MP4SYS_OBJECT_TYPE_Audio_ISO_13818_7_SSR_Profile:
        case MP4SYS_OBJECT_TYPE_Audio_ISO_13818_3:      /* Legacy Interface */
        case MP4SYS_OBJECT_TYPE_Audio_ISO_11172_3:      /* Legacy Interface */
            break;
        default:
            return -1;
    }
    /* Add ES Descriptor Box. */
    if( isom_append_audio_es_descriptor_extension( (isom_box_t *)audio, summary ) )
        return -1;
    /* In pure mp4 file, these "template" fields shall be default values according to the spec.
       But not pure - hybrid with other spec - mp4 file can take other values.
       Which is to say, these template values shall be ignored in terms of mp4, except some object_type_indications.
       see 14496-14, "Template fields used". */
    audio->version        = 0;
    audio->revision_level = 0;
    audio->vendor         = 0;
    audio->channelcount   = 2;
    audio->samplesize     = 16;
    audio->compression_ID = 0;
    audio->packet_size    = 0;
    /* WARNING: This field cannot retain frequency above 65535Hz.
       This is not "FIXME", I just honestly implemented what the spec says.
       BTW, who ever expects sampling frequency takes fixed-point decimal??? */
    audio->samplerate     = summary->frequency <= UINT16_MAX ? summary->frequency << 16 : 0;
    return 0;
}

static int isom_set_qtff_lpcm_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    lsmash_qt_audio_format_specific_flags_t *lpcm = NULL;
    for( lsmash_entry_t *entry = summary->opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific )
            continue;
        if( specific->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS
         && specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
        {
            lpcm = (lsmash_qt_audio_format_specific_flags_t *)specific->data.structured;
            break;
        }
    }
    if( !lpcm )
        return -1;
    uint32_t sample_type = audio->type;
    /* Convert the sample type into 'lpcm' if the description doesn't match the format or version = 2 fields are needed. */
    if( (sample_type == QT_CODEC_TYPE_RAW_AUDIO && (summary->sample_size != 8 || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT)))
     || (sample_type == QT_CODEC_TYPE_FL32_AUDIO && (summary->sample_size != 32 || !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT)))
     || (sample_type == QT_CODEC_TYPE_FL64_AUDIO && (summary->sample_size != 64 || !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT)))
     || (sample_type == QT_CODEC_TYPE_IN24_AUDIO && (summary->sample_size != 24 || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT)))
     || (sample_type == QT_CODEC_TYPE_IN32_AUDIO && (summary->sample_size != 32 || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT)))
     || (sample_type == QT_CODEC_TYPE_23NI_AUDIO && (summary->sample_size != 32 || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT) || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN)))
     || (sample_type == QT_CODEC_TYPE_SOWT_AUDIO && (summary->sample_size != 16 || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT) || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN)))
     || (sample_type == QT_CODEC_TYPE_TWOS_AUDIO && ((summary->sample_size != 16 && summary->sample_size != 8) || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT) || !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN)))
     || (sample_type == QT_CODEC_TYPE_NONE_AUDIO && ((summary->sample_size != 16 && summary->sample_size != 8) || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT) || !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN)))
     || (sample_type == QT_CODEC_TYPE_NOT_SPECIFIED && ((summary->sample_size != 16 && summary->sample_size != 8) || (lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT) || !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN)))
     || (summary->channels > 2 || summary->frequency > UINT16_MAX || summary->sample_size % 8) )
    {
        audio->type    = QT_CODEC_TYPE_LPCM_AUDIO;
        audio->version = 2;
    }
    else if( sample_type == QT_CODEC_TYPE_LPCM_AUDIO )
        audio->version = 2;
    else if( summary->sample_size > 16
     || (sample_type != QT_CODEC_TYPE_RAW_AUDIO && sample_type != QT_CODEC_TYPE_TWOS_AUDIO
     && sample_type != QT_CODEC_TYPE_NONE_AUDIO && sample_type != QT_CODEC_TYPE_NOT_SPECIFIED) )
        audio->version = 1;
    /* Set up constBytesPerAudioPacket field.
     * We use constBytesPerAudioPacket as the actual size of LPCM audio frame even when version is not 2. */
    audio->constBytesPerAudioPacket = (summary->sample_size * summary->channels) / 8;
    /* Set up other fields in this description by its version. */
    if( audio->version == 2 )
    {
        audio->channelcount                  = 3;
        audio->samplesize                    = 16;
        audio->compression_ID                = -2;
        audio->samplerate                    = 0x00010000;
        audio->sizeOfStructOnly              = 72;
        audio->audioSampleRate               = (union {double d; uint64_t i;}){summary->frequency}.i;
        audio->numAudioChannels              = summary->channels;
        audio->always7F000000                = 0x7F000000;
        audio->constBitsPerChannel           = summary->sample_size;
        audio->constLPCMFramesPerAudioPacket = 1;
        audio->formatSpecificFlags           = lpcm->format_flags;
        if( sample_type == QT_CODEC_TYPE_TWOS_AUDIO && summary->sample_size != 8 )
            audio->formatSpecificFlags |= QT_LPCM_FORMAT_FLAG_BIG_ENDIAN;
        if( lpcm->format_flags & QT_LPCM_FORMAT_FLAG_FLOAT )
            audio->formatSpecificFlags &= ~QT_LPCM_FORMAT_FLAG_SIGNED_INTEGER;
        if( lpcm->format_flags & QT_LPCM_FORMAT_FLAG_PACKED )
            audio->formatSpecificFlags &= ~QT_LPCM_FORMAT_FLAG_ALIGNED_HIGH;
    }
    else if( audio->version == 1 )
    {
        audio->channelcount = summary->channels;
        audio->samplesize   = 16;
        /* Audio formats other than 'raw ' and 'twos' are treated as compressed audio. */
        if( sample_type == QT_CODEC_TYPE_RAW_AUDIO || sample_type == QT_CODEC_TYPE_TWOS_AUDIO )
            audio->compression_ID = QT_AUDIO_COMPRESSION_ID_NOT_COMPRESSED;
        else
            audio->compression_ID = QT_AUDIO_COMPRESSION_ID_FIXED_COMPRESSION;
        audio->samplerate       = summary->frequency << 16;
        audio->samplesPerPacket = 1;
        audio->bytesPerPacket   = summary->sample_size / 8;
        audio->bytesPerFrame    = audio->bytesPerPacket * summary->channels;    /* sample_size field in stsz box is NOT used. */
        audio->bytesPerSample   = 1 + (summary->sample_size != 8);
        if( sample_type == QT_CODEC_TYPE_FL32_AUDIO || sample_type == QT_CODEC_TYPE_FL64_AUDIO
         || sample_type == QT_CODEC_TYPE_IN24_AUDIO || sample_type == QT_CODEC_TYPE_IN32_AUDIO )
        {
            isom_wave_t *wave = lsmash_malloc_zero( sizeof(isom_wave_t) );
            if( !wave )
                return -1;
            isom_init_box_common( wave, audio, QT_BOX_TYPE_WAVE );
            if( isom_add_frma( wave )
             || isom_add_enda( wave )
             || isom_add_terminator( wave )
             || isom_add_extension_box( &audio->extensions, wave, isom_remove_wave ) )
            {
                isom_remove_wave( wave );
                return -1;
            }
            wave->frma->data_format  = sample_type;
            wave->enda->littleEndian = !(lpcm->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN);
        }
    }
    else    /* audio->version == 0 */
    {
        audio->channelcount   = summary->channels;
        audio->samplesize     = summary->sample_size;
        audio->compression_ID = QT_AUDIO_COMPRESSION_ID_NOT_COMPRESSED;
        audio->samplerate     = summary->frequency << 16;
    }
    return 0;
}

static int isom_set_isom_dts_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    audio->version        = 0;
    audio->revision_level = 0;
    audio->vendor         = 0;
    audio->channelcount   = summary->channels;
    audio->samplesize     = 16;
    audio->compression_ID = 0;
    audio->packet_size    = 0;
    switch( summary->frequency )
    {
        case 12000 :    /* Invalid? (No reference in the spec) */
        case 24000 :
        case 48000 :
        case 96000 :
        case 192000 :
        case 384000 :   /* Invalid? (No reference in the spec) */
            audio->samplerate = 48000 << 16;
            break;
        case 22050 :
        case 44100 :
        case 88200 :
        case 176400 :
        case 352800 :   /* Invalid? (No reference in the spec) */
            audio->samplerate = 44100 << 16;
            break;
        case 8000 :     /* Invalid? (No reference in the spec) */
        case 16000 :
        case 32000 :
        case 64000 :
        case 128000 :
            audio->samplerate = 32000 << 16;
            break;
        default :
            audio->samplerate = 0;
            break;
    }
    return 0;
}

static int isom_set_qtff_template_audio_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    lsmash_qt_audio_format_specific_flags_t *specific_data = NULL;
    for( lsmash_entry_t *entry = summary->opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific )
            continue;
        if( specific->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS
         && specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
        {
            specific_data = (lsmash_qt_audio_format_specific_flags_t *)specific->data.structured;
            break;
        }
    }
    /* Check if a 'wave' extension is already present in the opaque CODEC specific info list.
     * If absent, create a 'wave' extension.
     * If present, append it as an extension. */
    isom_wave_t *wave = NULL;
    lsmash_codec_specific_t *wave_specific = isom_get_codec_specific_by_box_type( summary->opaque, QT_BOX_TYPE_WAVE );
    if( !wave_specific || wave_specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
    {
        wave = lsmash_malloc_zero( sizeof(isom_wave_t) );
        if( !wave )
            return -1;
        isom_init_box_common( wave, audio, QT_BOX_TYPE_WAVE );
        if( isom_add_frma( wave )
         || isom_add_terminator( wave )
         || isom_add_extension_box( &audio->extensions, wave, isom_remove_wave ) )
        {
            isom_remove_wave( wave );
            return -1;
        }
        wave->frma->data_format = audio->type;
    }
    else
    {
        uint8_t *data = lsmash_memdup( wave_specific->data.unstructured, wave_specific->size );
        if( !data )
            return -1;
        isom_extension_box_t *ext = lsmash_malloc_zero( sizeof(isom_extension_box_t) );
        if( !ext )
        {
            free( data );
            return -1;
        }
        ext->size        = wave_specific->size;
        ext->type        = QT_BOX_TYPE_WAVE;
        ext->format      = EXTENSION_FORMAT_BINARY;
        ext->form.binary = data;
        ext->destruct    = free;
        if( lsmash_add_entry( &audio->extensions, ext ) )
        {
            ext->destruct( ext );
            return -1;
        }
    }
    /* Set up common audio description fields. */
    audio->version        = (summary->channels > 2 || summary->frequency > UINT16_MAX) ? 2 : 1;
    audio->channelcount   = audio->version == 2 ? 3 : LSMASH_MIN( summary->channels, 2 );
    audio->samplesize     = 16;
    audio->compression_ID = QT_AUDIO_COMPRESSION_ID_VARIABLE_COMPRESSION;
    audio->packet_size    = 0;
    if( audio->version == 2 )
    {
        audio->channelcount                  = 3;
        audio->compression_ID                = -2;
        audio->samplerate                    = 0x00010000;
        audio->sizeOfStructOnly              = 72;
        audio->audioSampleRate               = (union {double d; uint64_t i;}){summary->frequency}.i;
        audio->numAudioChannels              = summary->channels;
        audio->always7F000000                = 0x7F000000;
        audio->constBitsPerChannel           = 0;
        audio->constBytesPerAudioPacket      = 0;
        audio->constLPCMFramesPerAudioPacket = summary->samples_in_frame;
        if( audio->type == QT_CODEC_TYPE_ALAC_AUDIO )
        {
            switch( summary->sample_size )
            {
                case 16 :
                    audio->formatSpecificFlags = QT_ALAC_FORMAT_FLAG_16BIT_SOURCE_DATA;
                    break;
                case 20 :
                    audio->formatSpecificFlags = QT_ALAC_FORMAT_FLAG_20BIT_SOURCE_DATA;
                    break;
                case 24 :
                    audio->formatSpecificFlags = QT_ALAC_FORMAT_FLAG_24BIT_SOURCE_DATA;
                    break;
                case 32 :
                    audio->formatSpecificFlags = QT_ALAC_FORMAT_FLAG_32BIT_SOURCE_DATA;
                    break;
                default :
                    break;
            }
        }
        else
        {
            if( specific_data )
            {
                audio->formatSpecificFlags = specific_data->format_flags;
                if( specific_data->format_flags & QT_AUDIO_FORMAT_FLAG_FLOAT )
                    audio->formatSpecificFlags &= ~QT_AUDIO_FORMAT_FLAG_SIGNED_INTEGER;
                if( specific_data->format_flags & QT_AUDIO_FORMAT_FLAG_PACKED )
                    audio->formatSpecificFlags &= ~QT_AUDIO_FORMAT_FLAG_ALIGNED_HIGH;
            }
            else
                audio->formatSpecificFlags = 0;
        }
    }
    else    /* if( audio->version == 1 ) */
    {
        audio->channelcount     = LSMASH_MIN( summary->channels, 2 );
        audio->samplerate       = summary->frequency << 16;
        audio->samplesPerPacket = summary->samples_in_frame;
        audio->bytesPerPacket   = summary->sample_size / 8;
        audio->bytesPerFrame    = audio->bytesPerPacket * summary->channels;    /* sample_size field in stsz box is NOT used. */
        audio->bytesPerSample   = 1 + (summary->sample_size != 8);
        if( specific_data )
        {
            if( wave )
            {
                if( isom_add_enda( wave ) )
                    return -1;
                wave->enda->littleEndian = !(specific_data->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN);
            }
            else
            {
                isom_extension_box_t *ext = isom_get_sample_description_extension( &audio->extensions, QT_BOX_TYPE_WAVE );
                assert( ext && ext->format == EXTENSION_FORMAT_BINARY );
                uint32_t enda_size;
                uint8_t *enda = isom_get_child_box_position( ext->form.binary, ext->size, QT_BOX_TYPE_ENDA, &enda_size );
                if( !enda )
                {
                    uint32_t wave_size = ext->size;
                    uint8_t *wave_data = ext->form.binary;
                    uint32_t frma_size;
                    uint8_t *frma_data = isom_get_child_box_position( ext->form.binary, ext->size, QT_BOX_TYPE_FRMA, &frma_size );
                    uint8_t *frma_end = frma_data + frma_size;
                    uint32_t remainder_size = ext->size - (frma_end - wave_data);
                    uint32_t enda_offset = wave_data - frma_end;
                    enda_size = ISOM_BASEBOX_COMMON_SIZE + 2;
                    wave_data = lsmash_memdup( wave_data, wave_size + enda_size );
                    enda = wave_data + enda_offset;
                    enda[0] = (enda_size >> 24) & 0xff;
                    enda[1] = (enda_size >> 16) & 0xff;
                    enda[2] = (enda_size >>  8) & 0xff;
                    enda[3] =  enda_size        & 0xff;
                    enda[4] = 'e';
                    enda[5] = 'n';
                    enda[6] = 'd';
                    enda[7] = 'a';
                    enda[8] = 0;
                    enda[9] = !(specific_data->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN);
                    memcpy( wave_data + enda_offset + enda_size, frma_end, remainder_size );
                    free( ext->form.binary );
                    ext->form.binary = wave_data;
                    ext->size       += enda_size;
                }
                else
                {
                    if( enda_size < ISOM_BASEBOX_COMMON_SIZE + 2 )
                        return -1;
                    if( specific_data->format_flags & QT_LPCM_FORMAT_FLAG_BIG_ENDIAN )
                        enda[9] &= ~0x01;
                    else
                        enda[9] |= 0x01;
                }
            }
        }
    }
    return 0;
}

static int isom_set_isom_template_audio_description( isom_audio_entry_t *audio, lsmash_audio_summary_t *summary )
{
    audio->version        = 0;
    audio->revision_level = 0;
    audio->vendor         = 0;
    audio->channelcount   = summary->channels;
    audio->samplesize     = 16;
    audio->compression_ID = 0;
    audio->packet_size    = 0;
    audio->samplerate     = summary->frequency <= UINT16_MAX ? summary->frequency << 16 : 0;
    return 0;
}

int isom_setup_audio_description( isom_stsd_t *stsd, uint32_t sample_type, lsmash_audio_summary_t *summary )
{
    if( !stsd || !stsd->list || !stsd->root || !summary )
        return -1;
    if( isom_check_valid_summary( (lsmash_summary_t *)summary ) )
        return -1;
    lsmash_entry_list_t *list = stsd->list;
    isom_audio_entry_t *audio = lsmash_malloc_zero( sizeof(isom_audio_entry_t) );
    if( !audio )
        return -1;
    isom_init_box_common( audio, stsd, sample_type );
    audio->manager             |= LSMASH_AUDIO_DESCRIPTION;
    audio->data_reference_index = 1;
    lsmash_root_t *root = stsd->root;
    int ret;
    if( sample_type == ISOM_CODEC_TYPE_MP4A_AUDIO )
    {
        if( root->ftyp && root->ftyp->major_brand == ISOM_BRAND_TYPE_QT )
            ret = isom_set_qtff_mp4a_description( audio, summary );
        else
            ret = isom_set_isom_mp4a_description( audio, summary );
    }
    else if( isom_is_lpcm_audio( audio ) )
        ret = isom_set_qtff_lpcm_description( audio, summary );
    else if( audio->type == ISOM_CODEC_TYPE_DTSC_AUDIO
          || audio->type == ISOM_CODEC_TYPE_DTSE_AUDIO
          || audio->type == ISOM_CODEC_TYPE_DTSH_AUDIO
          || audio->type == ISOM_CODEC_TYPE_DTSL_AUDIO )
        ret = isom_set_isom_dts_description( audio, summary );
    else if( root->qt_compatible )
        ret = isom_set_qtff_template_audio_description( audio, summary );
    else
        ret = isom_set_isom_template_audio_description( audio, summary );
    if( ret )
        goto fail;
    for( lsmash_entry_t *entry = summary->opaque->list.head; entry; entry = entry->next )
    {
        lsmash_codec_specific_t *specific = (lsmash_codec_specific_t *)entry->data;
        if( !specific )
            goto fail;
        if( specific->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN
         && specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED )
            continue;   /* LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN + LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED is not supported. */
        switch( specific->type )
        {
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_COMMON :
            {
                if( specific->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED )
                    continue;   /* Ignore since not fatal. */
                lsmash_qt_audio_common_t *data = (lsmash_qt_audio_common_t *)specific->data.structured;
                audio->revision_level  = data->revision_level;
                audio->vendor          = data->vendor;
                audio->compression_ID  = data->compression_ID;
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT :
            {
                if( !root->qt_compatible && audio->type != ISOM_CODEC_TYPE_ALAC_AUDIO )
                    continue;
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_qt_audio_channel_layout_t *data = (lsmash_qt_audio_channel_layout_t *)cs->data.structured;
                lsmash_channel_layout_tag channelLayoutTag = data->channelLayoutTag;
                lsmash_channel_bitmap     channelBitmap    = data->channelBitmap;
                if( channelLayoutTag == QT_CHANNEL_LAYOUT_USE_CHANNEL_DESCRIPTIONS    /* We don't support the feature of Channel Descriptions. */
                 || (channelLayoutTag == QT_CHANNEL_LAYOUT_USE_CHANNEL_BITMAP && (!channelBitmap || channelBitmap > QT_CHANNEL_BIT_FULL)) )
                {
                    channelLayoutTag = data->channelLayoutTag = QT_CHANNEL_LAYOUT_UNKNOWN | summary->channels;
                    channelBitmap    = data->channelBitmap    = 0;
                }
                /* Don't create Audio Channel Layout Box if the channel layout is unknown. */
                if( (channelLayoutTag ^ QT_CHANNEL_LAYOUT_UNKNOWN) >> 16 )
                {
                    isom_chan_t *box = lsmash_malloc_zero( sizeof(isom_chan_t) );
                    if( !box )
                    {
                        lsmash_destroy_codec_specific_data( cs );
                        goto fail;
                    }
                    isom_init_box_common( box, audio, QT_BOX_TYPE_CHAN );
                    box->channelLayoutTag          = channelLayoutTag;
                    box->channelBitmap             = channelBitmap;
                    box->numberChannelDescriptions = 0;
                    box->channelDescriptions       = NULL;
                    lsmash_destroy_codec_specific_data( cs );
                    if( isom_add_extension_box( &audio->extensions, box, isom_remove_chan ) )
                    {
                        free( box );
                        goto fail;
                    }
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !cs )
                    goto fail;
                lsmash_codec_global_header_t *data = (lsmash_codec_global_header_t *)cs->data.structured;
                isom_glbl_t *box = lsmash_malloc_zero( sizeof(isom_glbl_t) );
                if( !box )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                isom_init_box_common( box, audio, QT_BOX_TYPE_GLBL );
                box->header_size = data->header_size;
                box->header_data = lsmash_memdup( data->header_data, data->header_size );
                lsmash_destroy_codec_specific_data( cs );
                if( !box->header_data
                 || isom_add_extension_box( &audio->extensions, box, isom_remove_glbl ) )
                {
                    free( box );
                    goto fail;
                }
                break;
            }
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS :
            case LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG :
                break;  /* shall be set up already */
            default :
            {
                lsmash_codec_specific_t *cs = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
                if( !cs )
                    goto fail;
                if( cs->size < ISOM_BASEBOX_COMMON_SIZE )
                    continue;
                uint8_t *box_data = cs->data.unstructured;
                uint64_t box_size = cs->size;
                uint32_t box_type = LSMASH_4CC( box_data[4], box_data[5], box_data[6], box_data[7] );
                if( box_type == QT_BOX_TYPE_WAVE )
                {
                    /* CODEC specific info shall be already inside 'wave' extension. */
                    lsmash_destroy_codec_specific_data( cs );
                    continue;
                }
                isom_extension_box_t *extension = lsmash_malloc_zero( sizeof(isom_extension_box_t) );
                if( !extension )
                {
                    lsmash_destroy_codec_specific_data( cs );
                    goto fail;
                }
                extension->size        = box_size;
                extension->type        = box_type;
                extension->format      = EXTENSION_FORMAT_BINARY;
                extension->form.binary = box_data;
                extension->destruct    = free;
                cs->data.unstructured = NULL;   /* Avoid freeing the binary data of the extension. */
                lsmash_destroy_codec_specific_data( cs );
                if( lsmash_add_entry( &audio->extensions, extension ) )
                {
                    extension->destruct( extension );
                    goto fail;
                }
                break;
            }
        }
    }
    if( audio->version == 0 )
        audio->compression_ID = QT_AUDIO_COMPRESSION_ID_NOT_COMPRESSED;
    else if( audio->version == 2 )
        audio->compression_ID = QT_AUDIO_COMPRESSION_ID_VARIABLE_COMPRESSION;
    if( !lsmash_add_entry( list, audio ) )
        return 0;   /* successed */
fail:
    isom_remove_sample_description_extensions( &audio->extensions );
    free( audio );
    return -1;
}

isom_extension_box_t *isom_get_sample_description_extension( lsmash_entry_list_t *extensions, uint32_t box_type )
{
    for( lsmash_entry_t *entry = extensions->head; entry; entry = entry->next )
    {
        isom_extension_box_t *ext = (isom_extension_box_t *)entry->data;
        if( !ext )
            continue;
        if( ext->type == box_type )
            return ext;
    }
    return NULL;
}

void *isom_get_extension_box( lsmash_entry_list_t *extensions, uint32_t box_type )
{
    for( lsmash_entry_t *entry = extensions->head; entry; entry = entry->next )
    {
        isom_extension_box_t *ext = (isom_extension_box_t *)entry->data;
        if( !ext || ext->format != EXTENSION_FORMAT_BOX || ext->type != box_type )
            continue;
        return ext->form.box;
    }
    return NULL;
}

static lsmash_codec_specific_data_type isom_get_codec_specific_data_type( uint32_t extension_type )
{
    static const struct
    {
        uint32_t                        extension_type;
        lsmash_codec_specific_data_type data_type;
    } codec_specific_data_type_table[] =
        {
            { ISOM_BOX_TYPE_AVCC, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264 },
            { ISOM_BOX_TYPE_DVC1, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_VC_1 },
            { ISOM_BOX_TYPE_DAC3, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_AC_3 },
            { ISOM_BOX_TYPE_DEC3, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_EC_3 },
            { ISOM_BOX_TYPE_DDTS, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS },
            { ISOM_BOX_TYPE_ESDS, LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG },
            { ISOM_BOX_TYPE_STSL, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_SAMPLE_SCALE },
            { ISOM_BOX_TYPE_BTRT, LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE },
            { QT_BOX_TYPE_FIEL,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO },
            { QT_BOX_TYPE_CSPC,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_PIXEL_FORMAT },
            { QT_BOX_TYPE_SGBT,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS },
            { QT_BOX_TYPE_GAMA,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_GAMMA_LEVEL },
            { QT_BOX_TYPE_CHAN,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT },
            { QT_BOX_TYPE_GLBL,   LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER },
            { 0,                  LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN }
        };
    lsmash_codec_specific_data_type data_type = LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN;
    for( int i = 0; codec_specific_data_type_table[i].extension_type != LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN; i++ )
        if( extension_type == codec_specific_data_type_table[i].extension_type )
        {
            data_type = codec_specific_data_type_table[i].data_type;
            break;
        }
    return data_type;
}

lsmash_video_summary_t *isom_create_video_summary_from_description( isom_visual_entry_t *visual )
{
    lsmash_video_summary_t *summary = (lsmash_video_summary_t *)lsmash_create_summary( LSMASH_SUMMARY_TYPE_VIDEO );
    if( !summary )
        return NULL;
    summary->sample_type = visual->type;
    summary->width       = visual->width;
    summary->height      = visual->height;
    summary->depth       = visual->depth;
    memcpy( summary->compressorname, visual->compressorname, 32 );
    summary->compressorname[32] = '\0';
    if( isom_is_qt_video( summary->sample_type ) )
    {
        lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_COMMON,
                                                                               LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
        if( !specific )
            goto fail;
        lsmash_qt_video_common_t *data = (lsmash_qt_video_common_t *)specific->data.structured;
        data->revision_level        = visual->revision_level;
        data->vendor                = visual->vendor;
        data->temporalQuality       = visual->temporalQuality;
        data->spatialQuality        = visual->spatialQuality;
        data->horizontal_resolution = visual->horizresolution;
        data->vertical_resolution   = visual->vertresolution;
        data->dataSize              = visual->dataSize;
        data->frame_count           = visual->frame_count;
        data->color_table_ID        = visual->color_table_ID;
        if( lsmash_add_entry( &summary->opaque->list, specific ) )
        {
            lsmash_destroy_codec_specific_data( specific );
            goto fail;
        }
    }
    for( lsmash_entry_t *entry = visual->extensions.head; entry; entry = entry->next )
    {
        isom_extension_box_t *ext = (isom_extension_box_t *)entry->data;
        if( !ext )
            continue;
        if( ext->format == EXTENSION_FORMAT_BOX )
        {
            if( !ext->form.box )
                continue;
            lsmash_codec_specific_t *specific = NULL;
            switch( ext->type )
            {
                case ISOM_BOX_TYPE_CLAP :
                {
                    isom_clap_t *clap = (isom_clap_t *)ext->form.box;
                    summary->clap.width.n             = clap->cleanApertureWidthN;
                    summary->clap.width.d             = clap->cleanApertureWidthD;
                    summary->clap.height.n            = clap->cleanApertureHeightN;
                    summary->clap.height.d            = clap->cleanApertureHeightD;
                    summary->clap.horizontal_offset.n = clap->horizOffN;
                    summary->clap.horizontal_offset.d = clap->horizOffD;
                    summary->clap.vertical_offset.n   = clap->vertOffN;
                    summary->clap.vertical_offset.d   = clap->vertOffD;
                    continue;
                }
                case ISOM_BOX_TYPE_PASP :
                {
                    isom_pasp_t *pasp = (isom_pasp_t *)ext->form.box;
                    summary->par_h = pasp->hSpacing;
                    summary->par_v = pasp->vSpacing;
                    continue;
                }
                case QT_BOX_TYPE_COLR :
                {
                    isom_colr_t *colr = (isom_colr_t *)ext->form.box;
                    summary->color.primaries_index = colr->primaries_index;
                    summary->color.transfer_index  = colr->transfer_function_index;
                    summary->color.matrix_index    = colr->matrix_index;
                    continue;
                }
                case ISOM_BOX_TYPE_STSL :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_SAMPLE_SCALE,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_stsl_t *stsl = (isom_stsl_t *)ext->form.box;
                    lsmash_isom_sample_scale_t *data = (lsmash_isom_sample_scale_t *)specific->data.structured;
                    data->constraint_flag  = stsl->constraint_flag;
                    data->scale_method     = stsl->scale_method;
                    data->display_center_x = stsl->display_center_x;
                    data->display_center_y = stsl->display_center_y;
                    break;
                }
                case ISOM_BOX_TYPE_BTRT :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_btrt_t *btrt = (isom_btrt_t *)ext->form.box;
                    lsmash_h264_bitrate_t *data = (lsmash_h264_bitrate_t *)specific->data.structured;
                    data->bufferSizeDB = btrt->bufferSizeDB;
                    data->maxBitrate   = btrt->maxBitrate;
                    data->avgBitrate   = btrt->avgBitrate;
                    break;
                }
                case QT_BOX_TYPE_FIEL :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_FIELD_INFO,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_fiel_t *fiel = (isom_fiel_t *)ext->form.box;
                    lsmash_qt_field_info_t *data = (lsmash_qt_field_info_t *)specific->data.structured;
                    data->fields = fiel->fields;
                    data->detail = fiel->detail;
                    break;
                }
                case QT_BOX_TYPE_CSPC :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_PIXEL_FORMAT,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_cspc_t *cspc = (isom_cspc_t *)ext->form.box;
                    lsmash_qt_pixel_format_t *data = (lsmash_qt_pixel_format_t *)specific->data.structured;
                    data->pixel_format = cspc->pixel_format;
                    break;
                }
                case QT_BOX_TYPE_SGBT :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_VIDEO_SIGNIFICANT_BITS,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_sgbt_t *sgbt = (isom_sgbt_t *)ext->form.box;
                    lsmash_qt_significant_bits_t *data = (lsmash_qt_significant_bits_t *)specific->data.structured;
                    data->significantBits = sgbt->significantBits;
                    break;
                }
                case QT_BOX_TYPE_GLBL :
                {
                    specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_CODEC_GLOBAL_HEADER,
                                                                  LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_glbl_t *glbl = (isom_glbl_t *)ext->form.box;
                    lsmash_codec_global_header_t *data = (lsmash_codec_global_header_t *)specific->data.structured;
                    data->header_size = glbl->header_size;
                    data->header_data = lsmash_memdup( glbl->header_data, glbl->header_size );
                    if( !data->header_data )
                    {
                        lsmash_destroy_codec_specific_data( specific );
                        goto fail;
                    }
                    break;
                }
                default :
                    continue;
            }
            if( lsmash_add_entry( &summary->opaque->list, specific ) )
            {
                lsmash_destroy_codec_specific_data( specific );
                goto fail;
            }
        }
        else
        {
            if( ext->size < ISOM_BASEBOX_COMMON_SIZE )
                continue;
            uint8_t *data = ext->form.binary;
            uint32_t box_type = LSMASH_4CC( data[4], data[5], data[6], data[7] );
            lsmash_codec_specific_data_type type = isom_get_codec_specific_data_type( box_type );
            lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( type, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
            if( !specific )
                goto fail;
            specific->size              = ext->size;
            specific->data.unstructured = lsmash_memdup( ext->form.binary, ext->size );
            if( !specific->data.unstructured
             || lsmash_add_entry( &summary->opaque->list, specific ) )
            {
                lsmash_destroy_codec_specific_data( specific );
                goto fail;
            }
        }
    }
    return summary;
fail:
    lsmash_cleanup_summary( (lsmash_summary_t *)summary );
    return NULL;
}

lsmash_audio_summary_t *isom_create_audio_summary_from_description( isom_audio_entry_t *audio )
{
    if( !audio || !audio->root )
        return NULL;
    lsmash_audio_summary_t *summary = (lsmash_audio_summary_t *)lsmash_create_summary( LSMASH_SUMMARY_TYPE_AUDIO );
    if( !summary )
        return NULL;
    summary->sample_type = audio->type;
    summary->sample_size = audio->samplesize;
    summary->channels    = audio->channelcount;
    summary->frequency   = audio->samplerate >> 16;
    if( audio->root->qt_compatible
     && isom_is_qt_audio( audio->type ) )
    {
        if( audio->version == 1 )
        {
            summary->channels         = audio->bytesPerFrame / audio->bytesPerPacket;
            summary->sample_size      = audio->bytesPerPacket * 8;
            summary->samples_in_frame = audio->samplesPerPacket;
        }
        else if( audio->version == 2 )
        {
            summary->frequency        = (union {uint64_t i; double d;}){audio->audioSampleRate}.d;
            summary->channels         = audio->numAudioChannels;
            summary->sample_size      = audio->constBitsPerChannel;
            summary->samples_in_frame = audio->constLPCMFramesPerAudioPacket;
        }
        lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_COMMON,
                                                                               LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
        if( !specific )
            goto fail;
        lsmash_qt_audio_common_t *common = (lsmash_qt_audio_common_t *)specific->data.structured;
        common->revision_level = audio->revision_level;
        common->vendor         = audio->vendor;
        common->compression_ID = audio->compression_ID;
        if( lsmash_add_entry( &summary->opaque->list, specific ) )
        {
            lsmash_destroy_codec_specific_data( specific );
            goto fail;
        }
        if( isom_is_lpcm_audio( audio ) )
        {
            specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS,
                                                          LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
            if( !specific )
                goto fail;
            lsmash_qt_audio_format_specific_flags_t *data = (lsmash_qt_audio_format_specific_flags_t *)specific->data.structured;
            if( audio->version == 2 )
                data->format_flags = audio->formatSpecificFlags;
            else
            {
                data->format_flags = 0;
                /* Here, don't override samplesize.
                 * We should trust samplesize field in the description for misused CODEC indentifier. */
                switch( audio->type )
                {
                    case QT_CODEC_TYPE_FL32_AUDIO :
                    case QT_CODEC_TYPE_FL64_AUDIO :
                        data->format_flags = QT_LPCM_FORMAT_FLAG_FLOAT;
                        break;
                    case QT_CODEC_TYPE_TWOS_AUDIO :
                        data->format_flags = QT_LPCM_FORMAT_FLAG_BIG_ENDIAN | QT_AUDIO_FORMAT_FLAG_SIGNED_INTEGER;
                    case QT_CODEC_TYPE_NONE_AUDIO :
                    case QT_CODEC_TYPE_NOT_SPECIFIED :
                        if( summary->sample_size > 8 )
                            data->format_flags = QT_LPCM_FORMAT_FLAG_BIG_ENDIAN;
                        break;
                    default :
                        break;
                }
            }
            isom_wave_t *wave = (isom_wave_t *)isom_get_extension_box( &audio->extensions, QT_BOX_TYPE_WAVE );
            if( wave && wave->enda && !wave->enda->littleEndian )
                data->format_flags |= QT_LPCM_FORMAT_FLAG_BIG_ENDIAN;
            if( lsmash_add_entry( &summary->opaque->list, specific ) )
            {
                lsmash_destroy_codec_specific_data( specific );
                goto fail;
            }
        }
        else if( audio->type == QT_CODEC_TYPE_ALAC_AUDIO && audio->version == 2 )
            switch( audio->formatSpecificFlags )
            {
                case QT_ALAC_FORMAT_FLAG_16BIT_SOURCE_DATA :
                    summary->sample_size = 16;
                    break;
                case QT_ALAC_FORMAT_FLAG_20BIT_SOURCE_DATA :
                    summary->sample_size = 20;
                    break;
                case QT_ALAC_FORMAT_FLAG_24BIT_SOURCE_DATA :
                    summary->sample_size = 24;
                    break;
                case QT_ALAC_FORMAT_FLAG_32BIT_SOURCE_DATA :
                    summary->sample_size = 32;
                    break;
                default :
                    break;
            }
    }
    for( lsmash_entry_t *entry = audio->extensions.head; entry; entry = entry->next )
    {
        isom_extension_box_t *ext = (isom_extension_box_t *)entry->data;
        if( !ext )
            continue;
        if( ext->format == EXTENSION_FORMAT_BOX )
        {
            if( !ext->form.box )
                continue;
            switch( ext->type )
            {
                case QT_BOX_TYPE_CHAN :
                {
                    lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT,
                                                                                           LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                    if( !specific )
                        goto fail;
                    isom_chan_t *chan = (isom_chan_t *)ext->form.box;
                    lsmash_qt_audio_channel_layout_t *data = (lsmash_qt_audio_channel_layout_t *)specific->data.structured;
                    data->channelLayoutTag = chan->channelLayoutTag;
                    data->channelBitmap    = chan->channelBitmap;
                    if( lsmash_add_entry( &summary->opaque->list, specific ) )
                    {
                        lsmash_destroy_codec_specific_data( specific );
                        goto fail;
                    }
                    break;
                }
                case ISOM_BOX_TYPE_ESDS :
                {
                    isom_esds_t *esds = (isom_esds_t *)ext->form.box;
                    if( !esds || mp4sys_setup_summary_from_DecoderSpecificInfo( summary, esds->ES ) )
                        goto fail;
                    lsmash_bs_t *bs = lsmash_bs_create( NULL );
                    if( !bs )
                        goto fail;
                    lsmash_bs_put_be32( bs, 0 );
                    lsmash_bs_put_be32( bs, ISOM_BOX_TYPE_ESDS );
                    lsmash_bs_put_be32( bs, 0 );
                    mp4sys_put_ES_Descriptor( bs, esds->ES );
                    uint32_t esds_size;
                    uint8_t *esds_data = lsmash_bs_export_data( bs, &esds_size );
                    lsmash_bs_cleanup( bs );
                    if( !esds_data )
                        goto fail;
                    /* Update box size. */
                    esds_data[0] = ((esds_size) >> 24) & 0xff;
                    esds_data[1] = ((esds_size) >> 16) & 0xff;
                    esds_data[2] = ((esds_size) >>  8) & 0xff;
                    esds_data[3] =  (esds_size)        & 0xff;
                    lsmash_codec_specific_data_type type = isom_get_codec_specific_data_type( ISOM_BOX_TYPE_ESDS );
                    lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( type, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
                    if( !specific )
                    {
                        isom_remove_esds( esds );
                        goto fail;
                    }
                    specific->data.unstructured = esds_data;
                    specific->size              = esds_size;
                    if( lsmash_add_entry( &summary->opaque->list, specific ) )
                    {
                        lsmash_destroy_codec_specific_data( specific );
                        goto fail;
                    }
                    break;
                }
                case QT_BOX_TYPE_WAVE :
                {
                    isom_wave_t *wave = (isom_wave_t *)ext->form.box;
                    lsmash_bs_t *bs = lsmash_bs_create( NULL );
                    if( !bs )
                        goto fail;
                    lsmash_bs_put_be32( bs, wave->size );
                    lsmash_bs_put_be32( bs, QT_BOX_TYPE_WAVE );
                    /* Format Box */
                    lsmash_bs_put_be32( bs, ISOM_BASEBOX_COMMON_SIZE + 4 );
                    lsmash_bs_put_be32( bs, QT_BOX_TYPE_FRMA );
                    lsmash_bs_put_be32( bs, audio->type );
                    if( audio->type == ISOM_CODEC_TYPE_MP4A_AUDIO )
                    {
                        /* MPEG-4 Audio Box */
                        lsmash_bs_put_be32( bs, ISOM_BASEBOX_COMMON_SIZE + 4 );
                        lsmash_bs_put_be32( bs, QT_BOX_TYPE_MP4A );
                        lsmash_bs_put_be32( bs, 0 );
                    }
                    for( lsmash_entry_t *wave_entry = wave->extensions.head; wave_entry; wave_entry = wave_entry->next )
                    {
                        isom_extension_box_t *wave_ext = (isom_extension_box_t *)wave_entry->data;
                        if( !wave_ext )
                            continue;
                        if( wave_ext->format == EXTENSION_FORMAT_BOX )
                        {
                            if( !wave_ext->form.box || wave_ext->type != ISOM_BOX_TYPE_ESDS )
                                continue;
                            isom_esds_t *esds = (isom_esds_t *)wave_ext->form.box;
                            if( !esds || mp4sys_setup_summary_from_DecoderSpecificInfo( summary, esds->ES ) )
                            {
                                lsmash_bs_cleanup( bs );
                                goto fail;
                            }
                            uint64_t esds_pos = bs->store;
                            lsmash_bs_put_be32( bs, 0 );
                            lsmash_bs_put_be32( bs, ISOM_BOX_TYPE_ESDS );
                            lsmash_bs_put_be32( bs, 0 );
                            if( mp4sys_put_ES_Descriptor( bs, esds->ES ) )
                            {
                                lsmash_bs_cleanup( bs );
                                goto fail;
                            }
                            uint64_t esds_size = bs->store - esds_pos;
                            /* Update box size. */
                            bs->data[esds_pos    ] = ((esds_size) >> 24) & 0xff;
                            bs->data[esds_pos + 1] = ((esds_size) >> 16) & 0xff;
                            bs->data[esds_pos + 2] = ((esds_size) >>  8) & 0xff;
                            bs->data[esds_pos + 3] =  (esds_size)        & 0xff;
                        }
                        else
                        {
                            if( wave_ext->size < ISOM_BASEBOX_COMMON_SIZE )
                                continue;
                            uint8_t *box_data = wave_ext->form.binary;
                            uint32_t box_type = LSMASH_4CC( box_data[4], box_data[5], box_data[6], box_data[7] );
                            if( box_type == QT_BOX_TYPE_FRMA
                             || box_type == QT_BOX_TYPE_MP4A
                             || box_type == QT_BOX_TYPE_TERMINATOR )
                                continue;
                            lsmash_bs_put_bytes( bs, wave_ext->size, wave_ext->form.binary );
                            if( bs->error )
                            {
                                lsmash_bs_cleanup( bs );
                                goto fail;
                            }
                        }
                    }
                    /* Terminator Box */
                    lsmash_bs_put_be32( bs, ISOM_BASEBOX_COMMON_SIZE );
                    lsmash_bs_put_be32( bs, QT_BOX_TYPE_TERMINATOR );
                    uint32_t wave_size;
                    uint8_t *wave_data = lsmash_bs_export_data( bs, &wave_size );
                    lsmash_bs_cleanup( bs );
                    if( !wave_data )
                        goto fail;
                    /* Update box size of 'wave' extension. */
                    wave_data[0] = ((wave_size) >> 24) & 0xff;
                    wave_data[1] = ((wave_size) >> 16) & 0xff;
                    wave_data[2] = ((wave_size) >>  8) & 0xff;
                    wave_data[3] =  (wave_size)        & 0xff;
                    /* Append as an unstructured CODEC specific info. */
                    lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN,
                                                                                           LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
                    if( !specific )
                        goto fail;
                    specific->data.unstructured = wave_data;
                    specific->size              = wave_size;
                    if( lsmash_add_entry( &summary->opaque->list, specific ) )
                    {
                        lsmash_destroy_codec_specific_data( specific );
                        goto fail;
                    }
                    break;
                }
                default :
                    break;
            }
        }
        else
        {
            if( ext->size < ISOM_BASEBOX_COMMON_SIZE )
                continue;
            uint8_t *data = ext->form.binary;
            uint32_t box_type = LSMASH_4CC( data[4], data[5], data[6], data[7] );
            lsmash_codec_specific_data_type type = isom_get_codec_specific_data_type( box_type );
            lsmash_codec_specific_t *specific = lsmash_create_codec_specific_data( type, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED );
            if( !specific )
                goto fail;
            specific->size              = ext->size;
            specific->data.unstructured = lsmash_memdup( ext->form.binary, ext->size );
            if( !specific->data.unstructured
             || lsmash_add_entry( &summary->opaque->list, specific ) )
            {
                lsmash_destroy_codec_specific_data( specific );
                goto fail;
            }
            if( specific->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_AUDIO_DTS )
            {
                specific = lsmash_convert_codec_specific_format( specific, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED );
                if( !specific )
                    goto fail;
                lsmash_dts_specific_parameters_t *dts = (lsmash_dts_specific_parameters_t *)specific->data.structured;
                summary->frequency        = dts->DTSSamplingFrequency;
                summary->sample_size      = dts->pcmSampleDepth;
                summary->samples_in_frame = 512 << dts->FrameDuration;
                if( dts->LBRDurationMod )
                    summary->samples_in_frame = (summary->samples_in_frame * 3) / 2;
                lsmash_destroy_codec_specific_data( specific );
            }
        }
    }
    return summary;
fail:
    lsmash_cleanup_summary( (lsmash_summary_t *)summary );
    return NULL;
}

lsmash_codec_specific_t *lsmash_get_codec_specific_data( lsmash_summary_t *summary, uint32_t extension_number )
{
    if( !summary || !summary->opaque )
        return NULL;
    uint32_t i = 0;
    for( lsmash_entry_t *entry = summary->opaque->list.head; entry; entry = entry->next )
        if( ++i == extension_number )
            return (lsmash_codec_specific_t *)entry->data;
    return NULL;
}

uint32_t lsmash_count_codec_specific_data( lsmash_summary_t *summary )
{
    if( !summary || !summary->opaque )
        return 0;
    return summary->opaque->list.entry_count;
}