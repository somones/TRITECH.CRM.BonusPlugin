// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "CRM_Bonus_PluginInstance.h"

//+------------------------------------------------------------------+
//| Plugin description structure                                     |
//+------------------------------------------------------------------+
MTPluginInfo ExtPluginInfo =
{
	200,
	MTServerAPIVersion,
	L"CRM Bonus Plugin ST TEST VERSION NO ACTION",
	L"Copyright 2020, Launch FXM. v0.174",
	L"LUNCH FXM SERVER CRM BONUS V 1.O ST NO ACTION MEMORY TEST VERSION"
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
//+------------------------------------------------------------------+
//| Plugin default parameters                                        |
//+------------------------------------------------------------------+

MTPluginParam ExtPluginDefaults[] =
{
	{ MTPluginParam::TYPE_STRING,			L"crm.database",					L"plugin_crm_db" },
	{ MTPluginParam::TYPE_STRING,			L"crm.database.Host",				L"148.72.55.213" },
	{ MTPluginParam::TYPE_STRING,			L"crm.database.user",			    L"plugin_db_user"},
	{ MTPluginParam::TYPE_STRING,			L"crm.database.password",	        L"r9_WL#fw"},
	{ MTPluginParam::TYPE_STRING,			L"plugin.license",					L"r9_WL#fw"},
};

//+------------------------------------------------------------------+
//| Plugin About entry function                                      |
//+------------------------------------------------------------------+
MTAPIENTRY MTAPIRES MTServerAbout(MTPluginInfo& info)
{
	info = ExtPluginInfo;
	//--- copy default parameters values
	memcpy(info.defaults, ExtPluginDefaults, sizeof(ExtPluginDefaults));
	info.defaults_total = _countof(ExtPluginDefaults);
	return(MT_RET_OK);
}

//+------------------------------------------------------------------+
//| Plugin instance creation entry point                             |
//+------------------------------------------------------------------+
MTAPIENTRY MTAPIRES MTServerCreate(UINT apiversion, IMTServerPlugin** plugin)
{
	//--- check parameters;;
	if (!plugin) return(MT_RET_ERR_PARAMS);
	//--- create plugin instance
	if (((*plugin) = new(std::nothrow) CRM_Bonus_PluginInstance()) == NULL)
		return(MT_RET_ERR_MEM);
	//--- ok
	return(MT_RET_OK);
}
//+------------------------------------------------------------------+


