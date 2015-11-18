PWD:=$(shell pwd)
ROOTDIR=$(PWD)/../../
PLATFORM=$(ROOTDIR)/lcm/
XENSVR=/usr/include/xen/api
XENAGENT=/opt/xcp_agent

CC = gcc -g -O1 -Werror
XENCC = gcc -g -O1 -std=gnu99 -fPIC
PB = protoc-c --c_out=$(MESSAGE) --proto_path=$(MESSAGE)
PB_PY = protoc -I=$(MESSAGE) --python_out=$(MESSAGE)

MYSQL_CFLAGS=$(shell mysql_config --cflags)
MYSQL_LDFLAGS=$(shell mysql_config --libs_r)

MONGO_CFLAGS = -std=gnu99
MONGO_LDFLAGS = -lmongoc -lbson
RABBIT_LDFLAGS = -lrabbitmq

CFLAGS = -I../include -I$(XENAGENT)/ -g -O1

EXEC = lcpd vmdriver lcrmd healthcheck test lcmond lcmobj lcsnfd postman talker

KER = $(PLATFORM)/kernel
APP = $(PLATFORM)/application
DRV = $(PLATFORM)/driver
CHK = $(PLATFORM)/healthcheck
LIB = $(PLATFORM)/lib
MESSAGE = $(PLATFORM)/message
INCLUDE = $(PLATFORM)/include
TEST = $(PLATFORM)/test
VMDRIVER = $(PLATFORM)/vmdriver
RM = $(PLATFORM)/lcrmd
DB = $(PLATFORM)/db
LCMON = $(PLATFORM)/lcmonitor
LCSNF = $(PLATFORM)/lcsniffer
POSTMAN = $(PLATFORM)/postman
TALKER = $(PLATFORM)/talker
STOREKEEPER = $(PLATFORM)/storekeeper

.PHONY: all test pyproto

all: $(EXEC)
test: driver vmdebug ker_debug pb_test rmdebug lcs_mongo_test
pyproto: $(LIB)/header_pb2 $(LIB)/postman_pb2 $(LIB)/talker_pb2

driver: lc_driver.o
	$(CC) -o $@ $^  -lpthread
lc_driver.o: $(TEST)/lc_driver.c
	$(CC) -c $(CFLAGS) $<

vmdebug: lc_vm.o lc_bus.o
	$(CC) -o $@ $^ -lrabbitmq
lc_vm.o: $(TEST)/lc_vm.c
	$(CC) -c $(CFLAGS) $<

ker_debug: lc_kernel_dump.o
	$(CC) -o $@ $^
lc_kernel_dump.o: $(KER)/lc_kernel_dump.c
	$(CC) -c $(CFLAGS) $<

pb_test: protobuf_test.o cloudmessage.pb-c.o
	$(CC) -o $@ $^ -lprotobuf-c
protobuf_test.o: $(TEST)/protobuf_test.c $(INCLUDE)/cloudmessage.pb-c.h
	$(CC) -c $(CFLAGS) $<

rmdebug: lc_rm_test.o lc_bus.o lc_header.o header.pb-c.o talker.pb-c.o
	$(CC) -o $@ $^ -lrabbitmq -lprotobuf-c
lc_rm_test.o: $(TEST)/lc_rm_test.c $(INCLUDE)/header.pb-c.h $(INCLUDE)/talker.pb-c.h
	$(CC) -c $(CFLAGS) $<

LCPD_OBJS =  lc_kernel.o lc_kernel_socket.o lc_qctrl.o lc_kernel_worker.o lc_kernel_mon.o lc_msg.o \
	lc_vnet_worker.o ext_vl2_handler.o lc_db.o \
	lc_mongo_db.o lc_db_net.o ext_vl2_db.o \
	lc_msgpath.o lc_bus.o \
	lc_header.o header.pb-c.o lc_postman.o postman.pb-c.o cloudmessage.pb-c.o talker.pb-c.o\
	lc_utils.o lc_db_log.o lc_db_sys.o
lcpd: $(LCPD_OBJS) lc_agexec_msg.o lc_url_api.o nxjson.o
	$(CC) $^ $(MYSQL_LDFLAGS) $(MONGO_CFLAGS) $(MONGO_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lev -lrt -lm -lpthread -lcrypto -lssl -lprotobuf-c -lcurl -o $@
lc_kernel.o: $(KER)/lc_kernel.c $(INCLUDE)/header.pb-c.h
	echo $(ROOTDIR) $(PWD)
	$(CC) -c $(CFLAGS) $<
lc_kernel_socket.o: $(KER)/lc_kernel_socket.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_kernel_worker.o: $(KER)/lc_kernel_worker.c $(INCLUDE)/postman.pb-c.h $(INCLUDE)/cloudmessage.pb-c.h $(INCLUDE)/talker.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_kernel_mon.o: $(KER)/lc_kernel_mon.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_kernel_device.o: $(KER)/lc_kernel_device.c
	$(CC) -c $(CFLAGS) $<
lc_kernel_mapping.o: $(KER)/lc_kernel_mapping.c
	$(CC) -c $(CFLAGS) $<
lc_vnet_worker.o: $(KER)/lc_vnet_worker.c $(INCLUDE)/cloudmessage.pb-c.h
	$(CC) -c $(CFLAGS) $<
ext_vl2_handler.o: $(KER)/vl2/ext_vl2_handler.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_qctrl.o: $(LIB)/lc_qctrl.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_msg.o: $(LIB)/lc_msg.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_bus.o: $(LIB)/lc_bus.c
	$(CC) -c $(CFLAGS) $<

lc_kernel_dummy.o: $(KER)/lc_kernel_dummy.c
	$(CC) -c $(CFLAGS) $<

lcmobj: lc_allocator.o

#definition for VM Driver
vmdriver: lc_vm_main.o lc_vm_socket.o lc_xen_common.o lc_qctrl.o lc_vm_worker.o \
	lc_msg.o lc_bus.o lc_xen_host.o lc_xen_agexec.o\
	lc_db_pool.o lc_db_host.o lc_db.o lc_db_vm.o lc_db_storage.o lc_msgpath.o \
	lc_vmware.o lc_utils.o lc_db_log.o lc_agent_msg.o lc_agexec_msg.o \
	lc_header.o header.pb-c.o cloudmessage.pb-c.o talker.pb-c.o lc_db_sys.o
	#make -C $(XENSVR)
	#cp $(XENSVR)/libxenserver.so* ../obj/
	$(CC) -o $@ $^ $(MYSQL_LDFLAGS) $(MONGO_CFLAGS) $(MONGO_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lpthread -L../obj/ -lxenserver -lxml2 -lcurl -lz -lm -lprotobuf-c 
lc_vm_main.o: $(VMDRIVER)/lc_vm_main.c
	$(CC) -c $(CFLAGS) $<
lc_vm_socket.o: $(VMDRIVER)/lc_vm_socket.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_vm_worker.o: $(VMDRIVER)/lc_vm_worker.c $(INCLUDE)/cloudmessage.pb-c.h $(INCLUDE)/talker.pb-c.h
	$(CC) -c -DWITHIN_BASIC_TYPE $(CFLAGS) $<
lc_vmware.o: $(VMDRIVER)/lc_vmware.c $(INCLUDE)/header.pb-c.h $(INCLUDE)/cloudmessage.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_xen_common.o: $(VMDRIVER)/lc_xen_common.c
	$(XENCC) -c -DWITHIN_BASIC_TYPE $(CFLAGS) $<
lc_xen_host.o: $(VMDRIVER)/lc_xen_host.c
	$(CC) -c -DWITHIN_BASIC_TYPE $(CFLAGS) $<
lc_xen_client.o: $(VMDRIVER)/lc_xen_client.c
	$(CC) -c $(CFLAGS) $<
lc_xen_agexec.o: $(VMDRIVER)/lc_xen_agexec.c
	$(CC) -c -DWITHIN_BASIC_TYPE $(CFLAGS) $<
lc_db_pool.o: $(DB)/lc_db_pool.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_host.o: $(DB)/lc_db_host.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_storage.o: $(DB)/lc_db_storage.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_vm.o: $(DB)/lc_db_vm.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_net.o: $(DB)/lc_db_net.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_log.o: $(DB)/lc_db_log.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_db_sys.o: $(DB)/lc_db_sys.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<

#definition for lcrmd
RM_OBJS =  lc_rm.o lc_rm_socket.o lc_rm_worker.o lc_rm_msg.o lc_rm_action.o \
		   lc_rm_db.o lc_rm_alloc.o lc_alloc_db.o \
		   lc_msgpath.o lc_bus.o lc_utils.o lc_qctrl.o lc_msg.o \
		   lc_header.o header.pb-c.o talker.pb-c.o \
		   lc_db.o lc_db_log.o lc_db_sys.o lc_allocator.o
lcrmd: $(RM_OBJS)
	$(CC) $^ $(MYSQL_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lev -lrt -lm -lpthread -lcrypto -lssl -lprotobuf-c -lcurl -o $@
lc_rm.o: $(RM)/lc_rm.c
	echo $(ROOTDIR) $(PWD)
	$(CC) -c $(CFLAGS) $<
lc_rm_socket.o: $(RM)/lc_rm_socket.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_rm_worker.o: $(RM)/lc_rm_worker.c $(INCLUDE)/talker.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_rm_db.o: $(RM)/lc_rm_db.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_rm_msg.o: $(RM)/lc_rm_msg.c $(INCLUDE)/talker.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_rm_action.o: $(RM)/lc_rm_action.c
	$(CC) -c $(CFLAGS) $<
lc_rm_alloc.o: $(RM)/allocation/lc_rm_alloc.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<
lc_alloc_db.o: $(RM)/allocation/lc_alloc_db.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<

#definition for HealthCheck
healthcheck: lc_healthcheck.o lc_healthcheck_socket.o lc_healthcheck_worker.o lc_qctrl.o lc_msg.o
	$(CC) -o $@ $^ -lpthread
lc_healthcheck.o: $(CHK)/lc_healthcheck.c
	$(CC) -c $(CFLAGS) $<
lc_healthcheck_socket.o: $(CHK)/lc_healthcheck_socket.c
	$(CC) -c $(CFLAGS) $<
lc_healthcheck_worker.o: $(CHK)/lc_healthcheck_worker.c
	$(CC) -c $(CFLAGS) $<

#definition for LC Monitor
lcmond:lc_mon_main.o lc_mon_socket.o lc_mon_xen_common.o lc_qctrl.o lc_mon_worker.o lc_mon_nsp.o \
    lc_msg.o lc_bus.o lc_xen_monitor.o lc_db_pool.o lc_db_host.o lc_db.o lc_db_log.o \
    lc_mongo_db.o lc_db_vm.o lc_utils.o \
    lc_db_storage.o lc_msgpath.o lc_db_res_usg.o lc_mon_qctrl.o lc_agent_msg.o \
    lc_mon_vmware.o cloudmessage.pb-c.o \
    lc_lcm_api.o lc_update_lcm_db.o lc_db_sys.o lc_url_api.o lc_aes.o nxjson.o \
    lc_header.o header.pb-c.o lc_postman.o postman.pb-c.o talker.pb-c.o \
    lc_mon_rados.o
	#make -C $(XENSVR)
	#cp $(XENSVR)/libxenserver.so* ../obj/
	$(CC) -o $@ $^ $(MYSQL_LDFLAGS) $(MONGO_CFLAGS) $(MONGO_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lev -lrt -lm -lpthread -L../obj/ -lxenserver -lxml2 -lcurl -lz -lm -lprotobuf-c -lrados 
lc_mon_main.o: $(LCMON)/lc_mon_main.c $(INCLUDE)/header.pb-c.h
	$(CC) -c $(CFLAGS) -D_LC_MONITOR_ $<
lc_mon_socket.o: $(LCMON)/lc_mon_socket.c $(INCLUDE)/cloudmessage.pb-c.h
	$(CC) -c $(CFLAGS) -D_LC_MONITOR_ $<
lc_mon_worker.o: $(LCMON)/lc_mon_worker.c
	$(CC) -c -DWITHIN_BASIC_TYPE -D_LC_MONITOR_ $(CFLAGS) $<
lc_mon_nsp.o: $(LCMON)/lc_mon_nsp.c $(INCLUDE)/postman.pb-c.h $(INCLUDE)/header.pb-c.h
	$(CC) -c -DWITHIN_BASIC_TYPE -D_LC_MONITOR_ $(CFLAGS) $<
lc_mon_xen_common.o: $(LCMON)/lc_mon_xen_common.c
	$(XENCC) -c -DWITHIN_BASIC_TYPE $(CFLAGS) -D_LC_MONITOR_ $<
lc_xen_monitor.o: $(LCMON)/lc_xen_monitor.c $(INCLUDE)/header.pb-c.h $(INCLUDE)/postman.pb-c.h $(INCLUDE)/talker.pb-c.h
	$(CC) -c -DWITHIN_BASIC_TYPE -D_LC_MONITOR_ $(CFLAGS) $<
lc_mon_qctrl.o: $(LCMON)/lc_mon_qctrl.c
	$(CC) -c $(CFLAGS) $<
lc_mon_vmware.o: $(LCMON)/lc_mon_vmware.c $(INCLUDE)/cloudmessage.pb-c.h
	$(CC) -c $(CFLAGS) $<
lc_mon_rados.o: $(LCMON)/lc_mon_rados.c
	$(CC) -c $(CFLAGS) $<
lc_lcm_api.o: $(LCMON)/lc_lcm_api.c $(LCMON)/lc_lcm_api.h
	$(CC) -c $(CFLAGS) $<
lc_update_lcm_db.o: $(LCMON)/lc_update_lcm_db.c $(LCMON)/lc_update_lcm_db.h
	$(CC) -c $(CFLAGS) $<
lc_db_res_usg.o: $(DB)/lc_db_res_usg.c
	$(CC) -c $(CFLAGS) $(MYSQL_CFLAGS) $<

#definition for LC Sniffer
ovs_dfi.o: $(LCSNF)/ovs_dfi.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
ovs_process.o: $(LCSNF)/ovs_process.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
ovs_rsync.o: $(LCSNF)/ovs_rsync.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_socket.o: $(LCSNF)/lcs_socket.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_usage.o: $(LCSNF)/lcs_usage.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_usage_api.o: $(LCSNF)/lcs_usage_api.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_usage_db.o: $(LCSNF)/lcs_usage_db.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_usage_vmware.o: $(LCSNF)/lcs_usage_vmware.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_usage_xen.o: $(LCSNF)/lcs_usage_xen.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lc_snf_qctrl.o: $(LCSNF)/lc_snf_qctrl.c
	$(CC) $(CFLAGS) -c -o $@ $<
lcsniffer.o: $(LCSNF)/lcsniffer.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_utils.o: $(LCSNF)/lcs_utils.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
#LCSNFD_SRCS=$(wildcard $(LCSNF)/*.c)
#LCSNFD_OBJS=$(patsubst %.c, %.o, $(LCSNFD_SRCS))
LCSNFD_OBJS = lcsniffer.o \
        ovs_dfi.o ovs_process.o ovs_rsync.o lcs_socket.o lc_snf_qctrl.o \
        lcs_usage.o lcs_usage_api.o lcs_usage_db.o lcs_usage_vmware.o lcs_usage_xen.o \
        lc_header.o header.pb-c.o lc_postman.o postman.pb-c.o nxjson.o
lcsnfd: $(LCSNFD_OBJS) lc_db.o lc_mongo_db.o \
		lc_msg.o lc_bus.o lc_url_api.o lc_agent_msg.o cloudmessage.pb-c.o \
        lc_db_sys.o cloudmessage.pb-c.o lc_postman.o postman.pb-c.o \
        lcs_utils.o
	$(CC) -o $@ $^ $(CFLAGS) $(MYSQL_CFLAGS) $(MYSQL_LDFLAGS) \
		$(MONGO_CFLAGS) $(MONGO_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lev -lrt -lm -lpthread -L../obj/ -lxml2 -lcurl -lz -lm -lprotobuf-c

#definition for LC Mongo test
lcs_mongo_test.o: $(LCSNF)/lcs_mongo_test.c $(INCLUDE)/postman.pb-c.h
	$(CC) $(CFLAGS) -c -o $@ $<
lcs_mongo_test: lcs_mongo_test.o lc_db.o lc_mongo_db.o \
		lc_msg.o lc_bus.o lc_url_api.o lc_agent_msg.o cloudmessage.pb-c.o \
        lc_db_sys.o cloudmessage.pb-c.o lc_postman.o postman.pb-c.o nxjson.o
	$(CC) -o $@ $^ $(CFLAGS) $(MYSQL_CFLAGS) $(MYSQL_LDFLAGS) \
		$(MONGO_CFLAGS) $(MONGO_LDFLAGS) $(RABBIT_LDFLAGS) \
		-lev -lrt -lm -lpthread -L../obj/ -lxml2 -lcurl -lz -lm -lprotobuf-c

#header
$(LIB)/header_pb2: $(MESSAGE)/header.proto
	$(PB_PY) $<
	cp $(MESSAGE)/header_pb2.py $(POSTMAN)
	cp $(MESSAGE)/header_pb2.py $(TALKER)
	cp $(MESSAGE)/header_pb2.py $(STOREKEEPER)
	rm -rf $(MESSAGE)/header_pb2.py

#postman
$(LIB)/postman_pb2: $(MESSAGE)/postman.proto
	$(PB_PY) $<
	cp $(MESSAGE)/postman_pb2.py $(POSTMAN)
	cp $(MESSAGE)/postman_pb2.py $(TALKER)
	rm -rf $(MESSAGE)/postman_pb2.py
postman: $(LIB)/header_pb2 $(LIB)/postman_pb2

#talker
$(LIB)/talker_pb2: $(MESSAGE)/talker.proto
	$(PB_PY) $<
	mv $(MESSAGE)/talker_pb2.py $(TALKER)
talker: $(LIB)/header_pb2 $(LIB)/talker_pb2 $(LIB)/postman_pb2

#For DB
lc_db.o: $(DB)/lc_db.c $(INCLUDE)/header.pb-c.h
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) -c -o $@ $<
lc_mongo_db.o: $(DB)/lc_mongo_db.c
	$(CC) $(CFLAGS) $(MONGO_CFLAGS) -c -o $@ $<
ext_vl2_db.o: $(DB)/vl2/ext_vl2_db.c
	$(CC) $(CFLAGS) $(MYSQL_CFLAGS) -c -o $@ $<

#lib
lc_agent_msg.o: $(LIB)/lc_agent_msg.c
	$(CC) -c $(CFLAGS) $<
lc_agexec_msg.o: $(LIB)/lc_agexec_msg.c
	$(CC) -c $(CFLAGS) $<
lc_url_api.o: $(LIB)/lc_url_api.c
	$(CC) -c $(CFLAGS) -lcurl $<
lc_msgpath.o: $(LIB)/lc_msgpath.c
	$(CC) -c $(CFLAGS) $<
lc_utils.o: $(LIB)/lc_utils.c
	$(CC) -c $(CFLAGS) $<
lc_allocator.o: $(LIB)/lc_allocator.c $(INCLUDE)/lc_allocator.h
	$(CC) -c $(CFLAGS) $<
lc_aes.o: $(LIB)/lc_aes.c $(INCLUDE)/lc_aes.h
	$(CC) -c $(CFLAGS) $<
nxjson.o: $(LIB)/nxjson.c
	$(CC) -c $(CFLAGS) $<
lc_header.o: $(LIB)/lc_header.c $(INCLUDE)/lc_header.h header.pb-c.o
	$(CC) -c $(CFLAGS) $<
lc_postman.o: $(LIB)/lc_postman.c $(INCLUDE)/lc_postman.h postman.pb-c.o
	$(CC) -c $(CFLAGS) $<

#protobuf-c
cloudmessage.pb-c.o: $(LIB)/cloudmessage.pb-c.c
	$(CC) -c -std=c99 $(CFLAGS) $<
$(LIB)/cloudmessage.pb-c.c: $(MESSAGE)/cloudmessage.proto
	$(PB) $<
	mv $(MESSAGE)/cloudmessage.pb-c.c $(LIB)
	mv $(MESSAGE)/cloudmessage.pb-c.h $(INCLUDE)
$(INCLUDE)/cloudmessage.pb-c.h: $(LIB)/cloudmessage.pb-c.c
header.pb-c.o: $(LIB)/header.pb-c.c
	$(CC) -c -std=c99 $(CFLAGS) $<
$(LIB)/header.pb-c.c: $(MESSAGE)/header.proto
	$(PB) $<
	mv $(MESSAGE)/header.pb-c.c $(LIB)
	mv $(MESSAGE)/header.pb-c.h $(INCLUDE)
$(INCLUDE)/header.pb-c.h: $(LIB)/header.pb-c.c
postman.pb-c.o: $(LIB)/postman.pb-c.c
	$(CC) -c -std=c99 $(CFLAGS) $<
$(LIB)/postman.pb-c.c: $(MESSAGE)/postman.proto
	$(PB) $<
	mv $(MESSAGE)/postman.pb-c.c $(LIB)
	mv $(MESSAGE)/postman.pb-c.h $(INCLUDE)
$(INCLUDE)/postman.pb-c.h: $(LIB)/postman.pb-c.c
talker.pb-c.o: $(LIB)/talker.pb-c.c
	$(CC) -c -std=c99 $(CFLAGS) $<
$(LIB)/talker.pb-c.c: $(MESSAGE)/talker.proto
	$(PB) $<
	mv $(MESSAGE)/talker.pb-c.c $(LIB)
	mv $(MESSAGE)/talker.pb-c.h $(INCLUDE)
$(INCLUDE)/talker.pb-c.h: $(LIB)/talker.pb-c.c

clean:
	rm -rf $(EXEC) *.o client driver ker_debug lcmd lcmconsole vmdriver vmdebug core.* healthcheck pb_test lcmond lcsnfd lcs_mongo_test\
		   lcrmd rmdebug
	rm -rf ../obj/libxenserver.so ../obj/libxenserver.so.1 ../obj/libxenserver.so.1.10
	make -C $(XENSVR) clean
	rm -f *.so *.a
	rm -f $(INCLUDE)/*.pb-c.h
	rm -f $(LIB)/*.pb-c.c
	rm -f $(POSTMAN)/*_pb2.py $(POSTMAN)/*.pyc
	rm -f $(TALKER)/*_pb2.py $(TALKER)/*.pyc
	rm -f $(STOREKEEPER)/*_pb2.py $(STOREKEEPER)/*.pyc

# DO NOT DELETE
