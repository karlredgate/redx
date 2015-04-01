
# PLATFORM_OBJS := NetLink.o LinuxThread.o Bridge.o
PLATFORM_OBJS := LinuxThread.o Kernel.o Bridge.o

NetLink.o :: NetLink.h
LinuxThread.o :: PlatformThread.h
Bridge.o :: Network.h
