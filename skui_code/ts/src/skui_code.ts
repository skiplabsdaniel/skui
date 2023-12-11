import { run } from "#std/sk_types";

var wasm64 = "/*--NAME--*/";
var modules = [
  /*--MODULES--*/
];
var extensions = new Map();
/*--EXTENSIONS--*/

export async function launch(text: string) {
  let data = await run(wasm64, modules, extensions);
  data.main([], text);
}
