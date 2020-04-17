// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef NCNN_DATAREADER_H
#define NCNN_DATAREADER_H

#include <stdio.h>
#include "platform.h"

#ifdef __cplusplus 
extern "C" {
#endif

// data read wrapper
struct DataReader
{
#if NCNN_STRING
    // parse plain param text
    // return 1 if scan success
    int (*scan)(void *handle, const char* format, void* p);
#endif // NCNN_STRING

    // read binary param and model data
    // return bytes read
    size_t (*read)(void *handle, void* buf, size_t size);
};

#if NCNN_STDIO
#if NCNN_STRING
int DataReaderFromStdio_scan(void *handle, const char* format, void* p);
#endif // NCNN_STRING

size_t DataReaderFromStdio_read(void *handle, void* buf, size_t size);
#endif // NCNN_STDIO

#if NCNN_STRING
int DataReaderFromMemory_scan(void *handle, const char* format, void* p);
#endif // NCNN_STRING

size_t DataReaderFromMemory_read(void *handle, void* buf, size_t size);

#ifdef __cplusplus 
}
#endif

#endif // NCNN_DATAREADER_H
