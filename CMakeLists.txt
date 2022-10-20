set(SOURCES
	adefs/adefs.cpp
	adefs/package_fs.cpp
	adefs/package_gcf.cpp
	adefs/package_zip.cpp
)

add_library(adefs STATIC ${SOURCES})
target_include_directories(adefs PUBLIC ${CMAKE_CURRENT_LIST_DIR})
