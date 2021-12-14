#pragma once 
#include "..\..\..\API\MT5APIServer.h"
#include "BonusTimer.h"
#include <mysql.h>
#include <string>

using namespace std;
//+------------------------------------------------------------------+ 
//| Plugin implementation                                            | 
//+------------------------------------------------------------------+ 
class CRM_Bonus_PluginInstance :
	public IMTServerPlugin,
	public IMTServerSink,
	public IMTConPluginSink,
	public IMTConManagerSink,
	public IMTTradeSink,
	public IMTRequestSink,
	public IMTDealSink
{
private:
	//--- server API interface
	IMTServerAPI*	   m_api;
	CMTSync            m_sync;

	//--- plugin config interface
	IMTConPlugin*		m_config;
	IMTConManager*		m_manager;
	LPCWSTR				group_name;
	MYSQL*				m_conn;

	//--- plugin parameters
	CMTStr256			db_param_group;
	CMTStr256			db_param_cent_group;
	int					db_param_BonusValidity;
	CMTStr256			db_param_BonusValidityUnit;
	int					isBusy;

	BonusTimer			TritechTimer = BonusTimer();

	//CMTStr256			db_crm_user_login;
	//CMTStr256			db_crm_user_transaction_id;
	//CMTStr256			db_crm_user_deposit;
	//CMTStr256			db_crm_bonus_rate;
	
	//Multi Server Connection
	UINT64				m_managerLogin;
	const char*			m_server_ip;
	const char*			m_server_db;
	const char*			m_server_user;
	const char*			m_user_pwd;
	int					managerStatus;

	//Server Connection 
	const char*			char_database_pr;
	const char*			char__database_password_pr;
	const char*			char_database_user_pr;
	const char*			char_database_Host_pr;

	CMTStr256			crm_database_pr;
	CMTStr256			crm_database_Host_pr;
	CMTStr256			crm_database_user_pr;
	CMTStr256			crm_database_password_pr;
	CMTStr256			crm_plugin_license_pr;

	double				m_conf_mask;
	//--- 
	IMTDealArray*		m_deals;
	IMTDealArray*		order_deals;
	IMTConGroup*		m_group;
	CMTStr256*			m_groups;
	IMTConGroupSymbol*	m_group_symbol;
	//IMTUser*			m_user;
	//IMTAccount*			m_account;
	IMTRequest*			request;
	double lastCall = 0;

public:
	CRM_Bonus_PluginInstance(void);
	~CRM_Bonus_PluginInstance(void);

	int			LicenseCheck();
	//virtual void		OnDealAdd(const IMTDeal* deal);
	int					OnUserCredit(UINT64 usr_login, string comment);
	void				OnUserCreditExc(UINT64 usr_login, double creditOut);
	//void			    ParametersRead_db();
	void				PluginConfigRead();
	void				OnPramsFetch(string param_group, string param_BonusValidityUnit, string param_CentGroup, UINT64 m_managerLogin, int db_param_BonusValidity, double group_rate);
	void			    onCRMDeposit(MYSQL* p_conn, UINT64 managerId, LPCWSTR accountGroup, LPCWSTR centGroup, UINT ISize, double group_rate, double BonusValidity);
	void				centGroupProcess(MYSQL* ceConn, UINT64 managerId, LPCWSTR centGroup, CMTStr256 db_crm_contest_user_reason, CMTStr256 db_crm_deposit_comment, CMTStr256 db_crm_user_group, double BonusValidity);
	//MTAPIRES			HookTradeRequestProcess(const IMTRequest* request, const IMTConfirm* confirm, const IMTConGroup* group, const IMTConSymbol* symbol, IMTPosition* position, IMTOrder* order, IMTDeal* deal);
	void				CentGroupExecution(MYSQL* p_conn, double dep_amount, UINT64 usr_login, double bns_rate, int crm_user_transaction_id, double crm_user_deposit, int crm_user_id, LPCWSTR centGroup, UINT64 managerId, LPCWSTR db_crm_contest_user_reason, CMTStr256 db_crm_deposit_comment, CMTStr256 db_crm_user_group, double BonusValidity);
	void				ClassicGroupExecution(MYSQL* p_conn, UINT64 usr_login, double bns_rate, UINT64 crm_user_transaction_id, double crm_user_deposit, int crm_user_id, CMTStr256 db_crm_contest_user_reason, CMTStr256 db_crm_deposit_comment, CMTStr256 db_crm_user_group, double BonusValidity);
	void				creditOutCreditBalance(MYSQL* p_conn, UINT64 user_login, double credit_out_amount, CMTStr256 db_crm_deposit_comment, double dep_amount, double m_usr_balance, int crm_user_id, int crm_user_transaction_id, int times, double crm_user_deposit, CMTStr256 user_group, int cent, double BonusValidity);
	double				WithdrawAmount(MYSQL* Wconn, UINT64 user_login, double credit_out_amount);
	void				deposit_Failed(MYSQL* f_conn, int user_id, string Reason);
	void				deposit_Success(MYSQL* f_conn, int user_id);
	void				OnWithdraw(MYSQL* w_conn, UINT64 m_managerLogin, UINT64 user_login, int crm_user_transaction_id, double withdrawreq, double total_bonuscrediout, int times, string depositSts, string bonusSts, CMTStr256 comment);
	void				dep_cancelled(MYSQL* c_conn, int deposit_usr_id, string reason);
	void				CreditOutBalance(MYSQL* c_conn, int deposit_usr_id, double credOut, string reason);
	void				creditOutBalance(MYSQL* p_conn, UINT64 user_login, double credit_out_amount, CMTStr256 db_crm_deposit_comment, int crm_user_id, int crm_user_transaction_id, int times, CMTStr256 user_group, int cent, double BonusValidity);
	//void				check_the_equity_db(MYSQL* conn);
	double				get_user_balance(MYSQL* p_conn, double dep_amount, UINT64 usr_login);
	double				get_LimitBalance(MYSQL* p_conn, UINT64 usr_login);
	void				CreditInOperation(MYSQL* p_conn, UINT64 usr_login, double dep_amount, double bns_rate, CMTStr256 db_crm_deposit_comment, int crm_user_transaction_id, double crm_user_deposit, int now, int crm_user_id, int cent);
	//void				user_lot_update(int usrLogin, int volume);
	//void*				Allocate(const UINT  bytes);

	//int					check_usergroup();

	//--- IMTServerPlugin methods 
	virtual void		Release(void);
	virtual MTAPIRES	Start(IMTServerAPI* server);
	virtual MTAPIRES	Stop(void);
	//void				TimerStop(void);

	//--- open orders events 
	//MTAPIRES			HookTradeRequestProcess(const IMTRequest* request, const IMTConfirm* confirm, const IMTConGroup* group, const IMTConSymbol* symbol, IMTPosition* position, IMTOrder* order, IMTDeal* deal);

	static int			Compare(LPCWSTR  str1, LPCWSTR  str2 );
	void				check_bonus_validity(MYSQL* p_conn, UINT64 user_login, double BonusValidity);
	void				update_expred_bonus_db(MYSQL* p_conn, int bonus_id, UINT64 m_login, std::string transaction_id, double experied_bonus, int times);
	int					bonus_validity_expery(int unit, int timestamp);
	//void EnDealEntry();
	//void				check_user_lot(INT64 User_login);
	//void				update_lot_db(MYSQL* conn, UINT64 m_login, std::string transaction_id, double lot_credit_out, int timestamp);
	void				get_equity_users(MYSQL* p_conn, double rate, LPCWSTR classicGroup, LPCWSTR centGroup);
	void				check_user_equity(MYSQL* p_conn, UINT64 u_account, double accRate, LPCWSTR classicGroup, LPCWSTR centGroup ,IMTUser* ch_user, IMTAccount* ch_account);
	void				update_equity_db(int ID, MYSQL* p_conn, UINT64 m_login, std::string transaction_id, double credit_out, int timestamp, double total_bonus);
	 
	//void CALLBACK timerCallback(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime);
private:
	MTAPIRES			ParametersRead(void);

	//--- IMTConPluginSinc interface implementation
	virtual void		OnPluginUpdate(const IMTConPlugin* plugin);
};
//+------------------------------------------------------------------+