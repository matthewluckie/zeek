// See the file "COPYING" in the main distribution directory for copyright.

#pragma once

#include <ares.h>
#include <list>
#include <map>
#include <queue>
#include <utility>

#include "zeek/EventHandler.h"
#include "zeek/IPAddr.h"
#include "zeek/List.h"
#include "zeek/iosource/IOSource.h"
#include "zeek/util.h"

namespace zeek
	{
class Val;
class ListVal;
class TableVal;
class StringVal;

template <class T> class IntrusivePtr;
using ValPtr = IntrusivePtr<Val>;
using ListValPtr = IntrusivePtr<ListVal>;
using TableValPtr = IntrusivePtr<TableVal>;
using StringValPtr = IntrusivePtr<StringVal>;

	} // namespace zeek

namespace zeek::detail
	{
class DNS_Mapping;
class DNS_Request;

enum DNS_MgrMode
	{
	DNS_PRIME, // used to prime the cache
	DNS_FORCE, // internal error if cache miss
	DNS_DEFAULT, // lookup names as they're requested
	DNS_FAKE, // don't look up names, just return dummy results
	};

class DNS_Mgr final : public iosource::IOSource
	{
public:
	/**
	 * Base class for callback handling for asynchronous lookups.
	 */
	class LookupCallback
		{
	public:
		virtual ~LookupCallback() = default;

		/**
		 * Called when an address lookup finishes.
		 *
		 * @param name The resulting name from the lookup.
		 */
		virtual void Resolved(const char* name){};

		/**
		 * Called when a name lookup finishes.
		 *
		 * @param addrs A table of the resulting addresses from the lookup.
		 */
		virtual void Resolved(TableValPtr addrs){};

		/**
		 * Generic callback method for all request types.
		 *
		 * @param val A Val containing the data from the query.
		 */
		virtual void Resolved(ValPtr data, int request_type) { }

		/**
		 * Called when a timeout request occurs.
		 */
		virtual void Timeout() = 0;
		};

	explicit DNS_Mgr(DNS_MgrMode mode);
	~DNS_Mgr() override;

	/**
	 * Finalizes the manager initialization. This should be called only after all
	 * of the scripts have been parsed at startup.
	 */
	void InitPostScript();

	/**
	 * Attempts to process one more round of requests and then flushes the
	 * mapping caches.
	 */
	void Flush();

	/**
	 * Looks up the address(es) of a given host and returns a set of addresses.
	 * This is a shorthand method for doing A/AAAA requests. This is a
	 * synchronous request and will block until the request completes or times
	 * out.
	 *
	 * @param host The hostname to lookup an address for.
	 * @return A set of addresses for the host.
	 */
	TableValPtr LookupHost(const char* host);

	/**
	 * Looks up the hostname of a given address. This is a shorthand method for
	 * doing PTR requests. This is a synchronous request and will block until
	 * the request completes or times out.
	 *
	 * @param host The addr to lookup a hostname for.
	 * @return The hostname for the address.
	 */
	StringValPtr LookupAddr(const IPAddr& addr);

	/**
	 * Performs a generic request to the DNS server. This is a synchronous
	 * request and will block until the request completes or times out.
	 *
	 * @param name The name or address to make a request for. If this is an
	 * address it should be in arpa format (x.x.x.x.in-addr.arpa or x-*.ip6.arpa).
	 * Note that calling LookupAddr for PTR requests does this conversion
	 * automatically.
	 * @param request_type The type of request to make. This should be one of
	 * the type values defined in arpa/nameser.h or ares_nameser.h.
	 * @return The requested data.
	 */
	ValPtr Lookup(const char* name, int request_type);

	/**
	 * Looks up the address(es) of a given host. This is a shorthand method
	 * for doing A/AAAA requests. This is an asynchronous request. The
	 * response will be handled via the provided callback object.
	 *
	 * @param host The hostname to lookup an address for.
	 * @param callback A callback object for handling the response.
	 */
	void LookupHost(const char* host, LookupCallback* callback);

	/**
	 * Looks up the hostname of a given address. This is a shorthand method for
	 * doing PTR requests. This is an asynchronous request. The response will
	 * be handled via the provided callback object.
	 *
	 * @param host The addr to lookup a hostname for.
	 * @param callback A callback object for handling the response.
	 */
	void LookupAddr(const IPAddr& addr, LookupCallback* callback);

	/**
	 * Performs a generic request to the DNS server. This is an asynchronous
	 * request. The response will be handled via the provided callback
	 * object.
	 *
	 * @param name The name or address to make a request for. If this is an
	 * address it should be in arpa format (x.x.x.x.in-addr.arpa or x-*.ip6.arpa).
	 * Note that calling LookupAddr for PTR requests does this conversion
	 * automatically.
	 * @param request_type The type of request to make. This should be one of
	 * the type values defined in arpa/nameser.h or ares_nameser.h.
	 * @param callback A callback object for handling the response.
	 */
	void Lookup(const char* name, int request_type, LookupCallback* callback);

	/**
	 * Sets the directory where to store DNS data when Save() is called.
	 */
	void SetDir(const char* arg_dir) { dir = arg_dir; }

	/**
	 * Waits for responses to become available or a timeout to occur,
	 * and handles any responses.
	 */
	void Resolve();

	/**
	 * Saves the current name and address caches to disk.
	 */
	bool Save();

	struct Stats
		{
		unsigned long requests; // These count only async requests.
		unsigned long successful;
		unsigned long failed;
		unsigned long pending;
		unsigned long cached_hosts;
		unsigned long cached_addresses;
		unsigned long cached_texts;
		};

	/**
	 * Returns the current statistics for the DNS_Manager.
	 *
	 * @param stats A pointer to a stats object to return the data in.
	 */
	void GetStats(Stats* stats);

	/**
	 * Adds a result from a request to the caches. This is public so that the
	 * callback methods can call it from outside of the DNS_Mgr class.
	 *
	 * @param dr The request associated with the result.
	 * @param h A hostent structure containing the actual result data.
	 * @param ttl A ttl value contained in the response from the server.
	 * @param merge A flag for whether these results should be merged into
	 * an existing mapping. If false, AddResult will attempt to replace the
	 * existing mapping with the new data and delete the old mapping.
	 */
	void AddResult(DNS_Request* dr, struct hostent* h, uint32_t ttl, bool merge = false);

	/**
	 * Returns an empty set of addresses, used in various error cases and during
	 * cache priming.
	 */
	static TableValPtr empty_addr_set();

	/**
	 * This method is used to call the private Process() method during unit testing
	 * and shouldn't be used otherwise.
	 */
	void TestProcess();

	/**
	 * Returns the full path to the file used to store the DNS cache.
	 */
	std::string CacheFile() const { return cache_name; }

	/**
	 * Used by the c-ares socket call back to register/unregister a socket file descriptor.
	 */
	void RegisterSocket(int fd, bool active);

private:
	friend class LookupCallback;
	friend class DNS_Request;

	StringValPtr LookupAddrInCache(const IPAddr& addr, bool cleanup_expired = false,
	                               bool check_failed = false);
	TableValPtr LookupNameInCache(const std::string& name, bool cleanup_expired = false,
	                              bool check_failed = false);
	StringValPtr LookupTextInCache(const std::string& name, bool cleanup_expired = false);

	// Finish the request if we have a result.  If not, time it out if
	// requested.
	void CheckAsyncAddrRequest(const IPAddr& addr, bool timeout);
	void CheckAsyncHostRequest(const char* host, bool timeout);
	void CheckAsyncTextRequest(const char* host, bool timeout);

	void Event(EventHandlerPtr e, DNS_Mapping* dm);
	void Event(EventHandlerPtr e, DNS_Mapping* dm, ListValPtr l1, ListValPtr l2);
	void Event(EventHandlerPtr e, DNS_Mapping* old_dm, DNS_Mapping* new_dm);

	ValPtr BuildMappingVal(DNS_Mapping* dm);

	void CompareMappings(DNS_Mapping* prev_dm, DNS_Mapping* new_dm);
	ListValPtr AddrListDelta(ListVal* al1, ListVal* al2);

	using HostMap = std::map<std::string, std::pair<DNS_Mapping*, DNS_Mapping*>>;
	using AddrMap = std::map<IPAddr, DNS_Mapping*>;
	using TextMap = std::map<std::string, DNS_Mapping*>;
	void LoadCache(const std::string& path);
	void Save(FILE* f, const AddrMap& m);
	void Save(FILE* f, const HostMap& m);

	// Issue as many queued async requests as slots are available.
	void IssueAsyncRequests();

	// IOSource interface.
	void Process() override;
	void InitSource() override;
	const char* Tag() override { return "DNS_Mgr"; }
	double GetNextTimeout() override;

	DNS_MgrMode mode;

	HostMap host_mappings;
	AddrMap addr_mappings;
	TextMap text_mappings;

	std::string cache_name;
	std::string dir; // directory in which cache_name resides

	bool did_init = false;
	int asyncs_pending = 0;

	RecordTypePtr dm_rec;

	ares_channel channel;

	using CallbackList = std::list<LookupCallback*>;

	struct AsyncRequest
		{
		double time = 0.0;
		IPAddr addr;
		std::string host;
		CallbackList callbacks;
		bool is_txt = false;
		bool processed = false;

		bool IsAddrReq() const { return host.empty(); }

		void Resolved(const char* name);
		void Resolved(TableValPtr addrs);
		void Timeout();
		};

	struct AsyncRequestCompare
		{
		bool operator()(const AsyncRequest* a, const AsyncRequest* b) { return a->time > b->time; }
		};

	using AsyncRequestAddrMap = std::map<IPAddr, AsyncRequest*>;
	AsyncRequestAddrMap asyncs_addrs;

	using AsyncRequestNameMap = std::map<std::string, AsyncRequest*>;
	AsyncRequestNameMap asyncs_names;

	using AsyncRequestTextMap = std::map<std::string, AsyncRequest*>;
	AsyncRequestTextMap asyncs_texts;

	using QueuedList = std::list<AsyncRequest*>;
	QueuedList asyncs_queued;

	using TimeoutQueue =
		std::priority_queue<AsyncRequest*, std::vector<AsyncRequest*>, AsyncRequestCompare>;
	TimeoutQueue asyncs_timeouts;

	unsigned long num_requests = 0;
	unsigned long successful = 0;
	unsigned long failed = 0;

	std::set<int> socket_fds;
	};

extern DNS_Mgr* dns_mgr;

	} // namespace zeek::detail
