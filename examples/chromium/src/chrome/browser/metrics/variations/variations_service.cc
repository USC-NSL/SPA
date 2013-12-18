// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_service.h"

#include <set>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/proto/trials_seed.pb.h"
#include "chrome/browser/net/network_time_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace chrome_variations {

namespace {

// Default server of Variations seed info.
const char kDefaultVariationsServerURL[] =
    "https://clients4.google.com/chrome-variations/seed";
const int kMaxRetrySeedFetch = 5;

// TODO(mad): To be removed when we stop updating the NetworkTimeTracker.
// For the HTTP date headers, the resolution of the server time is 1 second.
const int64 kServerTimeResolutionMs = 1000;

// Maps Study_Channel enum values to corresponding chrome::VersionInfo::Channel
// enum values.
chrome::VersionInfo::Channel ConvertStudyChannelToVersionChannel(
    Study_Channel study_channel) {
  switch (study_channel) {
    case Study_Channel_CANARY:
      return chrome::VersionInfo::CHANNEL_CANARY;
    case Study_Channel_DEV:
      return chrome::VersionInfo::CHANNEL_DEV;
    case Study_Channel_BETA:
      return chrome::VersionInfo::CHANNEL_BETA;
    case Study_Channel_STABLE:
      return chrome::VersionInfo::CHANNEL_STABLE;
  }
  // All enum values of |study_channel| were handled above.
  NOTREACHED();
  return chrome::VersionInfo::CHANNEL_UNKNOWN;
}

// Wrapper around channel checking, used to enable channel mocking for
// testing. If the current browser channel is not UNKNOWN, this will return
// that channel value. Otherwise, if the fake channel flag is provided, this
// will return the fake channel. Failing that, this will return the UNKNOWN
// channel.
chrome::VersionInfo::Channel GetChannelForVariations() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel != chrome::VersionInfo::CHANNEL_UNKNOWN)
    return channel;
  std::string forced_channel =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFakeVariationsChannel);
  if (forced_channel == "stable")
    channel = chrome::VersionInfo::CHANNEL_STABLE;
  else if (forced_channel == "beta")
    channel = chrome::VersionInfo::CHANNEL_BETA;
  else if (forced_channel == "dev")
    channel = chrome::VersionInfo::CHANNEL_DEV;
  else if (forced_channel == "canary")
    channel = chrome::VersionInfo::CHANNEL_CANARY;
  else
    DVLOG(1) << "Invalid channel provided: " << forced_channel;
  return channel;
}

Study_Platform GetCurrentPlatform() {
#if defined(OS_WIN)
  return Study_Platform_PLATFORM_WINDOWS;
#elif defined(OS_MACOSX)
  return Study_Platform_PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return Study_Platform_PLATFORM_CHROMEOS;
#elif defined(OS_ANDROID)
  return Study_Platform_PLATFORM_ANDROID;
#elif defined(OS_IOS)
  return Study_Platform_PLATFORM_IOS;
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study_Platform_PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64 date_time) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(date_time);
}

// Gets the restrict parameter from |local_state| or from Chrome OS settings in
// the case of that platform.
std::string GetRestrictParameterPref(PrefService* local_state) {
  std::string parameter;
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetString(
      chromeos::kVariationsRestrictParameter, &parameter);
#else
  if (local_state)
    parameter = local_state->GetString(prefs::kVariationsRestrictParameter);
#endif
  return parameter;
}

}  // namespace

VariationsService::VariationsService(PrefService* local_state)
    : local_state_(local_state),
      variations_server_url_(GetVariationsServerURL(local_state)),
      create_trials_from_seed_called_(false),
      resource_request_allowed_notifier_(
          new ResourceRequestAllowedNotifier) {
  resource_request_allowed_notifier_->Init(this);
}

VariationsService::VariationsService(ResourceRequestAllowedNotifier* notifier,
                                     PrefService* local_state)
    : local_state_(local_state),
      variations_server_url_(GetVariationsServerURL(NULL)),
      create_trials_from_seed_called_(false),
      resource_request_allowed_notifier_(notifier) {
  resource_request_allowed_notifier_->Init(this);
}

VariationsService::~VariationsService() {
}

bool VariationsService::CreateTrialsFromSeed() {
  create_trials_from_seed_called_ = true;

  TrialsSeed seed;
  if (!LoadTrialsSeedFromPref(local_state_, &seed))
    return false;

  const int64 date_value = local_state_->GetInt64(prefs::kVariationsSeedDate);
  const base::Time seed_date = base::Time::FromInternalValue(date_value);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;

  const chrome::VersionInfo current_version_info;
  if (!current_version_info.is_valid())
    return false;

  chrome::VersionInfo::Channel channel = GetChannelForVariations();
  for (int i = 0; i < seed.study_size(); ++i) {
    if (ShouldAddStudy(seed.study(i), current_version_info, reference_date,
                       channel)) {
      CreateTrialFromStudy(seed.study(i), reference_date);
    }
  }

  // Log the "freshness" of the seed that was just used. The freshness is the
  // time between the last successful seed download and now.
  const int64 last_fetch_time_internal =
      local_state_->GetInt64(prefs::kVariationsLastFetchTime);
  if (last_fetch_time_internal) {
    const base::Time now = base::Time::Now();
    const base::TimeDelta delta =
        now - base::Time::FromInternalValue(last_fetch_time_internal);
    // Log the value in number of minutes.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", delta.InMinutes(),
        1, base::TimeDelta::FromDays(30).InMinutes(), 50);
  }

  return true;
}

void VariationsService::StartRepeatedVariationsSeedFetch() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check that |CreateTrialsFromSeed| was called, which is necessary to
  // retrieve the serial number that will be sent to the server.
  DCHECK(create_trials_from_seed_called_);

  DCHECK(!request_scheduler_.get());
  // Note that the act of instantiating the scheduler will start the fetch, if
  // the scheduler deems appropriate. Using Unretained is fine here since the
  // lifespan of request_scheduler_ is guaranteed to be shorter than that of
  // this service.
  request_scheduler_.reset(VariationsRequestScheduler::Create(
      base::Bind(&VariationsService::FetchVariationsSeed,
          base::Unretained(this)), local_state_));
  request_scheduler_->Start();
}

// static
GURL VariationsService::GetVariationsServerURL(PrefService* local_state) {
  std::string server_url_string(CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kVariationsServerURL));
  if (server_url_string.empty())
    server_url_string = kDefaultVariationsServerURL;
  GURL server_url = GURL(server_url_string);
  const std::string restrict_param = GetRestrictParameterPref(local_state);
  if (!restrict_param.empty()) {
    server_url = net::AppendOrReplaceQueryParameter(server_url,
                                                    "restrict",
                                                    restrict_param);
  }
  DCHECK(server_url.is_valid());
  return server_url;
}

#if defined(OS_WIN)
void VariationsService::StartGoogleUpdateRegistrySync() {
  registry_syncer_.RequestRegistrySync();
}
#endif

void VariationsService::SetCreateTrialsFromSeedCalledForTesting(bool called) {
  create_trials_from_seed_called_ = called;
}

// static
std::string VariationsService::GetDefaultVariationsServerURLForTesting() {
  return kDefaultVariationsServerURL;
}

// static
void VariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsSeed, std::string());
  registry->RegisterInt64Pref(prefs::kVariationsSeedDate,
                              base::Time().ToInternalValue());
  registry->RegisterInt64Pref(prefs::kVariationsLastFetchTime, 0);
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
}

// static
VariationsService* VariationsService::Create(PrefService* local_state) {
#if !defined(GOOGLE_CHROME_BUILD)
  // Unless the URL was provided, unsupported builds should return NULL to
  // indicate that the service should not be used.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVariationsServerURL))
    return NULL;
#endif
  return new VariationsService(local_state);
}

void VariationsService::DoActualFetch() {
  pending_seed_request_.reset(net::URLFetcher::Create(
      0, variations_server_url_, net::URLFetcher::GET, this));
  pending_seed_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                      net::LOAD_DO_NOT_SAVE_COOKIES);
  pending_seed_request_->SetRequestContext(
      g_browser_process->system_request_context());
  pending_seed_request_->SetMaxRetriesOn5xx(kMaxRetrySeedFetch);
  if (!variations_serial_number_.empty()) {
    pending_seed_request_->AddExtraRequestHeader("If-Match:" +
                                                 variations_serial_number_);
  }
  pending_seed_request_->Start();

  last_request_started_time_ = base::TimeTicks::Now();
}

void VariationsService::FetchVariationsSeed() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!resource_request_allowed_notifier_->ResourceRequestsAllowed()) {
    DVLOG(1) << "Resource requests were not allowed. Waiting for notification.";
    return;
  }

  DoActualFetch();
}

void VariationsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(pending_seed_request_.get(), source);
  // The fetcher will be deleted when the request is handled.
  scoped_ptr<const net::URLFetcher> request(pending_seed_request_.release());
  if (request->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    DVLOG(1) << "Variations server request failed.";
    return;
  }

  // Log the response code.
  const int response_code = request->GetResponseCode();
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Variations.SeedFetchResponseCode",
      net::HttpUtil::MapStatusCodeForHistogram(response_code),
      net::HttpUtil::GetStatusCodesForHistogram());

  const base::TimeDelta latency =
      base::TimeTicks::Now() - last_request_started_time_;

  base::Time response_date;
  if (response_code == net::HTTP_OK ||
      response_code == net::HTTP_NOT_MODIFIED) {
    bool success = request->GetResponseHeaders()->GetDateValue(&response_date);
    DCHECK(success || response_date.is_null());

    if (!response_date.is_null()) {
      NetworkTimeTracker::BuildNotifierUpdateCallback().Run(
          response_date,
          base::TimeDelta::FromMilliseconds(kServerTimeResolutionMs),
          latency);
    }
  }

  if (response_code != net::HTTP_OK) {
    DVLOG(1) << "Variations server request returned non-HTTP_OK response code: "
             << response_code;
    if (response_code == net::HTTP_NOT_MODIFIED) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchNotModifiedLatency", latency);
      RecordLastFetchTime();
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchOtherLatency", latency);
    }
    return;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Variations.FetchSuccessLatency", latency);

  std::string seed_data;
  bool success = request->GetResponseAsString(&seed_data);
  DCHECK(success);

  StoreSeedData(seed_data, response_date, local_state_);
}

void VariationsService::OnResourceRequestsAllowed() {
  // Note that this only attempts to fetch the seed at most once per period
  // (kSeedFetchPeriodHours). This works because
  // |resource_request_allowed_notifier_| only calls this method if an
  // attempt was made earlier that fails (which implies that the period had
  // elapsed). After a successful attempt is made, the notifier will know not
  // to call this method again until another failed attempt occurs.
  DVLOG(1) << "Retrying fetch.";
  DoActualFetch();

  // This service must have created a scheduler in order for this to be called.
  DCHECK(request_scheduler_.get());
  request_scheduler_->Reset();
}

bool VariationsService::StoreSeedData(const std::string& seed_data,
                                      const base::Time& seed_date,
                                      PrefService* local_prefs) {
  if (seed_data.empty()) {
    VLOG(1) << "Variations Seed data from server is empty, rejecting the seed.";
    return false;
  }

  // Only store the seed data if it parses correctly.
  TrialsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    VLOG(1) << "Variations Seed data from server is not in valid proto format, "
            << "rejecting the seed.";
    return false;
  }

  std::string base64_seed_data;
  if (!base::Base64Encode(seed_data, &base64_seed_data)) {
    VLOG(1) << "Variations Seed data from server fails Base64Encode, rejecting "
            << "the seed.";
    return false;
  }

  local_prefs->SetString(prefs::kVariationsSeed, base64_seed_data);
  local_prefs->SetInt64(prefs::kVariationsSeedDate,
                        seed_date.ToInternalValue());
  variations_serial_number_ = seed.serial_number();

  RecordLastFetchTime();

  return true;
}

// static
bool VariationsService::ShouldAddStudy(
    const Study& study,
    const chrome::VersionInfo& version_info,
    const base::Time& reference_date,
    const chrome::VersionInfo::Channel channel) {
  if (study.has_filter()) {
    if (!CheckStudyChannel(study.filter(), channel)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to channel.";
      return false;
    }

    if (!CheckStudyLocale(study.filter(),
                          g_browser_process->GetApplicationLocale())) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to locale.";
      return false;
    }

    if (!CheckStudyPlatform(study.filter(), GetCurrentPlatform())) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to platform.";
      return false;
    }

    if (!CheckStudyVersion(study.filter(), version_info.Version())) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to version.";
      return false;
    }

    if (!CheckStudyStartDate(study.filter(), reference_date)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to start date.";
      return false;
    }
  }

  DVLOG(1) << "Kept study " << study.name() << ".";
  return true;
}

// static
bool VariationsService::CheckStudyChannel(
    const Study_Filter& filter,
    chrome::VersionInfo::Channel channel) {
  // An empty channel list matches all channels.
  if (filter.channel_size() == 0)
    return true;

  for (int i = 0; i < filter.channel_size(); ++i) {
    if (ConvertStudyChannelToVersionChannel(filter.channel(i)) == channel)
      return true;
  }
  return false;
}

// static
bool VariationsService::CheckStudyLocale(
    const chrome_variations::Study_Filter& filter,
    const std::string& locale) {
  // An empty locale list matches all locales.
  if (filter.locale_size() == 0)
    return true;

  for (int i = 0; i < filter.locale_size(); ++i) {
    if (filter.locale(i) == locale)
      return true;
  }
  return false;
}

// static
bool VariationsService::CheckStudyPlatform(
    const Study_Filter& filter,
    Study_Platform platform) {
  // An empty platform list matches all platforms.
  if (filter.platform_size() == 0)
    return true;

  for (int i = 0; i < filter.platform_size(); ++i) {
    if (filter.platform(i) == platform)
      return true;
  }
  return false;
}

// static
bool VariationsService::CheckStudyVersion(
    const Study_Filter& filter,
    const std::string& version_string) {
  const Version version(version_string);
  if (!version.IsValid()) {
    NOTREACHED();
    return false;
  }

  if (filter.has_min_version()) {
    if (version.CompareToWildcardString(filter.min_version()) < 0)
      return false;
  }

  if (filter.has_max_version()) {
    if (version.CompareToWildcardString(filter.max_version()) > 0)
      return false;
  }

  return true;
}

// static
bool VariationsService::CheckStudyStartDate(
    const Study_Filter& filter,
    const base::Time& date_time) {
  if (filter.has_start_date()) {
    const base::Time start_date =
        ConvertStudyDateToBaseTime(filter.start_date());
    return date_time >= start_date;
  }

  return true;
}

bool VariationsService::IsStudyExpired(const Study& study,
                                       const base::Time& date_time) {
  if (study.has_expiry_date()) {
    const base::Time expiry_date =
        ConvertStudyDateToBaseTime(study.expiry_date());
    return date_time >= expiry_date;
  }

  return false;
}

// static
bool VariationsService::ValidateStudyAndComputeTotalProbability(
    const Study& study,
    base::FieldTrial::Probability* total_probability) {
  // At the moment, a missing default_experiment_name makes the study invalid.
  if (study.default_experiment_name().empty()) {
    DVLOG(1) << study.name() << " has no default experiment defined.";
    return false;
  }
  if (study.filter().has_min_version() &&
      !Version::IsValidWildcardString(study.filter().min_version())) {
    DVLOG(1) << study.name() << " has invalid min version: "
             << study.filter().min_version();
    return false;
  }
  if (study.filter().has_max_version() &&
      !Version::IsValidWildcardString(study.filter().max_version())) {
    DVLOG(1) << study.name() << " has invalid max version: "
             << study.filter().max_version();
    return false;
  }

  const std::string& default_group_name = study.default_experiment_name();
  base::FieldTrial::Probability divisor = 0;

  bool found_default_group = false;
  std::set<std::string> experiment_names;
  for (int i = 0; i < study.experiment_size(); ++i) {
    if (study.experiment(i).name().empty()) {
      DVLOG(1) << study.name() << " is missing experiment " << i << " name";
      return false;
    }
    if (!experiment_names.insert(study.experiment(i).name()).second) {
      DVLOG(1) << study.name() << " has a repeated experiment name "
               << study.experiment(i).name();
      return false;
    }
    divisor += study.experiment(i).probability_weight();
    if (study.experiment(i).name() == default_group_name)
      found_default_group = true;
  }

  if (!found_default_group) {
    DVLOG(1) << study.name() << " is missing default experiment in its "
             << "experiment list";
    // The default group was not found in the list of groups. This study is not
    // valid.
    return false;
  }

  *total_probability = divisor;
  return true;
}

bool VariationsService::LoadTrialsSeedFromPref(PrefService* local_prefs,
                                               TrialsSeed* seed) {
  std::string base64_seed_data = local_prefs->GetString(prefs::kVariationsSeed);
  if (base64_seed_data.empty()) {
    UMA_HISTOGRAM_BOOLEAN("Variations.SeedEmpty", true);
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string seed_data;
  if (!base::Base64Decode(base64_seed_data, &seed_data) ||
      !seed->ParseFromString(seed_data)) {
    VLOG(1) << "Variations Seed data in local pref is corrupt, clearing the "
            << "pref.";
    local_prefs->ClearPref(prefs::kVariationsSeed);
    return false;
  }
  variations_serial_number_ = seed->serial_number();
  return true;
}

void VariationsService::CreateTrialFromStudy(const Study& study,
                                             const base::Time& reference_date) {
  base::FieldTrial::Probability total_probability = 0;
  if (!ValidateStudyAndComputeTotalProbability(study, &total_probability))
    return;

  // The trial is created without specifying an expiration date because the
  // expiration check in field_trial.cc is based on the build date. Instead,
  // the expiration check using |reference_date| is done explicitly below.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          study.name(), total_probability, study.default_experiment_name(),
          base::FieldTrialList::kNoExpirationYear, 1, 1, NULL));

  if (study.has_consistency() &&
      study.consistency() == Study_Consistency_PERMANENT) {
    if (study.has_randomization_seed())
      trial->UseOneTimeRandomizationWithCustomSeed(study.randomization_seed());
    else
      trial->UseOneTimeRandomization();
  }

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.name() != study.default_experiment_name())
      trial->AppendGroup(experiment.name(), experiment.probability_weight());

    if (experiment.has_google_web_experiment_id()) {
      const VariationID variation_id =
          static_cast<VariationID>(experiment.google_web_experiment_id());
      AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES,
                                      study.name(),
                                      experiment.name(),
                                      variation_id);
    }
    if (experiment.has_google_update_experiment_id()) {
      const VariationID variation_id =
          static_cast<VariationID>(experiment.google_update_experiment_id());
      AssociateGoogleVariationIDForce(GOOGLE_UPDATE_SERVICE,
                                      study.name(),
                                      experiment.name(),
                                      variation_id);
    }
  }

  trial->SetForced();
  if (IsStudyExpired(study, reference_date))
    trial->Disable();
}

void VariationsService::RecordLastFetchTime() {
  // local_state_ is NULL in tests, so check it first.
  if (local_state_) {
    local_state_->SetInt64(prefs::kVariationsLastFetchTime,
                           base::Time::Now().ToInternalValue());
  }
}

}  // namespace chrome_variations
