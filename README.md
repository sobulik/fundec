# Description

An illustrative example of functional decomposition using message passing interface.

It only searches through an array and looks up an integer passed as a parameter.
Contrary to more common domain decomposition, the master process distributes
chunks of work to slave processes based on their availability.

Macro RND, if defined, randomizes the search time a bit for better observation.

# Requirements
- C++ compiler
- MPI tool (mpiCC, mpirun)
- GNU Make

# Build

```sh
make
```

# Run

```sh
make run
```
Tested with [Open MPI 1.1.1](https://www.open-mpi.org/software/ompi/v1.1/).
