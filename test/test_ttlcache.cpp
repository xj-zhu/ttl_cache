#include "ttl_cache.h"

#include <iostream>
#include <string>

using namespace std::chrono;
using namespace std::string_literals;

enum EmDataType : ttl::DataType	//数据类型
{
	DT_Err = ttl::DT_Err,	//错误
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

//使用场景下，不需要保持right不变（const），so，使用此种写法性能更高
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
	std::list<std::pair<long, std::string>> m_history;	//list<收支金额(+收入/-支出)，金额变动说明>
};

void testttlcache_1()//单条数据
{
	//1. 没有缓存时获取失败
	ttl::cache<AccountInfo> s1;
	//约定后两个参数表示取【持有人为"zxj"的账号为"100101"的账户】的缓存数据
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_AccountInfo, "zxj"s, 100101);
	//2.1) 增加一个生命周期10s的缓存数据
	{
		ttl::cache<AccountInfo> c1(new AccountInfo(100101, "zxj", "中国银行", 666666), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101);
	}
	//2.2) 即时离开了c1的作用域，一样能够获取到前面加入的缓存数据，【注意：后两个参数类型与增加缓存时对应】
	ttl::cache<AccountInfo> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_AccountInfo, "zxj"s, 100101);
	//3.1) 无法获取到未加入的缓存数据，【注意：后两个参数类型与增加缓存时对应】
	ttl::cache<AccountInfo> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_AccountInfo, "lxy"s, 100102);
	//3.2) 增加一个生命周期10s的缓存数据
	ttl::cache<AccountInfo> c2(new AccountInfo(100102, "lxy", "建设银行", 654321), ttl::DS_SINGLE, DT_AccountInfo, 10000, "lxy"s, 100102);
	//3.3) 这时可以获取到（对比3.1步骤）
	ttl::cache<AccountInfo> s4;
	bool ret4 = ttl::cache_mgr::Instance().GetCache(s4, DT_AccountInfo, "lxy"s, 100102);
	//4.1) 修改了缓存中的账户余额
	ttl::cache<AccountInfo> c3(new AccountInfo(100101, "zxj", "中国银行", 999999), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101);
	//4.2) 重新获取缓存数据，得到最新数据
	ttl::cache<AccountInfo> s5;
	bool ret5 = ttl::cache_mgr::Instance().GetCache(s5, DT_AccountInfo, "zxj"s, 100101);
	//5.1) 清除【持有人为"zxj"的账号为"100101"的账户】的缓存数据
	ttl::cache_mgr::Instance().ClrCache(DT_AccountInfo, "zxj"s, 100101);
	//5.2) 获取不到对应的缓存了
	ttl::cache<AccountInfo> s6;
	bool ret6 = ttl::cache_mgr::Instance().GetCache(s6, DT_AccountInfo, "zxj"s, 100101);
	//6. 使用和最开始调用不同的keys参数创建新缓存，会导致内存混乱，或致崩溃
	ttl::cache<AccountInfo> c4(new AccountInfo(100101, "zxj", "中国银行", 888888), ttl::DS_SINGLE, DT_AccountInfo, 10000, "zxj"s, 100101,"中国银行"s);
	ttl::cache<AccountInfo> s7;
	bool ret7 = ttl::cache_mgr::Instance().GetCache(s7, DT_AccountInfo, "zxj"s, 100101, "中国银行"s);
	//7. 等到缓存生命周期结束，则不能再获取到缓存数据【注意：具体能不能获取到和ttl::ttl_cache_mgr的缓存策略"ttl::ttl_cache_mgr::TtlStrategy"有关】
	std::this_thread::sleep_for(std::chrono::seconds(10));
	ttl::cache<AccountInfo> s8;
	bool ret8 = ttl::cache_mgr::Instance().GetCache(s8, DT_AccountInfo, "lxy"s, 100102);
}

void testttlcache_2()
{
	//1. 没有缓存时获取失败
	ttl::cache<std::list<AccountInfo>> s1;
	// keys只有一个参数，表示获取【持有人为"zxj"的账户列表】缓存数据
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_AccountList,"zxj"s);
	//2.1) 增加一个缓存数据,序列数据涉及到实时更新，比如订阅推送数据，可指定生命周期为无限长(time_t(-1))
	auto l1 = new std::list<AccountInfo>;
	l1->emplace_back(100103, "zxj", "交通银行", 900000);
	ttl::cache<std::list<AccountInfo>> c1(l1, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//2.2) 获取前面加入的缓存数据
	ttl::cache<std::list<AccountInfo>> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_AccountList, "zxj"s);
	//3.1) 增加两个缓存数据项
	auto l2 = new std::list<AccountInfo>;
	l2->emplace_back(100104, "zxj", "农业银行", 980000);
	l2->emplace_back(100105, "zxj", "工商银行", 990000);
	ttl::cache<std::list<AccountInfo>> c2(l2, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//3.2) 获取前面加入的缓存数据,应该有三条数据
	ttl::cache<std::list<AccountInfo>> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_AccountList, "zxj"s);
	//4. 清除前面加入的"zxj"名下的缓存
	ttl::cache_mgr::Instance().ClrCache(DT_AccountList,"zxj"s);
	//5.1) 增加一个新的缓存数据项
	auto l3 = new std::list<AccountInfo>;
	l3->emplace_back(100106, "zxj", "招商银行", 970000);
	ttl::cache<std::list<AccountInfo>> c3(l3, ttl::DS_QUEUE, DT_AccountList, -1, "zxj"s);
	//2.2) 获取前面加入的缓存数据,应该只有一条数据
	ttl::cache<std::list<AccountInfo>> s4;
	bool ret4 = ttl::cache_mgr::Instance().GetCache(s4, DT_AccountList, "zxj"s);
}

void testttlcache_3()
{
	//1. 没有缓存时获取失败
	ttl::cache<TradeHistory> s1;
	// keys只有一个参数，表示获取【账号为"100101"的账户的交易明细】缓存数据
	bool ret1 = ttl::cache_mgr::Instance().GetCache(s1, DT_TradeHistory, 100101);
	//2.1) 增加一个缓存数据,序列数据涉及到实时更新，比如订阅推送数据，可指定生命周期为无限长(time_t(-1))
	auto t1 = new TradeHistory(100101);
	t1->m_history.emplace_back(6666, "发工资啦");
	t1->m_history.emplace_back(-1000, "楼下足疗店一掷千金");
	ttl::cache<TradeHistory> c1(t1, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//2.2) 获取前面加入的缓存数据
	ttl::cache<TradeHistory> s2;
	bool ret2 = ttl::cache_mgr::Instance().GetCache(s2, DT_TradeHistory, 100101);
	//3.1) 增加两个缓存数据项
	auto t2 = new TradeHistory(100101);
	t2->m_history.emplace_back(100000104, "突然多了这么多，银行系统bug了？");
	t2->m_history.emplace_back(-100000000, "XX银行：sorry~，上一笔系清洁工手误。。");
	ttl::cache<TradeHistory> c2(t2, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//3.2) 获取前面加入的缓存数据,应该有四条数据
	ttl::cache<TradeHistory> s3;
	bool ret3 = ttl::cache_mgr::Instance().GetCache(s3, DT_TradeHistory, 100101);
	//4. 清除前面加入的"100101账户"名下的缓存
	ttl::cache_mgr::Instance().ClrCache(DT_TradeHistory, 100101);
	//5.1) 增加一个新的缓存数据项
	auto t3 = new TradeHistory(100101);
	t3->m_history.emplace_back(-1314520, "emmm,光棍节发红包支出");
	ttl::cache<TradeHistory> c3(t3, ttl::DS_QUEUE, DT_TradeHistory, -1, 100101);
	//2.2) 获取前面加入的缓存数据,应该只有一条数据
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
