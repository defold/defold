
#include "shaderc_private.h"

namespace dmShaderc
{
	ShaderCompilerSPIRV* NewShaderCompilerSPIRV(HShaderContext context)
	{
		ShaderCompilerSPIRV* compiler = (ShaderCompilerSPIRV*) malloc(sizeof(ShaderCompilerSPIRV));
		memset(compiler, 0, sizeof(ShaderCompilerSPIRV));

		compiler->m_BaseCompiler.m_Language = SHADER_LANGUAGE_SPIRV;

		return compiler;
	}

	void DeleteShaderCompilerSPIRV(ShaderCompilerSPIRV* compiler)
    {
        free(compiler);
    }

    void SetResourceLocationSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t location)
    {

    }

    void SetResourceBindingSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t binding)
    {

    }

    void SetResourceSetSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, uint64_t name_hash, uint8_t set)
    {

    }

    const char* CompileSPIRV(HShaderContext context, ShaderCompilerSPIRV* compiler, const ShaderCompilerOptions& options)
    {
    	return 0;
    }
}