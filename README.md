RedX
====

Pronounced red-cross.

A system tool providing scriptable access to low level
system calls, hypervisor calls, system information, etc.

* kernel symbols
* memory usage (free)
* process list (/proc)
* memory hogs

tests
-----
* generate a memory hog process

build
-----
* how to have build depend on xen headers present

## TODO

 * Add S3 commands
 * Add Windows commands
 * Build under mingw
 * KVM
 * DBus
 * VHD VHDX etc
 * filesystem events
 * process list
 * ptrace / strace / ltrace / dtrace ?

## Process List

 * For linux just look at `/proc/N*`
 * For OSX ??
 * For Windows look at ProcessHacker sources

## OSX Tools

 * dtrace.1
 * trace.1 - kernel trace events - where in C ?
 * latency.1
 * `fs_usage.1`
 * `sc_usage.1`
 * dapptrace.1
 * `ls /usr/bin/*trace*`
 * iosnoop.1
 * apropos dtrace
 * `/usr/bin/*.d`
 * `/Users/karred/ws/depot/node/src/node_dtrace.cc`
 * imptrace.1 - report importance donation events ??

imptrace is a bash script that uses Dtrace scripts and ruby.
Need to look for PDF of dtrace on OSX C interface.

