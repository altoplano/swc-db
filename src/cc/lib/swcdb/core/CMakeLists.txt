#
# Copyright (C) 2019 SWC-DB (author: Kashirin Alex (kashirin.alex@gmail.com))
#





# lib-swcdb_core,  #ifndef swc_core_common_FileName_h
set(swcdb_core_sources
	String.cc
	Error.cc
	Logger.cc
	FileUtils.cc
	Random.cc
)

set(SWC_LIB_CORE_SHARED ${ATOMIC_LIBRARIES_SHARED} ${RE2_LIBRARIES_SHARED} 
												${ZLIB_LIBRARIES_SHARED} ${LZMA_LIBRARIES_SHARED} 
												${BZIP2_LIBRARIES_SHARED} ${SNAPPY_LIBRARIES_SHARED})
set(SWC_LIB_CORE_STATIC ${ATOMIC_LIBRARIES_STATIC} ${RE2_LIBRARIES_STATIC} 
												${ZLIB_LIBRARIES_STATIC} ${LZMA_LIBRARIES_STATIC} 
												${BZIP2_LIBRARIES_STATIC} ${SNAPPY_LIBRARIES_STATIC})

set_property(GLOBAL PROPERTY SWC_LIB_CORE_SHARED ${SWC_LIB_CORE_SHARED})
set_property(GLOBAL PROPERTY SWC_LIB_CORE_STATIC ${SWC_LIB_CORE_STATIC})

ADD_LIB_TARGET(
	NAME 		swcdb_core
	SRCS 		${swcdb_core_sources}
	SHARED  ${SWC_LIB_CORE_SHARED}
	STATIC  ${SWC_LIB_CORE_STATIC}
)


# lib-swcdb_core_config, #ifndef swc_core_config_FileName_h
set(swcdb_core_config_sources
	config/Settings.cc
	# config/System.cc
	# config/SystemInfo.cc
)

ADD_LIB_TARGET(
	NAME 		swcdb_core_config
	SRCS 		${swcdb_core_config_sources}
)

ADD_TEST_TARGET(
	NAME core-serialization
	SRCS tests/sertest.cc
	TARGETS swcdb_core 
)


ADD_TEST_EXEC(
	NAME properties_parser_test
	SRCS tests/properties_parser_test.cc
	TARGETS swcdb_core swcdb_core_config 
)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/properties_parser_test.cfg ${CMAKE_CURRENT_BINARY_DIR}/properties_parser_test.cfg)
add_test(properties_parser_test ${TEST_DIFF}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/properties_parser_test.golden
  ./properties_parser_test argOne --i16 1k --i32 64K --i64 1G --boo true --strs=1 -strs 2 -strs 3 --strs 4 -strs=5 --strs=6 --b=1 --is_true  -config=properties_parser_test.cfg
)
	
ADD_EXEC_TARGET(
	NAME      check_mutex
	SRCS      tests/mutex.cc
	TARGETS swcdb_core
)