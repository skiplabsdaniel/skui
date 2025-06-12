import { run, type EnvInit, type ModuleInit } from "../skipwasm-std/index.js";
import type { GLEnv } from "../skui-opengl/skui_glenv.js";

import { init as runtimeInit } from "../skipwasm-std/sk_runtime.js";
import { init as posixInit } from "../skipwasm-std/sk_posix.js";
import { init as baseInit } from "../skui-base/skui_base.js";
import { init as regexpInit } from "../skui-common/skui_regexp.js";
import { init as commonInit } from "../skui-common/skui_common.js";
import { init as openglInit } from "../skui-opengl/skui_webgl.js";
import { init as fwkInit } from "../skui-fwk/skui_framework.js";
import { init as jsInit } from "../skui-js/index.js";
import { init as debuggerInit, run as runDebugger } from "./module.js";
import { complete as openglComplete } from "../skui-opengl/skui_glenv.js";
import { environment as createEnvironment } from "../skipwasm-std/sk_browser.js";

const modules: ModuleInit[] = [
  runtimeInit,
  posixInit,
  baseInit,
  regexpInit,
  commonInit,
  openglInit,
  fwkInit,
  debuggerInit,
  jsInit,
];

const wasmurl = new URL("./libdebugger_wasm.wasm", import.meta.url);
const extensions: EnvInit[] = [openglComplete];

export async function launch() {
  const data = await run(wasmurl, modules, extensions, createEnvironment);
  const window = (data.environment as GLEnv).window();
  const searchParams = new URLSearchParams(window.location.search);
  const host = searchParams.get("host");
  const port = searchParams.get("port");
  const securred = searchParams.get("s");

  if (port == null) throw new Error("Debug port must be specified");
  if (!port.match(/^[0-9]+$/g)) {
    throw new Error("Debug port must be an integer");
  }
  runDebugger(data.environment, {
    host: host ?? "localhost",
    port: parseInt(port),
    securred: securred == "true",
  });
}
