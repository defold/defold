
#include "shaderc_private.h"

namespace dmShaderc
{
	const ShaderReflection* GetReflection(HShaderContext context)
    {
        return &context->m_Reflection;
    }

    HShaderCompiler NewShaderCompiler(HShaderContext context, ShaderLanguage language)
    {
        if (language == SHADER_LANGUAGE_SPIRV)
        {
            return (HShaderCompiler) NewShaderCompilerSPIRV(context);
        }
        return (HShaderCompiler) NewShaderCompilerSPVC(context, language);
    }

    void DeleteShaderCompiler(HShaderCompiler compiler)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            DeleteShaderCompilerSPIRV((ShaderCompilerSPIRV*) compiler);
        }
        else
        {
            DeleteShaderCompilerSPVC((ShaderCompilerSPVC*) compiler);
        }
    }

    void SetResourceLocation(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t location)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceLocationSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, location);
        }
        else
        {
            SetResourceLocationSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, location);
        }
    }

    void SetResourceBinding(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t binding)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceBindingSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, binding);
        }
        else
        {
            SetResourceBindingSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, binding);
        }
    }

    void SetResourceSet(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t set)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            SetResourceSetSPIRV(context, (ShaderCompilerSPIRV*) compiler, name_hash, set);
        }
        else
        {
            SetResourceSetSPVC(context, (ShaderCompilerSPVC*) compiler, name_hash, set);
        }
    }

    ShaderCompileResult* Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options)
    {
        if (compiler->m_Language == SHADER_LANGUAGE_SPIRV)
        {
            return CompileSPIRV(context, (ShaderCompilerSPIRV*) compiler, options);
        }
        else
        {
            return CompileSPVC(context, (ShaderCompilerSPVC*) compiler, options);
        }
    }
}
