# Running the native views with VNC

The native views (`skuiviews`) open an SDL window (X11 on Linux). Inside a Docker container
there is no X server, and on macOS Docker runs Linux in a virtual machine
without access to the GPU. VNC gives a full interactive session (window,
mouse, keyboard) from the container:

```
skuiviews -> Xvfb (virtual X server) -> x11vnc -> VNC client or browser
```

## OpenGL

There is no GPU in the container (no `/dev/dri`): skui draws with OpenGL ES
3.0 through ANGLE (see NATIVE.md), which runs on Mesa's software renderer,
llvmpipe, on the CPU. It is slower than a GPU but fast enough to check user
interactions. It is the same renderer as the offscreen rendering tests of
`opengl/tests`. SDL 3 and ANGLE must be installed in `/opt/sdl3` and
`/opt/angle` (see `docker/Dockerfile`).

## Setup

Inside the container (Ubuntu 24.04):

```bash
sudo apt-get install -y xvfb x11vnc novnc websockify
```

The container must publish the VNC ports. Bind them to the local machine
only, since the setup below has no password:

```bash
docker run ... -p 127.0.0.1:5900:5900 -p 127.0.0.1:6080:6080 ...
```

or in `docker-compose.yml`:

```yaml
ports:
  - "127.0.0.1:5900:5900"
  - "127.0.0.1:6080:6080"
```

### Container already running

Docker cannot publish a port of a running container. Instead, run a small
relay container, which publishes the port and forwards it to the development
container. Docker only resolves container names on user-defined networks, so
put both containers on a dedicated network. On the host:

```bash
docker ps                                   # name of the development container
docker network create vnc
docker network connect vnc <container>      # no restart needed
docker run -d --rm --name vnc-relay --network vnc \
  -p 127.0.0.1:5900:5900 \
  alpine/socat \
  tcp-listen:5900,fork,reuseaddr tcp-connect:<container>:5900
```

`docker logs vnc-relay` shows the connection errors: `Name does not resolve`
when the containers are not on the same network, `Connection refused` when
x11vnc is not running. For noVNC, start a second relay with port 6080. Stop
the relay with `docker stop vnc-relay` (`--rm` removes it).

## Start a session

```bash
# Virtual screen :1, in device pixels.
Xvfb :1 -noreset -screen 0 1600x1000x24 &

# VNC server on port 5900.
x11vnc -display :1 -forever -shared -nopw -rfbport 5900 &

# Optional: browser access with noVNC on port 6080.
websockify --web /usr/share/novnc 6080 localhost:5900 &

# Run a view, from the build directory of views.
mkdir -p -m 700 /tmp/xdg
XDG_RUNTIME_DIR=/tmp/xdg DISPLAY=:1 env -u LD_LIBRARY_PATH ./skuiviews buttons
```

The VNC screen stays black until an application opens a window: Xvfb runs
no desktop or window manager. SDL needs `XDG_RUNTIME_DIR`, a directory
private to the user: without it, it prints `XDG_RUNTIME_DIR is invalid or not
set in the environment`. The image of `docker/Dockerfile` already installs
Xvfb, x11vnc and noVNC; in another container, install them as above.

`skuiviews help` lists the views: `buttons`, `sliders`, `split`, `progress`,
`tabpane`, `text`, `image`, `dropdown`.

Unset `LD_LIBRARY_PATH` when it points to other libraries (for instance the
ones of an editor extension): Mesa must load the system `libstdc++` and EGL
drivers.

## Connect

- Browser: http://localhost:6080/vnc.html
- macOS Screen Sharing: `open vnc://localhost:5900` in a terminal, or in the
  menu bar of the Finder, the Go menu, then Connect to Server (Cmd+K). It
  requires a password: see Password below.
- Any other VNC client: `localhost:5900`

To check the connection from the host, `nc localhost 5900 | head -c 12`
prints `RFB 003.008` when x11vnc answers.

## Pixel ratio

SDL reads the device pixel ratio from the `Xft.dpi` resource (96 dpi is a
ratio of 1). Start Xvfb with `-noreset`, otherwise the resource is lost when
`xrdb` disconnects. To test a HiDPI screen with a ratio of 2:

```bash
echo "Xft.dpi: 192" | DISPLAY=:1 xrdb -merge
DISPLAY=:1 ./skuiviews text
```

The 1600x1000 virtual screen is then 800x500 logical pixels. Run
`DISPLAY=:1 xrdb -remove` to go back to a ratio of 1.

## Password

To protect the session, or for the macOS Screen Sharing client, which does
not connect without a password, create a password and use it instead of
`-nopw` (VNC passwords are limited to 8 characters; leave the user name
empty):

```bash
x11vnc -storepasswd                       # writes ~/.vnc/passwd
x11vnc -display :1 -forever -shared -rfbauth ~/.vnc/passwd -rfbport 5900 &
```

## Stop

```bash
pkill -f skuiviews; pkill x11vnc; pkill websockify; pkill Xvfb
```
