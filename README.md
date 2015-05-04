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

 * Migrate all platform files to platform dir
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

## OSX Notes

sysctl is in man3.
What is the real system call that is made?

 * Add sysctl command set.

### OSX Bridges

Need to get source code for ifconfig

```
$ ifconfig bridge0 create
$ ifconfig bridge0 up addm en0 addm en1
```

last command line add en0 (ethernet) and en1 (wifi) to the bridge interface (bridge0)
  
This way i get a new interface in my network manager called "bridge Configuration".
This seems like something is planned by mac os.
Still, it doesnt work (pings from devices on the wifi network to 192.168.1.1 which is the main router get no return).

Setting up the bridge is only half the battle.
By default the OS isn't going to pass traffic across it,
nor do devices on either side of the bridge know to use the bridge link.
At the very least you need to configure ARP so that the Mac responds
to requests for devices on the other side of the bridge - i.e. when
the device on WiFi sends out an ARP request for the router, the Mac
responds, even though it isn't the gateway machine.

You could use proxyall to have the Mac proxy all ARP traffic across
the bridge, or add specific ARP entries to the ARP table.

```
sudo sysctl -w net.link.ether.inet.proxyall=1
```

You might also need to enable IP Forwarding:

```
sudo sysctl -w net.inet.ip.forwarding=1
```

With the usual caveats that sysctl changes like this are transient
and lost at reboot - add them to `/etc/sysctl.conf` to apply them at
boot.

### OSX Bond

```
networksetup -h | grep -i bond
networksetup -isBondSupported <hardwareport>
networksetup -createBond <bondname> <hardwareport1> <hardwareport2> <...>
networksetup -deleteBond <bonddevicename>
networksetup -addDeviceToBond <hardwareport> <bonddevicename>
networksetup -removeDeviceFromBond <hardwareport> <bonddevicename>
networksetup -listBonds
networksetup -showBondStatus <bonddevicename>
```

## Windows notes

Need to set up MINGW on Windows to test makefile

### Bridge notes

( http://stackoverflow.com/questions/17588957/programmatically-create-destroy-network-bridges-with-net-on-windows-7 )

<!-- vim: set autoindent expandtab sw=4 syntax=markdown: -->
