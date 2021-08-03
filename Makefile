
PACKAGE = redx
PWD := $(shell pwd)
DEPENDENCIES = tcl

MAJOR_VERSION=1
MINOR_VERSION=0
REVISION=0

CFLAGS += -fpic -g
CXXFLAGS += -fpic -g
LDFLAGS += -g

#
# default: build install
default: build

OS := $(shell uname -s)
include Makefiles/$(OS).mk
CXXFLAGS += -I$(OS)

build: all
# all: redx system-uuid
all: redx

include $(shell uname)/Platform.mk

# changed to -O1 from -O2, since -O2 screws up valgrind.  This
# should be good enough for shipping anyway.
# INCLUDE_DIRS = -I../xen/vendor/dist/install/usr/include -I../libservice -I../network
# INCLUDE_DIRS = -Ixen -Ilibservice -Inetwork -I$(shell pwd)
INCLUDE_DIRS += -I.
# CXXFLAGS += -g -O1 $(INCLUDE_DIRS) -Wall -rdynamic
CXXFLAGS += -g -O1 $(INCLUDE_DIRS)
CFLAGS += -g -O1 $(INCLUDE_DIRS) -Wall
LDFLAGS += -g -O1

OBJS = redx.o 
OBJS += Allocator.o
OBJS += IntKey.o
OBJS += CStringKey.o
OBJS += TCL_Keys.o
OBJS += List.o
OBJS += AppInit.o
OBJS += Thread.o
OBJS += SMBIOSStringList.o
OBJS += UUID.o
# OBJS += Kernel.o
OBJS += Neighbor.o
OBJS += ICMPv6.o
OBJS += Channel.o
OBJS += NetworkMonitor.o
OBJS += Service.o
OBJS += xuid.o
OBJS += string_util.o
OBJS += tcl_util.o
OBJS += traps.o
OBJS += TCL_Allocator.o
OBJS += TCL_UUID.o
OBJS += TCL_Bridge.o
OBJS += TCL_Interface.o
OBJS += TCL_ICMPv6.o
OBJS += TCL_NetworkMonitor.o
OBJS += TCL_Channel.o
OBJS += TCL_Thread.o
OBJS += TCL_Syslog.o
OBJS += TCL_Kernel.o
OBJS += TCL_Process.o
OBJS += TCL_Container.o
# OBJS += TCL_SMBIOS.o
# OBJS += TCL_SharedNetwork.o
# OBJS += TCL_Hypercall.o
# OBJS += TCL_XenStore.o
OBJS += Interface.o
OBJS += Bridge.o
OBJS += $(PLATFORM_OBJS)

#
# for static linking use "-static" and TCL then needs
# -ldl -lz -pthread
CLEANS += redx $(OBJS)
redx: $(OBJS)
	@: $(CXX) $(CXXFLAGS) -o $@ $^ -L../network -lnetmgr -L../network/netlib -lnetlib -L../network/netcfg -lnetcfg -L../libservice -lservice -lpthread -ltcl -lexpect5.44.1.15
	$(CXX) $(LDFLAGS) -o $@ $^ -lpthread -ltcl

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

SMBIOSStringList.o :: SMBIOSStringList.h
SMBIOS.o :: SMBIOS.h
Hypercall.o :: Hypercall.h
Kernel.o :: Kernel.h
Thread.o :: Thread.h PlatformThread.h
LinuxInterface.o :: PlatformInterface.h

.PHONY: test

test:
	for testcase in testcases/*; do ./redx $$testcase ; done
