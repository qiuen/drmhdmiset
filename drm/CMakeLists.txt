# vim: syntax=cmake
include_directories(.)

#HDMI_SET="libhdmiset"

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
# h264 decoder api

# h264 decoder header
set(HDMI_SET_H
	drmconnector.h
	drmcrtc.h
	drmencoder.h
	drmmode.h
	drmproperty.h
	drmresources.h
	drmeventlistener.h
	autofd.h
	worker.h
	drmplane.h
	tinyxml2.h
	config_drm.h
        hdmiset.h
    )

# h264 decoder sourse
set(HDMI_SET_SRC
	drmconnector.cpp
	drmcrtc.cpp
	drmencoder.cpp
	drmmode.cpp
	drmproperty.cpp
	drmresources.cpp 
	drmeventlistener.cpp 
	worker.cpp
	tinyxml2.cpp
        drmplane.cpp
	hdmiset.cpp
    )


include_directories("${PROJECT_SOURCE_DIR}/../../../..//buildroot/output/host/usr/aarch64-rockchip-linux-gnu/sysroot/usr/include
" "${PROJECT_SOURCE_DIR}/../../../../buildroot/output/host/usr/aarch64-rockchip-linux-gnu/sysroot/usr/lib/glib-2.0/include" "${PROJECT_SOURCE_DIR}/../../../../buildroot/output/host/usr/aarch64-rockchip-linux-gnu/sysroot/usr/include/libdrm")

link_directories("${PROJECT_SOURCE_DIR}/../../../../buildroot/output/host/usr/aarch64-rockchip-linux-gnu/sysroot/usr/lib")

link_libraries("libdrm.so")

link_libraries("libpthread.so")

add_library(hdmiset SHARED
    ${HDMI_SET_H}
    ${HDMI_SET_SRC}
    )

ADD_EXECUTABLE(testhdmiset main.cpp)

TARGET_LINK_LIBRARIES(testhdmiset hdmiset)

INSTALL(TARGETS hdmiset
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)


INSTALL(TARGETS testhdmiset RUNTIME DESTINATION bin)
INSTALL(FILES hdmiset.h DESTINATION include)
#target_link_libraries(${CODEC_H264D} mpp_base)
#set_target_properties(${CODEC_H264D} PROPERTIES FOLDER "mpp/codec")

