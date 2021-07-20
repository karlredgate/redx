$(warning Loading Linux/Platform.mk)

CFLAGS += -std=c99 -rdynamic
CXXFLAGS += -rdynamic

# PLATFORM_OBJS := NetLink.o LinuxThread.o Bridge.o
PLATFORM_OBJS  = Linux/LinuxKernel.o
PLATFORM_OBJS += Linux/LinuxThread.o
PLATFORM_OBJS += syslog_logger.o
PLATFORM_OBJS += Linux/LinuxInterface.o
PLATFORM_OBJS += Linux/LinuxBridge.o
PLATFORM_OBJS += Linux/LinuxKernelEvent.o
PLATFORM_OBJS += Linux/LinuxNetworkMonitor.o
PLATFORM_OBJS += Linux/NetLink.o
PLATFORM_OBJS += Linux/NetLinkMonitor.o
PLATFORM_OBJS += Linux/TCL_NetLink.o

NetLink.o :: NetLink.h
LinuxThread.o :: PlatformThread.h
Bridge.o :: Network.h
