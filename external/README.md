# External

Some libraries, we need to rebuild when we update to each compiler version.
However, we don't want to build them _every_ time the engine is rebuilt, so we instead
put them into a `build_external` command.

Build the packages, and place them under `defold/packages`

# Modifications

Always keep the original code separate from the modified code, so that it's easy to reason about and update.

Keep any engine changes in a `.patch` file.