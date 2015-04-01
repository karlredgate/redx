
PLATFORM_OBJS := NetLink.o LinuxThread.o Bridge.o

NetLink.o :: NetLink.h
LinuxThread.o :: PlatformThread.h
Bridge.o :: Network.h
