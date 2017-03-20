//
// C++ Interface: lru_stl
//
// Description:
//
//
// Author: ARH,,, <arh@aspire-one>, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef LRU_BLOOMF_H
#define LRU_BLOOMF_H

#include <map>
#include <list>
#include "cassert"
#include "iostream"
#include "iomanip"
#include "bitset"
#include "global.h"
#include "baseCache.h"



using namespace std;

extern int totalEvictedCleanPages;
extern int totalNonSeqEvictedDirtyPages;



template <typename K, typename V>
class BFLRUCache : public TestCache<K, V>
{
public:
// Key access history, most recent at back
	typedef list<K> key_tracker_type;
// Key to value and key history iterator
	typedef map
	< K, pair<V, typename key_tracker_type::iterator> > 	key_to_value_type;
    
// Constuctor specifies the cached function and
// the maximum number of records to be stored.
	BFLRUCache(
		V(*f)(const K & , V),
		size_t c,
		unsigned levelMinus
	) : _fn(f) , _capacity(c), levelMinusMinus(levelMinus) {
		///ARH: Commented for single level cache implementation
//         assert ( _capacity!=0 );
	}
	// Obtain value of the cached function for k

	uint32_t access(const K &k  , V &value, uint32_t status) {
		assert(_key_to_value.size() <= _capacity);
		assert(_capacity != 0);
		PRINTV(logfile << "Access key: " << k << endl;);

///ziqi: if request is write, mark the page status as DIRTY
		if(status & WRITE) {
			status |= DIRTY;
			value.updateFlags(status);
			//cout<<"flags are "<<value.getFlags()<<endl;
			//const V v1 = _fn(k, value);
			//insertDirtyPage(k, v1);
		}

// Attempt to find existing record
		const typename key_to_value_type::iterator it	= _key_to_value.find(k);
		//const typename key_to_value_type::iterator itNew	= _key_to_value.find(k);

		if(it == _key_to_value.end()) {
// We don’t have it:
			PRINTV(logfile << "Miss on key: " << k << endl;);
// Evaluate function and create new record
			const V v = _fn(k, value);
///ziqi: inserts new elements on read and write miss
//			status |=  insert(k, v);
//			PRINTV(logfile << "Insert done on key: " << k << endl;);
//			PRINTV(logfile << "Cache utilization: " << _key_to_value.size() << "/" << _capacity << endl;);
            
//wei xie: go to the next level to see if Hit, do not load it directly
//mark this level cache as miss
//only when the lowest level misses, load it to cache (level determined by hotness)
			return (status | PAGEMISS);
		}
		else {
			PRINTV(logfile << "Hit on key: " << k << endl;);
// We do have it. Before returning value,
// update access record by moving accessed
// key to back of list.
			value.updateFlags(status | (it->second.first.getReq().flags & DIRTY));
			_key_to_value.erase(it);
			_key_tracker.remove(k);
			assert(_key_to_value.size() < _capacity);
			const V v = _fn(k, value);
			// Record k as most-recently-used key
			typename key_tracker_type::iterator itNew
			= _key_tracker.insert(_key_tracker.end(), k);
			// Create the key-value entry,
			// linked to the usage record.
			_key_to_value.insert(make_pair(k, make_pair(v, itNew)));
			PRINTV(logfile << "Hitted key status: " << bitset<10>(v.getReq().flags) << endl;);
			return (status | PAGEHIT | BLKHIT);
		}
	} //end operator access

	/*
	 unsigned long long int get_min_key() {
	     return (_key_to_value.begin())->first;
	 }

	 unsigned long long int get_max_key() {
	// 			std::map< K, std::pair<V,typename key_tracker_type::iterator> >::iterator it;
	     return (_key_to_value.rbegin())->first;
	 }
	 */


	///ziqi: alireza version
	/*
	void remove(const K &k) {
	    PRINTV(logfile << "Removing key " << k << endl;);
	// Assert method is never called when cache is empty
	    assert(!_key_tracker.empty());
	// Identify  key
	    const typename key_to_value_type::iterator it
	    = _key_to_value.find(k);
	    assert(it != _key_to_value.end());
	    PRINTV(logfile << "Remove value " << endl;);
	// Erase both elements to completely purge record
	    _key_to_value.erase(it);
	    _key_tracker.remove(k);
	}
	*/

///ziqi: k is used to denote the actual entry with key value of "k" to be evicted
///ziqi: v is used to denote the original entry that passed to access() method. We only replace the time stamp of k by the time stamp of v
	void remove(const K &k, const V &v) {
		PRINTV(logfile << "Removing key " << k << endl;);
// Assert method is never called when cache is empty
		assert(!_key_tracker.empty());
// Identify  key
		typename key_to_value_type::iterator it = _key_to_value.find(k);
		assert(it != _key_to_value.end());
		PRINTV(logfile << "Before eviting, key bit status: " << bitset<10>(it->second.first.getReq().flags) << endl;);

		if(it->second.first.getReq().flags & DIRTY) {
///ARH: record flashSim output log
//wei xie: need to record trace for each level of device
			recordOutTrace(k,v);
///ziqi: DiskSim format Request_arrival_time Device_number Block_number Request_size Request_flags
///ziqi: Device_number is set to 1. About Request_flags, 0 is for write and 1 is for read
			PRINTV(DISKSIMINPUTSTREAM << setfill(' ') << left << fixed << setw(25) << v.getReq().issueTime << left << setw(8) << "0" << left << fixed << setw(12) << it->second.first.getReq().fsblkno << left << fixed << setw(8) << it->second.first.getReq().reqSize << "0" << endl;);
			PRINTV(logfile << "Remove value " << endl;);
			// Erase both elements to completely purge record
			PRINTV(logfile << "evicting dirty key " << k <<  endl;);
			totalNonSeqEvictedDirtyPages++;
			PRINTV(logfile << "Key dirty bit status: " << bitset<10>(it->second.first.getReq().flags) << endl;);
			it = _key_to_value.find(k);
			assert(it != _key_to_value.end());
			_key_to_value.erase(it);
			_key_tracker.remove(k);
            //record evicted pages
            typename key_tracker_type::iterator itNew =_evicted_key_tracker.insert(_evicted_key_tracker.end(), k);
            //record key to value 
            _evicted_key_to_value.insert(make_pair(k, make_pair(v, itNew)));
            
			PRINTV(logfile << "Cache utilization: " << _key_to_value.size() << "/" << _capacity << endl << endl;);
		}
		else {
			PRINTV(logfile << "evicting clean key without flushing back to DiskSim input trace " << k <<  endl;);
			totalEvictedCleanPages++;
			PRINTV(logfile << "Key clean bit status: " << bitset<10>(it->second.first.getReq().flags) << endl;);
			it = _key_to_value.find(k);
			assert(it != _key_to_value.end());
			_key_to_value.erase(it);
			_key_tracker.remove(k);
            //record evicted pages
            typename key_tracker_type::iterator itNew=_evicted_key_tracker.insert(_evicted_key_tracker.end(),k);
            _evicted_key_to_value.insert(make_pair(k, make_pair(v, itNew)));
            
			///PRINTV(logfile << "Cache utilization: " << _key_to_value.size() <<"/"<<_capacity <<endl<<endl;);
		}
	}


	void recordOutTrace(const K& k, const V& v){
		
		unsigned level =0; //temporary for the highet level only
		reqAtom newReq;
		
		if(_gConfiguration.outTraceStream[level].is_open() ==  false ){
			assert(0);
		}
		
		
		if(_gConfiguration.outTraceFormat[level].compare("uflip") == 0 )
		{
			if(newReq.flags & READ)
				_gConfiguration.outTraceStream[level] << "s; "<<"R; ";	
			else
				_gConfiguration.outTraceStream[level] << "s; "<<"W; ";	
			
			_gConfiguration.outTraceStream[level] << k   << "; 0; ";
			_gConfiguration.outTraceStream[level] << "1; "<<  v.getReq().issueTime ;
			_gConfiguration.outTraceStream[level] << endl; 
		}
	}

// Record a fresh key-value pair in the cache
	int insert(const K &k, const V &v) {
		PRINTV(logfile << "insert key " << k  << endl;);
		PRINTV(logfile << "Key bit status: " << bitset<10>(v.getReq().flags) << endl;);
		int status = 0;
// Method is only called on cache misses
		assert(_key_to_value.find(k) == _key_to_value.end());

// Make space if necessary
		if(_key_to_value.size() == _capacity) {
			PRINTV(logfile << "Cache is Full " << _key_to_value.size() << " sectors" << endl;);
			evict(v);
			status = EVICT;
		}

// Record k as most-recently-used key
		typename key_tracker_type::iterator it
		= _key_tracker.insert(_key_tracker.end(), k);
// Create the key-value entry,
// linked to the usage record.
		_key_to_value.insert(make_pair(k, make_pair(v, it)));
// No need to check return,
// given previous assert.
// 			add_sram_entry(k,false);
		return status;
	}
	///ziqi: alireza version
	/*
	// Purge the least-recently-used element in the cache
	void evict() {
	// Assert method is never called when cache is empty
	    assert(!_key_tracker.empty());
	// Identify least recently used key
	    const typename key_to_value_type::iterator it
	    = _key_to_value.find(_key_tracker.front());
	    assert(it != _key_to_value.end());
	    PRINTV(logfile << "evicting victim key " << (*it).first <<  endl;);
	// Erase both elements to completely purge record
	    _key_to_value.erase(it);
	    _key_tracker.pop_front();
	}
	*/

	// Purge the least-recently-used element in the cache
	void evict(const V &v) {
// Assert method is never called when cache is empty
		assert(!_key_tracker.empty());
// Identify least recently used key
		typename key_tracker_type::iterator itTracker = _key_tracker.begin();
		assert(itTracker != _key_tracker.end());
        //determine if need to demote
		remove(*(itTracker), v);
	}
	
	int evict_empty(){
        if(_evicted_key_tracker.empty())
            return 1;
        else 
            return 0;
    }
    
    list<K> get_evict_entries()
    {
        list<K> newlist(_evicted_key_tracker);
        return newlist;
    }

// The function to be cached
	V(*_fn)(const K & , V);
// Maximum number of key-value pairs to be retained
	const size_t _capacity;

// Key access history
	key_tracker_type _key_tracker;
// Key eviction history
    key_tracker_type _evicted_key_tracker;
// Key-to-value lookup
	key_to_value_type _key_to_value;
// Evicted Key-to-value lookup
    key_to_value_type _evicted_key_to_value;
	unsigned levelMinusMinus;
};

#endif //end lru_stl
