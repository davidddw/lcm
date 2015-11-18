cmake_minimum_required (VERSION 2.8)
PROJECT (LCM)
SET(CMAKE_C_FLAGS "-std=gnu99 -g -O1 ")
SET(XCP_AGENT "/opt/xcp_agent")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
SET(LIVECLOUDDIR "/usr/local/livecloud/bin")

FIND_LIBRARY(CRYPTO_LIB NAMES crypto PATHS /usr/lib /usr/lib64)
FIND_LIBRARY(MYSQL_LIB NAMES mysql libmysql mysqlclient PATHS /usr/lib/mysql)
FIND_LIBRARY(RABBITMQ_LIB NAMES rabbitmq)
FIND_LIBRARY(EV_LIB NAMES ev)
FIND_LIBRARY(RT_LIB NAMES rt)
FIND_LIBRARY(M_LIB NAMES m)
FIND_LIBRARY(PTHREAD_LIB NAMES pthread)
FIND_LIBRARY(SSL_LIB NAMES ssl)
FIND_LIBRARY(PROTOBUF_C_LIB NAMES protobuf-c)
FIND_LIBRARY(CURL_LIB NAMES curl)
FIND_LIBRARY(MONGOC_LIB NAMES mongoc-1.0)
FIND_LIBRARY(BSON_LIB NAMES bson-1.0)
FIND_LIBRARY(XEN_LIB NAMES xenserver)
FIND_LIBRARY(Z_LIB NAMES z)
FIND_LIBRARY(XML2_LIB NAMES xml2)
FIND_LIBRARY(RADOS_LIB NAMES rados)

FIND_PROGRAM(MYSQL_CONFIG mysql_config
    $ENV{MYSQL_HOME}/bin
    /usr/mysql/bin/
    /usr/bin/
)

# protobuf
FILE(GLOB PROTO_INPUT ${PROJECT_SOURCE_DIR}/message/*.proto)
SET(PROTOC "/usr/bin/protoc-c")
SET(PROTO_GEN_DIR ${PROJECT_SOURCE_DIR}/message)
SET(PROTOC_C_OUT_FLAG --c_out)
MESSAGE(STATUS "PROTOC = ${PROTOC}")

FOREACH(PROTO_FILE ${PROTO_INPUT})
  GET_FILENAME_COMPONENT(PROTO_NAME ${PROTO_FILE} NAME_WE)
  SET(CUR_PROTO_GEN
     ${PROTO_GEN_DIR}/${PROTO_NAME}.pb-c.h
     ${PROTO_GEN_DIR}/${PROTO_NAME}.pb-c.c
  )
  SET(PROTO_GEN
     ${PROTO_GEN}
     ${CUR_PROTO_GEN}
  )

  MESSAGE(STATUS "CUR_PROTO_GEN = ${CUR_PROTO_GEN}")
  ADD_CUSTOM_COMMAND(
    OUTPUT ${CUR_PROTO_GEN}
    COMMAND ${PROTOC} ${PROTO_FILE} ${PROTOC_C_OUT_FLAG}=${PROTO_GEN_DIR}
            -I${PROTO_GEN_DIR}
    DEPENDS ${PROTOC} ${PROTO_FILE}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach(PROTO_FILE ${PROTO_INPUT})
add_custom_target(proto DEPENDS ${PROTO_GEN})

IF(MYSQL_CONFIG)
  MESSAGE(STATUS "Using mysql-config: ${MYSQL_CONFIG}")
  # set INCLUDE_DIR
  EXECUTE_PROCESS(
    COMMAND ${MYSQL_CONFIG} "--include"
    OUTPUT_VARIABLE MY_TMP
  )
  STRING(REGEX REPLACE "-I([^ ]*)( .*)?" "\\1" MY_TMP "${MY_TMP}" )
  SET(MYSQL_ADD_INCLUDE_DIRECTORIES ${MY_TMP} CACHE FILEPATH INTERNAL)
  # set LIBRARY_DIR
  EXECUTE_PROCESS(
    COMMAND ${MYSQL_CONFIG} "--libs"
    OUTPUT_VARIABLE MY_TMP
  )
  SET(MYSQL_ADD_LIBRARIES "")
  STRING(REGEX MATCHALL "-l[^ ]*" MYSQL_LIB_LIST "${MY_TMP}" )
  FOREACH(LIB ${MYSQL_LIB_LIST})
    STRING(REGEX REPLACE "[ ]*-l([^ ]*)" "\\1" LIB "${LIB}")
    LIST(APPEND MYSQL_ADD_LIBRARIES "${LIB}" )
  ENDFOREACH( LIB ${MYSQL_LIBS})
  SET(MYSQL_ADD_LIBRARY_PATH "")
  STRING(REGEX MATCHALL "-L[^ ]*" MYSQL_LIBDIR_LIST "${MY_TMP}" )
  FOREACH(LIB ${MYSQL_LIBDIR_LIST})
    STRING(REGEX REPLACE "[ ]*-L([^ ]*)" "\\1" LIB "${LIB}")
    LIST(APPEND MYSQL_ADD_LIBRARY_PATH "${LIB}" )
  ENDFOREACH( LIB ${MYSQL_LIBS})
ELSE(MYSQL_CONFIG)
  SET(MYSQL_ADD_LIBRARIES "")
  LIST(APPEND MYSQL_ADD_LIBRARIES "mysqlclient")
ENDIF(MYSQL_CONFIG)

INCLUDE_DIRECTORIES (${PROJECT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES (${PROJECT_SOURCE_DIR}/message)
INCLUDE_DIRECTORIES (${PROJECT_SOURCE_DIR}/mntnct)
INCLUDE_DIRECTORIES (${MYSQL_ADD_INCLUDE_DIRECTORIES})
INCLUDE_DIRECTORIES (${XCP_AGENT})
INCLUDE_DIRECTORIES (/usr/include/protobuf-c
  /usr/include/libmongoc-1.0
  /usr/include/libbson-1.0
)

SET(LCRM_SRC "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm_socket.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm_worker.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm_msg.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm_action.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/lc_rm_db.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/allocation/lc_rm_alloc.c"
  "${PROJECT_SOURCE_DIR}/lcrmd/allocation/lc_alloc_db.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msgpath.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_utils.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_header.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_allocator.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/talker.pb-c.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_log.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_sys.c"
)

ADD_EXECUTABLE(lcrmd ${LCRM_SRC})
ADD_DEPENDENCIES(lcrmd proto)
TARGET_LINK_LIBRARIES (lcrmd ${MYSQL_LIB} ${RABBITMQ_LIB} ${EV_LIB} ${RT_LIB}
  ${M_LIB} ${PTHREAD_LIB} ${CRYPTO_LIB} ${SSL_LIB} ${PROTOBUF_C_LIB}
  ${CURL_LIB}
)

SET(LCVM_SRC "${PROJECT_SOURCE_DIR}/vmdriver/lc_vm_main.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_vm_socket.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_common.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_vm_worker.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_host.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_agexec.c"
  "${PROJECT_SOURCE_DIR}/vmdriver/lc_vmware.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msgpath.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_utils.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agent_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agexec_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_header.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_pool.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_host.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_vm.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_storage.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_log.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_sys.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/cloudmessage.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/talker.pb-c.c"
)
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/vmdriver/lc_vm_worker.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_common.c
  PROPERTIES COMPILE_FLAGS "-fPIC -DWITHIN_BASIC_TYPE" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_host.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/vmdriver/lc_xen_agexec.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE" )
ADD_EXECUTABLE(vmdriver ${LCVM_SRC})
ADD_DEPENDENCIES(vmdriver proto)
TARGET_LINK_LIBRARIES (vmdriver ${MYSQL_LIB} ${RABBITMQ_LIB} ${MONGOC_LIB}
  ${M_LIB} ${PTHREAD_LIB} ${Z_LIB} ${XML2_LIB} ${PROTOBUF_C_LIB} ${CURL_LIB}
  ${XEN_LIB} ${BSON_LIB}
)

SET(LCPD_SRC "${PROJECT_SOURCE_DIR}/kernel/lc_kernel.c"
  "${PROJECT_SOURCE_DIR}/kernel/lc_kernel_socket.c"
  "${PROJECT_SOURCE_DIR}/kernel/lc_kernel_worker.c"
  "${PROJECT_SOURCE_DIR}/kernel/lc_kernel_mon.c"
  "${PROJECT_SOURCE_DIR}/kernel/lc_vnet_worker.c"
  "${PROJECT_SOURCE_DIR}/kernel/vl2/ext_vl2_handler.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msgpath.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_header.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_postman.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_utils.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agexec_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_url_api.c"
  "${PROJECT_SOURCE_DIR}/lib/nxjson.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_mongo_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_net.c"
  "${PROJECT_SOURCE_DIR}/db/vl2/ext_vl2_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_log.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_sys.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/postman.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/cloudmessage.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/talker.pb-c.c")
ADD_EXECUTABLE(lcpd ${LCPD_SRC})
ADD_DEPENDENCIES(lcpd proto)
TARGET_LINK_LIBRARIES (lcpd ${MYSQL_LIB} ${RABBITMQ_LIB} ${MONGOC_LIB}
  ${BSON_LIB} ${EV_LIB} ${RT_LIB} ${M_LIB} ${PTHREAD_LIB} ${CRYPTO_LIB}
  ${SSL_LIB} ${PROTOBUF_C_LIB} ${CURL_LIB}
)

SET(LCMOND_SRC "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_main.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_socket.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_xen_common.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_xen_monitor.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_worker.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_nsp.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_vmware.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_lcm_api.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_rados.c"
  "${PROJECT_SOURCE_DIR}/lcmonitor/lc_update_lcm_db.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_utils.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msgpath.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agent_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_url_api.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_aes.c"
  "${PROJECT_SOURCE_DIR}/lib/nxjson.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_postman.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_header.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_pool.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_host.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_log.c"
  "${PROJECT_SOURCE_DIR}/db/lc_mongo_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_vm.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_storage.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_sys.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_res_usg.c"
  "${PROJECT_SOURCE_DIR}/message/cloudmessage.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/postman.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/talker.pb-c.c" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_main.c
  PROPERTIES COMPILE_FLAGS "-D_LC_MONITOR_" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_socket.c
  PROPERTIES COMPILE_FLAGS "-D_LC_MONITOR_" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_worker.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE -D_LC_MONITOR_" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_nsp.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE -D_LC_MONITOR_" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_mon_xen_common.c
  PROPERTIES COMPILE_FLAGS "-fPIC -DWITHIN_BASIC_TYPE -D_LC_MONITOR_" )
SET_SOURCE_FILES_PROPERTIES(${PROJECT_SOURCE_DIR}/lcmonitor/lc_xen_monitor.c
  PROPERTIES COMPILE_FLAGS "-DWITHIN_BASIC_TYPE -D_LC_MONITOR_" )
ADD_EXECUTABLE(lcmond ${LCMOND_SRC})
ADD_DEPENDENCIES(lcmond proto)
TARGET_LINK_LIBRARIES (lcmond ${MYSQL_LIB} ${RABBITMQ_LIB} ${MONGOC_LIB}
  ${BSON_LIB} ${EV_LIB} ${RT_LIB} ${M_LIB} ${PTHREAD_LIB} ${XEN_LIB}
  ${SSL_LIB} ${CRYPTO_LIB} ${XML2_LIB} ${CURL_LIB} ${Z_LIB}
  ${PROTOBUF_C_LIB} ${RADOS_LIB}
)

SET(LCSNFD_SRC "${PROJECT_SOURCE_DIR}/lcsniffer/lcsniffer.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/ovs_dfi.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/ovs_process.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/ovs_rsync.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_socket.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lc_snf_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_usage.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_usage_api.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_usage_db.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_usage_vmware.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_usage_xen.c"
  "${PROJECT_SOURCE_DIR}/lcsniffer/lcs_utils.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_header.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_postman.c"
  "${PROJECT_SOURCE_DIR}/lib/nxjson.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_mongo_db.c"
  "${PROJECT_SOURCE_DIR}/db/lc_db_sys.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_url_api.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agent_msg.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_postman.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/postman.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/cloudmessage.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/postman.pb-c.c"
)
ADD_EXECUTABLE(lcsnfd ${LCSNFD_SRC})
ADD_DEPENDENCIES(lcsnfd proto)
TARGET_LINK_LIBRARIES (lcsnfd ${MYSQL_LIB} ${RABBITMQ_LIB} ${MONGOC_LIB}
  ${BSON_LIB} ${EV_LIB} ${RT_LIB} ${M_LIB} ${PTHREAD_LIB} ${XEN_LIB}
  ${SSL_LIB} ${CRYPTO_LIB} ${XML2_LIB} ${CURL_LIB} ${Z_LIB}
  ${PROTOBUF_C_LIB}
)

SET(HEALTH_SRC "${PROJECT_SOURCE_DIR}/healthcheck/lc_healthcheck.c"
  "${PROJECT_SOURCE_DIR}/healthcheck/lc_healthcheck_socket.c"
  "${PROJECT_SOURCE_DIR}/healthcheck/lc_healthcheck_worker.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_qctrl.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msg.c"
)
ADD_EXECUTABLE(healthcheck ${HEALTH_SRC})
ADD_DEPENDENCIES(healthcheck proto)
TARGET_LINK_LIBRARIES (healthcheck ${PTHREAD_LIB})

SET(MNTNCT_SRC "${PROJECT_SOURCE_DIR}/mntnct/mntnct.c"
  "${PROJECT_SOURCE_DIR}/mntnct/mntnct_commands.c"
  "${PROJECT_SOURCE_DIR}/mntnct/mntnct_utils.c"
  "${PROJECT_SOURCE_DIR}/mntnct/mntnct_socket.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_msgpath.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_bus.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_aes.c"
  "${PROJECT_SOURCE_DIR}/lib/nxjson.c"
  "${PROJECT_SOURCE_DIR}/lib/lc_agexec_msg.c"
  "${PROJECT_SOURCE_DIR}/message/header.pb-c.c"
  "${PROJECT_SOURCE_DIR}/message/talker.pb-c.c"
)
ADD_EXECUTABLE(mntnct ${MNTNCT_SRC})
ADD_DEPENDENCIES(mntnct proto)
TARGET_LINK_LIBRARIES (mntnct ${RABBITMQ_LIB} ${M_LIB} ${SSL_LIB}
  ${CRYPTO_LIB} ${CURL_LIB} ${PROTOBUF_C_LIB}
)

INSTALL(TARGETS lcmond RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS healthcheck RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS lcpd RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS lcrmd RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS vmdriver RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS lcsnfd RUNTIME DESTINATION ${LIVECLOUDDIR})
INSTALL(TARGETS mntnct RUNTIME DESTINATION /usr/local/bin)

