import type { T, SKVector, SKArray } from "../skiplang-std/internal.js";
export type * from "../skiplang-std/internal.js";

declare const _Range: unique symbol;
export type Range = T<typeof _Range>;

export type RangesVector = SKVector<SKArray<Range>>;
export type RangesArray = SKArray<SKArray<Range>>;
