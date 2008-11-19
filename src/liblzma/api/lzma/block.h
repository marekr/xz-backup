/**
 * \file        lzma/block.h
 * \brief       .xz Block handling
 *
 * \author      Copyright (C) 1999-2006 Igor Pavlov
 * \author      Copyright (C) 2007 Lasse Collin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef LZMA_H_INTERNAL
#	error Never include this file directly. Use <lzma.h> instead.
#endif


/**
 * \brief       Options for the Block Header encoder and decoder
 *
 * Different things use different parts of this structure. Some read
 * some members, other functions write, and some do both. Only the
 * members listed for reading need to be initialized when the specified
 * functions are called. The members marked for writing will be assigned
 * new values at some point either by calling the given function or by
 * later calls to lzma_code().
 */
typedef struct {
	/**
	 * \brief       Size of the Block Header
	 *
	 * Read by:
	 *  - lzma_block_header_encode()
	 *  - lzma_block_header_decode()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_size()
	 */
	uint32_t header_size;
#	define LZMA_BLOCK_HEADER_SIZE_MIN 8
#	define LZMA_BLOCK_HEADER_SIZE_MAX 1024

	/**
	 * \brief       Type of integrity Check
	 *
	 * The type of the integrity Check is not stored into the Block
	 * Header, thus its value must be provided also when decoding.
	 *
	 * Read by:
	 *  - lzma_block_header_encode()
	 *  - lzma_block_header_decode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_check check;

	/**
	 * \brief       Size of the Compressed Data in bytes
	 *
	 * Usually you don't know this value when encoding in streamed mode.
	 * In non-streamed mode you can reserve space for this field when
	 * encoding the Block Header the first time, and then re-encode the
	 * Block Header and copy it over the original one after the encoding
	 * of the Block has been finished.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli compressed_size;

	/**
	 * \brief       Uncompressed Size in bytes
	 *
	 * Encoder: If this value is not LZMA_VLI_UNKNOWN, it is stored
	 * to the Uncompressed Size field in the Block Header. The real
	 * uncompressed size of the data being compressed must match
	 * the Uncompressed Size or LZMA_OPTIONS_ERROR is returned.
	 *
	 * If Uncompressed Size is unknown, End of Payload Marker must
	 * be used. If uncompressed_size == LZMA_VLI_UNKNOWN and
	 * has_eopm == 0, LZMA_OPTIONS_ERROR will be returned.
	 *
	 * Decoder: If this value is not LZMA_VLI_UNKNOWN, it is
	 * compared to the real Uncompressed Size. If they do not match,
	 * LZMA_OPTIONS_ERROR is returned.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 */
	lzma_vli uncompressed_size;

	/**
	 * \brief       Array of filters
	 *
	 * There can be 1-4 filters. The end of the array is marked with
	 * .id = LZMA_VLI_UNKNOWN.
	 *
	 * Read by:
	 *  - lzma_block_header_size()
	 *  - lzma_block_header_encode()
	 *  - lzma_block_encoder()
	 *  - lzma_block_decoder()
	 *
	 * Written by:
	 *  - lzma_block_header_decode(): Note that this does NOT free()
	 *    the old filter options structures. All unused filters[] will
	 *    have .id == LZMA_VLI_UNKNOWN and .options == NULL. If
	 *    decoding fails, all filters[] are guaranteed to be
	 *    LZMA_VLI_UNKNOWN and NULL.
	 *
	 * \note        Because of the array is terminated with
	 *              .id = LZMA_VLI_UNKNOWN, the actual array must
	 *              have LZMA_FILTERS_MAX + 1 members or the Block
	 *              Header decoder will overflow the buffer.
	 */
	lzma_filter *filters;

} lzma_block;


/**
 * \brief       Decodes the Block Header Size field
 *
 * To decode Block Header using lzma_block_header_decode(), the size of the
 * Block Header has to be known and stored into lzma_block.header_size.
 * The size can be calculated from the first byte of a Block using this macro.
 * Note that if the first byte is 0x00, it indicates beginning of Index; use
 * this macro only when the byte is not 0x00.
 *
 * There is no encoding macro, because Block Header encoder is enough for that.
 */
#define lzma_block_header_size_decode(b) (((uint32_t)(b) + 1) * 4)


/**
 * \brief       Calculates the size of Block Header
 *
 * \return      - LZMA_OK: Size calculated successfully and stored to
 *                options->header_size.
 *              - LZMA_OPTIONS_ERROR: Unsupported filters or filter options.
 *              - LZMA_PROG_ERROR: Invalid options
 *
 * \note        This doesn't check that all the options are valid i.e. this
 *              may return LZMA_OK even if lzma_block_header_encode() or
 *              lzma_block_encoder() would fail.
 */
extern lzma_ret lzma_block_header_size(lzma_block *options)
		lzma_attr_warn_unused_result;


/**
 * \brief       Encodes Block Header
 *
 * Encoding of the Block options is done with a single call instead of
 * first initializing and then doing the actual work with lzma_code().
 *
 * \param       out         Beginning of the output buffer. This must be
 *                          at least options->header_size bytes.
 * \param       options     Block options to be encoded.
 *
 * \return      - LZMA_OK: Encoding was successful. options->header_size
 *                bytes were written to output buffer.
 *              - LZMA_OPTIONS_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_block_header_encode(
		const lzma_block *options, uint8_t *out)
		lzma_attr_warn_unused_result;


/**
 * \brief       Decodes Block Header
 *
 * Decoding of the Block options is done with a single call instead of
 * first initializing and then doing the actual work with lzma_code().
 *
 * \param       options     Destination for block options
 * \param       allocator   lzma_allocator for custom allocator functions.
 *                          Set to NULL to use malloc().
 * \param       in          Beginning of the input buffer. This must be
 *                          at least options->header_size bytes.
 *
 * \return      - LZMA_OK: Decoding was successful. options->header_size
 *                bytes were written to output buffer.
 *              - LZMA_OPTIONS_ERROR: Invalid or unsupported options.
 *              - LZMA_PROG_ERROR
 */
extern lzma_ret lzma_block_header_decode(lzma_block *options,
		lzma_allocator *allocator, const uint8_t *in)
		lzma_attr_warn_unused_result;


/**
 * \brief       Sets Compressed Size according to Unpadded Size
 *
 * Block Header stores Compressed Size, but Index has Unpadded Size. If the
 * application has already parsed the Index and is now decoding Blocks,
 * it can calculate Compressed Size from Unpadded Size. This function does
 * exactly that with error checking, so application doesn't need to check,
 * for example, if the value in Index is too small to contain even the
 * Block Header. Note that you need to call this function _after_ decoding
 * the Block Header field.
 *
 * \return      - LZMA_OK: options->compressed_size was set successfully.
 *              - LZMA_DATA_ERROR: unpadded_size is too small compared to
 *                options->header_size and lzma_check_sizes[options->check].
 *              - LZMA_PROG_ERROR: Some values are invalid. For example,
 *                options->header_size must be a multiple of four, and
 *                options->header_size between 8 and 1024 inclusive.
 */
extern lzma_ret lzma_block_compressed_size(
		lzma_block *options, lzma_vli unpadded_size)
		lzma_attr_warn_unused_result;


/**
 * \brief       Calculates Unpadded Size
 *
 * This function can be useful after decoding a Block to get Unpadded Size
 * that is stored in Index.
 *
 * \return      Unpadded Size on success, or zero on error.
 */
extern lzma_vli lzma_block_unpadded_size(const lzma_block *options)
		lzma_attr_pure;


/**
 * \brief       Calculates the total encoded size of a Block
 *
 * This is equivalent to lzma_block_unpadded_size() except that the returned
 * value includes the size of the Block Padding field.
 *
 * \return      On success, total encoded size of the Block. On error,
 *              zero is returned.
 */
extern lzma_vli lzma_block_total_size(const lzma_block *options)
		lzma_attr_pure;


/**
 * \brief       Initializes .lzma Block encoder
 *
 * This function is required for multi-thread encoding. It may also be
 * useful when implementing custom file formats.
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_OPTIONS_ERROR
 *              - LZMA_UNSUPPORTED_CHECK: options->check specfies a Check
 *                that is not supported by this buid of liblzma. Initializing
 *                the encoder failed.
 *              - LZMA_PROG_ERROR
 *
 * lzma_code() can return FIXME
 */
extern lzma_ret lzma_block_encoder(lzma_stream *strm, lzma_block *options)
		lzma_attr_warn_unused_result;


/**
 * \brief       Initializes decoder for .lzma Block
 *
 * \return      - LZMA_OK: All good, continue with lzma_code().
 *              - LZMA_UNSUPPORTED_CHECK: Initialization was successful, but
 *                the given Check type is not supported, thus Check will be
 *                ignored.
 *              - LZMA_PROG_ERROR
 *              - LZMA_MEM_ERROR
 */
extern lzma_ret lzma_block_decoder(lzma_stream *strm, lzma_block *options)
		lzma_attr_warn_unused_result;
