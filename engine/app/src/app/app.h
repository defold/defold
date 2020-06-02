#ifndef DM_APP_H
#define DM_APP_H

namespace dmApp
{
	enum Result
	{
		RESULT_OK		=  0,
		RESULT_REBOOT	=  1,
		RESULT_EXIT		= -1,
	};

	// Creates and initializes the engine. Returns the engine instance
    typedef void* (*EngineCreate)(int argc, char** argv);
    // Destroys the engine instance after finalizing each system
    typedef void (*EngineDestroy)(void* engine);
    // Steps the engine 1 tick
    typedef Result (*EngineUpdate)(void* engine);
    // Called before the destroy function
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

	struct Params
	{
		int 	m_Argc;
		char**	m_Argv;

		void*   m_AppCtx;
		void    (*m_AppCreate)(void* ctx);
		void    (*m_AppDestroy)(void* ctx);

		EngineCreate  		m_EngineCreate;
		EngineDestroy  		m_EngineDestroy;
		EngineUpdate  		m_EngineUpdate;
		EngineGetResult  	m_EngineGetResult;
	};

	/**
	 *
	 */
	int Run(const Params* params);
}

#endif // DM_APP_H
