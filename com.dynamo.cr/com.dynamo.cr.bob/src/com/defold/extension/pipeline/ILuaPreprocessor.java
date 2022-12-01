package com.defold.extension.pipeline;

/**
 * Interface for Lua source code preprocessing prior to compilation.
 * We will scan for public non-abstract classes that implement this interface inside the com.defold.extension.pipeline
 * package and instantiate them to transform Lua source code. Implementors must provide a no-argument constructor.
 */
public interface ILuaPreprocessor {
    /**
     * Apply Lua source code preprocessing prior to compilation.
     * @param source The unprocessed Lua source code.
     * @param path The path to the resource containing the unprocessed source inside the project.
     * @param buildVariant The build variant. Typically, "debug", "release", or "headless".
     * @return The processed Lua source code.
     * @throws Exception In case of an error.
     */
    String preprocess(String source, String path, String buildVariant) throws Exception;
}
