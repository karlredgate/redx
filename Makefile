
include $(shell uname).mk

# changed to -O1 from -O2, since -O2 screws up valgrind.  This
# should be good enough for shipping anyway.
INCLUDE_DIRS = -I../xen/vendor/dist/install/usr/include -I../libservice -I../network
INCLUDE_DIRS = -Ixen -Ilibservice -Inetwork
# CXXFLAGS += -g -O1 $(INCLUDE_DIRS) -Wall -rdynamic
CXXFLAGS += -g -O1 $(INCLUDE_DIRS) -rdynamic
CFLAGS += -g $(INCLUDE_DIRS) -Wall -rdynamic

default: build install
build: all
all: redx system-uuid

OBJS = redx.o

CLEANS += redx $(OBJS)
redx: $(OBJS) ../libservice/libservice.so ../network/libnetmgr.so
	# $(CXX) $(CXXFLAGS) -o $@ $^ -L../network -lnetmgr -L../network/netlib -lnetlib -L../network/netcfg -lnetcfg -L../libservice -lservice -lpthread -ltcl -lexpect5.44.1.15
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread -ltcl -lexpect5.44.1.15

CLEANS += system-uuid
system-uuid: system-uuid.o ../libservice/libservice.so
	$(CXX) $(CXXFLAGS) -o $@ $^ -L../libservice -lservice -lpthread

install: rpm
	$(INSTALL) --directory --mode 755 $(RPM_DIR)
	rm -f $(RPM_DIR)/redx-*.rpm
	cp rpm/RPMS/*/redx-*.rpm $(RPM_DIR)/

uninstall:
	rm -f $(RPM_DIR)/redx-*.rpm

rpm: dist
	rm -rf rpm
	mkdir -p rpm/BUILD rpm/RPMS rpm/BUILDROOT
	rpmbuild -bb --buildroot=$(TOP)platform/redx/rpm/BUILDROOT redx.spec

dist: build
	$(RM) -rf exports
	mkdir -p exports
	$(INSTALL) -d --mode=755 exports/usr/sbin
	$(INSTALL) --mode=755 redx exports/usr/sbin
	$(INSTALL) --mode=755 system-uuid exports/usr/sbin
	$(INSTALL) -d --mode=755 exports/usr/share/redx
	$(INSTALL) -d --mode=755 exports/usr/share/man/man8
	$(INSTALL) redx.8 exports/usr/share/man/man8

clean:
	$(RM) -rf $(CLEANS)
	$(RM) -rf rpm exports

distclean: uninstall clean

.PHONY: test
