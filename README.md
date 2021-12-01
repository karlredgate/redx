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

CLI
---

Event loop ...
 * http://jefftrull.github.io/eda/qt/tcl/eventloop/2012/05/25/integrating-qt-tcl-event-loops.html
 * https://www.tcl.tk/community/tcl2019/assets/talk160/Paper.pdf
 * http://free-online-ebooks.appspot.com/tcl/practical-programming-in-tcl-and-tk-ed4/0130385603_ch50lev1sec1.html
 * seach: Tcl_SetMainLoop usage example

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
 * protobuf tools
   * generic read
   * how to represent the structures in procs
 * ebpf tools?  bpf(2)
   * loader
   * compiler
   * peg parser?
   * https://core.tcl-lang.org/tcllib/doc/tcllib-1-17/embedded/www/tcllib/files/apps/pt.html
 * atop
 * clear buf cache - turn on/off swap
 * add capabilities() checks - to reduce root
 * iptstate - track iptables
 * pktstat - netwox - ngrep - ...
 * curl/h2/h3 requests
 * inotify fanotify
 * add event sources
   * Tcl_CreateEventSource(3tcl)
   * https://wiki.tcl-lang.org/page/Tcl_CreateEventSource
 * PCAP
 * zeek?
 * embed CLIPS?
 * sqlite
 * perf_event_open(2)
 * subreaper?  prctl(2) - PR_SET_CHILD_SUBREAPER
 * https://alexanderanokhin.com/tools/digger/
 * dapptrace on osx??
 * digger - https://alexanderanokhin.com/tools/digger/
 * packetdrill
   * https://github.com/google/packetdrill
   * https://groups.google.com/g/packetdrill
   * https://www.usenix.org/system/files/login/articles/10_cardwell-online.pdf
   * https://www.usenix.org/system/files/conference/atc13/atc13-cardwell.pdf
   * https://github.com/ligurio/packetdrill-testcases
   * https://conferences.sigcomm.org/sigcomm/2020/files/slides/epiq/1%20Testing%20QUIC%20with%20packetdrill.pdf
   * https://developpaper.com/packetdrill-a-powerful-tool-for-testing-tcp-stack-behavior/
 * ethtool
 * lshw https://github.com/lyonel/lshw
 * https://upx.github.io/
 * lspci - http://mj.ucw.cz/sw/pciutils/
   * https://en.wikipedia.org/wiki/Lspci
   * http://mj.ucw.cz/download/linux/pci/
 * lsscsi
 * lscpu
 * lv, vg, pv commands
 * dmesg
 * uname
 * packetforge-ng
 * linux hwmon - https://www.kernel.org/doc/html/latest/hwmon/index.html
 * devlink??
 * https://www.kernel.org/doc/html/latest/networking/devlink/index.html
 * devlink api
 * https://lwn.net/Articles/674867/
 * https://github.com/jpirko/prometheus-devlink-exporter
 * https://github.com/jpirko
 * ping and ping6
 * ping sendmsg: "Operation not permitted"
 * https://forums.cpanel.net/threads/problem-with-iptables-sendmsg-operation-not-permitted.423261/
 * ipv6 ping: socket: Permission denied
 * https://github.com/MichaIng/DietPi/issues/3573
 * https://github.com/Microsoft/WSL/issues/69
 * socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6) = -1 EACCES (Permission denied)
 * https://events19.linuxfoundation.org/wp-content/uploads/2017/12/Debugging-Network-Applications-Using-Container-Technology-Pavel-%C5%A0imerda-prgcont.cz_.pdf
 * https://github.com/iputils/iputils/issues/293
 * Darwin - hypervisor framework
 * LLDP
 * OUI lookup
 * packet construction
 * probe for addresses not configured on an intf
 * systemd journal ... uses siphash
 * send API calls easily
 * curl library - or related
 * fstrim - util-linux
 * dpkg -S /sbin/fstrim
 * dpkg -L util-linux
 * atop stuff ...
   * https://atoptool.nl/perftrain.php
   * https://atoptool.nl/extras.php
   * https://atoptool.nl/netatop.php
   * https://atoptool.nl/downloadnetatop.php
   * https://atoptool.nl/
   * https://lwn.net/Articles/387202/
 * whiptail for dialogs...
   * https://askubuntu.com/questions/776831/whiptail-change-background-color-dynamically-from-magenta
   * libnewt
 * taskstats in procfs - and netlink (dom wants to monitor fs per thread)
 * https://www.kernel.org/doc/html/latest/accounting/taskstats-struct.html
 * https://www.kernel.org/doc/html/v5.14/accounting/taskstats.html
 * https://mjmwired.net/kernel/Documentation/accounting/taskstats.txt

2021-10-20
 * userfaultfd(2)
 * ioctl_userfaultfd(2)
 * rtld-audit(7)
 * download pci.ids - and construct data in memory
   * tie to the sysfs entries to show hardware
 * 

2021-11-09
 * iperf3 - add client code ?

## uring

 * https://unixism.net/loti/what_is_io_uring.html
 * https://unixism.net/loti/tutorial/cat_liburing.html
 * https://blogs.oracle.com/linux/post/an-introduction-to-the-io_uring-asynchronous-io-framework

## systemd - journal

 * https://systemd.io/JOURNAL_FILE_FORMAT/
 * https://www.freedesktop.org/wiki/Software/systemd/journal-files/
 * https://github.com/systemd/systemd/blob/main/src/libsystemd/sd-journal/journal-def.h
 * https://www.freedesktop.org/wiki/Software/systemd/json/
 * https://www.freedesktop.org/wiki/Software/systemd/export/
 * https://manpages.debian.org/stretch/systemd-journal-remote/systemd-journal-remote.8.en.html
 * https://www.freedesktop.org/software/systemd/man/systemd-journal-remote.html

## Protobuf

Can I compile protobuf description to a TCL script describing the format.
 * https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
 * http://crockford.com/javascript/
 * http://crockford.com/javascript/tdop/tdop.html
 * http://crockford.com/javascript/tdop/index.html
 * https://github.com/munificent/bantam

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
 * virtualization library...

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
   * https://github.com/Tomas-M/iotop
   * https://haydenjames.io/linux-server-performance-disk-io-slowing-application/
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

## References

 * <https://nelkinda.com/blog/suppress-warnings-in-gcc-and-clang/>

## NetLink

Need an event receiver for NetLink events.

<!-- vim: set autoindent expandtab sw=4 syntax=markdown: -->
