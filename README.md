# d-gate

d-gate is a "D-STAR packet router." The main process, dgate, listens on
a UNIX domain socket and accepts connections. Client programs can
exchange D-STAR packets on arbitrary modules, facilitating the
development and extension of repeater, gateway, and hotspot software. In
this sense, d-gate could also be considered an IPC software.

# Usage

d-gate is not fully ready yet. the source code needs to be changed to
configure it.

d-gate only requires `libev` and `lmdb`. Use `meson` to compile:

```
meson setup builddir
cd builddir
meson compile
```

First run the `dgate` process, then any d-gate software you want.

# Included d-gate software

## d-link
d-link will support linking to DExtra, DCS, and DPlus reflectors and
repeaters. It can be configured to accept DV on any number of modules.

## d-itap
d-itap will support ICOM radios' Terminal/Access Point modes via USB
serial or Bluetooth. Any number of instances can be linked to a module,
allowing quick exchange of D-STAR voice over any link.

## d-ircddb
d-ircddb will support callsign routing on the QuadNet IRCDDB network. It
will listen for incoming callsign routing requests on port 9001/40000
and send the DV to the corresponding module with the desired user.

# TODO
Implement software for ICOM repeaters, and test the software in the wild
to find edge cases that break it.
