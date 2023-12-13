# PosixScheduling

The program `lets_schedule` serves as an entry point to understand the basics of the POSIX scheduler that is
used in Linux, MacOS, and other UNIX-like operating systems. The current focus is on FIFO- and RR-Scheduling, other
scheduling policies might be added later.

The basic idea is to start a group of pthreads, each using a specific scheduling discipline. These threads exhibit
a typical alternation between a CPU bound burst and an IO burst. Both burst lengths can be specified. The CPU burst
is simulated by doing some computation inside a loop where the number of iterations needed to run for one millisecond
is determined in a calibration phase at the beginning. The IO burst is simulated by a simple usleep(); as such,
bottlenecks in the IO path that might break out threads are not part of this simulation.

The threads are specified by a list of configuration arguments, each argument characterizing a specific thread:

`<Policy>/<Priority>/<ms>cpu/<ms>io`

where `<Policy>` is either `FF` or `RR`, `<Priority>` is a value between 1 and 99 (may depend on the operating system). The final 2 elements are the lengths of the CPU burst and the IO burst. An example configuration string is `FF/42/300cpu/100io` which creates a thread with priority 42 in the FF class that alternates between a 300ms CPU burst and a 100ms IO burst.

The program is started with a list of these configurations:

`lets_schedule <Time> <Config 1> ... <Config n>`

The first argument defines the length of the experiment in seconds.

At the end of the simulation, the mean and standard deviation for the observed CPU and IO burst lengths are listed. All raw measurements are also written to a file `raw_data_<pid>.csv`where `<pid>` is the PID of the program. For convience, a small Python program `plot_csv.py` is part of this repository, to visualize these raw data in a simple diagram.