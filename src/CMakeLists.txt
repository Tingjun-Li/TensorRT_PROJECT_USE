SET(common_dir /usr/src/tensorrt/samples/common)
INCLUDE_DIRECTORIES(${common_dir})

SET(cuda_dir /usr/local/cuda-10.2/targets/x86_64-linux/include)
INCLUDE_DIRECTORIES(${cuda_dir})

SET(LOG_CPP /usr/src/tensorrt/samples/common/logger.cpp)

ADD_EXECUTABLE(trt_test serialize_engine.cpp ${LOG_CPP})

find_library(LIBNVINFER nvinfer /usr/lib/x86_64-linux-gnu/)
TARGET_LINK_LIBRARIES(trt_test ${LIBNVINFER})

find_library(LIBNVONNXPARSER nvonnxparser /usr/include/x86_64-linux-gnu/)
TARGET_LINK_LIBRARIES(trt_test ${LIBNVONNXPARSER})

find_library(LIBCUDART cudart /usr/local/cuda-10.2/lib64/)
TARGET_LINK_LIBRARIES(trt_test ${LIBCUDART})

