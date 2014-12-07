#ifndef __TILE_CACHE_H_
#define __TILE_CACHE_H_

#include <list>
#include <map>
#include <memory>
#include <functional>
#include <cassert>
#include "TileTypes.h"

// General LRU cache implementation. Note that on insertion, if the key
// is present this cache will evict the value and overwrite the slot. This 
// cache is NOT thread safe - it is designed to only be accessed from the 
// TileRenderer event/context thread, so no need for locks.
template <typename K, typename V> 
class LRUCache { 
    typedef std::list<K> KeyList;
    typedef std::map<K, std::pair<V, typename KeyList::iterator>> KeyMap;
    typedef std::function<void (V value)> Callback;

public:
    LRUCache(size_t size, Callback evict)
        : m_size(size), 
        m_evict(evict) 
    {
    }

    // retruns true and sets 'value' if the 'key' is present
    bool query(const K& key, V& value) {
        const typename KeyMap::iterator it = m_map.find(key);
        if (it == m_map.end()) {
            return false;
        } else {
            m_list.splice(m_list.end(), m_list, (*it).second.second);
            value = (*it).second.first;
            return true;
        }
    }

    // inserts 'value' in the slot for 'key'
    void insert(const K& key, const V& value) {
        typename KeyMap::iterator ret = m_map.find(key);
        if (ret == m_map.end()) {
            if (m_list.size() == m_size) {
                const typename KeyMap::iterator it = m_map.find(m_list.front()); 
                m_evict(it->second.first);
                m_map.erase(it);
                m_list.pop_front(); 
            }
            typename KeyList::iterator it = m_list.insert(m_list.end(), key);
            m_map.insert(std::make_pair(key, std::make_pair(value, it)));
        } else {
            // evict existing key/value pairs and overwrite
            m_evict(ret->second.first);
            ret->second.first = value;
            // update the LRU poliy tracking
            m_list.splice(m_list.end(), m_list, ret->second.second);
        }
    }
    size_t size() const {
        return m_map.size();
    }

private:
    size_t m_size;
    KeyList m_list;
    KeyMap m_map;
    Callback m_evict;
};

// The tile cache maps tile indices to tile images
typedef LRUCache<TileIndex, TileImage*> TileCache;

#endif