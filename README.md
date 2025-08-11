# D-Gate
d-star gateway/repeater software

## Architecture

The main dgate process:
 - accepts command line arguments or config file
 - handles ircDDB connections
 - handles incoming callsign routing or link requests
 - supports arbitary modules (A/B/C..)
 - creates outgoing links from modules to other repeaters/reflectors
 - listens on an PF_UNIX/SOCK_SEQPACKET socket for connections

connections to the dgate process:
 - receive all module data
 - can send d-star packets to a module
 - can control certain features of d-gate

sub-processes connect to the dgate process and adapt d-star data to a
d-star repeater, MMDVM, modem, HT with access point/terminal mode, etc.

software used: libev to handle listening for new events and connections.

### IRC handling
we must be able to handle multiple IRC databases. some database's data
should be of a higher priority. the goal is to keep at least one v4 and
v6 of each repeater in some big cache.

if we have all the IRC connections on one thread, it should be fine to
use with a separate libev event loop. reading from the cache data
structure should be thread-safe.

probably use an std::map with std::shared_mutex. consider deadlocks that
could happen, though it seems unlikely if only one thread (main IRC
thread) will write to it.

## Goals
 - standards-conformant C/C++
 - performant yet extensible
 - use a simple protocol to allow easy creation of interesting programs
   that connect to d-gate
 - run on BSDs, Linux, and other UNIX-likes
 - easy enough to run for Terminal Mode, but reliable enough to be used
   on a repeater

