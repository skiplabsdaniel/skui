# Profiling the native views with perf

`perf` needs the `perf_event_open` system call, which a Docker container
cannot use by default: it lacks the `CAP_PERFMON` capability and the default
seccomp profile blocks the call. These are set when the container is created
and cannot be changed afterwards.

Instead of recreating the development container, run `perf` from a second,
privileged container that shares its process namespace.

## Start the perf container

On the host, find the name of the development container:

```bash
docker ps
```

Then start the perf container:

```bash
docker run --rm -it --privileged \
  --pid=container:<container> \
  -v /home/daniel/workspace:/home/daniel/workspace \
  ubuntu:24.04 bash
```

- `--pid=container:<container>` lets `perf` see the processes of the
  development container.
- `--privileged` only applies to the perf container.
- Mounting the workspace at the same path helps `perf` find the binaries and
  their symbols if it cannot read them through `/proc/<pid>/root`.

## Install perf

In the perf container, install the tools of the running kernel (the
`/usr/bin/perf` wrapper requires the exact kernel version, so
`linux-tools-generic` does not work when it is a different version):

```bash
apt-get update
apt-get install -y linux-tools-$(uname -r) linux-tools-common
```

Allow profiling and kernel symbols:

```bash
sysctl kernel.perf_event_paranoid=1
sysctl kernel.kptr_restrict=0
```

These settings are for the kernel shared by all the containers, until Docker
restarts.

## Profile

Start the application in the development container (see VNC.md), then in the
perf container:

```bash
ps aux | grep skuiviews
perf record -g -p <pid> -- sleep 10
perf report
```

For better call stacks through code without frame pointers:

```bash
perf record --call-graph dwarf -p <pid> -- sleep 10
```

To show only the application code, without the OpenGL renderer:

```bash
perf report --dso skuiviews
```

Without a GPU, OpenGL runs on Mesa's software renderer (llvmpipe), so most of
the time is spent in Mesa. Compare the profile with and without drawing to
find the cost of the application itself.

## Stop

Exit the perf container's shell: `--rm` removes it.
