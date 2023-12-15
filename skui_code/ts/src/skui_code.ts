import { run } from "#std/sk_types";

var wasm64 = "/*--NAME--*/";
// prettier-ignore
var modules = [ /*--MODULES--*/];
var extensions = new Map();
/*--EXTENSIONS--*/

export async function launch(text: string) {
  let data = await run(wasm64, modules, extensions);
  console.time("skcode launch");
  data.main([], text);
  console.timeEnd("skcode launch");
}
