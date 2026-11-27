# Making Float.negate inlinable

Status: to do, in the skip repository. Not done yet because skip is in use
for other work.

## Problem

`Float.negate` (unary minus on floats, used by `Math.abs`) is an external C
function in the prelude:

```skip
// skiplang/prelude/src/core/primitives/Float.sk
@cpp_extern("SKIP_Float_negate")
native fun negate(): Float;
```

```c
// skiplang/prelude/runtime/string.c
double SKIP_Float_negate(double x) {
  return -x;
}
```

The compiler cannot inline it: every negation is a function call, which also
prevents other optimizations in the calling loop. In MTSDF, replacing
`Math.abs` with an inlined `fabs` (`if (x < 0.0) 0.0 - x else x`) made MSDF
generation about 20% faster (skuimtsdfbench: 1.14 to 0.91 ms per glyph), much
more than the 4% of the samples in `SKIP_Float_negate`: the call also kept
`isEdgeRelevant` from being inlined in `addEdge`.

## What already exists

Commit `17a9b1688` ("Fix Float.negate() to use IEEE 754 sign-bit flip") added
everything the compiler needs except the link to `Float::negate`:

- the `FloatNeg` IR instruction (`compiler/src/IR.sk`);
- its LLVM output, `fneg double` (`compiler/src/AsmOutput.sk`);
- its peephole optimizations, constant folding and double negation
  elimination (`compiler/src/peephole.sk`).

`fneg` flips the sign bit, which is the behavior documented for `negate`
(0.0 to -0.0, and the sign of NaN values).

## Change

1. Map the method to the instruction, next to `Float::toBits`:

   ```skip
   // skiplang/compiler/src/intrinsics.sk
   | "Float::negate" -> this.makeUnaryOp(FloatNeg, call, a[0])
   ```

2. Make the method an intrinsic:

   ```skip
   // skiplang/prelude/src/core/primitives/Float.sk
   @intrinsic
   native fun negate(): Float;
   ```

3. Remove `SKIP_Float_negate` from `prelude/runtime/string.c` once nothing
   uses it (check the wasm runtime and the JS host too).

## Bootstrap

The prelude is compiled by the installed `skc`, which does not know the new
intrinsic yet. Build and install a compiler with step 1 first, then make the
change of step 2:

```bash
cd skip/skiplang/compiler
make install STAGE=0    # long; replaces ~/.local/bin/skc
```

If the bootstrap compiler rejects an `@intrinsic` it does not know, keep the
prelude change for a second round, after the compiler with step 1 is
installed.

## Checks

- Prelude tests on `Float.negate`: 0.0 and -0.0, infinities, NaN sign.
- Native and wasm builds (the wasm backend uses the same LLVM output).
- Performance, with skuimtsdfbench (see `bench/`):
  1. on a test branch of skui, put `Math.abs` back in `mtsdf/src` instead of
     `fabs` (see the commit "Avoid runtime calls for absolute values in
     MTSDF");
  2. `cd mtsdf/bench && skargo b -r && target/host/release/skuimtsdfbench`;
  3. compare with `fabs`: about 0.91 ms per glyph, total ~1720 ms with the
     default options, on the development container (aarch64). With the
     intrinsic, `Math.abs` should be as fast, and `fabs` can be removed.
