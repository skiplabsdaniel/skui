import { launch as runEngine, type ModuleInit } from "../skui-engine/index.js";
import { init as initLauncher } from "./launcher.js";

import type { Renderable } from "../skui-engine/types.js";
export * from "../skui-engine/types.js";

const wasmurl = new URL("./libskuieng_launcher.wasm", import.meta.url);

export async function launch(title: string, renderer: Renderable) {
  const init: ModuleInit = (env) => {
    return initLauncher(env).then((res) => res(title, renderer));
  };
  await runEngine("skuieng_launch", wasmurl, [init]);
}
