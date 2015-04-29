
CFLAGS += -std=c99 -I/usr/local/include/tcl8.6
CXXFLAGS += -I/usr/local/include/tcl8.6
CXX = c++

# PLATFORM_OBJS  = LinuxThread.o
PLATFORM_OBJS += Kernel.o
PLATFORM_OBJS += syslog_logger.o
# PLATFORM_OBJS += LinuxInterface.o
# PLATFORM_OBJS += LinuxBridge.o
# PLATFORM_OBJS += LinuxKernelEvent.o
# PLATFORM_OBJS += LinuxNetworkMonitor.o
# PLATFORM_OBJS += NetLink.o
# PLATFORM_OBJS += NetLinkMonitor.o
# PLATFORM_OBJS += TCL_NetLink.o

