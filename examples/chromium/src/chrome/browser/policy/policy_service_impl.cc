// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_service_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

PolicyServiceImpl::PolicyChangeInfo::PolicyChangeInfo(
    const PolicyNamespace& policy_namespace,
    const PolicyMap& previous,
    const PolicyMap& current)
    : policy_namespace_(policy_namespace) {
  previous_.CopyFrom(previous);
  current_.CopyFrom(current);
}

PolicyServiceImpl::PolicyChangeInfo::~PolicyChangeInfo() {
}

typedef PolicyServiceImpl::Providers::const_iterator Iterator;

PolicyServiceImpl::PolicyServiceImpl(const Providers& providers)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    initialization_complete_[domain] = true;
  providers_ = providers;
  for (Iterator it = providers.begin(); it != providers.end(); ++it) {
    ConfigurationPolicyProvider* provider = *it;
    provider->AddObserver(this);
    for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
      initialization_complete_[domain] &=
          provider->IsInitializationComplete(static_cast<PolicyDomain>(domain));
    }
  }
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

PolicyServiceImpl::~PolicyServiceImpl() {
  for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
    (*it)->RemoveObserver(this);
  STLDeleteValues(&observers_);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    PolicyService::Observer* observer) {
  Observers*& list = observers_[domain];
  if (!list)
    list = new Observers();
  list->AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       PolicyService::Observer* observer) {
  ObserverMap::iterator it = observers_.find(domain);
  if (it == observers_.end()) {
    NOTREACHED();
    return;
  }
  it->second->RemoveObserver(observer);
  if (it->second->size() == 0) {
    delete it->second;
    observers_.erase(it);
  }
}

void PolicyServiceImpl::RegisterPolicyDomain(
    PolicyDomain domain,
    const std::set<std::string>& components) {
  for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
    (*it)->RegisterPolicyDomain(domain, components);
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    const PolicyNamespace& ns) const {
  return policy_bundle_.Get(ns);
}

bool PolicyServiceImpl::IsInitializationComplete(PolicyDomain domain) const {
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return initialization_complete_[domain];
}

void PolicyServiceImpl::RefreshPolicies(const base::Closure& callback) {
  if (!callback.is_null())
    refresh_callbacks_.push_back(callback);

  if (providers_.empty()) {
    // Refresh is immediately complete if there are no providers.
    MergeAndTriggerUpdates();
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
      refresh_pending_.insert(*it);
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
      (*it)->RefreshPolicies();
  }
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(1, std::count(providers_.begin(), providers_.end(), provider));
  refresh_pending_.erase(provider);
  MergeAndTriggerUpdates();
}

void PolicyServiceImpl::NotifyNamespaceUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  // If running a unit test that hasn't setup a MessageLoop, don't send any
  // notifications.
  if (!MessageLoop::current())
    return;

  // Don't queue up a task if we have no observers - that way Observers added
  // later don't get notified of changes that happened during construction time.
  if (observers_.find(ns.domain) == observers_.end())
    return;

  // Notify Observers via a queued task, so Observers can't trigger a re-entrant
  // call to MergeAndTriggerUpdates() by modifying policy.
  scoped_ptr<PolicyChangeInfo> changes(
      new PolicyChangeInfo(ns, previous, current));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PolicyServiceImpl::NotifyNamespaceUpdatedTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&changes)));
}

void PolicyServiceImpl::NotifyNamespaceUpdatedTask(
    scoped_ptr<PolicyChangeInfo> changes) {
  ObserverMap::iterator iterator = observers_.find(
      changes->policy_namespace_.domain);
  if (iterator != observers_.end()) {
    FOR_EACH_OBSERVER(
        PolicyService::Observer,
        *iterator->second,
        OnPolicyUpdated(changes->policy_namespace_,
                        changes->previous_,
                        changes->current_));
  }
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // Merge from each provider in their order of priority.
  PolicyBundle bundle;
  for (Iterator it = providers_.begin(); it != providers_.end(); ++it)
    bundle.MergeFrom((*it)->policies());

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  policy_bundle_.Swap(&bundle);

  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = bundle.begin();
  PolicyBundle::const_iterator end_old = bundle.end();
  while (it_new != end_new && it_old != end_old) {
    if (it_new->first < it_old->first) {
      // A new namespace is available.
      NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);
      ++it_new;
    } else if (it_old->first < it_new->first) {
      // A previously available namespace is now gone.
      NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);
      ++it_old;
    } else {
      if (!it_new->second->Equals(*it_old->second)) {
        // An existing namespace's policies have changed.
        NotifyNamespaceUpdated(it_new->first, *it_old->second, *it_new->second);
      }
      ++it_new;
      ++it_old;
    }
  }

  // Send updates for the remaining new namespaces, if any.
  for (; it_new != end_new; ++it_new)
    NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old)
    NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);

  CheckInitializationComplete();
  CheckRefreshComplete();
}

void PolicyServiceImpl::CheckInitializationComplete() {
  // Check if all the providers just became initialized for each domain; if so,
  // notify that domain's observers.
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
    if (initialization_complete_[domain])
      continue;

    PolicyDomain policy_domain = static_cast<PolicyDomain>(domain);

    bool all_complete = true;
    for (Iterator it = providers_.begin(); it != providers_.end(); ++it) {
      if (!(*it)->IsInitializationComplete(policy_domain)) {
        all_complete = false;
        break;
      }
    }
    if (all_complete) {
      initialization_complete_[domain] = true;
      ObserverMap::iterator iter = observers_.find(policy_domain);
      if (iter != observers_.end()) {
        FOR_EACH_OBSERVER(PolicyService::Observer,
                          *iter->second,
                          OnPolicyServiceInitialized(policy_domain));
      }
    }
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::Closure> callbacks;
    callbacks.swap(refresh_callbacks_);
    std::vector<base::Closure>::iterator it;
    for (it = callbacks.begin(); it != callbacks.end(); ++it)
      it->Run();
  }
}

}  // namespace policy
