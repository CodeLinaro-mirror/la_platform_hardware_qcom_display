/*
* Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __MULTI_CORE_INSTANTIATOR_H__
#define __MULTI_CORE_INSTANTIATOR_H__

#include<map>

namespace sdm {

template<typename Key, typename Value>
using MultiCoreIterator = typename std::map<Key, Value>::const_iterator;

template<typename Key, typename Value>
class MultiCoreInstance {
 public:
  MultiCoreInstance() { }

  void Insert(Key k, Value v) {
    mp[k] = v;
  }

  MultiCoreIterator<Key, Value> Find(Key key) {
    return mp.find(key);
  }

  MultiCoreIterator<Key, Value> End() {
    return mp.end();
  }

  MultiCoreIterator<Key, Value> Begin() {
    return mp.begin();
  }

  Value& operator[](int index) {
    return mp[index];
  }

  void Erase(const Key& key) {
    mp.erase(key);
  }

  void Erase(MultiCoreIterator<Key, Value> position) {
    mp.erase(position);
  }

 private:
  std::map<Key, Value> mp;
};

}  // namespace sdm

#endif  // __MULTI_CORE_INSTANTIATOR_H__
