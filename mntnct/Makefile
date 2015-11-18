CC     = gcc -g -O1 -Werror
SHELL  = sh
MAKE   = make
RABBITMQ =../../3parties/rabbitmq-c
CURL_CFLAGS = $(shell curl-config --cflags)
CFLAGS = -I../include -I$(RABBITMQ)/librabbitmq -I/opt/xcp_agent
CURL_LIBS = $(shell curl-config --libs)
LIBS = -lm -lrabbitmq -lcrypto -lprotobuf-c $(CURL_LIBS)
LCOBJ  = ../obj/lc_msgpath.o ../obj/lc_bus.o ../obj/lc_aes.o ../obj/nxjson.o ../obj/header.pb-c.o ../obj/talker.pb-c.o ../obj/lc_agexec_msg.o
LCPATH = /usr/local/bin
INS    = $(SHELL) mntnct_install.sh

mntnct: mntnct.o mntnct_commands.o mntnct_utils.o mntnct_socket.o $(LCOBJ)
	$(CC) -o $@ $^ $(LIBS)
mntnct.o: mntnct.c $(CURL_CFLAGS)
	$(CC) -c $(CFLAGS) $<
mntnct_utils.o: mntnct_utils.c $(CURL_CFLAGS)
	$(CC) -c $(CFLAGS) $<
mntnct_commands.o: mntnct_commands.c mntnct_db.h mntnct_utils.h ../include/header.pb-c.h ../include/talker.pb-c.h
	$(CC) -c $(CFLAGS) $<
mntnct_socket.o: mntnct_socket.c ../include/header.pb-c.h
	$(CC) -c $(CFLAGS) $<
../include/header.pb-c.h:
	$(MAKE) -C ../obj header.pb-c.o
../include/talker.pb-c.h:
	$(MAKE) -C ../obj talker.pb-c.o
../obj/header.pb-c.o:
	$(MAKE) -C ../obj header.pb-c.o
../obj/talker.pb-c.o:
	$(MAKE) -C ../obj talker.pb-c.o
../obj/lc_msgpath.o:
	$(MAKE) -C ../obj lc_msgpath.o $(CFLAGS)
../obj/lc_bus.o:
	$(MAKE) -C ../obj lc_bus.o $(CFLAGS)
../obj/lc_aes.o:
	$(MAKE) -C ../obj lc_aes.o $(CFLAGS)
../obj/nxjson.o:
	$(MAKE) -C ../obj nxjson.o $(CFLAGS)
../obj/lc_agexec_msg.o:
	$(MAKE) -C ../obj lc_agexec_msg.o $(CFLAGS)

install: mntnct
	$(INS) mntnct $(LCPATH)

.PHONY: clean

clean:
	rm -f *.o
	rm -f mntnct
