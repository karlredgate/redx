
CFLAGS += -IDarwin
CXXFLAGS += -IDarwin
LDFLAGS += -rdynamic

PLATFORM_OBJS  = Darwin/DarwinThread.o
PLATFORM_OBJS += Darwin/DarwinKernelEvent.o
PLATFORM_OBJS += Darwin/DarwinInterface.o
PLATFORM_OBJS += Darwin/DarwinBridge.o
PLATFORM_OBJS += Darwin/DarwinNetworkMonitor.o
PLATFORM_OBJS += syslog_logger.o

Darwin/DarwinThread.o :: PlatformThread.h
