# A new incremental UI framework: design proposal

Status: proposal, nothing implemented yet. This document describes a new UI
package, written next to `framework/` (`rui/`, package `skui_rui`, module `RUi` for Reactive UI), which replaces it
step by step. `framework/` keeps working until the new package covers its
users (`engine`, `views`, `sdl`), and is then removed.

## 1. Goals and decisions

Goals:

- Nodes describe *what* the interface is; skins decide *how it looks*. A skin
  can be replaced without touching nodes, and nodes never read the theme.
- Layout is a separate role, as in `engine`: containers are ordinary nodes,
  per-child layout settings belong to the child and are read by the parent.
- Events and state are clearly separated: an event is applied once, by a pure
  reducer, outside of the reactive graph; the graph only derives (layout,
  look, drawing, semantics).
- Depth is computed apart from layout and drawing, and becomes the OpenGL z.
  Changing the depth of a node only recomputes depths.
- Clipping is done by the shaders (rectangles with rounded corners), not with
  GL viewports.
- Accessibility: every node can expose its semantics (role, label, value,
  states, actions).
- A shader system that allows custom shaders, materials and lighting.
- One frozen convention for reactive properties and SKStore usage.
- Incremental at every level: an edit, a hover or a scroll only recomputes
  what depends on it.

Decisions taken:

| Subject | Decision |
|---|---|
| Location | A new package (`rui/`, `skui_rui`, module `RUi`); `framework/` stays until the switch. |
| Core model | Typed descriptions in Skip (the ideas of `engine`, natively typed). The JS API of `engine` becomes a thin serialization layer above. |
| Events | An ordered input queue, pure reducers applied in one transaction at the platform entry, no side effects in the graph. |
| Depth | A separate stacking computation, translated into the GL z at draw time. No Background/Main/Foreground layers. |
| Clipping | Clip shapes resolved per drawable and applied in the shaders. |

Non-goals for the first steps: 3D, animations beyond what timers and
transitions need, right-to-left layout, complex text shaping (see
`font/SHAPING.md`).

## 2. What is wrong today

A short summary of the analysis of `framework/` (references are to the `vnc`
branch).

- **Everything on the widget.** `Ui.Widget` (`framework/src/widgets/BaseTypes.sk:253`)
  defines layout (`getNode`), events (`getHoverHandler`...), pre-drawing
  (`computeDrawInfo`) and drawing (`computeLayer`). `Button` reads the theme
  24 times, including in `layout` (`widgets/control/Button.sk:28-79`), with
  hard-coded sizes (36, 56, 4.0). `Decoration` mixes padding (layout) and
  painting. Only pure containers such as `Flex` are clean.
- **Theme in an untracked global.** `context.getTheme()` reads the global
  `"Ui.Theme"`; `getGlobal` is not a tracked read, and the tokens are closed
  enums (`Theme.sk:59-164`).
- **Events stored as state.** An event is written as a `Todo(command, tick)`
  in a single slot, then folded into the previous output read untracked from
  `pre` directories (`framework/src/skfs/Utils.sk:36-84`). A key press takes
  2 to 3 engine passes and 2 round trips through the platform event loop.
  Maps have side effects (`FWKKeys.execute(Next())`, `setTodo`, timers,
  clipboard), commands are lost or merged differently per widget, every
  command goes through JSON, and the same 7-directory chain is copied, with
  differences, in input, monospace, tree, tabs and slider.
- **Depth and clipping through viewports.** Overlap is only possible by
  creating a viewport (`Stack`, menus, dialogs); the order within a viewport
  is three global passes (Background, Main, Foreground) and then the order of
  store keys. Clipping is `glViewport` only: integer, rectangular, never
  rounded. Viewports also carry separate layout roots, a two-pass constraint
  feedback, scrolling, culling and hit testing.
- **Reactive conventions from 2023.** The property layer predates the move to
  SKStore: an `isUntracked` flag passed at runtime (299 uses), three ways to
  build directory names, `StrID` keys, handles kept in globals, 1029 lines of
  per-type property boilerplate.
- **Shaders.** One draw call per drawable, uniforms by index with the name
  table duplicated in Skip, C++ and TypeScript, and no entry point for custom
  shaders: the GLSL generator of `opengl/glsl` (materials, lights) is only
  used by the `sdl` demos.

## 3. Overview

The framework is a chain of SKStore stages. Each stage is a set of
directories keyed by node identity, so a change only flows through the keys
that depend on it.

```
inputs (props, input state, window, theme)
  └→ descriptions ──→ element tree (identity, parent, children)
        ├→ resolved styles (skin + theme + node state)
        │     └→ layout (sizes, child positions)
        │           ├→ geometry (absolute rects, transforms, clips)
        │           ├→ paint (drawables per element, in local coordinates)
        │           ├→ semantics (accessibility tree)
        │           └→ hit-test index
        └→ depth (stacking order → z)

renderer: paint + geometry + clips + depth → sorted draw list → GL
platform input → routing (hit test, focus) → reducers → input/state dirs
```

The renderer resolves positions, scroll offsets, clips and z at draw time
from small directories, so scrolling or restacking does not recompute any
drawable.

## 4. Nodes

### 4.1 Descriptions

A node is a typed, immutable description. Every field that can change is a
`Value<T>`: either a constant or a reactive property (`engine`'s `Box`, made
generic and typed).

```skip
base class Value<+T: frozen> {
  children =
  | Const(value: T)
  | Bound(prop: Prop<T>)
}

class Button{
  key: ?Key = None(),
  text: Value<String>,
  icon: Value<?Icon> = Const(None()),
  kind: Value<ButtonKind> = Const(Standard()),
  enabled: Value<Bool> = Const(true),
  onPress: Action,
  parentData: ?ParentData = None(),
} extends Node
```

- A node has no layout, drawing or event code: those belong to its layout,
  skin and behaviour (sections 5 to 7), found by node type.
- `parentData` is the per-child layout setting (`engine`'s `configuration`),
  typed by the parent that reads it (`FlexData{flex, fit}`,
  `StackData{alignment}`...). A parent ignores data that is not its own.
- Components are functions from props to a node, as in `engine`:
  `Component{render: Context ~> Node}`. They are how applications compose
  nodes; they do not get special treatment by the framework.

### 4.2 Elements and identity

The element tree is the descriptions expanded into one entry per node, keyed
by an `ElementId` (the path of keys from the root). Children get their key
from `key` when given, from their index otherwise; content hashes (today's
`engine`) are not used, so two equal children stay distinct.

An element record holds its node, its parent and its children ids. Expanding
a component reads its props: when a prop changes, only that component's
subtree is expanded again.

### 4.3 Kinds of nodes

| Role | Examples | Has |
|---|---|---|
| Layout | `Flex`, `Stack`, `Wrap`, `Padding`, `Align`, `Constrained`, `AspectRatio` | a layout |
| View | `Text`, `Icon`, `Image`, `Shape` | a layout (intrinsic size) and a skin |
| Control | `Button`, `Toggle`, `Slider`, `Input`, `Tree`, `Tabs` | a layout, a skin, a behaviour, a state |
| Structure | `Scroll`, `Clip`, `Overlay`, `Layer`, `FocusScope`, `Semantics` | geometry effects (offset, clip, depth, focus, semantics) |

Decoration (backgrounds, borders, shadows) is no longer a node with padding:
backgrounds and borders are drawn by skins; padding is a layout node.

## 5. Layout

Layout keeps the constraint model of `framework` and `engine`, which is
already clean (`framework/src/rendering/Constraints.sk`): constraints go
down, the sizes of the children come back up to their parent, and the parent
places its children. All of it happens in one pass, from the root.

```skip
base class Layout<N: Node> {
  fun layout(
    context: mutable SKStore.Context,
    node: N,
    style: ResolvedStyle,
    constraints: BoxConstraints,
    children: Children,      // child.layout(constraints) -> size
  ): LayoutResult;           // size + child offsets (+ baseline)
}
```

- A layout reads the node and its **resolved style** (section 6.2), never the
  theme: the minimum size of a button comes from its style, which comes from
  its skin.
- Layout results are memoized per (element, constraints) in a lazy directory, as
  today (`KConstraints`), but the directory is created once per element
  type, not each time a widget value is rebuilt.
- Layout produces sizes and offsets relative to the parent. Absolute
  positions, scroll translations and clips are computed in a separate
  geometry stage.
- A layout never reads layout results of the previous pass. Today, viewports
  get their constraints from the previous pass (`vpConstraintsPath`, read
  through its `pre` copy, `framework/src/framework/Pipeline.sk:244-248`), and
  `Monospace`, `Table` and `Tree` read their visible range from the previous
  node infos (`FWKPipeline.gNodeInfo`): each of them needs one more pass to
  settle. Here, everything a layout needs comes down in the same pass.
- There are no separate layout roots: overlays (menus, tooltips, dialogs)
  are laid out against the window, from their anchor (section 8.2).

### 5.1 Visible area and virtualized content

The content of a scroll area must know what is visible, so that long
contents (lists, tables, trees, text editors) only build and lay out the
visible part. Both parts of the visible area are known before the content is
laid out:

- the size of the `Scroll` element comes from the constraints of its parent,
  computed on the way down;
- the scroll offset is element state, changed by reducers (wheel, scroll
  bar, keyboard), not a layout result.

```
Scroll.layout(constraints):
  size    = constraints.biggest
  visible = Rect(scrollOffset, size)           // known on the way down
  content.layout(unbounded on the scroll axis) // ordinary content
  // or, for a virtualized content:
  lazyList.layout(width, visiblePages(visible))
```

Virtualization is a contract between a `Scroll` and its direct content, as
Flutter's `ListView` is a scrollable with a list built for it. A virtualized
list (`LazyList`, the base of tables, trees and text editors) gets an item
count and a function from index to node instead of an array of children. Its
layout takes the range of visible pages from its `Scroll`, expands and lays
out only the items of those pages (plus a margin), and computes its total
extent (for the scroll bar) from fixed, estimated or cached item sizes.

Other layouts never get the visible area: it is not part of their
constraints, so scrolling does not invalidate their memoized results.

To keep scrolling cheap, the visible range is rounded to pages, as
`Monospace` and `Table` already do: the layout of a list depends on the
range of visible pages, not on the exact offset. Scrolling within a page
only changes the translation applied at draw time; reaching a new page lays
out the list again, where only the entering items are new (items are
memoized by key).

A scroll area whose size depends on its virtualized content would be a
cycle: such a `Scroll` takes the size its parent gives it.

## 6. Skins and themes

### 6.1 Skin

A skin gives the look of one node type. It does not change the structure of
the tree.

```skip
base class Skin<N: Node, S: State> {
  // Metrics and look values used by layout and paint.
  fun style(node: N, state: S, theme: Theme): ResolvedStyle;
  // Drawables in the local coordinates of the element, in painting order.
  fun paint(node: N, state: S, style: ResolvedStyle, size: Size): Array<Drawable>;
}
```

- `paint` returns drawables in the element's local coordinates, in painter
  order (background, content, focus ring...). There are no global
  Background/Main/Foreground layers: within an element the order is the
  array order, and between elements it is the depth order (section 8).
- Interaction state (hovered, pressed, focused) is part of `S`, so a hover
  only re-runs the skin of the hovered element.
- A skin can be stateless code or parameterised by data (a "Material skin"
  with its colors and radii), so several looks can share code.

### 6.2 Theme

A theme is data stored in an SKStore directory (tracked, unlike today's
global), made of:

- tokens: colors, fonts, sizes, radii, shadows, keyed by name or typed key,
  open instead of the closed `UiTheme` enums;
- a skin registry: node type → skin, with an optional variant (`kind`,
  `style` field of the node).

`style(node, state, theme)` is memoized per element: changing one token only
recomputes the styles that read it, then the layouts and paints that depend
on those styles. A subtree can use another theme (`ThemeScope` node).

## 7. Events and state

### 7.1 Principles

- **Facts and events are different.** Facts are the current input state:
  pointer position, buttons and keys down, focused element, window size.
  They are input directories, and what depends on them is derived in the
  graph: *hover is derived* from the pointer position and the geometry, so it
  is right even when the layout moves under a still pointer.
- **Events are applied once.** A click, a key press, a text insertion or an
  accessibility action is put in an ordered queue and applied by reducers,
  outside of the graph. They are never stored as state.
- **No side effects in the graph.** Maps only compute. Timers, clipboard
  access, callbacks to the application and focus changes are effects
  returned by reducers and run by the runtime after the transaction.

### 7.2 Processing an event

At the platform entry (JS or SDL), for each event in order:

1. **Route.** Hit test against the geometry of the last committed frame
   (pointer events), or take the focused element (keyboard events). The
   route is the path from the root to the target, for capture and bubbling.
2. **Reduce.** Each behaviour on the route gets the event:

   ```skip
   base class Behavior<N: Node, S: State> {
     fun reduce(node: N, state: S, event: Event): Reduced<S>;
     // Reduced = new state, handled flag, effects
   }
   ```

   Reducers are pure: they read the committed state of their element (plain
   reads, outside of any map) and return the new state.
3. **Commit.** All writes of the event (element states, facts, application
   props) are done together, then one `context.update`.
4. **Run effects** (callbacks, timers, clipboard, focus moves). Effects that
   produce new input come back as events in the queue.

One event therefore costs one update, with no `pre` directories and no tick
comparisons. Several events of one frame (fast typing, pointer moves) can be
reduced in a row and committed once.

### 7.3 State

- **Element state** (pressed, caret, selection, scroll position, expanded
  items) is a typed record per element, in one state directory keyed by
  element id. It is written only by reducers.
- **Application state** lives in props. A control is *controlled* when its
  value is a `Bound` prop: its reducer returns a write to that prop, and the
  control displays the prop. With a `Const` value, the control keeps the
  value in its own state.
- Large states are split into keys so that a change rewrites only what
  changed: the text of a multi-line input is a directory of lines, not one
  file (today's `Lines` file is rewritten on every key).
- Time is part of the event (`event.time`), never read from the clock in the
  graph, so a replay gives the same result. Key repeat comes from the
  platform; timers (caret blinking) are effects that post `Timer` events.

### 7.4 Focus

Focus is a fact (the focused element id). Tab order is derived from the
semantics tree (section 9) and `FocusScope` nodes; moving focus is an effect
of the reducers of Tab, Shift+Tab, clicks and accessibility actions.

## 8. Depth and clipping

### 8.1 Depth

Depth is computed in its own stage, from the element tree only:

- Each element has a **stacking layer** (`Base`, `Overlay`, `Modal`,
  `Tooltip`, `Debug`, or a number) and an order within its parent. Elements
  are stacked in tree pre-order within their layer; `Layer` nodes start a new
  stacking context.
- The result is one order index per element, in a depth directory. The
  renderer turns it into the GL z when it submits the drawables.
- Paint does not depend on depth: moving a dialog to the front or opening a
  menu only recomputes the depth directory, then the draw list order.

**Transparency.** Almost every drawable has antialiased, translucent edges
(SDF shapes, MSDF text, shadows), so a depth buffer cannot replace ordering:
an edge written first hides what is drawn behind it later. The renderer
therefore:

- sorts the draw list by (z, element order, drawable index) and draws back to
  front with depth testing off (painter's algorithm) — the first version;
- later, may draw fully opaque interiors front to back with depth writes,
  then translucent drawables back to front with depth test on and depth
  writes off, to reduce overdraw.

The z is also written to `gl_Position.z`, so both modes use the same shaders.
The WebGL context must be created with `depth: true` for the second mode.

### 8.2 Clipping

- A `Clip` node, or a skin (`clipsChildren` in its style: a dropdown body, a
  rounded card), defines a clip shape for its descendants: a rectangle with
  corner radii, in the coordinates of the element.
- The geometry stage resolves, for each element, its **clip id**: the
  intersection of the clips of its ancestors. Clip shapes live in a clip
  directory; drawables only reference the id.
- The shaders apply the clip in the fragment shader: the output alpha is
  multiplied by the coverage of a rounded box at `gl_FragCoord`, with the
  same one-pixel antialiasing as the shapes. A shared GLSL prelude provides
  the function, so every program gets it, including custom ones. The MSDF
  shader must apply it after its `fwidth` computations, without `discard`.
- Scrolling is a translation of the content: a `Scroll` node gives its
  content a transform id. Scrolling changes one transform and the visible
  set, not the drawables.
- Culling and hit testing use the same clips: an element outside its clip is
  neither drawn nor hit.
- Overlays (menus, tooltips, dialogs) are portals: they are anchored to an
  element but belong to an overlay layer at the root, so they escape the
  clips of their anchor.
- GL viewports are no longer used for content; `glScissor` stays available
  as a fallback for user shaders that do not include the prelude.

## 9. Accessibility

- Each element can expose semantics: role (button, checkbox, text field,
  list, tree item, dialog...), label, value, states (checked, expanded,
  disabled, selected, focused), actions (press, increment, set value,
  scroll, focus) and relations (labelled by, controls).
- Controls get their semantics from their behaviour (a `Button` is a button
  with a press action); `Semantics` nodes add or override them for
  applications.
- The semantics tree is derived in the graph, keyed by element id, with the
  absolute bounds from the geometry stage. Elements without semantics are
  merged into their parent, as in Flutter.
- Platform bridges consume the changes of the semantics directory:
  - web: a hidden DOM mirror with ARIA roles and attributes;
  - native: AT-SPI (Linux), NSAccessibility (macOS), UI Automation
    (Windows), later.
- Accessibility actions come back as events in the input queue and go
  through the same reducers as pointer and keyboard input.
- The focus order and keyboard navigation use the same tree.

## 10. Rendering and shaders

### 10.1 Drawables

A drawable keeps today's role (backend-neutral description of a primitive),
with three additions resolved at draw time: the element it belongs to (for
its transform), its clip id and its z (from the element's depth).

### 10.2 Materials

Custom shaders and lighting are built on the GLSL generator of
`opengl/glsl`, which already composes shapes, colors and lights:

- A **material** is a frozen, hashable value: vertex and fragment chunks
  (GLSL package functions), a typed uniform schema, the attributes it needs
  and a blend mode. The program cache key is the material hash, the GLSL
  version and its defines (today's `PrgKey` memoization).
- Built-in drawables (rectangles, rounded rectangles, shadows, gradients,
  MSDF text, images) become materials, so custom and built-in shaders share
  one path, with the clip and z prelude.
- `DrawableSupport.custom(bounds, material, values)` is the entry point for
  skins and applications.
- Uniforms are typed (`Uniform<T>`) and found by name once per program; the
  index tables duplicated in `opengl/src/program/BaseTypes.sk`,
  `opengl/native/src/GL.cpp` and `opengl/ts/wasm/src/skui_glcontext.ts` are
  removed.
- **Lighting**: the lights are a theme value (ambient, directional, point),
  uploaded once per frame (a uniform block with ES 3.00, uniforms with
  GLSL 1.00). Materials that use lighting read them from there instead of
  per-drawable uniforms. Normals come from the relief functions of the GLSL
  package or from the gradient of an SDF (bevelled text and shapes).
- Later: instancing for rectangles, shadows and glyphs (one draw call per run
  of text instead of one per glyph), and effects rendered in offscreen
  passes (blur, glow), on top of the offscreen framebuffers that already
  exist.

The `shader-updates` branch only holds an older box shadow, replaced since by
the shadow of `vnc`; its commented GLSL (per-corner radii, `fwidth`
antialiasing, MSDF lighting from the SDF gradient) are ideas for materials.

### 10.3 Frames

Today every drawable is drawn again at each frame. With the draw list sorted
by the renderer, a frame is only needed when the draw list, a transform, a
clip, a depth or an animated material changes.

## 11. Reactive conventions

These rules apply to the new package from its first line.

1. **Storage.** Typed `SKStore.EHandle<K, V>` for inputs and derived data,
   `SKStore.LHandle` for data computed on demand. Create them only with
   `context.mkdir`, `EHandle.map` (and the other `EHandle` combinators) and
   `LHandle::create`.
2. **Reads.** `get` / `maybeGet` for tracked reads, `unsafeGet` /
   `unsafeMaybeGet` for untracked ones, as separate calls. No runtime
   `isUntracked` flag. Untracked reads are only allowed outside of the graph
   (reducers, renderer, platform bridges).
3. **Writes.** Only at the platform entry (reducer commits, window and theme
   changes), never inside a map. A write inside a map is a bug, reported by an
   invariant, not postponed.
4. **Names.** Directory names are built with `DirName.sub` from one root per
   module: `/rui/<module>/<stage>/`, lowercase, `-` between words.
5. **Keys.** `SKStore.IID`, `SKStore.SID`, `SKStore.UnitID` for single-value
   directories, `ElementId`, and domain keys only where needed. No `StrID`.
6. **Properties.** `Prop<T>` with `get(context)`, `maybeGet(context)` and
   `set` (allowed only at the platform entry); one generic implementation,
   no per-type modules. `Value<T>` for node fields.
7. **Context.** Passed explicitly, always named `context`.
   `SKStore.getWorkingContext()` only at FFI entry points.
8. **Globals.** Only for non-reactive configuration, one prefix (`RUi.`),
   never for handles or reactive data. The theme is a directory.
9. **Handle variables** end with `Hdl` (`elementsHdl`, `layoutHdl`).
10. **No `pre` directories.** No stage reads results of the previous update
    (`EHandle.pre`, `context.pre`, `makePreDir`): what a stage needs comes
    from the current update, and what must outlive an update is element
    state written by reducers. Once `framework/` is removed, nothing in skui
    uses `pre` any more, and SKStore can stop copying every changed
    directory into `X/pre` at each update (`EagerDir.updatePre`).

## 12. Engine and JavaScript

- The descriptions of section 4 are the core. The JS API of `engine`
  (`skui.flex({...})`, `scope.create`, `scope.combine`) is kept, as a
  serialization layer: `extension class X extends SObject` on the typed
  nodes, as `engine/src/serialization/` does today.
- `Value<T>` replaces `Box<T>`; JS props map to `Prop<T>`.
- JS callbacks (`onPress`) are effects run after the commit (section 7.2),
  not calls from inside the graph.

## 13. Migration plan

Each step ends with tests (offscreen GL images for drawing, unit tests for
reducers and layout) and is usable on its own.

1. **Package and conventions.** `rui/` with its Skargo.toml (`skui_rui`, module `RUi`), the reactive
   helpers of section 11 (`Prop`, `Value`, directory naming), and their
   tests.
2. **Renderer.** Drawables with element, clip id and z; the clip prelude in
   all existing programs; a sorted draw list. Offscreen tests: rounded clip
   of each primitive, ordering of translucent drawables. This step is in
   `opengl/` and can be used by `framework/` too.
3. **Core.** Descriptions, element tree, layout protocol, `Flex`, `Stack`,
   `Padding`, `Align`, `Constrained`, geometry stage, depth stage, paint of
   plain shapes. A first `sdl` example on top of it.
4. **Skins and theme.** Theme directory, skin registry, resolved styles,
   `Text` and `Icon` (reusing `font` and `mtsdf`), `Label`.
5. **Input.** Input queue, routing, reducers, commits, effects, focus,
   derived hover; `Button` and `Toggle`, with their semantics.
6. **Structure.** `Scroll`, `Clip`, overlays (menus, tooltips, dialogs),
   culling.
7. **Accessibility.** Semantics tree and the web bridge.
8. **Materials.** Material abstraction, built-in drawables as materials,
   `DrawableSupport.custom`, lights in the theme.
9. **Controls.** `Slider`, `Input`, multi-line text (lines as keys), `Tree`,
   `Tabs`, `Table`, `Dropdown`.
10. **Switch.** `engine`'s JS layer, `views` and `sdl` on the new package;
    removal of `framework/`.

## 14. Open questions

- Whether `common/` (geometry, drawables, `DrawableSupport`) is kept as is or
  partly moved into the new package.
- Text editing: how much of `framework/src/widgets/control/monospace` can be
  reused once its state is split into keys and its reducers extracted.
- Platform accessibility on native targets, and which one first.
- Instancing and the second depth mode (opaque front to back): only if
  measurements show they are needed.
