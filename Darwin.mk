
PLATFORM_OBJS  = DarwinThread.o
PLATFORM_OBJS += DarwinKernelEvent.o
PLATFORM_OBJS += DarwinInterface.o
PLATFORM_OBJS += DarwinBridge.o
PLATFORM_OBJS += syslog_logger.o

DarwinThread.o :: PlatformThread.h
