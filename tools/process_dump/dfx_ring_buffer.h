/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This files contains process dump ring buffer module. */

#ifndef DFX_RING_BUFFER_H
#define DFX_RING_BUFFER_H

#include "dfx_ring_buffer_block.h"

/**
 * @brief            A ring buffer is a FIFO structure that can be used to
 *                     spool data between devices.
 *
 *                     There is a Skip() function that allows the client to
 *                     control when the read cursor is changed. This is so the
 *                     client can perform an action after Read() without the
 *                     write cursor overwriting data while the read block is used.
 *
 *                     For e.g with the sequence of events:
 *                         1.    Read(1000, false)
 *                         2.    Busy writing to sd card for 5 seconds
 *                         3.    Skip()
 *
 *                     Because the skip isn't called until the writing
 *                     has finished, another thread can .Append() without
 *                     corrupting the data being written.
 *
 *
 * @attention        The ring buffer can only contain Length-1 number of entries,
 *                   because the last index is reserved for overrun checks.
 *
 * @tparam Length    The length of the backing store array.
 * @tparam T         The type of data stored.
 */
template<unsigned int LENGTH, class T>
class DfxRingBuffer {
public:
    DfxRingBuffer() : readPosition(0), writePosition(0), data{{T()}}, overrunFlag(false)
    {
    }

    ~DfxRingBuffer()
    {
    }

    /**
     * @brief     Appends a value the end of the
     *            buffer.
     */
    void Append(T& value)
    {
        /*
         * If the next position is where the read cursor
         * is then we have a full buffer.
         */
        bool bufferFull;

        bufferFull = ((writePosition + 1U) % LENGTH) == readPosition;

        if (bufferFull) {
            /*
             * Tried to append a value while the buffer is full.
             */
            overrunFlag = true;
        } else {
            /*
             * Buffer isn't full yet, write to the curr write position
             * and increment it by 1.
             */
            overrunFlag         = false;
            data[writePosition] = value;
            writePosition       = (writePosition + 1U) % LENGTH;
        }
    }

    /**
     * @brief                        Retrieve a continuous block of
     *                               valid buffered data.
     * @param numReadsRequested    How many reads are required.
     * @return                       A block of items containing the maximum
     *                               number the buffer can provide at this time.
     */
    DfxRingBufferBlock<T> Read(unsigned int numReadsRequested)
    {
        bool bridgesZero;
        DfxRingBufferBlock<T> block;

        /*
          * Make sure the number of reads does not bridge the 0 index.
          * This is because we can only provide 1 contiguous block at
          * a time.
          */
        bridgesZero = (readPosition > writePosition);

        if (bridgesZero) {
            unsigned int readsToEnd;
            bool reqSurpassesBufferEnd;

            readsToEnd = LENGTH - readPosition;
            reqSurpassesBufferEnd = numReadsRequested > readsToEnd;

            if (reqSurpassesBufferEnd) {
                /*
                 * If the block requested exceeds the buffer end. Then
                 * return a block that reaches the end and no more.
                 */
                block.SetStart(&(data[readPosition]));
                block.SetLength(readsToEnd);
            } else {
                /*
                 * If the block requested does not exceed 0
                 * then return a block that reaches the number of reads required.
                 */
                block.SetStart(&(data[readPosition]));
                block.SetLength(numReadsRequested);
            }
        } else {
            /*
             * If the block doesn't bridge the zero then
             * return the maximum number of reads to the write
             * cursor.
             */
            unsigned int maxNumReads;
            unsigned int numReadsToWritePosition;

            numReadsToWritePosition = (writePosition - readPosition);

            if (numReadsRequested > numReadsToWritePosition) {
                /*
                 * If the block length requested exceeds the
                 * number of items available, then restrict
                 * the block length to the distance to the write position.
                 */
                maxNumReads = numReadsToWritePosition;
            } else {
                /*
                 * If the block length requested does not exceed the
                 * number of items available then the entire
                 * block is valid.
                 */
                maxNumReads = numReadsRequested;
            }

            block.SetStart(&(data[readPosition]));
            block.SetLength(maxNumReads);
        }

        return block;
    }

    /**
     * @brief    Advances the read position.
     *
     */
    void Skip(unsigned int numReads)
    {
        readPosition = (readPosition + numReads) % LENGTH;
    }

    bool Overrun()
    {
        return overrunFlag;
    }

    /**
     * @brief    The total size of the ring buffer including the full position.
     *
     */
    unsigned int Length()
    {
        return LENGTH;
    }

    /**
     * @brief    Returns the number of reads available.
     *
     */
    unsigned int Available()
    {
        bool bridgesZero;
        unsigned availableReads;

        bridgesZero = readPosition > writePosition;
        availableReads = 0;

        if (bridgesZero) {
            /* Add the number of reads to zero, and number of reads from 0 to the write cursor */
            unsigned int numReadsToZero;
            unsigned int numReadsToWritePosition;

            numReadsToZero = LENGTH - readPosition;
            numReadsToWritePosition = writePosition;

            availableReads = numReadsToZero + numReadsToWritePosition;
        } else {
            /* The number of available reads is between the write position and the read position. */
            availableReads = writePosition - readPosition;
        }

        return availableReads;
    }

private:
    volatile unsigned int readPosition;
    volatile unsigned int writePosition;

    T data[LENGTH] = {T()};

    bool overrunFlag = false;
};

#endif
