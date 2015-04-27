
CFLAGS += -std=c99

# PLATFORM_OBJS := NetLink.o LinuxThread.o Bridge.o
PLATFORM_OBJS  = LinuxThread.o
PLATFORM_OBJS += Kernel.o
PLATFORM_OBJS += syslog_logger.o
PLATFORM_OBJS += LinuxInterface.o
PLATFORM_OBJS += LinuxBridge.o
PLATFORM_OBJS += LinuxKernelEvent.o
PLATFORM_OBJS += LinuxNetworkMonitor.o
PLATFORM_OBJS += TCL_NetLink.o

NetLink.o :: NetLink.h
LinuxThread.o :: PlatformThread.h
Bridge.o :: Network.h
