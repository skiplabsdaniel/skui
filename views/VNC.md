# Running the native views with VNC

The native views (`skuiviews`) open an X11 window. Inside a Docker container
there is no X server, and on macOS Docker runs Linux in a virtual machine
without access to the GPU. VNC gives a full interactive session (window,
mouse, keyboard) from the container:

```
skuiviews -> Xvfb (virtual X server) -> x11vnc -> VNC client or browser
```

## OpenGL

There is no GPU in the container (no `/dev/dri`): OpenGL is Mesa's software
renderer, llvmpipe (OpenGL 4.5, GLSL 4.50), running on the CPU. It supports
the skui shaders; it is slower than a GPU but fast enough to check user
interactions. It is the same renderer as the offscreen rendering tests of
`opengl/tests`.

Check it with:

```bash
glxinfo -B | grep "OpenGL renderer"   # from mesa-utils, inside Xvfb
```

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

## Start a session

```bash
# Virtual screen :1, in device pixels.
Xvfb :1 -screen 0 1600x1000x24 &

# VNC server on port 5900.
x11vnc -display :1 -forever -shared -nopw -rfbport 5900 &

# Optional: browser access with noVNC on port 6080.
websockify --web /usr/share/novnc 6080 localhost:5900 &

# Run a view, from the build directory of views.
DISPLAY=:1 env -u LD_LIBRARY_PATH ./skuiviews buttons
```

`skuiviews help` lists the views: `buttons`, `sliders`, `split`, `progress`,
`tabpane`, `text`, `image`, `dropdown`.

Unset `LD_LIBRARY_PATH` when it points to other libraries (for instance the
ones of an editor extension): Mesa must load the system `libstdc++` and EGL
drivers.

## Connect

- Browser: http://localhost:6080/vnc.html
- macOS VNC client: Finder, Go > Connect to Server, `vnc://localhost:5900`
- Any other VNC client: `localhost:5900`

## Pixel ratio

The X11 window manager of skui reads the device pixel ratio from the
`Xft.dpi` resource (96 dpi is a ratio of 1), when the window is created.
To test a HiDPI screen with a ratio of 2:

```bash
echo "Xft.dpi: 192" | DISPLAY=:1 xrdb -merge
DISPLAY=:1 ./skuiviews text
```

The 1600x1000 virtual screen is then 800x500 logical pixels. Run
`DISPLAY=:1 xrdb -remove` to go back to a ratio of 1.

## Password

To protect the session, create a password and use it instead of `-nopw`:

```bash
x11vnc -storepasswd                       # writes ~/.vnc/passwd
x11vnc -display :1 -forever -shared -rfbauth ~/.vnc/passwd -rfbport 5900 &
```

## Stop

```bash
pkill -f skuiviews; pkill x11vnc; pkill websockify; pkill Xvfb
```
