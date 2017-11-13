#include "ttl_cache.h"

#include <iostream>
#include <string>

using namespace std::chrono;
using namespace std::string_literals;

enum EmDataType : ttl::DataType	//��������
{
	DT_Err = ttl::DT_Err,	//����
	DT_AccountInfo,
	DT_AccountList,
	DT_TradeHistory,
};

class AccountInfo
{
public:
	AccountInfo(long id, const std::string& owner, const std::string& bank, unsigned long balance)
		: m_id(id)
		, m_owner(owner)
		, m_bank(bank)
		, m_balance(balance)
	{
		std::cout << "AccountInfo standard constructor." << std::endl;
	}
	AccountInfo(const AccountInfo& right)
	{
		m_id = (right.m_id);
		m_owner = (right.m_owner);
		m_bank = (right.m_bank);
		m_balance = (right.m_balance);
		std::cout << "AccountInfo copy constructor." << std::endl;
	}
	AccountInfo(AccountInfo&& right)
	{
		m_id = std::move(right.m_id);
		m_owner = std::move(right.m_owner);
		m_bank = std::move(right.m_bank);
		m_balance = std::move(right.m_balance);
		std::cout << "AccountInfo move constructor." << std::endl;
	}
	~AccountInfo()
	{
		std::cout << "AccountInfo destructor." << std::endl;
	}

public:
	long m_id;
	std::string m_owner;
	std::string m_bank;
	unsigned long m_balance;
};

//ʹ�ó����£�����Ҫ����right���䣨const����so��ʹ�ô���д�����ܸ���
std::list<AccountInfo>& operator+=(std::list<AccountInfo>& left, std::list<AccountInfo>& right)
{
	for(auto it = right.begin(); it != right.end(); ++it)
		left.push_back(std::move(*it));
	return left;
}

class TradeHistory
{
public:
	TradeHistory(long account)
		: m_account(account)
	{
		std::cout << "TradeHistory standard constructor." << std::endl;
	}
	TradeHistory(const TradeHistory& right)
	{
		m_account = (right.m_account);
		m_history = (right.m_history);
		std::cout << "TradeHistory copy constructor." << std::endl;
	}
	TradeHistory(TradeHistory&& right)
	{
		m_account = std::move(right.m_account);
		m_history = std::move(right.m_history);
		std::cout << "TradeHistory move constructor." << std::endl;
	}
	~TradeHistory()
	{
		std::cout << "TradeHistory destructor." << std::endl;
	}
	TradeHistory& operator+=(TradeHistory& that)
	{
		if (m_account == that.m_account)
			for (auto it = that.m_history.begin(); it != that.m_history.end(); ++it)
				m_history.push_back(std::move(*it));
		return *this;
	}

public:
	long m_account;
	std::list<std::pair<long, std::string>> m_history;	//list<��֧���(+����/-֧��)�����䶯˵��>
};

void testttlcache_1()//��������
{
	//1. û�л���ʱ��ȡʧ��
	ttl::cache<AccountInfo> s1;
	//Լ��������������ʾȡ��������Ϊ"zxj"���˺�Ϊ"100101"���˻����Ļ�������
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_AccountInfo, "zxj"s, 100101);
	//2.1) ����һ����������10s�Ļ�������
	{
		ttl::cache<AccountInfo> c1(new AccountInfo(100101, "zxj", "�й�����", 666666), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101);
	}
	//2.2) ��ʱ�뿪��c1��������һ���ܹ���ȡ��ǰ�����Ļ������ݣ���ע�⣺�������������������ӻ���ʱ��Ӧ��
	ttl::cache<AccountInfo> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_AccountInfo, "zxj"s, 100101);
	//3.1) �޷���ȡ��δ����Ļ������ݣ���ע�⣺�������������������ӻ���ʱ��Ӧ��
	ttl::cache<AccountInfo> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_AccountInfo, "lxy"s, 100102);
	//3.2) ����һ����������10s�Ļ�������
	ttl::cache<AccountInfo> c2(new AccountInfo(100102, "lxy", "��������", 654321), ttl::DS_SINGLE, DT_AccountInfo, 10000, "lxy"s, 100102);
	//3.3) ��ʱ���Ի�ȡ�����Ա�3.1���裩
	ttl::cache<AccountInfo> s4;
	bool ret4 = ttl::cache_mgr::Instance().GetCache(s4, DT_AccountInfo, "lxy"s, 100102);
	//4.1) �޸��˻����е��˻����
	ttl::cache<AccountInfo> c3(new AccountInfo(100101, "zxj", "�й�����", 999999), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101);
	//4.2) ���»�ȡ�������ݣ��õ���������
	ttl::cache<AccountInfo> s5;
	bool ret5 = ttl::cache_mgr::Instance().GetCache(s5, DT_AccountInfo, "zxj"s, 100101);
	//5.1) �����������Ϊ"zxj"���˺�Ϊ"100101"���˻����Ļ�������
	ttl::cache_mgr::Instance().ClrCache(DT_AccountInfo, "zxj"s, 100101);
	//5.2) ��ȡ������Ӧ�Ļ�����
	ttl::cache<AccountInfo> s6;
	bool ret6 = ttl::cache_mgr::Instance().GetCache(s6, DT_AccountInfo, "zxj"s, 100101);
	//6. ʹ�ú��ʼ���ò�ͬ��keys���������»��棬�ᵼ���ڴ���ң����±���
	ttl::cache<AccountInfo> c4(new AccountInfo(100101, "zxj", "�й�����", 888888), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101,"�й�����"s);
	ttl::cache<AccountInfo> s7;
	bool ret7 = ttl::cache_mgr::Instance().GetCache(s7, DT_AccountInfo, "zxj"s, 100101, "�й�����"s);
	//7. �ȵ������������ڽ����������ٻ�ȡ���������ݡ�ע�⣺�����ܲ��ܻ�ȡ����ttl::ttl_cache_mgr�Ļ������"ttl::ttl_cache_mgr::TtlStrategy"�йء�
	std::this_thread::sleep_for(std::chrono::seconds(10));
	ttl::cache<AccountInfo> s8;
	bool ret8 = ttl::cache_mgr::Instance().GetCache(s8, DT_AccountInfo, "lxy"s, 100102);
}

void testttlcache_2()
{
	//1. û�л���ʱ��ȡʧ��
	ttl::cache<std::list<AccountInfo>> s1;
	// keysֻ��һ����������ʾ��ȡ��������Ϊ"zxj"���˻��б���������
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_AccountList,"zxj"s);
	//2.1) ����һ����������,���������漰��ʵʱ���£����綩���������ݣ���ָ����������Ϊ���޳�(time_t(-1))
	auto l1 = new std::list<AccountInfo>;
	l1->emplace_back(100103, "zxj", "��ͨ����", 900000);
	ttl::cache<std::list<AccountInfo>> c1(l1, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//2.2) ��ȡǰ�����Ļ�������
	ttl::cache<std::list<AccountInfo>> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_AccountList, "zxj"s);
	//3.1) ������������������
	auto l2 = new std::list<AccountInfo>;
	l2->emplace_back(100104, "zxj", "ũҵ����", 980000);
	l2->emplace_back(100105, "zxj", "��������", 990000);
	ttl::cache<std::list<AccountInfo>> c2(l2, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//3.2) ��ȡǰ�����Ļ�������,Ӧ������������
	ttl::cache<std::list<AccountInfo>> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_AccountList, "zxj"s);
	//4. ���ǰ������"zxj"���µĻ���
	ttl::cache_mgr::Instance().ClrCache(DT_AccountList,"zxj"s);
	//5.1) ����һ���µĻ���������
	auto l3 = new std::list<AccountInfo>;
	l3->emplace_back(100106, "zxj", "��������", 970000);
	ttl::cache<std::list<AccountInfo>> c3(l3, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//2.2) ��ȡǰ�����Ļ�������,Ӧ��ֻ��һ������
	ttl::cache<std::list<AccountInfo>> s4;
	bool ret4 = ttl::cache_mgr::Instance().GetCache(s4, DT_AccountList, "zxj"s);
}

void testttlcache_3()
{
	//1. û�л���ʱ��ȡʧ��
	ttl::cache<TradeHistory> s1;
	// keysֻ��һ����������ʾ��ȡ���˺�Ϊ"100101"���˻��Ľ�����ϸ����������
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_TradeHistory, 100101);
	//2.1) ����һ����������,���������漰��ʵʱ���£����綩���������ݣ���ָ����������Ϊ���޳�(time_t(-1))
	auto t1 = new TradeHistory(100101);
	t1->m_history.emplace_back(6666, "��������");
	t1->m_history.emplace_back(-1000, "¥�����Ƶ�һ��ǧ��");
	ttl::cache<TradeHistory> c1(t1, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//2.2) ��ȡǰ�����Ļ�������
	ttl::cache<TradeHistory> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_TradeHistory, 100101);
	//3.1) ������������������
	auto t2 = new TradeHistory(100101);
	t2->m_history.emplace_back(100000104, "ͻȻ������ô�࣬����ϵͳbug�ˣ�");
	t2->m_history.emplace_back(-100000000, "XX���У�sorry~����һ��ϵ��๤���󡣡�");
	ttl::cache<TradeHistory> c2(t2, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//3.2) ��ȡǰ�����Ļ�������,Ӧ������������
	ttl::cache<TradeHistory> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_TradeHistory, 100101);
	//4. ���ǰ������"100101�˻�"���µĻ���
	ttl::cache_mgr::Instance().ClrCache(DT_TradeHistory, 100101);
	//5.1) ����һ���µĻ���������
	auto t3 = new TradeHistory(100101);
	t3->m_history.emplace_back(-1314520, "emmm,����ڷ����֧��");
	ttl::cache<TradeHistory> c3(t3, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//2.2) ��ȡǰ�����Ļ�������,Ӧ��ֻ��һ������
	ttl::cache<TradeHistory> s4;
	bool ret4 = ttl::cache_mgr::Instance().GetCache(s4, DT_TradeHistory, 100101);
}

void test_ttlcache()
{
	testttlcache_1();
	testttlcache_2();
	testttlcache_3();
}

int main()
{
	test_ttlcache();
	return 1;
}
