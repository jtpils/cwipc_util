
cmake_minimum_required(VERSION 3.10.0)

add_executable(cwipc_ply2dump cwipc_ply2dump.cpp)
include_directories(
	"../../include"
	${PCL_INCLUDE_DIRS}
)
target_link_libraries(cwipc_ply2dump cwipc_util ${PCL_LIBRARIES})

install(TARGETS cwipc_ply2dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_test(NAME cwipc_ply2dump COMMAND cwipc_ply2dump ${CMAKE_SOURCE_DIR}/tests/fixtures/input/pcl_frame1.ply ./pcl_frame1.cwipcdump )
