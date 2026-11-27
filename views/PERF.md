# Profiling the native views with perf

`perf` needs the `perf_event_open` system call, which a Docker container
cannot use by default: it lacks the `CAP_PERFMON` capability and the default
seccomp profile blocks the call. These are set when the container is created
and cannot be changed afterwards.

Instead of recreating the development container, run `perf` from a second,
privileged container that shares its process namespace. It is set up as
root, then used as the same user as the development container (uid 1000),
so that the files it writes are not owned by root.

## Start the perf container

On the host, find the name of the development container:

```bash
docker ps
```

Then start the perf container in the background:

```bash
docker run -d --rm --name perf --privileged \
  --pid=container:<container> \
  -v /home/daniel/workspace:/home/daniel/workspace \
  ubuntu:24.04 sleep infinity
```

- `--pid=container:<container>` lets `perf` see the processes of the
  development container.
- `--privileged` lifts the seccomp restriction. It only applies to the perf
  container.
- Mounting the workspace at the same path helps `perf` find the binaries and
  their symbols if it cannot read them through `/proc/<pid>/root`.

## Install perf, as root

Install the tools of the running kernel (the `/usr/bin/perf` wrapper
requires the exact kernel version, so `linux-tools-generic` does not work
when it is a different version):

```bash
docker exec perf bash -c 'apt-get update && apt-get install -y linux-tools-$(uname -r) linux-tools-common'
```

Allow users to profile their own processes, and show kernel symbols:

```bash
docker exec perf sysctl kernel.perf_event_paranoid=1 kernel.kptr_restrict=0
```

These settings are for the kernel shared by all the containers, until Docker
restarts.

## Open a shell as uid 1000

```bash
docker exec -it -u 1000:1000 -w /home/daniel/workspace perf bash
```

With `perf_event_paranoid=1`, a user can profile the processes with the same
uid: the application in the development container also runs as uid 1000.
The shell has no capabilities, even in a privileged container, and the image
has neither `sudo` nor Docker: an agent running in it cannot become root.
Only `docker exec` on the host can.

In the `ubuntu:24.04` image, uid 1000 is the user `ubuntu`, whose home is
`/home/ubuntu`: only the uid matters for `perf` and file ownership.

## Profile

Start the application in the development container (see VNC.md), then in the
uid 1000 shell of the perf container:

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

```bash
docker stop perf
```

`--rm` removes the container.
