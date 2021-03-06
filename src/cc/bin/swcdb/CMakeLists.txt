#
# Copyright Since 2019 SWC-DB© [author: Kashirin Alex kashirin.alex@gmail.com]
#


#add_subdirectory(manager)
#add_subdirectory(ranger)

#add_subdirectory(thriftbroker)

get_property(SWC_LIB_CORE_SHARED GLOBAL PROPERTY SWC_LIB_CORE_SHARED)
get_property(SWC_LIB_CORE_STATIC GLOBAL PROPERTY SWC_LIB_CORE_STATIC)

get_property(SWC_LIB_FS_SHARED GLOBAL PROPERTY SWC_LIB_FS_SHARED)
get_property(SWC_LIB_FS_STATIC GLOBAL PROPERTY SWC_LIB_FS_STATIC)

set(SHARED ${SWC_LIB_CORE_SHARED} ${SWC_LIB_FS_SHARED})
set(STATIC ${SWC_LIB_CORE_STATIC} ${SWC_LIB_FS_STATIC})

set(FLAGS )
set(TARGETS )

set(SHARED_TARGETS 
	"swcdb_core;swcdb_core_config;swcdb_core_comm;swcdb_core_sys;swcdb_fs;swcdb;${BUILTIN_FS_TARGETS}")

if(SWC_IMPL_SOURCE)
	set(FLAGS "-DSWC_IMPL_SOURCE")
else()
	set(TARGETS ${SHARED_TARGETS})
endif()


# swcdbRanger
ADD_EXEC_TARGET(
	NAME 		swcdbRanger
	SRCS  	ranger/main.cc
	TARGETS ${TARGETS}

	SHARED  ${SHARED}
	STATIC  ${STATIC}
	FLAGS 	${FLAGS}
)


# swcdbManager
ADD_EXEC_TARGET(
	NAME 		swcdbManager
	SRCS  	manager/main.cc
	TARGETS ${TARGETS}

	SHARED  ${SHARED}
	STATIC  ${STATIC}
	FLAGS 	${FLAGS}
)


# swcdbThriftBroker
if(THRIFT_FOUND)
ADD_EXEC_TARGET(
	NAME 		swcdbThriftBroker
	SRCS  	thriftbroker/main.cc
	TARGETS ${TARGETS}

	SHARED ${SHARED} ${THRIFT_LIBRARIES_SHARED}
	STATIC ${STATIC} ${THRIFT_LIBRARIES_STATIC}
	FLAGS  ${FLAGS}
)
endif()


# swcdbFsBroker
ADD_EXEC_TARGET(
	NAME 		swcdbFsBroker
	SRCS  	fsbroker/main.cc
	TARGETS swcdb_core 
					swcdb_core_config 
					swcdb_core_comm 
					swcdb_core_sys 
					swcdb_fs 
					${BUILTIN_FS_TARGETS}

	SHARED  ${SHARED}
	STATIC  ${STATIC}
	FLAGS "-DFS_BROKER_APP"

	ONLY_DYN TRUE
)


# swcdb
ADD_EXEC_TARGET(
	NAME 		swcdb
	SRCS  	utils/main.cc
	TARGETS ${SHARED_TARGETS}

	SHARED  ${SHARED}
	STATIC  ${STATIC}

	ONLY_DYN TRUE
)


# swcdb_load_generator
ADD_EXEC_TARGET(
	NAME	swcdb_load_generator
	SRCS	utils/load_generator.cc
	TARGETS ${TARGETS}

	SHARED  ${SHARED}
	STATIC  ${STATIC}
	FLAGS 	${FLAGS}
)
