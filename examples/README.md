# saramOS Examples

Standalone examples have been moved into the `apps/` directory so that every
application can be built on top of the shared `os/default` image.

See:

- `os/default/` — the minimal default OS image
- `apps/example/game/sudoku/` — representative example app (default `APP_DIR`)
- `Makefile` at the project root — build any app with `make APP_DIR=<path>`
