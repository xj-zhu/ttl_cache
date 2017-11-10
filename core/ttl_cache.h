#ifndef _TTL_CACHE_H_
#define _TTL_CACHE_H_

#include <memory>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <list>
#include <map>
#include <algorithm>
#include <type_traits>
#include <assert.h>

namespace ttl
{
	namespace type_traits
	{
		template<typename _Tp, typename _Up>
		class _is_appendable_helper
		{
			template<typename _Tp1, typename _Up1,
				typename = decltype(std::declval<_Tp1>() += std::declval<_Up1>())>
				static std::true_type __test(int);

			template<typename, typename>
			static std::false_type __test(...);

		public:
			typedef decltype(__test<_Tp&, _Up&>(0)) type;
		};

		template<typename _Tp, typename _Up>
		struct is_appendable : public _is_appendable_helper<_Tp, _Up>::type
		{ };
	}

	using namespace std::chrono;

	enum DataStoreType	//数据暂存方式
	{
		DS_Err,			//错误
		DS_SINGLE,		//单条
		DS_QUEUE,		//序列
	};

	typedef int DataType;

	static constexpr DataType DT_Err = -1;

	class cache_mgr;

	class cache_base
	{
	public:
		virtual ~cache_base() {}
		virtual long use_count() const noexcept
		{
			assert(false);
			return 0;
		}

	private:
		virtual void _OnCopy() {};
		virtual void _OnAppend(const cache_base& that) {};

	public:
		cache_base& operator = (const cache_base& that)
		{
			if (this != &that)
			{
				m_managed = that.m_managed;
				_OnCopy();
			}
			return *this;
		}
		cache_base& operator+=(const cache_base& that)
		{
			_OnAppend(that);
			return *this;
		}

	public:
		cache_base*	m_managed = nullptr;

		friend class cache_mgr;
	};

	class cache_mgr
	{
		enum TtlStrategy
		{
			TTL_WHEN_START,
			TTL_WHEN_ALL_RELEASE,
		};
	public:
		static cache_mgr& Instance() { static cache_mgr inst; return inst; }

	public:
		template <typename... Args>
		bool GetCache(cache_base& _cache, DataType edt, Args&&... args)
		{
			typedef std::map<std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>
				, std::weak_ptr<cache_base>> CacheMap;

			std::lock_guard<std::recursive_mutex> l(m_mutex);
			auto it1 = m_records.find(edt);
			if (it1 != m_records.end())
			{
				CacheMap* pmap = (CacheMap*)(it1->second);
				if (pmap)
				{
					auto it2 = pmap->find(std::forward_as_tuple(std::forward<Args>(args)...));
					if (it2 != pmap->end())
					{
						auto shared = it2->second.lock();
						if (shared && shared.get())
						{
							_cache = *(shared.get());
							return true;
						}
						pmap->erase(it2);
						if (pmap->empty())
						{
							delete pmap;
							m_records.erase(edt);
						}
					}
				}
			}
			return false;
		}

		template <typename... Args>
		void SetCache(cache_base* _cache, DataStoreType edst, DataType edt, time_t lifems, Args&&... args)
		{
			typedef std::map<std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>
				, std::weak_ptr<cache_base>> CacheMap;

			std::shared_ptr<cache_base> shared(_cache);
			std::lock_guard<std::recursive_mutex> l(m_mutex);
			auto it = m_records.find(edt);
			CacheMap* pmap = nullptr;
			if (it != m_records.end() && it->second)
			{
				pmap = (CacheMap*)(it->second);
			}
			else
			{
				pmap = new CacheMap;
				m_records[edt] = pmap;
			}
			if (pmap)
			{
				auto& ptr = (*pmap)[std::forward_as_tuple(std::forward<Args>(args)...)];
				switch (edst)
				{
				default:
					assert(false);
					break;
				case DS_QUEUE:
					if (ptr.expired())
						ptr = shared;
					else
					{
						*(ptr.lock().get()) += *(shared.get());
						shared.reset();
					}
					break;
				case DS_SINGLE:
					m_caches.erase(ptr.lock());
					ptr = shared;
					break;
				}
				if (shared)
					m_caches.insert(std::make_pair(std::move(shared), lifems));
				if (_CheckStrategy(TTL_WHEN_START))
					_StartTTL(_cache);
			}
		}

		template <typename... Args>
		void ClrCache(DataType edt, Args&&... args)
		{
			typedef std::map<std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>
				, std::weak_ptr<cache_base>> CacheMap;

			std::lock_guard<std::recursive_mutex> l(m_mutex);
			auto it = m_records.find(edt);
			if (it != m_records.end() && it->second)
			{
				CacheMap* pmap = (CacheMap*)(it->second);
				if (pmap)
				{
					auto tp = std::forward_as_tuple(std::forward<Args>(args)...);
					auto itf = pmap->find(tp);
					if (itf != pmap->end())
					{
						m_caches.erase(itf->second.lock());
						pmap->erase(itf);
					}
					if (pmap->empty())
					{
						delete pmap;
						m_records.erase(edt);
					}
				}
			}
		}

		void StartTTL(cache_base* _cache)
		{
			if (_CheckStrategy(TTL_WHEN_START))
				return;
			std::lock_guard<std::recursive_mutex> l(m_mutex);
			_StartTTL(_cache);
		}

		void StopTTL(cache_base* _cache) {/*donothing.*/ }

	private:
		void _ThreadLoop()
		{
			while (1)
			{
				std::unique_lock<std::recursive_mutex> l(m_mutex);
				m_condvar.wait_for(l, m_perchackduration);
				for (auto it = m_queue.begin(); it != m_queue.end();)
				{
					if ((!m_loop_running) || (it->first <= std::chrono::steady_clock::now()))
					{
						if ((_CheckStrategy(TTL_WHEN_ALL_RELEASE)) ? (it->second && it->second->use_count() == 1) : true)
							m_caches.erase(it->second);
						it = m_queue.erase(it);
					}
					else if ((m_loop_running) || false)
						break;
				}
				if (!m_loop_running)
					break;
			}
		}

		void _StartTTL(cache_base* _cache)
		{
			std::shared_ptr<cache_base> shared;
			auto it = std::find_if(m_caches.begin()
				, m_caches.end()
				, [&shared, _cache](std::pair<const std::shared_ptr<cache_base>, time_t>& pr)
			{
				if (pr.first.get() == _cache)
				{
					shared = pr.first;
					return true;
				}
				return false;
			});
			if (it != m_caches.end())
			{
				if (time_t(-1) != it->second)
					m_queue.insert(std::make_pair(std::chrono::steady_clock::now()
						+ std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(it->second))
						, std::move(shared)));
				m_condvar.notify_one();
			}
		}

		bool _CheckStrategy(TtlStrategy strategy)
		{
			return (m_strategy == TTL_WHEN_START)
				? (strategy == TTL_WHEN_START)
				: (strategy == TTL_WHEN_ALL_RELEASE);
		}

	private:
		TtlStrategy m_strategy = TTL_WHEN_START;
		std::chrono::milliseconds m_perchackduration = 5000ms;
		bool m_loop_running = true;
		std::thread* m_thread = nullptr;

		std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<cache_base>> m_queue;
		std::map<std::shared_ptr<cache_base>, time_t> m_caches;
		std::map<DataType, void*> m_records;
		std::recursive_mutex m_mutex;
		std::condition_variable_any m_condvar;

	private:
		cache_mgr()
		{
			m_thread = new std::thread(std::bind(&cache_mgr::_ThreadLoop, this));
		}
		~cache_mgr()
		{
			m_loop_running = false;
			m_condvar.notify_one();
			if (m_thread)
				m_thread->join();
			delete m_thread;
			m_thread = nullptr;
			std::for_each(m_records.begin()
				, m_records.end()
				, [](std::pair<const DataType, void*>& pr) {delete pr.second; }
			);
		}
		cache_mgr(const cache_mgr& that) = delete;
		cache_mgr(cache_mgr&& that) = delete;
		cache_mgr& operator=(const cache_mgr& that) = delete;
		cache_mgr& operator=(cache_mgr&& that) = delete;
	};

	template <typename _Ty>
	class cache : public cache_base
	{
		typedef cache<_Ty> _Myt;

	private:
		template <typename... Args>
		void _CheckInConstructor(Args&&... _args)
		{
			if (use_count() == 1)
			{
				assert(nullptr == m_managed);
				ManageTTL(std::forward<Args>(_args)...);
			}
			else/* if (use_count() == 2)*/
			{
				assert(nullptr != m_managed);
				StopTTL();
			}
		}
		void _CheckInDestructor()
		{
			if (use_count() == 2)
			{
				StartTTL();
			}
		}
		void _OnCopy()
		{
			if (_CopyCache(this, dynamic_cast<_Myt*>(m_managed)))
			{
				_CheckInConstructor();
			}
		}
		void _OnAppend(const cache_base& that)
		{
			__OnAppend(dynamic_cast<_Myt*>(that.m_managed), type_traits::is_appendable<_Ty, _Ty>());
		}
		void __OnAppend(const _Myt* that, std::true_type&&)
		{
			if (that)
			{
				if (m_shared && that->m_shared)
				{
					*m_shared += *(that->m_shared);
				}
			}
		}
		void __OnAppend(...)
		{
			assert(false);
		}
		bool _CopyCache(cache<_Ty>* const dst, const cache<_Ty>* const src)
		{
			if (!dst || !src)
				return false;
			dst->m_shared = src->m_shared;
			dst->m_Edst = src->m_Edst;
			dst->m_Edt = src->m_Edt;
			dst->m_lifeMs = src->m_lifeMs;
			return true;
		}
		template <typename... Args>
		void _ManageTTL(Args&&... _args)
		{
			cache_mgr::Instance().SetCache(m_managed, m_Edst, m_Edt, m_lifeMs, std::forward<Args>(_args)...);
		}

	private:
		template <typename... Args>
		void ManageTTL(Args&&... _args)
		{
			m_managed = new cache<_Ty>;
			m_managed->m_managed = m_managed;
			_CopyCache(dynamic_cast<_Myt*>(m_managed), this);
			_ManageTTL(std::forward<Args>(_args)...);
		}

		void StartTTL()
		{
			cache_mgr::Instance().StartTTL(m_managed);
		}

		void StopTTL()
		{
			cache_mgr::Instance().StopTTL(m_managed);
		}

	public:
		cache() noexcept
		{	// construct empty cache
		}

		template<class _Ux,
			typename... Args>
			explicit cache(_Ux *_Px, DataStoreType _Edst, DataType _Edt, time_t _lifeMs, Args&&... _args)
			: m_shared(_Px)
			, m_Edst(_Edst)
			, m_Edt(_Edt)
			, m_lifeMs(_lifeMs)
		{	// construct cache object that owns _Px
			_CheckInConstructor(std::forward<Args>(_args)...);
		}

		template<class _Ux,
			class _Dx,
			typename... Args>
			cache(_Ux *_Px, _Dx _Dt, DataStoreType _Edst, DataType _Edt, time_t _lifeMs, Args&&... _args)
			: m_shared(_Px, _Dt)
			, m_Edst(_Edst)
			, m_Edt(_Edt)
			, m_lifeMs(_lifeMs)
		{	// construct with _Px, deleter
			_CheckInConstructor(std::forward<Args>(_args)...);
		}

		cache(nullptr_t) noexcept
		{	// construct empty cache
		}

		template<class _Dx,
			typename... Args>
			cache(nullptr_t _N, _Dx _Dt, DataStoreType _Edst, DataType _Edt, time_t _lifeMs, Args&&... _args)
			: m_shared(_N, _Dt)
			, m_Edst(_Edst)
			, m_Edt(_Edt)
			, m_lifeMs(_lifeMs)
		{	// construct with nullptr, deleter
			_CheckInConstructor(std::forward<Args>(_args)...);
		}

		template<class _Dx,
			class _Alloc,
			typename... Args>
			cache(nullptr_t _N, _Dx _Dt, _Alloc _Ax, DataStoreType _Edst, DataType _Edt, time_t _lifeMs, Args&&... _args)
			: m_shared(_N, _Dt, _Ax)
			, m_Edst(_Edst)
			, m_Edt(_Edt)
			, m_lifeMs(_lifeMs)
		{	// construct with nullptr, deleter, allocator
			_CheckInConstructor(std::forward<Args>(_args)...);
		}

		template<class _Ux,
			class _Dx,
			class _Alloc,
			typename... Args>
			cache(_Ux *_Px, _Dx _Dt, _Alloc _Ax, DataStoreType _Edst, DataType _Edt, time_t _lifeMs, Args&&... _args)
			: m_shared(_Px, _Dt, _Ax)
			, m_Edst(_Edst)
			, m_Edt(_Edt)
			, m_lifeMs(_lifeMs)
		{	// construct with _Px, deleter, allocator
			_CheckInConstructor(std::forward<Args>(_args)...);
		}

		template<class _Ty2>
		cache(const cache<_Ty2>& _Right, _Ty *_Px) noexcept : m_shared(_Right.m_shared, _Px)
		{	// construct cache object that aliases _Right
			m_managed = _Right.m_managed;
			m_Edst = _Right.m_Edst;
			m_Edt = _Right.m_Edt;
			m_lifeMs = _Right.m_lifeMs;
			_CheckInConstructor();
		}

		cache(const _Myt& _Other) noexcept : m_shared(_Other.m_shared)
		{	// construct cache object that owns same resource as _Other
			m_managed = _Other.m_managed;
			m_Edst = _Other.m_Edst;
			m_Edt = _Other.m_Edt;
			m_lifeMs = _Other.m_lifeMs;
			_CheckInConstructor();
		}

		template<class _Ty2,
			class = typename std::enable_if<std::is_convertible<_Ty2 *, _Ty *>::value,
			void>::type>
			cache(const cache<_Ty2>& _Other) noexcept : m_shared(_Other.m_shared)
		{	// construct cache object that owns same resource as _Other
			m_managed = _Other.m_managed;
			m_Edst = _Other.m_Edst;
			m_Edt = _Other.m_Edt;
			m_lifeMs = _Other.m_lifeMs;
			_CheckInConstructor();
		}
		_Myt& operator=(_Myt&& _Right) noexcept
		{	// take resource from _Right
			cache(std::move(_Right)).swap(*this);
			return (*this);
		}

		template<class _Ty2>
		_Myt& operator=(cache<_Ty2>&& _Right) noexcept
		{	// take resource from _Right
			cache(std::move(_Right)).swap(*this);
			return (*this);
		}

		_Myt& operator=(const _Myt& _Right) noexcept
		{	// assign shared ownership of resource owned by _Right
			cache(_Right).swap(*this);
			return (*this);
		}

		template<class _Ty2>
		_Myt& operator=(const cache<_Ty2>& _Right) noexcept
		{	// assign shared ownership of resource owned by _Right
			cache(_Right).swap(*this);
			return (*this);
		}

		cache(_Myt&& _Right) noexcept
			: cache_base(std::move(_Right))
			, m_shared(std::move(_Right.m_shared))
			, m_Edst(std::move(_Right.m_Edst))
			, m_Edt(std::move(_Right.m_Edt))
			, m_lifeMs(std::move(_Right.m_lifeMs))
		{	// construct cache object that takes resource from _Right
		}

		void swap(_Myt& _Other) noexcept
		{	// swap pointers
			m_shared.swap(_Other.m_shared);
			std::swap(m_managed, _Other.m_managed);
			std::swap(m_Edst, _Other.m_Edst);
			std::swap(m_Edt, _Other.m_Edt);
			std::swap(m_lifeMs, _Other.m_lifeMs);
		}

		_Ty *get() const noexcept
		{	// return pointer to resource
			return (m_shared.get());
		}

		long use_count() const noexcept
		{	// return use count
			return m_shared.use_count();
		}

		typename std::add_lvalue_reference<_Ty>::type operator*() const noexcept
		{	// return reference to resource
			return (*(m_shared.get()));
		}

		_Ty *operator->() const noexcept
		{	// return pointer to resource
			return (m_shared.get());
		}

		~cache() noexcept
		{	// release resource
			_CheckInDestructor();
		}

	private:
		std::shared_ptr<_Ty> m_shared;
		DataStoreType m_Edst = DS_Err;
		DataType m_Edt = DT_Err;
		time_t m_lifeMs = 60000;
	};
}

template<class _Ty>
void swap(ttl::cache<_Ty>& _Left,
	ttl::cache<_Ty>& _Right) noexcept
{	// swap _Left and _Right shared_ptrs
	_Left.swap(_Right);
}

#endif // _TTL_CACHE_H_
