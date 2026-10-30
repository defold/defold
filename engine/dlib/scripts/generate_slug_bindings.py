#!/usr/bin/env python3
from generate_java_bindings import generate_bindings

if __name__ == "__main__":
    generate_bindings("com.dynamo.bob.slug.generated", "SlugFFM", "SlugSymbols",
                      ("SlugCompileAtlas", "SlugFreeTexture"),
                      ("SlugPath", "SlugImage", "SlugTexture"), ("SlugResult",),
                      ("SLUG_RESULT_OK", "SLUG_RESULT_INVALID_ARGUMENT", "SLUG_RESULT_PATH_ERROR",
                       "SLUG_RESULT_OUT_OF_MEMORY", "SLUG_RESULT_CAPACITY"),
                      "Generate Java 25 FFM bindings for Slug")
