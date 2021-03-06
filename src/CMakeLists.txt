
##############################################

configure_file(platform.h.in ${CMAKE_CURRENT_BINARY_DIR}/platform.h)

# Add source file to list, and add to special visual folder
function(ncnn_src_group ncnn_src_string folder)
    string(REPLACE " " ";" _ncnn_src_list ${ncnn_src_string})

    string(REGEX REPLACE "/" "\\\\" _target_folder "${folder}")

    foreach(_file IN LISTS ${_ncnn_src_list}) 
        source_group ("${_target_folder}" FILES "${_file}")
    endforeach ()
endfunction()

set(ncnn_SRCS
    allocator.cpp
    blob.cpp
    cpu.c
    datareader.c
    layer.cpp
    mat.cpp
    mat_pixel.cpp
    mat_pixel_resize.cpp
    mat_pixel_rotate.cpp
    modelbin.cpp
    net.cpp
    option.cpp
    paramdict.cpp
    benchmark.cpp
    cstl/class.c
)

ncnn_src_group(ncnn_SRCS "sources")

macro(ncnn_add_layer class)
    string(TOLOWER ${class} name)

    # WITH_LAYER_xxx option
    if(${ARGC} EQUAL 2)
        option(WITH_LAYER_${name} "build with layer ${name}" ${ARGV1})
    else()
        option(WITH_LAYER_${name} "build with layer ${name}" ON)
    endif()

    if(NCNN_CMAKE_VERBOSE)
        message(STATUS "WITH_LAYER_${name} = ${WITH_LAYER_${name}}")
    endif()

    if(WITH_LAYER_${name})
        list(APPEND ncnn_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/layer/${name}.cpp)

        # look for arch specific implementation and append source
        # optimized implementation for armv7, aarch64 or x86
        # [TODO] disable arm and mips first
        # if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
        #     set(arch arm)
        # elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(mips)")
        #     set(arch mips)
        # endif()

        set(LAYER_ARCH_SRC ${CMAKE_CURRENT_SOURCE_DIR}/layer/${arch}/${name}_${arch}.cpp)
        if(EXISTS ${LAYER_ARCH_SRC})
            set(WITH_LAYER_${name}_${arch} 1)
            list(APPEND ncnn_SRCS ${LAYER_ARCH_SRC})
        endif()
    endif()

    # cpu implemented layer
    if(WITH_LAYER_${name})
        # header include
        set(layer_declaration "${layer_declaration}#include \"layer.h\"\n")
        set(layer_declaration "${layer_declaration}#include \"layer/${name}.h\"\n")
        # ctor and dtor function (call in order)
        set(ctor_content "    { Layer_ctor(_self, args); }\n    { ${class}_ctor(_self, args); }\n")
        set(dtor_content "    { ${class}_dtor(_self); }\n    { Layer_dtor(_self); }\n")
        # pipeline function (call in order)
        set(create_pipeline_content "    { int ret = ${class}_create_pipeline(_self, opt); if (ret) return ret; }\n")
        set(destroy_pipeline_content "    { int ret = ${class}_destroy_pipeline(_self, opt); if (ret) return ret; }\n")
        # forward function override
        set(forward_content "")
        set(forward_content "${forward_content}#define ${class}_final_forward_multi ${class}_forward_multi\n")
        set(forward_content "${forward_content}#define ${class}_final_forward ${class}_forward\n")
        set(forward_content "${forward_content}#define ${class}_final_forward_inplace_multi ${class}_forward_inplace_multi\n")
        set(forward_content "${forward_content}#define ${class}_final_forward_inplace ${class}_forward_inplace\n")
        set(forward_content "${forward_content}#define ${class}_final ${class}\n")

        source_group ("sources\\\\layers" FILES "${CMAKE_CURRENT_SOURCE_DIR}/layer/${name}.cpp")

        # platform optimization
        if(WITH_LAYER_${name}_${arch})
            # header include
            set(layer_declaration "${layer_declaration}#include \"layer/${arch}/${name}_${arch}.h\"\n")
            # ctor and dtor function (call in order)
            set(ctor_content "${ctor_content}    { ${class}_${arch}_ctor(_self, args); }\n")
            set(dtor_content "    { ${class}_${arch}_dtor(_self); }\n${dtor_content}")
            # pipeline function (call in order)
            set(create_pipeline_content "${create_pipeline_content}    { int ret = ${class}_${arch}_create_pipeline(_self, opt); if (ret) return ret; }\n")
            set(destroy_pipeline_content "    { int ret = ${class}_${arch}_destroy_pipeline(_self, opt); if (ret) return ret; }\n${destroy_pipeline_content}")
            # forward function override
            set(forward_content "")
            set(forward_content "${forward_content}#define ${class}_final_forward_multi ${class}_${arch}_forward_multi\n")
            set(forward_content "${forward_content}#define ${class}_final_forward ${class}_${arch}_forward\n")
            set(forward_content "${forward_content}#define ${class}_final_forward_inplace_multi ${class}_${arch}_forward_inplace_multi\n")
            set(forward_content "${forward_content}#define ${class}_final_forward_inplace ${class}_${arch}_forward_inplace\n")
            set(forward_content "${forward_content}#define ${class}_final ${class}_${arch}\n")

            source_group ("sources\\\\layers\\\\${arch}" FILES "${CMAKE_CURRENT_SOURCE_DIR}/layer/${arch}/${name}_${arch}.cpp")
        endif()
    endif()

    # generate layer_declaration and layer_registry file
    if(WITH_LAYER_${name})
        # header include
        set(layer_declaration "${layer_declaration}\n")
        # ctor and dtor function (call in order)
        set(layer_declaration "${layer_declaration}void *${class}_final_ctor(void *_self, va_list *args) {\n${ctor_content}    return _self;\n}\n")
        set(layer_declaration "${layer_declaration}void *${class}_final_dtor(void *_self) {\n${dtor_content}    return _self;\n}\n")
        # pipeline function (call in order)
        set(layer_declaration "${layer_declaration}int ${class}_final_create_pipeline(void *_self, const Option& opt) {\n${create_pipeline_content}    return 0;\n}\n")
        set(layer_declaration "${layer_declaration}int ${class}_final_destroy_pipeline(void *_self, const Option& opt) {\n${destroy_pipeline_content}    return 0;\n}\n")
        # forward functions (override)
        set(layer_declaration "${layer_declaration}${forward_content}")
        set(layer_declaration "${layer_declaration}DEFINE_LAYER_CREATOR(${class})\n\n")
    endif()

    if(WITH_LAYER_${name})
        set(layer_registry "${layer_registry}#if NCNN_STRING\n{\"${class}\",${class}_final_layer_creator},\n#else\n{${class}_final_layer_creator},\n#endif\n")
    else()
        set(layer_registry "${layer_registry}#if NCNN_STRING\n{\"${class}\",0},\n#else\n{0},\n#endif\n")
    endif()

    # generate layer_type_enum file
    set(layer_type_enum "${layer_type_enum}Layer${class} = ${__LAYER_TYPE_ENUM_INDEX},\n")
    math(EXPR __LAYER_TYPE_ENUM_INDEX "${__LAYER_TYPE_ENUM_INDEX}+1")
endmacro()

set(__LAYER_TYPE_ENUM_INDEX 0)

# layer implementation
ncnn_add_layer(AbsVal)
ncnn_add_layer(ArgMax OFF)
ncnn_add_layer(BatchNorm OFF) # TODO
ncnn_add_layer(Bias)
ncnn_add_layer(BNLL OFF) # TODO
ncnn_add_layer(Concat)
ncnn_add_layer(Convolution)
ncnn_add_layer(Crop OFF) # TODO
ncnn_add_layer(Deconvolution OFF) # TODO
ncnn_add_layer(Dropout)
ncnn_add_layer(Eltwise)
ncnn_add_layer(ELU OFF) # TODO
ncnn_add_layer(Embed OFF) # TODO
ncnn_add_layer(Exp OFF) # TODO
ncnn_add_layer(Flatten OFF) # TODO
ncnn_add_layer(InnerProduct)
ncnn_add_layer(Input)
ncnn_add_layer(Log OFF) # TODO
ncnn_add_layer(LRN OFF) # TODO
ncnn_add_layer(MemoryData OFF) # TODO
ncnn_add_layer(MVN OFF) # TODO
ncnn_add_layer(Pooling)
ncnn_add_layer(Power OFF) # TODO
ncnn_add_layer(PReLU OFF) # TODO
ncnn_add_layer(Proposal OFF) # TODO
ncnn_add_layer(Reduction OFF) # TODO
ncnn_add_layer(ReLU)
ncnn_add_layer(Reshape)
ncnn_add_layer(ROIPooling OFF) # TODO
ncnn_add_layer(Scale OFF) # TODO
ncnn_add_layer(Sigmoid OFF) # TODO
ncnn_add_layer(Slice)
ncnn_add_layer(Softmax)
ncnn_add_layer(Split)
ncnn_add_layer(SPP OFF)
ncnn_add_layer(TanH OFF) # TODO
ncnn_add_layer(Threshold OFF) # TODO
ncnn_add_layer(Tile OFF)
ncnn_add_layer(RNN OFF)
ncnn_add_layer(LSTM OFF) # TODO
ncnn_add_layer(BinaryOp)
ncnn_add_layer(UnaryOp OFF) # TODO
ncnn_add_layer(ConvolutionDepthWise)
ncnn_add_layer(Padding)
ncnn_add_layer(Squeeze OFF) # TODO
ncnn_add_layer(ExpandDims OFF) # TODO
ncnn_add_layer(Normalize OFF) # TODO
ncnn_add_layer(Permute OFF) # TODO
ncnn_add_layer(PriorBox OFF) # TODO
ncnn_add_layer(DetectionOutput OFF) # TODO
ncnn_add_layer(Interp OFF) # TODO
ncnn_add_layer(DeconvolutionDepthWise OFF) # TODO
ncnn_add_layer(ShuffleChannel)
ncnn_add_layer(InstanceNorm OFF) # TODO
ncnn_add_layer(Clip OFF) # TODO
ncnn_add_layer(Reorg OFF) # TODO
ncnn_add_layer(YoloDetectionOutput OFF) # TODO
ncnn_add_layer(Quantize)
ncnn_add_layer(Dequantize OFF) # TODO
ncnn_add_layer(Yolov3DetectionOutput OFF) # TODO
ncnn_add_layer(PSROIPooling OFF) # TODO
ncnn_add_layer(ROIAlign OFF)
ncnn_add_layer(Packing)
ncnn_add_layer(Requantize OFF) # TODO
ncnn_add_layer(Cast OFF) # TODO
ncnn_add_layer(HardSigmoid)
ncnn_add_layer(SELU OFF) # TODO
ncnn_add_layer(HardSwish)
ncnn_add_layer(Noop OFF) # TODO
ncnn_add_layer(PixelShuffle OFF) # TODO
ncnn_add_layer(DeepCopy OFF) # TODO

# create new
configure_file(layer_declaration.h.in ${CMAKE_CURRENT_BINARY_DIR}/layer_declaration.h)
configure_file(layer_registry.h.in ${CMAKE_CURRENT_BINARY_DIR}/layer_registry.h)
configure_file(layer_type_enum.h.in ${CMAKE_CURRENT_BINARY_DIR}/layer_type_enum.h)

add_library(ncnn STATIC ${ncnn_SRCS})

target_include_directories(ncnn
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/ncnn>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/layer>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cstl>)

if(NCNN_OPENMP)
    find_package(OpenMP)
    if(NOT TARGET OpenMP::OpenMP_CXX AND (OpenMP_CXX_FOUND OR OPENMP_FOUND))
        target_compile_options(ncnn PRIVATE ${OpenMP_CXX_FLAGS})
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC"
    OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID MATCHES "MSVC"))
    target_compile_definitions(ncnn
        PRIVATE _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE)
else()
    target_compile_options(ncnn
        PRIVATE -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter)
    if(NOT NCNN_DISABLE_PIC)
        set_target_properties(ncnn
            PROPERTIES
                POSITION_INDEPENDENT_CODE ON
                INTERFACE_POSITION_INDEPENDENT_CODE ON)
    endif()

    if(CMAKE_BUILD_TYPE MATCHES "(Release|RELEASE|release)")
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
            target_compile_options(ncnn PRIVATE -Ofast)
        endif()

        target_compile_options(ncnn PRIVATE -ffast-math -fpermissive)
    endif()

    if(PI3)
        target_compile_options(ncnn
            PRIVATE -march=native -mfpu=neon -mfloat-abi=hard)
        target_compile_definitions(ncnn
            PRIVATE __ARM_NEON __ANDROID__)
    endif()
    # target_compile_options(ncnn PRIVATE -march=native)
    # set_target_properties(ncnn PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    target_compile_options(ncnn PRIVATE -fvisibility=hidden)
endif()

if(ANDROID)
    # disable shared library on android
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
endif()

if(NCNN_OPENMP AND (OpenMP_CXX_FOUND OR OPENMP_FOUND))
    if(NCNN_CMAKE_VERBOSE)
        message("Building with OpenMP")
    endif()
    if(OpenMP_CXX_FOUND)
        target_link_libraries(ncnn PUBLIC OpenMP::OpenMP_CXX)
    else()
        target_link_libraries(ncnn PRIVATE "${OpenMP_CXX_FLAGS}")
    endif()
endif()

if(NCNN_INSTALL_SDK)
    install(TARGETS ncnn EXPORT ncnn ARCHIVE DESTINATION lib)
    install(FILES
        allocator.h
        blob.h
        cpu.h
        datareader.h
        layer.h
        layer_type.h
        mat.h
        modelbin.h
        net.h
        option.h
        paramdict.h
        benchmark.h
        ${CMAKE_CURRENT_BINARY_DIR}/layer_type_enum.h
        ${CMAKE_CURRENT_BINARY_DIR}/platform.h
        DESTINATION include/ncnn
    )
    install(EXPORT ncnn
        DESTINATION lib/cmake/ncnn)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/../cmake/ncnnConfig.cmake.in
        ncnnConfig.cmake
        @ONLY)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ncnnConfig.cmake
        DESTINATION lib/cmake/ncnn)
endif()

# add ncnn to a virtual project group
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET ncnn PROPERTY FOLDER "libncnn")
