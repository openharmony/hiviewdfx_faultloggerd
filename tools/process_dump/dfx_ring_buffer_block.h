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

#ifndef DFX_RING_BUFFER_BLOCK_H
#define DFX_RING_BUFFER_BLOCK_H

#include <cstddef>

/**
 * @brief        A block represents a continuous section
 *               of the ring buffer.
 * @tparam T     The type of data stored in the ring buffer.
 */
template<class T>
class DfxRingBufferBlock {
public:
    DfxRingBufferBlock() : start_(NULL), length_(0)
    {
    }

    ~DfxRingBufferBlock()
    {
    }

    /**
     * @brief    Sets the block's starting
     *           position to a point in memory.
     */
    void SetStart(T* start)
    {
        this->start_ = start;
    }

    /**
     * @brief    Sets the number of items in the
     *           block.
     */
    void SetLength(unsigned int length)
    {
        this->length_ = length;
    }

    /**
     * @return    The block's starting
     *            point in memory.
     */
    T* Start()
    {
        return this->start_;
    }

    /**
     * @return    The number of items in the block.
     */
    unsigned int Length()
    {
        return this->length_;
    }

    /**
     * @param index        The index of the item in the block.
     * @return             The item in the block at the index.
     */
    T At(unsigned int index)
    {
        return this->start_[index];
    }

    size_t ElementSize()
    {
        return sizeof(T);
    }

private:
    T* start_;

    unsigned int length_;
};

#endif
