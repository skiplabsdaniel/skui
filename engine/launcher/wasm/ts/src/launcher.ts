import type {
  ptr,
  Nullable,
  Utils,
  Links,
  ToWasmManager,
  Environment,
} from "../skui-engine/index.js";
import * as eng from "../skui-engine/types.js";
import type * as Internal from "../skui-engine/internal.js";

interface ToWasm {
  SKUI_UiEngineJS_getTitle: () => ptr<Internal.SKString>;
  SKUI_UiEngineJS_getRenderer: () => Nullable<ptr<Internal.IJson>>;
}

class LinksImpl implements Links, ToWasm {
  utils!: Utils;
  exports!: object;

  constructor(
    private env: Environment,
    private title: string,
    private renderer: eng.Renderable
  ) {}

  complete(utils: Utils, exports: object) {
    this.utils = utils;
    this.exports = exports;
  }

  SKUI_UiEngineJS_getRenderer() {
    const shared = this.env.shared.get("skuieng") as Nullable<eng.EngShared>;
    if (shared) {
      const state = this.utils.getState("skuieng", () => new eng.State());
      const engUtils = new eng.SkuiEngUtils(
        this.utils,
        this.exports,
        shared.createScope,
        state
      );
      return engUtils.exportValue(
        null,
        null,
        new eng.RenderableElement(this.renderer)
      );
    }
    return null;
  }

  SKUI_UiEngineJS_getTitle() {
    return this.utils.exportString(this.title);
  }
}

class Manager implements ToWasmManager {
  constructor(
    private env: Environment,
    private title: string,
    private renderer: eng.Renderable
  ) {}

  prepare = (wasm: object) => {
    const toWasm = wasm as ToWasm;
    const links = new LinksImpl(this.env, this.title, this.renderer);
    toWasm.SKUI_UiEngineJS_getRenderer =
      links.SKUI_UiEngineJS_getRenderer.bind(links);
    toWasm.SKUI_UiEngineJS_getTitle =
      links.SKUI_UiEngineJS_getTitle.bind(links);
    return links;
  };
}

export function init(env: Environment) {
  return Promise.resolve(
    (title: string, renderer: eng.Renderable) =>
      new Manager(env, title, renderer)
  );
}
