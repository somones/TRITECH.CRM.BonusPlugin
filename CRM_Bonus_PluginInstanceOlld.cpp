#include "pch.h"
#include "CRM_Bonus_PluginInstance.h"
#include "BonusTimer.h"
#include <sstream>
#include <mysql.h>
#include <vector>
#include <thread>

using namespace std;

//+------------------------------------------------------------------+
//| Constructor                                                      |
//+------------------------------------------------------------------+
CRM_Bonus_PluginInstance::CRM_Bonus_PluginInstance(void) : m_api(NULL), m_config(NULL), m_deals(NULL), m_group(NULL), m_group_symbol(NULL), m_user(NULL),
m_symbol(NULL), m_conf_mask(0), m_account(NULL), m_groups(NULL), order_deals(NULL), request(NULL), char__database_password_pr(NULL), managerStatus(NULL),
char_database_user_pr(NULL), char_database_Host_pr(NULL), db_param_BonusValidity(0), char_database_pr(NULL), db_param_account_equity_rate(0),
m_managerLogin(NULL), m_server_ip(NULL), m_server_db(NULL), m_server_user(NULL), m_user_pwd(NULL), group_name(NULL), m_conn(NULL), m_manager(NULL), isBusy(0)
{
}
//+------------------------------------------------------------------+
//| Destructor                                                       |
//+------------------------------------------------------------------+
CRM_Bonus_PluginInstance::~CRM_Bonus_PluginInstance(void)
{
	Stop();
}
//+------------------------------------------------------------------+
//| Plugin release function                                          |
//+------------------------------------------------------------------+
void CRM_Bonus_PluginInstance::Release(void)
{
	delete this;
}
//+------------------------------------------------------------------+
//| Plugin start                                                     |
//+------------------------------------------------------------------+
MTAPIRES CRM_Bonus_PluginInstance::Start(IMTServerAPI* api)
{
	MTAPIRES retcode;
	//IMTConPlugin* config;
	MTAPIRES res = MT_RET_OK;
	//--- 
	Stop();
	//--- check pointer
	if (!api)
		return(MT_RET_ERR_PARAMS);
	//--- save pointer to Server API interface
	m_api = api;
	//--- create plugin config instance
	if ((m_config = m_api->PluginCreate()) == NULL)
		return(MT_RET_OK);
	//--- receive current plugin m_config
	if ((m_api->PluginCurrent(m_config)) != MT_RET_OK)
	{
		m_config->Release();
		return(MT_RET_OK);
	}
	//--- create interfaces
	//--- group
	if (!(m_group = m_api->GroupCreate()))
	{
		m_api->LoggerOut(MTLogErr, L"GroupCreate failed");
		return(MT_RET_ERR_MEM);
	}
	//--- user
	if ((m_account = m_api->UserCreateAccount()) == NULL)
		return(MT_RET_ERR_MEM);

	if (!(m_user = m_api->UserCreate()))
	{
		m_api->LoggerOut(MTLogErr, L"UserCreateAccount failed");
		return(MT_RET_ERR_MEM);
	}
	//--- deals
	if (!(m_deals = m_api->DealCreateArray()))
	{
		m_api->LoggerOut(MTLogErr, L"DealCreateArray failed");
		return(MT_RET_ERR_MEM);
	}
	if (!(request = m_api->TradeRequestCreate()))
	{
		m_api->LoggerOut(MTLogErr, L"TradeRequestCreate failed");
		return(MT_RET_ERR_MEM);
	}
	//--- deal symbol
	if (!(m_symbol = m_api->SymbolCreate()))
	{
		m_api->LoggerOut(MTLogErr, L"SymbolCreate failed");
		return(MT_RET_ERR_MEM);
	}
	//--- subscribe plugin on server events
	if ((retcode = m_api->ServerSubscribe(this)) != MT_RET_OK)
		m_api->LoggerOut(MTLogOK, L"Server events subscribe failed [%d]", retcode);
	//--- subscribe to plugin plugin
	if ((res = m_api->PluginSubscribe(this)) != MT_RET_OK)
		return(res);

	if ((res = m_api->TradeSubscribe(this)) != MT_RET_OK)
		return(res);

	//--- Server Manager
	if (!(m_manager = m_api->ManagerCreate()))
	{
		m_api->LoggerOut(MTLogErr, L"ManagerCreate failed");
		return(MT_RET_ERR_MEM);
	}

	//--- read parameters
	if ((res = ParametersRead()) != MT_RET_OK)
		return(res);
	try {
		TritechTimer.setInterval([this]() {
			if (isBusy == 0) {
				isBusy = 1;
				this->PluginConfigRead();
				isBusy = 0;
			}

			}, 5000);
	}
	catch (...) {
	}
	m_config->Release();
	return(MT_RET_OK);
}
//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
MTAPIRES CRM_Bonus_PluginInstance::Stop(void)
{
	m_sync.Lock();
	//Stop The Timer
	//--- unsubscribe
	if (m_api)
	{
		m_api->ServerUnsubscribe(this);
		m_api->PluginUnsubscribe(this);
		m_api->TradeUnsubscribe(this);
		TritechTimer.stop();
		//TimerStop();
		m_api = NULL;
	}
	//--- clear all parameters
	m_conf_mask = 0;
	//--- delete interfaces
	if (m_group) { m_group->Release();        m_group = NULL; }
	//--- reset api
	m_api = NULL;
	//--- 
	m_sync.Unlock();
	//--- ok
	return(MT_RET_OK);
}

//void CRM_Bonus_PluginInstance::TimerStop(void)
//{
//	TritechTimer.stop();
//}

//+------------------------------------------------------------------+
//| Plugin parameters read function                                  |
//+------------------------------------------------------------------+
MTAPIRES CRM_Bonus_PluginInstance::ParametersRead(void)
{
	MTAPIRES     res = MT_RET_OK;
	IMTConParam* param = NULL;
	CMTStr128    tmp;

	//--- check pointers
	if (!m_api || !m_config)
		return(MT_RET_ERR_PARAMS);
	//--- get current plugin configuration
	if ((res = m_api->PluginCurrent(m_config)) != MT_RET_OK)
	{
		m_api->LoggerOut(MTLogErr, L"failed to get current plugin configuration [%s (%u)]", SMTFormat::FormatError(res), res);
		return(res);
	}
	//--- create plugin parameter object   
	if ((param = m_api->PluginParamCreate()) == NULL)
	{
		m_api->LoggerOut(MTLogErr, L"failed to create plugin parameter object");
		return(MT_RET_ERR_MEM);
	}
	//--- lock parameters
	m_sync.Lock();

	//{ MTPluginParam::TYPE_STRING,	L"Contest.ValidityUnit",		L"D" },
	if ((res = m_config->ParameterGet(L"crm.database", param)) != MT_RET_OK ||
		param->Type() != IMTConParam::TYPE_STRING ||
		param->ValueString()[0] == 0)
	{
		m_api->LoggerOut(MTLogErr, L"crm.database is missing");
		param->Release();
		m_sync.Unlock();
		return(MT_RET_ERR_PARAMS);
	}
	crm_database_pr.Assign(param->ValueString());

	//{ MTPluginParam::TYPE_STRING,	L"Contest.ValidityUnit",		L"D" },
	if ((res = m_config->ParameterGet(L"crm.database.Host", param)) != MT_RET_OK ||
		param->Type() != IMTConParam::TYPE_STRING ||
		param->ValueString()[0] == 0)
	{
		m_api->LoggerOut(MTLogErr, L"crm.database.Host is missing");
		param->Release();
		m_sync.Unlock();
		return(MT_RET_ERR_PARAMS);
	}
	crm_database_Host_pr.Assign(param->ValueString());

	//{ MTPluginParam::TYPE_STRING,	L"Contest.ValidityUnit",		L"D" },
	if ((res = m_config->ParameterGet(L"crm.database.user", param)) != MT_RET_OK ||
		param->Type() != IMTConParam::TYPE_STRING ||
		param->ValueString()[0] == 0)
	{
		m_api->LoggerOut(MTLogErr, L"crm.database.user is missing");
		param->Release();
		m_sync.Unlock();
		return(MT_RET_ERR_PARAMS);
	}
	crm_database_user_pr.Assign(param->ValueString());

	//{ MTPluginParam::TYPE_STRING,	L"Contest.ValidityUnit",		L"D" },
	if ((res = m_config->ParameterGet(L"crm.database.password", param)) != MT_RET_OK ||
		param->Type() != IMTConParam::TYPE_STRING ||
		param->ValueString()[0] == 0)
	{
		m_api->LoggerOut(MTLogErr, L"crm.database.password is missing");
		param->Release();
		m_sync.Unlock();
		return(MT_RET_ERR_PARAMS);
	}
	crm_database_password_pr.Assign(param->ValueString());

	//--- unlock parameters
	m_sync.Unlock();
	//--- free objects
	param->Release();
	//--- ok
	return(MT_RET_OK);
}
//+------------------------------------------------------------------+
//| Plugin config update notification                                |
//+------------------------------------------------------------------+
void CRM_Bonus_PluginInstance::OnPluginUpdate(const IMTConPlugin* plugin)
{
	//--- check parameters
	if (plugin == NULL || m_api == NULL || m_config == NULL)
		return;
	//--- update config
	if (CMTStr::Compare(plugin->Name(), m_config->Name()) == 0 &&
		plugin->Server() == m_config->Server())
		ParametersRead();
}
//+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//| Reading Plugin Parameters from DATABASE																																		|
//| The CRM Bonus Plugin is using 4 parameters from the plugin_config in this Function we will get all parameters from the Data base to be used in the other functions          |
//| Here is the 04 parametrs from the DataBase																																	|
//| param_group : here we will get the Groups included on the plugin																											|
//| param_account_equity_rate :  The Equity Rate is used to define when we can credit out the Bonus																				|
//| param_BonusValidity : the Bonus Validity is an INT used to define in Number for how many time the bonus will be valide														|
//| param_BonusValidityUnit  : Here is the Unite example D -> Day, M-> Month, Y-> Year																							|
//| Reading Plugin Parameters from DATABASE																																		|
//+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//[[unused]] void CRM_Bonus_PluginInstance::ParametersRead_db() {
//	string dummy;
//	MYSQL* conn;
//	MYSQL_ROW row;
//	MYSQL_RES* Qres;
//
//	string param_group;
//	string param_account_equity_rate;
//	string param_BonusValidity;
//	string param_BonusValidityUnit;
//
//	string sever_manager;
//	string server_db;
//	string server_ip;
//	string server_usr_db;
//	string server_pwd_db;
//
//	//In this function we are converting the parameters to const cher *
//	wstring ws_database(crm_database_pr.Str());
//	string str_database(ws_database.begin(), ws_database.end());
//	char_database_pr = str_database.c_str();
//
//	wstring ws_database_password(crm_database_password_pr.Str());
//	string str_database_password(ws_database_password.begin(), ws_database_password.end());
//	char__database_password_pr = str_database_password.c_str();
//
//	wstring ws_database_user(crm_database_user_pr.Str());
//	string str_database_user(ws_database_user.begin(), ws_database_user.end());
//	char_database_user_pr = str_database_user.c_str();
//
//	wstring ws_database_Host(crm_database_Host_pr.Str());
//	string str_database_Host(ws_database_Host.begin(), ws_database_Host.end());
//	char_database_Host_pr = str_database_Host.c_str();
//
//	// connecting to the server we can update it from the plugin parameters
//	conn = mysql_init(0);
//	conn = mysql_real_connect(conn, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0);
//
//	if (conn) {
//		// SELECT THE PLUGIN PARAMETERS FROM THE PLUGIN CONFIG TABLE
//		int qstate = mysql_query(conn, "SELECT * FROM server_database_config");
//		//m_api->LoggerOut(MTLogErr, L"ParametersRead_db CONNECTED");
//		if (!qstate) {
//			Qres = mysql_store_result(conn);
//
//			while (row = mysql_fetch_row(Qres)) {
//
//				sever_manager	= row[1];
//				m_server_ip		= row[2];
//				m_server_db		= row[3];
//				m_server_user	= row[4];
//				m_user_pwd		= row[5];
//
//				m_managerLogin = std::atoi(row[1]);
//
//				m_api->LoggerOut(MTLogErr, L"Executed Manager %I64u", m_managerLogin);
//
//				PluginConfigRead();
//			}
//
//			/*mysql_close(conn);
//			mysql_library_end();*/
//		}
//	}
//	else
//	{
//		m_api->LoggerOut(MTLogErr, L"Alerts : ParametersRead_db Not CONNECTED");
//	}
//}

[[unused]] void CRM_Bonus_PluginInstance::PluginConfigRead()
{
	MYSQL_ROW	row;
	MYSQL_RES* Qres;
	string param_group;
	string param_account_equity_rate;
	string param_BonusValidity;
	string param_BonusValidityUnit;
	string param_CentGroup;

	string sever_manager;
	string server_db;
	string server_ip;
	string server_usr_db;
	string server_pwd_db;
	string param_conn;

	//In this function we are converting the parameters to const cher *
	wstring ws_database(crm_database_pr.Str());
	string str_database(ws_database.begin(), ws_database.end());
	char_database_pr = str_database.c_str();

	wstring ws_database_password(crm_database_password_pr.Str());
	string str_database_password(ws_database_password.begin(), ws_database_password.end());
	char__database_password_pr = str_database_password.c_str();

	wstring ws_database_user(crm_database_user_pr.Str());
	string str_database_user(ws_database_user.begin(), ws_database_user.end());
	char_database_user_pr = str_database_user.c_str();

	wstring ws_database_Host(crm_database_Host_pr.Str());
	string str_database_Host(ws_database_Host.begin(), ws_database_Host.end());
	char_database_Host_pr = str_database_Host.c_str();

	// connecting to the server we can update it from the plugin parameters
	m_conn = mysql_init(0);
	m_conn = mysql_real_connect(m_conn, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0);

	if (m_conn) {
		// SELECT THE PLUGIN PARAMETERS FROM THE PLUGIN CONFIG TABLE
		int qstate = mysql_query(m_conn, "SELECT * FROM bonus_plugin_config");
		//m_api->LoggerOut(MTLogErr, L"ParametersRead_db CONNECTED");
		if (!qstate) {
			Qres = mysql_store_result(m_conn);
			
			if (mysql_num_rows(Qres) != 0)
			{
				while (row = mysql_fetch_row(Qres)) {

					sever_manager				= row[1];
					param_group					= row[2];
					param_account_equity_rate	= row[3];
					param_BonusValidity			= row[4];
					param_BonusValidityUnit		= row[5];
					param_CentGroup				= row[6];
					param_conn					= row[7];

					//m_api->LoggerOut(MTLogErr, L"====>>> >> SHOULD BE MANAGER : %s", sever_manager);

					m_managerLogin = std::atoll(row[1]);
					db_param_BonusValidity = std::atoi(row[4]);
					db_param_account_equity_rate = std::atof(row[3]);

					std::wstring group_stemp				= std::wstring(param_group.begin(), param_group.end());
					std::wstring account_equity_rate_stemp	= std::wstring(param_account_equity_rate.begin(), param_account_equity_rate.end());
					std::wstring BonusValidity_stemp		= std::wstring(param_BonusValidity.begin(), param_BonusValidity.end());
					std::wstring BonusValidityUnit_stemp	= std::wstring(param_BonusValidityUnit.begin(), param_BonusValidityUnit.end());
					std::wstring cent_group_stemp			= std::wstring(param_CentGroup.begin(), param_CentGroup.end());

					db_param_group				= group_stemp.c_str();
					db_param_BonusValidityUnit	= BonusValidityUnit_stemp.c_str();
					db_param_cent_group			= cent_group_stemp.c_str();

					//m_api->LoggerOut(MTLogErr, L"DB MANAGER %I64u", m_managerLogin);

					onCRMDeposit(m_conn);
				}
			}
		}
		mysql_close(m_conn);
	}
	else
	{
		m_api->LoggerOut(MTLogErr, L"002 !!!!! NOT ParametersRead_db Not CONNECTED : %I64u", m_managerLogin);
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(m_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(m_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//| Processing of adding a new Deposit Credit in and out																																						  |
//| Before deposit Process we need to connect to the server and get the pending Deposit																															  |
//| In this function we dont need to passe any parameters only to connect to the server and get the pending action and check it the users are in same groups as defined in the plugin Config table                |
//| we are executing 2 function the deposit and the withdrawal, the both Pending action are executing in the same way mean we nned to get the record on the database afther checking the status thats is pending  |
//| we will execute the prending action and updating the table  crm_bonus_user_deposit by updating the crm_deposit_status from pending to Success, and saving the action in plugin_user_deposit                   |
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
[[unused]] void CRM_Bonus_PluginInstance::onCRMDeposit(MYSQL* p_conn) {
	string dummy;
	MYSQL_ROW row;
	MYSQL_RES* Qres;

	string	crm_user_login;
	string	crm_user_group;
	string	crm_user_transaction_id;
	string	crm_user_deposit;
	string	crm_bonus_rate;
	string	crm_deposit_comment;
	string	crm_deposit_status;
	string	crm_user_id;
	string	contest_user_reason;
	time_t	now = time(0);
	UINT64	usr_login;
	double	dep_amount = 0;
	double	bns_rate = 0;
	string	sum_deposit;
	double	deposit_amount = 0;
	int		m_group_matched = 0;

	wstring ws(crm_database_pr.Str());
	string str(ws.begin(), ws.end());
	const char* crm_database_pr = str.c_str();

	if (p_conn)
	{
		// SELCET ALL THE DEPOSIT ON CRM BONUS USER DEPOSIT TABLE
		//int qstate = mysql_query(p_conn, "SELECT * FROM bonus_crm_bonus_user_deposit WHERE crm_deposit_status = 'Pending' AND crm_user_manager = '" << m_managerLogin << "' ");
		int qstate = 0;
		
		stringstream ss;
		
		ss << "SELECT * FROM bonus_crm_bonus_user_deposit WHERE crm_deposit_status = 'Pending' AND crm_user_manager = '" << m_managerLogin << "' ";
		
		string query	= ss.str();
		const char* q	= query.c_str();
		qstate			= mysql_query(p_conn, q);

		if (!qstate)
		{	
			Qres = mysql_store_result(p_conn);
			int num_fields = mysql_num_fields(Qres);
			if (mysql_num_rows(Qres) != 0)
			{
				while (row = mysql_fetch_row(Qres))
				{
					crm_user_id = row[0];
					crm_user_login = row[2];
					crm_user_group = row[3];
					crm_user_transaction_id = row[4];
					crm_user_deposit = row[5];
					crm_bonus_rate = row[6];
					crm_deposit_comment = row[7];
					crm_deposit_status = row[8];
					contest_user_reason = row[9];

					usr_login = std::atoll(row[2]);
					dep_amount = std::atof(row[5]);
					bns_rate = std::atof(row[6]);

					std::wstring crm_user_login_stemp = std::wstring(crm_user_login.begin(), crm_user_login.end());
					std::wstring crm_user_group_stemp = std::wstring(crm_user_group.begin(), crm_user_group.end());
					std::wstring crm_user_transaction_id_stemp = std::wstring(crm_user_transaction_id.begin(), crm_user_transaction_id.end());
					std::wstring crm_user_deposit_stemp = std::wstring(crm_user_deposit.begin(), crm_user_deposit.end());
					std::wstring crm_bonus_rate_stemp = std::wstring(crm_bonus_rate.begin(), crm_bonus_rate.end());
					std::wstring crm_deposit_comment_stemp = std::wstring(crm_deposit_comment.begin(), crm_deposit_comment.end());
					std::wstring crm_deposit_status_stemp = std::wstring(crm_deposit_status.begin(), crm_deposit_status.end());
					std::wstring contest_user_reason_stemp = std::wstring(contest_user_reason.begin(), contest_user_reason.end());

					db_crm_user_login = crm_user_login_stemp.c_str();
					db_crm_user_group = crm_user_group_stemp.c_str();
					db_crm_user_transaction_id = crm_user_transaction_id_stemp.c_str();
					db_crm_user_deposit = crm_user_deposit_stemp.c_str();
					db_crm_bonus_rate = crm_bonus_rate_stemp.c_str();
					db_crm_deposit_comment = crm_deposit_comment_stemp.c_str();
					db_crm_deposit_status = crm_deposit_status_stemp.c_str();
					db_crm_contest_user_reason = contest_user_reason_stemp.c_str();

					std::wstring delay_group_strng = db_param_group.Str();
					std::vector<std::string> delay_group_result;

					const std::string delay_group_str(delay_group_strng.begin(), delay_group_strng.end());
					stringstream delay_group_stream(delay_group_str); //create string stream from the string

					while (delay_group_stream.good()) {
						std::string delay_group_streamsubstr;
						getline(delay_group_stream, delay_group_streamsubstr, ','); //get first string delimited by comma
						delay_group_result.push_back(delay_group_streamsubstr);
					}
					for (int i = 0; i < delay_group_result.size(); i++)
					{
						std::wstring delay_group_stemp = std::wstring(delay_group_result.at(i).begin(), delay_group_result.at(i).end());
						LPCWSTR delay_group_sw = delay_group_stemp.c_str();

						//Check if the USER is in the Same group
						int TT = CMTStr::Compare(db_crm_user_group.Str(), delay_group_sw);
						if (TT == 0)
						{
							m_api->LoggerOut(MTLogErr, L"SAME GROUP %I64u", usr_login);
							++m_group_matched;
							ClassicGroupExecution(p_conn, dep_amount, usr_login, bns_rate, crm_user_transaction_id, crm_user_deposit, crm_user_id);
						}
					}

					if (m_group_matched == 0)
					{
						CentGroupExecution(p_conn, dep_amount, usr_login, bns_rate, crm_user_transaction_id, crm_user_deposit, crm_user_id);
					}
				}
			}
			//check_the_equity_db(conn);
		}
		else {
			m_api->LoggerOut(MTLogErr, L"Query error crm_bonus_user_deposit");
		}
	}
	else
	{
		m_api->LoggerOut(MTLogErr, L"NOT onCRMDeposit The Connection is Not Established");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
	get_equity_users(p_conn);
}

void CRM_Bonus_PluginInstance::CentGroupExecution(MYSQL* p_conn, double dep_amount, UINT64 usr_login, double bns_rate, string crm_user_transaction_id, string crm_user_deposit, string crm_user_id)
{
	time_t now = time(0);
	double deposit_amount = 0;

	std::wstring cent_group_strng = db_param_cent_group.Str();
	std::vector<std::string> cent_group_result;

	const std::string cent_group_str(cent_group_strng.begin(), cent_group_strng.end());
	stringstream cent_group_stream(cent_group_str); //create string stream from the string

	while (cent_group_stream.good()) {
		std::string cent_group_streamsubstr;
		getline(cent_group_stream, cent_group_streamsubstr, ','); //get first string delimited by comma
		cent_group_result.push_back(cent_group_streamsubstr);
	}

	for (int i = 0; i < cent_group_result.size(); i++)
	{
		std::wstring cent_group_stemp = std::wstring(cent_group_result.at(i).begin(), cent_group_result.at(i).end());
		LPCWSTR cent_group_sw = cent_group_stemp.c_str();

		//Check if the USER is in the Same group
		int cent_group_compar = CMTStr::Compare(db_crm_user_group.Str(), cent_group_sw);

		if (cent_group_compar == 0)
		{
			deposit_amount = dep_amount * 100;

			int creditInResult = CMTStr::Compare(db_crm_contest_user_reason.Str(), L"Credit In");

			if (creditInResult == 0)
			{
				CreditInOperation(p_conn, usr_login, deposit_amount, bns_rate, db_crm_deposit_comment, crm_user_transaction_id, crm_user_deposit, now, crm_user_id,1);
			}

			else if (int creditOutResult = CMTStr::Compare(db_crm_contest_user_reason.Str(), L"Credit Out") == 0)
			{
				double	credit_out_amount = deposit_amount * (-1);
				double	bonus_db_stemp_pl = 0;
				UINT64	ZRO = 0;
				int		qstate_sum = 0;
				double	balance_wCredit = 0;
				double	bonus_db_deposit_sum = 0;
				double	bonus_db_bonus_sum = 0;
				string	sum_bonus;

				double m_usr_balance = get_user_balance(p_conn, deposit_amount, usr_login);

				// we are credit out the withdrawal Amount and Bonus 
				if (m_usr_balance < deposit_amount)
				{
					creditOutCreditBalance(p_conn, m_account->Login(), credit_out_amount, db_crm_deposit_comment, deposit_amount, m_usr_balance, crm_user_id, crm_user_transaction_id, now, crm_user_deposit, db_crm_user_group.Str(),1);
				}
				else
				{
					// here we are checking is the Credit out value is less then the value without credit the no need to credit out the Bonus
					creditOutBalance(p_conn, m_account->Login(), credit_out_amount, db_crm_deposit_comment, crm_user_id, crm_user_transaction_id, now, db_crm_user_group.Str(),1);
				}
			}
		}
	}
}
void CRM_Bonus_PluginInstance::ClassicGroupExecution(MYSQL* p_conn, double dep_amount, UINT64 usr_login, double bns_rate, string crm_user_transaction_id, string crm_user_deposit, string crm_user_id)
{
	time_t now = time(0);
	double deposit_amount = 0;

	deposit_amount = dep_amount;

	int creditInResult = CMTStr::Compare(db_crm_contest_user_reason.Str(), L"Credit In");

	if (creditInResult == 0)
	{
		CreditInOperation(p_conn, usr_login, deposit_amount, bns_rate, db_crm_deposit_comment, crm_user_transaction_id, crm_user_deposit, now, crm_user_id,0);
	}
	else if (int creditOutResult = CMTStr::Compare(db_crm_contest_user_reason.Str(), L"Credit Out") == 0)
	{
		double	credit_out_amount = deposit_amount * (-1);
		double	bonus_db_stemp_pl = 0;
		UINT64	ZRO = 0;
		int		qstate_sum = 0;
		double	balance_wCredit = 0;
		double	bonus_db_deposit_sum = 0;
		double	bonus_db_bonus_sum = 0;
		string	sum_bonus;

		double m_usr_balance = get_user_balance(p_conn, deposit_amount, usr_login);

		// we are credit out the withdrawal Amount and Bonus 
		if (m_usr_balance < deposit_amount)
		{
			creditOutCreditBalance(p_conn, m_account->Login(), credit_out_amount, db_crm_deposit_comment, deposit_amount, m_usr_balance, crm_user_id, crm_user_transaction_id, now, crm_user_deposit, db_crm_user_group.Str(), 0);
		}
		else
		{
			// here we are checking is the Credit out value is less then the value without credit the no need to credit out the Bonus
			creditOutBalance(p_conn, m_account->Login(), credit_out_amount, db_crm_deposit_comment, crm_user_id, crm_user_transaction_id, now, db_crm_user_group.Str(), 0);
		}
	}
}
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//| This funcion we are executing 2 major operation one is executing the credit out in MT5 and if the operation is done without errors then we will save the operation in plugin_user_deposit               |
//| and updating the two table transaction in plugin_user_deposit with Cancelled if the bonus is credited out																								|
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::creditOutCreditBalance(MYSQL* p_conn, UINT64 user_login, double credit_out_amount, CMTStr256 db_crm_deposit_comment, double dep_amount, double m_usr_balance, string crm_user_id, string crm_user_transaction_id, int times, string crm_user_deposit, CMTStr256 user_group, int cent)
{
	UINT64			ZRO = 0;
	MYSQL_RES*		res_up;
	stringstream	ss;
	string			bonus_db;
	string			deposit_usr_id;
	double			avl_amount = m_usr_balance;
	double			deposit_amount = 0;
	int				qstate = 0;
	MYSQL_ROW		row;

	if (p_conn)
	{
		// here we are selecting the Bonus of the user 
		ss << "SELECT * FROM bonus_plugin_user_deposit WHERE plugin_user_login = '" << user_login << "' AND plugin_user_bonus_status = 'Credit In' AND plugin_user_bonus != 0 ORDER BY plugin_user_id ASC";

		string query = ss.str();
		const char* q = query.c_str();
		qstate = mysql_query(p_conn, q);

		if (qstate == 0)
		{
			res_up = mysql_store_result(p_conn);
			double total_bonus_crediout = 0;

			while (row = mysql_fetch_row(res_up))
			{
				//Here we are checking if the credit out is reached 
				if (avl_amount > dep_amount || avl_amount == dep_amount)
				{
					break;
				}

				deposit_usr_id = row[0];
				bonus_db = row[5];

				double bonus_db_stemp_pl = std::atof(row[5]);
				double deposit_db_stemp_pl = std::atof(row[4]);

				double bonus_credit_out = bonus_db_stemp_pl * (-1);

				if (bonus_credit_out != 0)
				{
					if (m_api->UserDepositChangeRaw(user_login, bonus_credit_out, IMTDeal::DEAL_CREDIT, db_crm_deposit_comment.Str(), ZRO) == MT_RET_OK)
					{
						int qstate_Dep_up = 0;
						total_bonus_crediout = total_bonus_crediout + bonus_db_stemp_pl;
						stringstream sss;

						//sss << "INSERT INTO plugin_user_deposit(plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status,plugin_user_bonus_status) VALUES('" << usr_login << "','" << crm_user_transaction_id << "','" << credit_out_amount << "','" << bonus_credit_out << "','" << now << "','Deposit','Cancelled')";
						sss << "UPDATE bonus_plugin_user_deposit SET plugin_user_bonus_status = 'Cancelled', plugin_user_status = '0'  WHERE plugin_user_id = '" << deposit_usr_id << "'";
						string query_in = sss.str();
						const char* q_in = query_in.c_str();
						qstate = mysql_query(p_conn, q_in);

						/*if (qstate == 0)
						{
							avl_amount = avl_amount + deposit_db_stemp_pl;
							stringstream s_COUT;
							int qOUTStat = 0;
							s_COUT << "INSERT INTO bonus_plugin_user_deposit (plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status,plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << user_login << "','" << crm_user_transaction_id << "','" << credit_out_amount << "','" << bonus_credit_out << "','" << times << "','Deposit','Cancelled')";
							string query_OUT = s_COUT.str();
							const char* q_OUT = query_OUT.c_str();
							qOUTStat = mysql_query(p_conn, q_OUT);
						}*/
					}
				}
				//m_api->LoggerOut(MTLogErr, L"avl_amount %f", avl_amount);
			}

			std::wstring cent_group_strng = db_param_cent_group.Str();
			std::vector<std::string> cent_group_result;

			const std::string cent_group_str(cent_group_strng.begin(), cent_group_strng.end());
			stringstream cent_group_stream(cent_group_str); //create string stream from the string

			while (cent_group_stream.good()) {
				std::string cent_group_streamsubstr;
				getline(cent_group_stream, cent_group_streamsubstr, ','); //get first string delimited by comma
				cent_group_result.push_back(cent_group_streamsubstr);
			}

			for (int i = 0; i < cent_group_result.size(); i++)
			{
				std::wstring cent_group_stemp = std::wstring(cent_group_result.at(i).begin(), cent_group_result.at(i).end());
				LPCWSTR cent_group_sw = cent_group_stemp.c_str();

				//Check if the USER is in the Same group
				int cent_group_compar = CMTStr::Compare(user_group.Str(), cent_group_sw);

				if (cent_group_compar == 0)
				{
					double u_credit_out = credit_out_amount * (-1);
					deposit_amount = (u_credit_out * 100) * (-1);
				}
				else
				{
					deposit_amount = credit_out_amount;
				}
			}

			if (m_api->UserDepositChangeRaw(user_login, credit_out_amount, IMTDeal::DEAL_BALANCE, db_crm_deposit_comment.Str(), ZRO) == MT_RET_OK)
			{
				stringstream s_COUT;
				int qOUTStat = 0, qstate_cnn = 0;
				double withdrawreq = 0;
				double total_bonuscrediout = 0;

				if (cent == 1)
				{
					withdrawreq = deposit_amount / 100;
					total_bonuscrediout = (total_bonus_crediout * (-1)) / 100;
				}
				else
				{
					withdrawreq = deposit_amount;
					total_bonuscrediout = total_bonus_crediout * (-1);
				}

				s_COUT << "INSERT INTO bonus_plugin_user_deposit(plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status,plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << user_login << "','" << crm_user_transaction_id << "','" << withdrawreq << "','" << total_bonuscrediout << "','" << times << "','Withdraw','Bonus CreditOut')";
				string query_OUT = s_COUT.str();
				const char* q_OUT = query_OUT.c_str();
				qOUTStat = mysql_query(p_conn, q_OUT);

				stringstream ss_cnn;
				ss_cnn << "UPDATE bonus_crm_bonus_user_deposit SET crm_deposit_status = 'Success', crm_status = '0' WHERE crm_user_id = '" << crm_user_id << "'";

				string query_cnn = ss_cnn.str();
				const char* q_cnn = query_cnn.c_str();
				qstate_cnn = mysql_query(p_conn, q_cnn);
			}
			else
			{
				int qOUTStat = 0, qstate_cnn = 0;

				stringstream ss_cnn;
				ss_cnn << "UPDATE bonus_crm_bonus_user_deposit SET crm_deposit_status = 'Failed', crm_deposit_reason = 'No Money ', crm_status = '0' WHERE crm_user_id = '" << crm_user_id << "'";

				string query_cnn = ss_cnn.str();
				const char* q_cnn = query_cnn.c_str();
				qstate_cnn = mysql_query(p_conn, q_cnn);
			}
		}
		check_bonus_validity(p_conn, m_account->Login());
		//mysql_close(p_conn);
		//mysql_library_end();
	}
	else
	{
		m_api->LoggerOut(MTLogOK, L"creditOutCreditBalance NOT CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//| This funcion we are executing 2 major operation one is executing the credit out in MT5 and if the operation is done without errors then we will save the operation in plugin_user_deposit               |
//| and updating the two table transaction in plugin_user_deposit with Cancelled if the bonus is credited out																								|
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
void CRM_Bonus_PluginInstance::creditOutBalance(MYSQL* p_conn, UINT64 user_login, double credit_out_amount, CMTStr256 db_crm_deposit_comment, string crm_user_id, string crm_user_transaction_id, int times, CMTStr256 user_group, int cent)
{
	UINT64 ZRO				= 0;
	double deposit_amount	= 0;
	double cent_amount		= 0;

	if (p_conn)
	{
		if (m_api->UserDepositChange(user_login, credit_out_amount, IMTDeal::DEAL_BALANCE, db_crm_deposit_comment.Str(), ZRO) == MT_RET_OK)
		{
			int qstate = 0, qstate_cn = 0;

			/*user_group.Str();*/

			stringstream ss_cn;
			// AND plugin_user_transaction_id = '" << crm_user_transaction_id << "'
			ss_cn << "UPDATE bonus_crm_bonus_user_deposit SET crm_deposit_status = 'Success', crm_status = '0' WHERE crm_user_id = '" << crm_user_id << "'";

			string query_cn = ss_cn.str();
			const char* q_cn = query_cn.c_str();
			qstate_cn = mysql_query(p_conn, q_cn);

			std::wstring cent_group_strng = db_param_cent_group.Str();
			std::vector<std::string> cent_group_result;

			const std::string cent_group_str(cent_group_strng.begin(), cent_group_strng.end());
			stringstream cent_group_stream(cent_group_str); //create string stream from the string

			while (cent_group_stream.good()) {
				std::string cent_group_streamsubstr;
				getline(cent_group_stream, cent_group_streamsubstr, ','); //get first string delimited by comma
				cent_group_result.push_back(cent_group_streamsubstr);
			}

			for (int i = 0; i < cent_group_result.size(); i++)
			{
				std::wstring cent_group_stemp = std::wstring(cent_group_result.at(i).begin(), cent_group_result.at(i).end());
				LPCWSTR cent_group_sw = cent_group_stemp.c_str();

				//Check if the USER is in the Same group
				int cent_group_compar = CMTStr::Compare(user_group.Str(), cent_group_sw);

				if (cent_group_compar == 0)
				{
					double u_credit_out = credit_out_amount * (-1);
					deposit_amount = (u_credit_out * 100) * (-1);
				}
				else
				{
					deposit_amount = credit_out_amount;
				}
			}

			double withdrawreq = 0;

			if (cent == 1)
			{
				withdrawreq = deposit_amount / 100;
			}
			else
			{
				withdrawreq = deposit_amount;
			}

			/*m_api->LoggerOut(MTLogErr, L"withdrawreq 02 %f", withdrawreq);
			m_api->LoggerOut(MTLogErr, L"credit_out_amount 02 %f", credit_out_amount);*/
			stringstream s_insert;
			s_insert << "INSERT INTO bonus_plugin_user_deposit(plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status,plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << user_login << "','" << crm_user_transaction_id << "','" << withdrawreq << "','" << 0 << "','" << times << "','Withdraw','No Bonus')";
			string query_insert = s_insert.str();
			const char* q_insert = query_insert.c_str();
			qstate = mysql_query(p_conn, q_insert);
			check_bonus_validity(p_conn,user_login);
		}
		//mysql_close(p_conn);
		//mysql_library_end();
	}
	else
	{
		m_api->LoggerOut(MTLogOK, L"creditOutBalance NOT CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}
//+------------------------------------------------------------------+ 
//| Here we are getting                                              | 
//+------------------------------------------------------------------+ 
double CRM_Bonus_PluginInstance::get_user_balance(MYSQL* p_conn, double dep_amount, UINT64 usr_login)
{
	double		credit_out_amount = dep_amount * (-1);
	double		bonus_db_stemp_pl = 0;
	UINT64		ZRO = 0;
	int			qstate_sum = 0;
	double		balance_wCredit = 0;
	double		bonus_db_deposit_sum = 0;
	double		bonus_db_bonus_sum = 0;
	string		sum_bonus;
	MYSQL_ROW	row;
	MYSQL_RES*	Qres;

	if (p_conn)
	{
		if (m_api->UserAccountGet(usr_login, m_account) != MT_RET_OK)
			m_api->LoggerOut(MTLogErr, L"Getting Account FAILD %I64u (On GET USER BALANCE FUNCTION)", m_account->Login());

		stringstream ss_sum;

		ss_sum << "SELECT SUM(plugin_user_deposit), SUM(plugin_user_bonus) FROM bonus_plugin_user_deposit WHERE plugin_user_login = '" << m_account->Login() << "' AND plugin_user_bonus != 0 AND plugin_user_bonus_status = 'Credit In'";

		string query_sum = ss_sum.str();
		const char* q_sum = query_sum.c_str();
		qstate_sum = mysql_query(p_conn, q_sum);

		if (qstate_sum == 0)
		{
			Qres = mysql_store_result(p_conn);
			while (row = mysql_fetch_row(Qres))
			{
				bonus_db_deposit_sum = std::atof(row[0]);//1500
				bonus_db_bonus_sum = std::atof(row[1]);//1500
			}
		}

		double m_usr_balance = m_account->Equity() - (bonus_db_deposit_sum + bonus_db_bonus_sum); // 1000

		if (m_usr_balance < 0)
		{
			m_usr_balance = 0;
		}
		//mysql_close(p_conn);
		//mysql_library_end();
		
		return m_usr_balance;
	}
	else
	{
		m_api->LoggerOut(MTLogOK, L"get_user_balance NOT CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
		return 0;
	}
}
//+------------------------------------------------------------------+ 
//|                                                                  | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::CreditInOperation(MYSQL* p_conn, UINT64 usr_login, double dep_amount, double bns_rate, CMTStr256 db_crm_deposit_comment, string crm_user_transaction_id, string crm_user_deposit, int now, string crm_user_id, int cent)
{
	double cent_bonus	= 0;
	double cent_amount	= 0;

	if (p_conn)
	{
		if (m_api->UserAccountGet(usr_login, m_account) != MT_RET_OK)
			m_api->LoggerOut(MTLogErr, L"Getting Account FAILD %I64u (On Credit In Operation)", m_account->Login());

		double credit_bonus = (dep_amount * bns_rate) / 100;

		if (cent == 1)
		{
			cent_bonus = credit_bonus / 100;
			cent_amount = dep_amount / 100;
		}
		else
		{
			cent_bonus = credit_bonus;
			cent_amount = dep_amount;
		}

		UINT64 ZRO = 0;

		if (m_api->UserDepositChange(m_account->Login(), dep_amount, IMTDeal::DEAL_BALANCE, db_crm_deposit_comment.Str(), ZRO) == MT_RET_OK)
		{
			int qstate = 0;
			stringstream sss;

			sss << "INSERT INTO bonus_plugin_user_deposit(plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status, plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << m_account->Login() << "','" << crm_user_transaction_id << "','" << cent_amount << "','" << cent_bonus << "','" << now << "','Deposit', 'Credit In')";

			string query_in = sss.str();
			const char* q_in = query_in.c_str();
			qstate = mysql_query(p_conn, q_in);

			int qstate_up = 0;
			stringstream ss_up;
			ss_up << "UPDATE bonus_crm_bonus_user_deposit SET crm_deposit_status = 'Success', crm_status = '0' WHERE crm_user_id = '" << crm_user_id << "'";

			string query_up = ss_up.str();
			const char* q_up = query_up.c_str();
			qstate_up = mysql_query(p_conn, q_up);

			if (qstate_up == 0)
			{
				m_api->LoggerOut(MTLogErr, L"ACTION UPDATED");
			}
			else
			{
				m_api->LoggerOut(MTLogErr, L"ACTION NOT UPDATED");
			}
			if (bns_rate != 0)
			{
				if (m_api->UserDepositChange(m_account->Login(), credit_bonus, IMTDeal::DEAL_CREDIT, db_crm_deposit_comment.Str(), ZRO) == MT_RET_OK)
				{
					m_api->LoggerOut(MTLogErr, L"ACTION UPDATED");
				}
			}

		}
		//mysql_close(p_conn);
		//mysql_library_end();
	}
	else
	{
		m_api->LoggerOut(MTLogOK, L"CreditInOperation NOT CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}
//+------------------------------------------------------------------+ 
//|                                                                  | 
//+------------------------------------------------------------------+ 
//void CRM_Bonus_PluginInstance::user_lot_update(int usrLogin, int volume)
//{
//	MTAPIRES		retcode = MT_RET_OK;
//	IMTOrderArray*	ordersarray = m_api->OrderCreateArray();
//	//double		volume = 0;
//	IMTOrder*		m_order;
//	time_t			now = time(0);
//	time_t			rawtime;
//	struct tm*		timeinfo;
//	int				year, month, day;
//	double			lot_value = 0;
//
//	string			dummy;
//	MYSQL*			conn;
//	MYSQL_ROW		row;
//	MYSQL_RES*		Qres;
//	string			sum_bonus;
//	string			name, location;
//	UINT64			ZRO = 0;
//
//	conn = mysql_init(0);
//	conn = mysql_real_connect(conn, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0);
//
//	time(&rawtime);
//	struct tm newtime;
//	localtime_s(&newtime, &now);
//	std::string strf = std::to_string(newtime.tm_mday);
//	newtime.tm_mday = newtime.tm_mday + 1;
//
//	time_t t_tomorrow = mktime(&newtime);
//	time_t		t_today = time(0);
//
//	if (m_api->UserGet(usrLogin, m_user) != MT_RET_OK)
//		m_api->LoggerOut(MTLogOK, L"USR NOT Connected %I64u (On USER LOT UPDATE)", m_user->Login());
//
//	if (m_api->UserAccountGet(m_user->Login(), m_account) != MT_RET_OK)
//		return;
//	double voulume_value = volume;
//	double lot_value_double = voulume_value / 10000;
//
//	int qstate = 0, qstate_up = 0, qstate_lot = 0, qstate_lot_up = 0, qstate_uup = 0;
//
//	if (conn)
//	{
//		stringstream ss;
//		ss << "INSERT INTO plugin_user_lot (pl_user_login, pl_user_lot, pl_user_lot_date) VALUES('" << m_account->Login() << "','" << lot_value_double << "','" << now << "')";
//
//		string query = ss.str();
//		const char* q = query.c_str();
//		qstate = mysql_query(m_conn, q);
//
//		if (qstate == 0)
//		{
//			//mysql_close(conn);
//			check_user_lot(m_user->Login());
//			//mysql_library_end();
//		}
//		else
//		{
//			m_api->LoggerOut(MTLogOK, L"The LOT UPDATE IS NOT UPDATING");
//		}
//	}
//	else
//	{
//		m_api->LoggerOut(MTLogOK, L"user_lot_update NOT CONNECTED TO THE CRM");
//	}
//}

//In this Function am checking the USER Group if he is in same group or no
// Here we dont need any external parametrs am using a Global Paramentes from the Plugin DB
int CRM_Bonus_PluginInstance::check_usergroup()
{
	//---
	std::wstring delay_group_strng = db_param_group.Str();
	std::vector<std::string> delay_group_result;

	const std::string delay_group_str(delay_group_strng.begin(), delay_group_strng.end());
	stringstream delay_group_stream(delay_group_str); //create string stream from the string

	while (delay_group_stream.good()) {
		std::string delay_group_streamsubstr;
		getline(delay_group_stream, delay_group_streamsubstr, ','); //get first string delimited by comma
		delay_group_result.push_back(delay_group_streamsubstr);
	}
	for (int i = 0; i < delay_group_result.size(); i++)
	{
		std::wstring delay_group_stemp = std::wstring(delay_group_result.at(i).begin(), delay_group_result.at(i).end());
		LPCWSTR delay_group_sw = delay_group_stemp.c_str();

		//Check if the USER is in the Same group
		int TT = CMTStr::Compare(db_crm_user_group.Str(), delay_group_sw);
		if (TT == 0)
		{
			return 0;
		}
	}
	return 1;
}
//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
//MTAPIRES CRM_Bonus_PluginInstance::HookTradeRequestProcess(const IMTRequest* request, const IMTConfirm* confirm, const IMTConGroup* group, const IMTConSymbol* symbol, IMTPosition* position, IMTOrder* order, IMTDeal* deal)
//{
//	MTAPISTR		str;
//	IMTRequest*		request_new = NULL;
//	MTAPIRES		res = MT_RET_OK_NONE;
//
//	MYSQL* conn;
//	MYSQL_ROW		row;
//	MYSQL_RES*		Qres;
//	UINT64			ZRO = 0;
//	time_t			now = time(0);
//	double			bonus_amount_value = 0;
//	double			deposit_bonus_value = 0;
//	double			m_group_matched = 0;
//	double			total_bonus_out = 0;
//	double			user_profit = 0;
//	double			m_user_equity = 0;
//	double			total_bonus = 0;
//	double			total_deposit = 0;
//	int				other_group = 0;
//
//	//--- check
//	if (!request || !m_api) return(MT_RET_ERR_PARAMS);
//	
//	//--- check 
//	if (!position || !deal || !order)
//	{
//		m_api->LoggerOut(MTLogOK, L"=====> HookTradeRequestProcess: position deal or order = NULL");
//	}
//	else
//	{
//		//---
//		if (deal->Entry() == IMTDeal::ENTRY_IN)
//		{
//			//return(MT_RET_OK_NONE);
//			if (m_api->UserGet(order->Login(), m_user) != MT_RET_OK)
//				m_api->LoggerOut(MTLogOK, L"Error GETTING USER");
//			//---
//			IMTPosition* temp_position = m_api->PositionCreate();
//			res = m_api->PositionGetByTicket(position->Position(), temp_position);
//			if (res != MT_RET_OK)
//				m_api->LoggerOut(MTLogOK, L"HookTradeRequestProcess: PositionGetByTicket failed");
//			//---
//			int qstate = 0, qs = 0;
//
//			conn = mysql_init(0);
//			conn = mysql_real_connect(conn, m_server_ip, m_server_user, m_user_pwd, m_server_db, 0, NULL, 0);
//
//			if (m_api->UserAccountGet(m_user->Login(), m_account) != MT_RET_OK)
//			{
//				m_api->LoggerOut(MTLogErr, L"USER ACCOUNT FAILD %I64u (Bonus Validity Check)", m_account->Login());
//			}
//
//			if (conn)
//			{
//				stringstream ss;
//				ss << "SELECT * FROM plugin_user_deposit WHERE plugin_user_login = '" << m_user->Login() << "' AND plugin_user_deposit_status = 'Deposit' AND plugin_user_bonus_status = 'Credit In' AND plugin_user_bonus != 0";
//
//				string query	= ss.str();
//				const char* q	= query.c_str();
//				qstate			= mysql_query(m_conn, q);
//
//				if (qstate == 0)
//				{
//					Qres = mysql_store_result(conn);
//					while (row = mysql_fetch_row(Qres))
//					{
//						deposit_bonus_value = std::atof(row[3]);
//						bonus_amount_value = std::atof(row[4]);
//
//						total_deposit = total_deposit + deposit_bonus_value;
//						total_bonus = total_bonus + bonus_amount_value;
//					}
//
//					double credit_out_bonus = 0;
//					double equity_value = 0;
//
//					double account_equity_rate = db_param_account_equity_rate;
//
//					std::wstring cent_group_strng = db_param_cent_group.Str();
//					std::vector<std::string> cent_group_result;
//
//					const std::string cent_group_str(cent_group_strng.begin(), cent_group_strng.end());
//					stringstream cent_group_stream(cent_group_str); //create string stream from the string
//
//					while (cent_group_stream.good()) {
//						std::string cent_group_streamsubstr;
//						getline(cent_group_stream, cent_group_streamsubstr, ','); //get first string delimited by comma
//						cent_group_result.push_back(cent_group_streamsubstr);
//					}
//
//					for (int i = 0; i < cent_group_result.size(); i++)
//					{
//						std::wstring cent_group_stemp = std::wstring(cent_group_result.at(i).begin(), cent_group_result.at(i).end());
//						LPCWSTR cent_group_sw = cent_group_stemp.c_str();
//
//						//Check if the USER is in the Same group
//						int cent_group_compar = CMTStr::Compare(m_user->Group(), cent_group_sw);
//
//						if (cent_group_compar == 0)
//						{
//							++m_group_matched;
//							++other_group;
//
//							if (position->Profit() < 0)
//							{
//								user_profit = position->Profit() * (-1);
//
//								m_user_equity = m_account->Equity() - user_profit;
//
//								double cent_bonus = total_bonus * 100;
//								double cent_deposit = total_deposit * 100;
//
//								total_bonus_out = cent_bonus;
//
//								credit_out_bonus = (cent_bonus * account_equity_rate) / 100;
//								equity_value = cent_deposit + credit_out_bonus;
//							}
//							else
//							{
//								user_profit = position->Profit();
//
//								m_user_equity = m_account->Equity() + user_profit;
//
//								double cent_bonus = total_bonus * 100;
//								double cent_deposit = total_deposit * 100;
//
//								total_bonus_out = cent_bonus;
//
//								credit_out_bonus = (cent_bonus * account_equity_rate) / 100;
//								equity_value = cent_deposit + credit_out_bonus;
//							}
//						}
//					}
//
//					if (m_group_matched == 0)
//					{
//						std::wstring delay_group_strng = db_param_group.Str();
//						std::vector<std::string> delay_group_result;
//
//						const std::string delay_group_str(delay_group_strng.begin(), delay_group_strng.end());
//						stringstream delay_group_stream(delay_group_str); //create string stream from the string
//
//						while (delay_group_stream.good()) {
//							std::string delay_group_streamsubstr;
//							getline(delay_group_stream, delay_group_streamsubstr, ','); //get first string delimited by comma
//							delay_group_result.push_back(delay_group_streamsubstr);
//						}
//						for (int i = 0; i < delay_group_result.size(); i++)
//						{
//							std::wstring delay_group_stemp = std::wstring(delay_group_result.at(i).begin(), delay_group_result.at(i).end());
//							LPCWSTR delay_group_sw = delay_group_stemp.c_str();
//
//							//Check if the USER is in the Same group
//							int TT = CMTStr::Compare(m_user->Group(), delay_group_sw);
//							if (TT == 0)
//							{
//								++other_group;
//								if (position->Profit() < 0)
//								{
//									user_profit = position->Profit() * (-1);
//									m_user_equity = m_account->Equity() - user_profit;
//									total_bonus_out = total_bonus;
//									credit_out_bonus = (total_bonus * account_equity_rate) / 100;
//									equity_value = total_deposit + credit_out_bonus;
//									
//								}
//								else
//								{
//									user_profit = position->Profit();
//									m_user_equity = m_account->Equity() + user_profit;
//									total_bonus_out = total_bonus;
//									credit_out_bonus = (total_bonus * account_equity_rate) / 100;
//									equity_value = total_deposit + credit_out_bonus;
//								}
//							}
//						}
//					}
//					if (other_group == 0)
//					{
//						return(MT_RET_OK);
//					}
//
//					if (m_user_equity == equity_value || m_user_equity < equity_value)
//					{
//						return(MT_RET_OK_NONE);
//					}
//					else
//					{
//						return(MT_RET_OK);
//					}
//				}
//				
//			}
//			else
//			{
//				m_api->LoggerOut(MTLogOK, L"CHECK EQUITY NOT CONNECTED");
//			}
//			
////			mysql_close(conn);
////			mysql_library_end();
//			//---
//			if (temp_position)
//				temp_position->Release();
//		}
//	}
//}
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
//| In This Function we are comparing btween Two LPCWSTR VALUE must of time we are using this function to compare the User group                                  | 
//+---------------------------------------------------------------------------------------------------------------------------------------------------------------+ 
int CRM_Bonus_PluginInstance::Compare(LPCWSTR str1, LPCWSTR str2)
{
	if (wcscmp(str1, str2) == 0)
	{
		return 0;
	}
	else {
		return 1;
	}
}
//+------------------------------------------------------------------+ 
//|                                                                  | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::check_bonus_validity(MYSQL* p_conn,UINT64 user_login)
{
	string dummy;
	MYSQL_ROW row;
	MYSQL_RES* res;
	string st_credit_amount;
	string st_user_credit_deposit;
	double credit_amount = 0;
	string plugin_user_deposit_date_db;
	string plugin_user_bonus_db;
	string deposit_id;
	time_t now = time(0);
	UINT64 ZRO = 0;

	if (m_api->UserAccountGet(user_login, m_account) != MT_RET_OK)
	{
		m_api->LoggerOut(MTLogErr, L"USER ACCOUNT FAILD %I64u (Bonus Validity Check)", m_account->Login());
	}
	if (p_conn)
	{
		int qstate = 0, quantity, qstate_cr = 0;

		stringstream ss;

		ss << "SELECT * FROM bonus_plugin_user_deposit WHERE plugin_user_login = '" << m_account->Login() << "' AND plugin_user_bonus_status = 'Credit In'";

		string query = ss.str();
		const char* q = query.c_str();
		qstate = mysql_query(p_conn, q);

		if (qstate == 0) {
			res = mysql_store_result(p_conn);
			while (row = mysql_fetch_row(res)) {
				deposit_id = row[0];
				plugin_user_bonus_db = row[5];
				plugin_user_deposit_date_db = row[6];

				int deposit_id_st = std::atoi(row[0]);
				int user_bonus_validity = std::atoi(row[6]);
				double plugin_user_bonus_stm = std::atof(row[5]);
				int bonus_unit_validity = db_param_BonusValidity; //90

				
				if (now > bonus_validity_expery(bonus_unit_validity, user_bonus_validity))
				{
					double credit_experied = plugin_user_bonus_stm * (-1);
					if (m_api->UserDepositChange(m_account->Login(), credit_experied, IMTDeal::DEAL_CREDIT, L"INTIAL BONUS", ZRO) == MT_RET_OK)
					{
						update_expred_bonus_db(p_conn, deposit_id_st, m_account->Login(), "Bonus Experied", credit_experied, now);
					}
				}
			}
		}
		
		//mysql_close(p_conn);
		//mysql_library_end();
	}
	else
	{
		m_api->LoggerOut(MTLogErr, L"ParametersRead_db Not CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}

//MTAPIRES CRM_Bonus_PluginInstance::HookTradeRequestProcess(const IMTRequest* request, const IMTConfirm* confirm, const IMTConGroup* group, const IMTConSymbol* symbol, IMTPosition* position, IMTOrder* order, IMTDeal* deal)
//{
//	MYSQL*			a_conn;
//	MTAPISTR		str;
//	//--- check
//	if (!request || !m_api) return(MT_RET_ERR_PARAMS);
//
//	if (deal->Action() == IMTDeal::ENTRY_IN)
//	{
//		m_api->LoggerOut(MTLogOK, L"NEW ORDER DETECTED");
//		//In this function we are converting the parameters to const cher *
//		wstring ws_database(crm_database_pr.Str());
//		string str_database(ws_database.begin(), ws_database.end());
//		char_database_pr = str_database.c_str();
//
//		wstring ws_database_password(crm_database_password_pr.Str());
//		string str_database_password(ws_database_password.begin(), ws_database_password.end());
//		char__database_password_pr = str_database_password.c_str();
//
//		wstring ws_database_user(crm_database_user_pr.Str());
//		string str_database_user(ws_database_user.begin(), ws_database_user.end());
//		char_database_user_pr = str_database_user.c_str();
//
//		wstring ws_database_Host(crm_database_Host_pr.Str());
//		string str_database_Host(ws_database_Host.begin(), ws_database_Host.end());
//		char_database_Host_pr = str_database_Host.c_str();
//
//		// connecting to the server we can update it from the plugin parameters
//		a_conn = mysql_init(0);
//		a_conn = mysql_real_connect(a_conn, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0);
//
//		if (a_conn) {
//			get_equity_users(m_conn);
//			mysql_close(a_conn);
//		}
//		else
//		{
//			m_api->LoggerOut(MTLogOK, L"HookTradeRequestAdd Not CONNECTED");
//		}
//	}
//	return(MT_RET_OK);
//}

//+------------------------------------------------------------------+ 
//| Updating the bonus status                                        | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::update_expred_bonus_db(MYSQL* p_conn, int bonus_id, UINT64 m_login, string transaction_id, double experied_bonus, int times)
{
	// In this function we are updating the bonus status after the expary date and crdit out operation
	int qstate = 0, quantity;
	if (p_conn)
	{
		stringstream ss;
		// Query to update the bonus status
		ss << "UPDATE bonus_plugin_user_deposit SET plugin_user_bonus_status = 'Bonus Experied', plugin_user_status = '0' WHERE plugin_user_id = '" << bonus_id << "'";

		string query = ss.str();
		const char* q = query.c_str();
		qstate = mysql_query(p_conn, q);

		//if (qstate == 0)
		//{
		//	int qstate = 0, quantity;
		//	string name, location;

		//	stringstream sss;
		//	// inserting the credit out operation in the data base
		//	sss << "INSERT INTO bonus_plugin_user_deposit(plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status, plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << m_login << "','" << transaction_id << "','" << experied_bonus << "','" << 0 << "','" << times << "','Credit Experied', 'Credit Experied')";

		//	string query_in = sss.str();
		//	const char* q_in = query_in.c_str();
		//	qstate = mysql_query(p_conn, q_in);
		//}
		//else
		//{
		//	m_api->LoggerOut(MTLogErr, L"ACTION NOT UPDATED");
		//}

		//mysql_close(p_conn);
		//mysql_library_end();
	}
	else
	{
		m_api->LoggerOut(MTLogOK, L"update_expred_bonus_db NOT CONNECTED");
		m_api->LoggerOut(MTLogErr, L"mysql_errno----->>>> LIVE 8.0 LOCAL DB ParametersRead_db Not CONNECTED fprintf 02");
		m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 1=====>>> %u\n", mysql_errno(p_conn));
		m_api->LoggerOut(MTLogErr, L"ERRORS 2=====>>> %s\n", mysql_error(p_conn));

		MYSQL mysql;

		mysql_init(&mysql);
		mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "your_prog_name");
		if (mysql_real_connect(&mysql, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0))
		{
			m_api->LoggerOut(MTLogErr, L"NO ERROR IN HOST USER PWD or DB");
		}
		else
		{
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 3=====>>> %u\n", mysql_errno(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 4=====>>> %c\n", mysql_error(&mysql));
			m_api->LoggerOut(MTLogErr, L"mysql_errno ERRORS 5=====>>> %s\n", mysql_error(&mysql));
		}
		mysql_close(&mysql);
	}
}
//+------------------------------------------------------------------+ 
//| Processing of adding a new order                                 | 
//+------------------------------------------------------------------+ 
int CRM_Bonus_PluginInstance::bonus_validity_expery(int unit, int timestamp)
{
	time_t now = timestamp;
	time_t rawtime;
	time_t t_tomorrow{};
	struct tm* timeinfo;
	int year, month, day;

	time(&rawtime);
	struct tm newtime;
	localtime_s(&newtime, &now);
	std::string str = std::to_string(newtime.tm_mday);
	newtime.tm_mday = newtime.tm_mday + unit;

	t_tomorrow = mktime(&newtime);

	return t_tomorrow;
}
//+------------------------------------------------------------------+ 
//| Processing of adding a new order                                 | 
//+------------------------------------------------------------------+ 
//void CRM_Bonus_PluginInstance::check_user_lot(INT64 User_login)
//{
//	MYSQL_ROW row;
//	MYSQL_RES* res;
//	MYSQL_RES* Qres;
//	string sum_bonus;
//	string sum_lot;
//	time_t now = time(0);
//	UINT64 ZRO;
//	MYSQL* conn;
//	double sum_bonus_pl = 0;
//	double sum_lot_pl = 0;
//	int qstate = 0, qstate_lot_up = 0, qstate_up = 0, qstate_lot_sum = 0;
//
//	conn = mysql_init(0);
//	conn = mysql_real_connect(conn, char_database_Host_pr, char_database_user_pr, char__database_password_pr, char_database_pr, 0, NULL, 0);
//
//	if (m_api->UserGet(User_login, m_user) != MT_RET_OK)
//		m_api->LoggerOut(MTLogOK, L"Error GETTING USER %I64u (On USER CHECK LOT)", m_user->Login());
//
//	if (m_api->UserAccountGet(m_user->Login(), m_account) != MT_RET_OK)
//		m_api->LoggerOut(MTLogOK, L"check_user_lot->Error GETTING ACCOUNT %I64u (On USER CHECK LOT)");
//
//	//--- print order description 
//	std::wstring delay_group_strng = db_param_group.Str();
//	std::vector<std::string> delay_group_result;
//
//	const std::string delay_group_str(delay_group_strng.begin(), delay_group_strng.end());
//	stringstream delay_group_stream(delay_group_str); //create string stream from the string
//
//	while (delay_group_stream.good()) {
//		std::string delay_group_streamsubstr;
//		getline(delay_group_stream, delay_group_streamsubstr, ','); //get first string delimited by comma
//		delay_group_result.push_back(delay_group_streamsubstr);
//	}
//
//	for (int i = 0; i < delay_group_result.size(); i++)
//	{
//		std::wstring delay_group_stemp = std::wstring(delay_group_result.at(i).begin(), delay_group_result.at(i).end());
//		LPCWSTR delay_group_sw = delay_group_stemp.c_str();
//
//		//Check if the USER is in the Same group
//		int TT = CMTStr::Compare(m_user->Group(), delay_group_sw);
//
//		if (TT == 0)
//		{
//			stringstream ss_up;
//			// we are slecting the sum of the user crdit in order to compare it with the user lot
//			ss_up << "SELECT SUM(plugin_user_bonus) FROM plugin_user_deposit WHERE plugin_user_login = '" << m_account->Login() << "' AND plugin_user_bonus_status = 'Credit In'";
//
//			string query_up = ss_up.str();
//			const char* q_up = query_up.c_str();
//			qstate_up = mysql_query(m_conn, q_up);
//			if (qstate_up == 0)
//			{
//				res = mysql_store_result(conn);
//				while (row = mysql_fetch_row(res))
//				{
//					//sum_bonus = row[0];
//					sum_bonus_pl = std::atof(row[0]);
//				}
//
//				//std::wstring sum_bonus_stemp = std::wstring(sum_bonus.begin(), sum_bonus.end());
//
//				stringstream ss_lot_sum;
//				// we are slecting the sum of the user lot in order to compare it with the user credit
//				ss_lot_sum << "SELECT SUM(pl_user_lot) FROM plugin_user_lot WHERE pl_user_login = '" << m_account->Login() << "'";
//
//				string query_lot_sum = ss_lot_sum.str();
//				const char* q_lot_sum = query_lot_sum.c_str();
//				qstate_lot_sum = mysql_query(m_conn, q_lot_sum);
//
//				if (qstate_lot_sum == 0)
//				{
//					Qres = mysql_store_result(conn);
//					while (row = mysql_fetch_row(Qres))
//					{
//						//sum_lot = row[0];
//						sum_lot_pl = std::atof(row[0]);
//					}
//
//					//std::wstring sum_lot_stemp = std::wstring(sum_lot.begin(), sum_lot.end());
//
//					if (sum_bonus_pl == sum_lot_pl || sum_bonus_pl < sum_lot_pl)
//					{
//						double bonus_credit_out = sum_lot_pl * (-1);
//
//						if (m_api->UserDepositChange(m_account->Login(), bonus_credit_out, IMTDeal::DEAL_CREDIT, L"CreditOut by reached lot size of credit", ZRO) == MT_RET_OK)
//						{
//							update_lot_db(conn, m_account->Login(), "LOT Trans", sum_lot_pl, now);
//						}
//					}
//				}
//				mysql_close(conn);
//				mysql_library_end();
//			}
//			else
//			{
//				m_api->LoggerOut(MTLogOK, L"The LOT UPDATE IS NOT UPDATING");
//			}
//		}
//	}
//}
//+------------------------------------------------------------------+ 
//| Processing The Update of lot in CRM DB                           | 
//+------------------------------------------------------------------+ 
//void CRM_Bonus_PluginInstance::update_lot_db(MYSQL* conn, UINT64 m_login, string transaction_id, double lot_credit_out, int timestamp)
//{
//	// Here in this function we are checking if the user lot is same then User credit in this case we will credit our the credit and we will insert it to the user balance
//	MYSQL_ROW row;
//	int qstate_lot_m = 0, qstate_lot_in = 0, qstate_lot_up = 0, qstate = 0;
//	UINT64 ZRO = 0;
//
//	double bonus_credit_out = lot_credit_out * (-1);
//
//	stringstream s_lot_m;
//	// Here saving the credit out operation
//	s_lot_m << "INSERT INTO plugin_user_deposit(plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status, plugin_user_bonus_status) VALUES('" << m_login << "','" << transaction_id << "','" << bonus_credit_out << "','" << 0 << "','" << timestamp << "','Credit Out', 'CreditOut by reached lot size of credit')";
//
//	string query_lot_m = s_lot_m.str();
//	const char* q_lot_m = query_lot_m.c_str();
//	qstate_lot_m = mysql_query(m_conn, q_lot_m);
//
//
//	if (m_api->UserDepositChange(m_login, lot_credit_out, IMTDeal::DEAL_BALANCE, L"Deposit for reached lot size of credit", ZRO) == MT_RET_OK)
//	{
//		stringstream s_lot_in;
//		//Saveing the user deporit operaration after the credit out
//		s_lot_in << "INSERT INTO plugin_user_deposit(plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status, plugin_user_bonus_status) VALUES('" << m_login << "','" << transaction_id << "','" << lot_credit_out << "','" << 0 << "','" << timestamp << "','Deposit', 'Deposit for reached lot size of credit')";
//
//		string query_lot_in = s_lot_in.str();
//		const char* q_lot_in = query_lot_in.c_str();
//		qstate_lot_in = mysql_query(m_conn, q_lot_in);
//
//		stringstream ss_uup;
//		// we are selecting all the credit in of ther user in order to update the bonus status 
//		ss_uup << "SELECT * FROM plugin_user_deposit WHERE plugin_user_login = '" << m_login << "' plugin_user_bonus_status = 'Credit In'";
//
//		string query_uup = ss_uup.str();
//		const char* q_uup = query_uup.c_str();
//		qstate = mysql_query(m_conn, q_uup);
//
//		if (qstate == 0)
//		{
//			MYSQL_RES* res_up;
//			res_up = mysql_store_result(conn);
//
//			while (row = mysql_fetch_row(res_up))
//			{
//				string action_id = row[0];
//				stringstream ss_lot_up;
//				// here we are updating the bonus status 
//				ss_lot_up << "UPDATE plugin_user_deposit SET plugin_user_bonus_status = 'Credit Out' WHERE plugin_user_id = '" << action_id << "'";
//
//				string query_lot_up = ss_lot_up.str();
//				const char* q_lot_up = query_lot_up.c_str();
//				qstate_lot_up = mysql_query(p_conn, q_lot_up);
//			}
//		}
//	}
//}
//+------------------------------------------------------------------+ 
//| Processing of adding a new order                                 | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::get_equity_users(MYSQL* p_conn)
{
	MYSQL_ROW		row;
	MYSQL_RES*		res;
	string			user_login;
	MYSQL*			e_conn;
	int qstate = 0;

	stringstream ss;

	ss << "SELECT plugin_user_login FROM bonus_plugin_user_deposit GROUP BY plugin_user_login";

	string query = ss.str();
	const char* q = query.c_str();
	qstate = mysql_query(p_conn, q);

	if (qstate == 0)
	{
		res = mysql_store_result(p_conn);
		UINT64 m_user_login;

		while (row = mysql_fetch_row(res))
		{
			user_login = row[0];
			m_user_login = std::atoll(row[0]);

			if (m_api->UserAccountGet(m_user_login, m_account) != MT_RET_OK)
				m_api->LoggerOut(MTLogOK, L"Error GETTING ACCOUNT %I64u", m_user_login);

			check_user_equity(p_conn, m_account);
		}
	}
	//mysql_close(p_conn);
	//mysql_library_end();

}
//+------------------------------------------------------------------+ 
//| Processing of adding a new order                                 | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::check_user_equity(MYSQL* p_conn, IMTAccount* u_account)
{
	MYSQL_ROW		row;
	MYSQL_RES*		Qres;
	UINT64			ZRO = 0;
	time_t			now = time(0);
	double			bonus_amount_value = 0;
	double			deposit_bonus_value = 0;
	double			m_group_matched = 0;
	double			total_bonus_out = 0;

	if (m_api->UserGet(u_account->Login(), m_user) != MT_RET_OK)
		m_api->LoggerOut(MTLogOK, L"Error GETTING ACCOUNT %I64u", m_account->Login());

	double			total_bonus = 0;
	double			total_deposit = 0;

	int qstate = 0, qs = 0;

	stringstream ss;
	ss << "SELECT * FROM bonus_plugin_user_deposit WHERE plugin_user_login = '" << u_account->Login() << "' AND plugin_user_deposit_status = 'Deposit' AND plugin_user_bonus_status = 'Credit In' AND plugin_user_bonus != 0";

	string query = ss.str();
	const char* q = query.c_str();
	qstate = mysql_query(p_conn, q);

	if (qstate == 0)
	{
		Qres = mysql_store_result(p_conn);
		while (row = mysql_fetch_row(Qres))
		{
			deposit_bonus_value = std::atof(row[4]);
			bonus_amount_value = std::atof(row[5]);

			total_deposit = total_deposit + deposit_bonus_value;
			total_bonus = total_bonus + bonus_amount_value;
		}

		double credit_out_bonus = 0;
		double equity_value = 0;

		double account_equity_rate = db_param_account_equity_rate;
			
		std::wstring cent_group_strng = db_param_cent_group.Str();
		std::vector<std::string> cent_group_result;

		const std::string cent_group_str(cent_group_strng.begin(), cent_group_strng.end());
		stringstream cent_group_stream(cent_group_str); //create string stream from the string

		while (cent_group_stream.good()) {
			std::string cent_group_streamsubstr;
			getline(cent_group_stream, cent_group_streamsubstr, ','); //get first string delimited by comma
			cent_group_result.push_back(cent_group_streamsubstr);
		}

		for (int i = 0; i < cent_group_result.size(); i++)
		{
			std::wstring cent_group_stemp = std::wstring(cent_group_result.at(i).begin(), cent_group_result.at(i).end());
			LPCWSTR cent_group_sw = cent_group_stemp.c_str();

			//Check if the USER is in the Same group
			int cent_group_compar = CMTStr::Compare(m_user->Group(), cent_group_sw);

			if (cent_group_compar == 0)
			{
				++m_group_matched;
					
				double cent_bonus		= total_bonus * 100;
				double cent_deposit		= total_deposit * 100;
					
				total_bonus_out			= cent_bonus;
					
				credit_out_bonus = (cent_bonus * db_param_account_equity_rate) / 100; 
				equity_value = cent_deposit + credit_out_bonus;
			}
		}
			
		if (m_group_matched == 0)
		{
			std::wstring delay_group_strng = db_param_group.Str();
			std::vector<std::string> delay_group_result;

			const std::string delay_group_str(delay_group_strng.begin(), delay_group_strng.end());
			stringstream delay_group_stream(delay_group_str); //create string stream from the string

			while (delay_group_stream.good()) {
				std::string delay_group_streamsubstr;
				getline(delay_group_stream, delay_group_streamsubstr, ','); //get first string delimited by comma
				delay_group_result.push_back(delay_group_streamsubstr);
			}
			for (int i = 0; i < delay_group_result.size(); i++)
			{
				std::wstring delay_group_stemp = std::wstring(delay_group_result.at(i).begin(), delay_group_result.at(i).end());
				LPCWSTR delay_group_sw = delay_group_stemp.c_str();

				//Check if the USER is in the Same group
				int TT = CMTStr::Compare(m_user->Group(), delay_group_sw);
				if (TT == 0)
				{
					total_bonus_out = total_bonus;
					credit_out_bonus = (total_bonus * db_param_account_equity_rate) / 100;
					equity_value = total_deposit + credit_out_bonus;
				}
			}
		}

		if (u_account->Equity() == equity_value || u_account->Equity() < equity_value)
		{
			stringstream ss_qs;
			ss_qs << "SELECT * FROM bonus_plugin_user_deposit WHERE plugin_user_login = '" << u_account->Login() << "' AND plugin_user_deposit_status = 'Deposit' AND plugin_user_bonus_status = 'Credit In' AND plugin_user_bonus != 0";

			string query_qs = ss_qs.str();
			const char* q_qs = query_qs.c_str();
			qs = mysql_query(p_conn, q_qs);

			if (qs == 0)
			{
				MYSQL_RES* res_up;
				res_up = mysql_store_result(p_conn);
				int m_plugin_id = 0;

				while (row = mysql_fetch_row(res_up))
				{
					int m_plugin_id = std::atoi(row[0]);
					double m_bonus_credit_out = total_bonus_out * (-1);
					if (m_api->UserDepositChangeRaw(m_account->Login(), m_bonus_credit_out, IMTDeal::DEAL_CREDIT, L"50% Credit used", ZRO) == MT_RET_OK)
					{
						update_equity_db(m_plugin_id, p_conn, m_account->Login(), "50% Credit used", m_bonus_credit_out, now, total_bonus);
					}
				}
			}
		}
	}
	/*mysql_close(p_conn);
	mysql_library_end();*/
}
//+------------------------------------------------------------------+ 
//| Processing of adding a new order                                 | 
//+------------------------------------------------------------------+ 
void CRM_Bonus_PluginInstance::update_equity_db(int ID, MYSQL* p_conn, UINT64 m_login, string transaction_id, double credit_out, int timestamp, double total_bonus)
{
	int qstate_lot_up = 0;

	stringstream ss_lot_up;
	ss_lot_up << "UPDATE bonus_plugin_user_deposit SET plugin_user_bonus_status = 'Equity Credit Out', plugin_user_status = '0' WHERE plugin_user_id = '" << ID << "'";

	string query_lot_up = ss_lot_up.str();
	const char* q_lot_up = query_lot_up.c_str();
	qstate_lot_up = mysql_query(p_conn, q_lot_up);

	/*int qstate_eq = 0;

	stringstream s_eq;

	s_eq << "INSERT INTO bonus_plugin_user_deposit(plugin_user_manager,plugin_user_login, plugin_user_transaction_id, plugin_user_deposit, plugin_user_bonus,plugin_user_deposit_date,plugin_user_deposit_status, plugin_user_bonus_status) VALUES('" << m_managerLogin << "','" << m_login << "','" << transaction_id << "','" << total_bonus << "','" << 0 << "','" << timestamp << "','50% Credit used', 'Credit Out')";

	string query_eq = s_eq.str();
	const char* q_eq = query_eq.c_str();
	qstate_eq = mysql_query(p_conn, q_eq);*/

	//mysql_close(p_conn);
	//mysql_library_end();
}
//+------------------------------------------------------------------+
