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

#ifndef LAYER_BIAS_H
#define LAYER_BIAS_H

#include "layer.h"

struct Bias
{
    // layer base
    Layer layer;

    // proprietary data
    // param
    int bias_data_size;

    // model
    Mat bias_data;
};

void *Bias_ctor(void *_self, va_list *args);

int Bias_load_param(void *_self, const ParamDict& pd);

int Bias_load_model(void *_self, const ModelBin& mb);

int Bias_forward_inplace(void *_self, Mat& bottom_top_blob, const Option& opt);

// default operators
#define Bias_dtor                     Layer_dtor
#define Bias_create_pipeline          Layer_create_pipeline
#define Bias_destroy_pipeline         Layer_destroy_pipeline
#define Bias_forward_multi            Layer_forward_multi
#define Bias_forward                  Layer_forward
#define Bias_forward_inplace_multi    Layer_forward_inplace_multi

#endif // LAYER_BIAS_H
