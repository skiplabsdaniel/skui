
## Now text gliph draw is no more related to its positioning, split items draw from its to avoid unecesary recompute

- Remove offset drawable 
- Manage depth with opengl

## Use cliping mechanisme instead of opengl viewports.

## Remove heavy message mecanisme, use Postponable for command management.

## Native split skstore logic from X11 events logic to easyly reuse it with other window manager

## Create MacOS Version using Metal

## Wasm: Uses Shared worker when it's possible

## Simplify property mechanisme and use it instead of controller

## Simplify engige widget create to allow DOM/CSS like creation

- Add simple node notion draw / event interaction to easily create composed widget
- Create really basic widgets: TextField / TextArea / Document / StyledText
- Create sane style mechanisme

## Manage accesibility

- Add accessibility property to widget
- Or add accessibility wrapper widget
  