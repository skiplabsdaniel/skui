import type { SKPair, T, SKVector, SKString } from "../skui-base/internal.js";
export type * from "../skui-base/internal.js";

declare const _JSON: unique symbol;
export type IJson<Sub = any, Sub2 = any> = T<typeof _JSON> & {
  sub: Sub;
  sub2: Sub2;
};

declare const _JSNull: unique symbol;
export type JSNull = IJson<typeof _JSNull>;

declare const _JSBool: unique symbol;
export type IJSBool = IJson<typeof _JSBool>;

declare const _JSInt: unique symbol;
export type IJSInt = IJson<typeof _JSInt>;

declare const _JSFloat: unique symbol;
export type IJSFloat = IJson<typeof _JSFloat>;

declare const _JSString: unique symbol;
export type IJSString = IJson<typeof _JSString>;

declare const _JSArray: unique symbol;
export type IJSArray<Sub extends IJson = IJson> = IJson<typeof _JSArray, Sub>;

declare const _JSObject: unique symbol;
export type IJSObject = IJson<typeof _JSObject>;

export type IPartialJSObj = SKVector<SKPair<SKString, IJson>>;
export type PartialJSArray<T extends IJson> = SKVector<T>;
