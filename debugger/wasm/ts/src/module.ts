import type { Nullable } from "../skiplang-std/index.js";
import type {
  ptr,
  Links,
  Utils,
  ToWasmManager,
  Environment,
  Shared,
} from "../skipwasm-std/index.js";

import type * as Internal from "./internal.js";

interface FromWasm {
  skui_fetch_result(
    url: ptr<Internal.String>,
    method: ptr<Internal.String>,
    headers: ptr<Internal.String>,
    body: Nullable<ptr<Internal.String>>,
    executor: ptr<Internal.String>,
    time: bigint,
    result: ptr<Internal.FetchResult>
  ): void;

  sk_create_result(
    status: bigint,
    headers: Nullable<ptr<Internal.String>>,
    payload: Nullable<ptr<Internal.String>>,
    error: Nullable<ptr<Internal.String>>
  ): ptr<Internal.FetchResult>;

  sk_debugger_launch(url: ptr<Internal.String>): void;
}

interface ToWasm {
  skui_fetch(
    wId: bigint,
    url: ptr<Internal.String>,
    method: ptr<Internal.String>,
    headers: ptr<Internal.String>,
    body: Nullable<ptr<Internal.String>>,
    timeout_s: bigint,
    executor: ptr<Internal.String>,
    time: bigint
  ): void;
}

export class DShared implements Shared {
  getName = () => "debugger";

  constructor(
    public readonly run: (host: string, port: number, securred: boolean) => void
  ) {}
}

function errorMessage(error: unknown): string {
  if (error instanceof Error) return error.message;
  if (typeof error == "string") return error;
  if (typeof error == "number") return error.toString();
  if (typeof error == "boolean") return error.toString();
  return JSON.stringify(error, Object.getOwnPropertyNames(error));
}

class LinksImpl implements Links {
  utils!: Utils;
  exports!: FromWasm;

  constructor(private readonly env: Environment) {}

  skui_fetch(
    _wId: bigint,
    url: ptr<Internal.String>,
    method: ptr<Internal.String>,
    headers: ptr<Internal.String>,
    body: Nullable<ptr<Internal.String>>,
    timeout_s: bigint,
    executor: ptr<Internal.String>,
    time: bigint
  ): void {
    const jsUrl = this.utils.importString(url);
    const jsMethod = this.utils.importString(method);
    const jsHeaders = this.utils.importString(headers);
    const jsBody = body ? this.utils.importOptString(body) : null;
    const jsExecutor = this.utils.importString(executor);
    const exec = (
      status: bigint,
      headers: Nullable<string>,
      payload: Nullable<string>,
      error: Nullable<string>
    ) => {
      this.utils.runWithGc(() => {
        const sk_result = this.exports.sk_create_result(
          status,
          headers != null ? this.utils.exportString(headers) : null,
          payload != null ? this.utils.exportString(payload) : null,
          error != null ? this.utils.exportString(error) : null
        );
        this.exports.skui_fetch_result(
          this.utils.exportString(jsUrl),
          this.utils.exportString(jsMethod),
          this.utils.exportString(jsHeaders),
          jsBody ? this.utils.exportString(jsBody) : null,
          this.utils.exportString(jsExecutor),
          time,
          sk_result
        );
      });
    };

    fetch(jsUrl, {
      method: jsMethod,
      body: jsBody,
      headers: JSON.parse(jsHeaders),
      signal: AbortSignal.timeout(Number(timeout_s) * 1000),
    })
      .then((response) => {
        const res: Promise<[Response, string | null]> = response.ok
          ? response.text().then((txt) => [response, txt])
          : Promise.resolve([response, null]);
        return res;
      })
      .then((res) => {
        const [response, text] = res;
        exec(
          BigInt(response.status),
          JSON.stringify(response.headers),
          text,
          text != null ? null : response.statusText
        );
      })
      .catch((exc: unknown) => exec(BigInt(0), null, null, errorMessage(exc)));
  }

  run(host: string, port: number, securred: boolean) {
    this.utils.runWithGc(() => {
      const scheme = securred ? "https" : "http";
      const url = `${scheme}://${host}:${port}`;
      this.exports.sk_debugger_launch(this.utils.exportString(url));
    });
  }

  complete(utils: Utils, exports: object) {
    this.utils = utils;
    this.exports = exports as FromWasm;
    this.env.shared.set("debugger", new DShared(this.run.bind(this)));
  }
}

class Manager implements ToWasmManager {
  constructor(private readonly env: Environment) {}

  prepare = (wasm: object) => {
    const toWasm = wasm as ToWasm;
    const links = new LinksImpl(this.env);
    toWasm.skui_fetch = links.skui_fetch.bind(links);
    return links;
  };
}

export function init(env: Environment) {
  return Promise.resolve(new Manager(env));
}

export function run(
  env: Environment,
  entrypoint: { host: string; port: number; securred: boolean }
) {
  (env.shared.get("debugger") as DShared).run(
    entrypoint.host,
    entrypoint.port,
    entrypoint.securred
  );
}
