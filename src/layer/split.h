// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef LAYER_SPLIT_H
#define LAYER_SPLIT_H

#include "layer.h"

struct Split
{
    // layer base
    Layer layer;
};

void *Split_ctor(void *_self, va_list *args);

int Split_forward_multi(void *_self, const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt);

// default operators
#define Split_dtor                     Layer_dtor
#define Split_load_param               Layer_load_param
#define Split_load_model               Layer_load_model
#define Split_create_pipeline          Layer_create_pipeline
#define Split_destroy_pipeline         Layer_destroy_pipeline
#define Split_forward                  Layer_forward
#define Split_forward_inplace_multi    Layer_forward_inplace_multi
#define Split_forward_inplace          Layer_forward_inplace

#endif // LAYER_SPLIT_H
