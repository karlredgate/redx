
# default: build install
default: build
build: all
# all: redx system-uuid
all: redx

include $(shell uname).mk

# changed to -O1 from -O2, since -O2 screws up valgrind.  This
# should be good enough for shipping anyway.
INCLUDE_DIRS = -I../xen/vendor/dist/install/usr/include -I../libservice -I../network
INCLUDE_DIRS = -Ixen -Ilibservice -Inetwork
# CXXFLAGS += -g -O1 $(INCLUDE_DIRS) -Wall -rdynamic
CXXFLAGS += -g -O1 $(INCLUDE_DIRS) -rdynamic
CFLAGS += -g $(INCLUDE_DIRS) -Wall -rdynamic

OBJS = redx.o 
OBJS += AppInit.o
OBJS += Thread.o
OBJS += StringList.o
OBJS += UUID.o
OBJS += ICMPv6.o
OBJS += xuid.o
OBJS += string_util.o
OBJS += tcl_util.o
OBJS += traps.o
OBJS += TCL_Bridge.o
OBJS += TCL_Interface.o
OBJS += Interface.o
OBJS += UUID_Module.o
OBJS += $(PLATFORM_OBJS)

CLEANS += redx $(OBJS)
redx: $(OBJS)
	@: $(CXX) $(CXXFLAGS) -o $@ $^ -L../network -lnetmgr -L../network/netlib -lnetlib -L../network/netcfg -lnetcfg -L../libservice -lservice -lpthread -ltcl -lexpect5.44.1.15
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread -ltcl

CLEANS += system-uuid
system-uuid: system-uuid.o
	@: $(CXX) $(CXXFLAGS) -o $@ $^ -L../libservice -lservice -lpthread
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

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

StringList.o :: StringList.h
BIOS.o :: BIOS.h
Hypercall.o :: Hypercall.h
Kernel.o :: Kernel.h
Thread.o :: Thread.h PlatformThread.h

.PHONY: test

test:
	for testcase in testcases/*; do ./redx $$testcase ; done
