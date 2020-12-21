/*******************************************************************************
 * This file is part of the ARK Ledger App.
 *
 * Copyright (c) ARK Ecosystem <info@ark.io>
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#include "transactions/deserializer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "platform.h"

#include "transactions/transaction.h"

#include "transactions/offsets.h"
#include "transactions/types/types.h"

#include "utils/str.h"
#include "utils/unpack.h"
#include "utils/utils.h"

#include "transactions/ux/display_ux.h"

////////////////////////////////////////////////////////////////////////////////
Transaction transaction;

////////////////////////////////////////////////////////////////////////////////
// Deserialize a v2 VendorField.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t *size:             of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
static bool deserializeVendorField(Transaction *transaction,
                                   const uint8_t *buffer,
                                   size_t size) {
    if (size < buffer[0] || buffer[0] > VENDORFIELD_V2_MAX_LEN) {
        return false;
    }

    transaction->vendorFieldLength = buffer[0];     // 1 Byte

    if (transaction->vendorFieldLength > 0U &&
        IsPrintableAscii((const char*)&buffer[1],
                         transaction->vendorFieldLength,
                         false) == false) {
        return false;
    }
    
    // 0 <=> 255 Bytes
    transaction->vendorField = (uint8_t*)&buffer[1];

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize a v1 VendorField.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t *size:             of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
static bool deserializeVendorFieldV1(Transaction *transaction,
                                     const uint8_t *buffer,
                                     size_t size) {
    if (size < buffer[0] || buffer[0] > VENDORFIELD_V1_MAX_LEN) {
        return false;
    }

    transaction->vendorFieldLength = buffer[0];     // 1 Byte

    if (transaction->vendorFieldLength > 0U &&
        IsPrintableAscii((const char*)&buffer[1],
                         transaction->vendorFieldLength,
                         false) == false) {
        return false;
    }

    // 0 <=> 64 Bytes
    transaction->vendorField = (uint8_t*)&buffer[1];

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize v2 Transaction common header values.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t *size:             of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
static bool deserializeCommon(Transaction *transaction,
                              const uint8_t *buffer,
                              size_t size) {
    if (size < FEE_OFFSET + sizeof(uint64_t)) {
        return false;
    }

    transaction->header     = buffer[HEADER_OFFSET];        // 1 Byte
    transaction->version    = buffer[VERSION_OFFSET];       // 1 Byte
    transaction->network    = buffer[NETWORK_OFFSET];       // 1 Byte
    transaction->type       = U2LE(buffer, TYPE_OFFSET);    // 2 Bytes

    MEMCOPY(transaction->senderPublicKey,                   // 33 Bytes
            &buffer[SENDER_PUBLICKEY_OFFSET],
            PUBLICKEY_COMPRESSED_LEN);

    transaction->fee        = U8LE(buffer, FEE_OFFSET);     // 8 Bytes

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize v1 Transaction common header values.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t *size:             of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
static bool deserializeCommonV1(Transaction *transaction,
                                const uint8_t *buffer,
                                size_t size) {
    if (size < FEE_OFFSET_V1 + sizeof(uint64_t)) { return false; }

    transaction->header     = buffer[HEADER_OFFSET];        // 1 Byte
    transaction->version    = buffer[VERSION_OFFSET];       // 1 Byte
    transaction->network    = buffer[NETWORK_OFFSET];       // 1 Byte
    transaction->type       = buffer[TYPE_OFFSET_V1];       // 1 Byte

    MEMCOPY(transaction->senderPublicKey,                   // 33 Bytes
            &buffer[SENDER_PUBLICKEY_OFFSET_V1],
            PUBLICKEY_COMPRESSED_LEN);

    transaction->fee        = U8LE(buffer, FEE_OFFSET_V1);  // 8 Bytes

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize v2 and v1 Core Transaction Assets.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction[asset offset].
// @param size_t size:              of the current buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
// Supported:
//
// - case TRANSFER
// - case VOTE
// - case IPFS
// - case HTLC_LOCK
// - case HTLC_CLAIM
// - case HTLC_REFUND
//
// ---
static bool deserializeCoreAsset(Transaction *transaction,
                                 const uint8_t *buffer,
                                 size_t size) {
    switch (transaction->type) {
        // Transfer
        case TRANSFER_TYPE:
            return deserializeTransfer(
                    &transaction->asset.transfer, buffer, size);

        // Vote
        case VOTE_TYPE:
            return deserializeVote(
                    &transaction->asset.vote, buffer, size);

        // case MULTI_SIGNATURE_TYPE:

        // Ipfs
        case IPFS_TYPE:
            return deserializeIpfs(
                    &transaction->asset.ipfs, buffer, size);

        // Htlc Lock
        case HTLC_LOCK_TYPE:
            return deserializeHtlcLock(
                    &transaction->asset.htlcLock, buffer, size);

        // Htlc Claim
        case HTLC_CLAIM_TYPE:
            return deserializeHtlcClaim(
                    &transaction->asset.htlcClaim, buffer, size);

        // Htlc Refund
        case HTLC_REFUND_TYPE:
            return deserializeHtlcRefund(
                    &transaction->asset.htlcRefund, buffer, size);

        // Unknown Transaction Type
        default: return false;
    };
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize v2 and v1 Transaction Headers.
//
// @param Transaction *transaction: transaction object ptr.
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t size:              of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
static size_t deserializeHeader(Transaction *transaction,
                                const uint8_t *buffer,
                                size_t size) {
    switch (buffer[VERSION_OFFSET]) {
        // v2
        case TRANSACTION_VERSION_TYPE_2:
            if (deserializeCommon(transaction, buffer, size) == false ||
                deserializeVendorField(transaction,
                                       &buffer[VF_LEN_OFFSET],
                                       size - VF_LEN_OFFSET) == false) {
                return 0U;
            }

            return VF_OFFSET + transaction->vendorFieldLength;

        // v1
        case TRANSACTION_VERSION_TYPE_1:
            if (deserializeCommonV1(transaction, buffer, size) == false ||
                deserializeVendorFieldV1(transaction,
                                         &buffer[VF_LEN_OFFSET_V1],
                                         size - VF_LEN_OFFSET_V1) == false) {
                return 0U;
            }

            return VF_OFFSET_V1 + transaction->vendorFieldLength;

        default: return 0U;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Deserialize v2 and v1 Transactions
//
// @param const uint8_t *buffer:    of the serialized transaction.
// @param size_t size:              of the buffer.
//
// @return bool: true if deserialization was successful.
//
// ---
bool deserialize(const uint8_t *buffer, size_t size) {
    MEMSET_TYPE_BZERO(&transaction, Transaction);

    const size_t cursor = deserializeHeader(&transaction, buffer, size);

    if (cursor == 0U ||
        deserializeCoreAsset(&transaction,
                             &buffer[cursor],
                             size - cursor) == false) {
        MEMSET_TYPE_BZERO(&transaction, Transaction);
        return false;
    }

    SetUx(&transaction);

    return true;
}
