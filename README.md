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

Other DTrace tools

 * diskhits
 * dtruss
 * errinfo
 * execsnoop
 * fddist
 * iopattern
 * iosnoop
 * iotop
 * lastwords
 * opensnoop
 * procsystime
 * rwsnoop
 * sampleproc
 * timerfires
 * topsyscall

Check dustmite ( https://github.com/CyberShadow/DustMite/wiki )

 * pubsub
 * Rex/DeRez
 * mig - Mach Interface Generator ??
 * allmemory
 * heap
 * leaks
 * vmmap
 * hdiutil
 * hdik
 * ioreg
 * mkbom -- create a bill-of-materials file
 * bom(5), ditto(8)  lsbom(8)
 * asctl -- App Sandbox Control Tool
 * codesign -- Create and manipulate code signatures
 * csreq(1), xcodebuild(1), `codesign_allocate(1)`

<!-- vim: set autoindent expandtab sw=4 syntax=markdown: -->
