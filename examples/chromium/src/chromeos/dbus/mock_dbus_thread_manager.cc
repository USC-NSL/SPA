// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager.h"

#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_config_client.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/ibus/mock_ibus_panel_service.h"
#include "chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chromeos/dbus/mock_bluetooth_device_client.h"
#include "chromeos/dbus/mock_bluetooth_input_client.h"
#include "chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chromeos/dbus/mock_bluetooth_node_client.h"
#include "chromeos/dbus/mock_bluetooth_out_of_band_client.h"
#include "chromeos/dbus/mock_cros_disks_client.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_shill_device_client.h"
#include "chromeos/dbus/mock_shill_ipconfig_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/dbus/mock_gsm_sms_client.h"
#include "chromeos/dbus/mock_image_burner_client.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_system_clock_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"
#include "chromeos/dbus/power_policy_controller.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

namespace {

std::vector<uint8>* GetMockSystemSalt() {
  static std::vector<uint8>* s_system_salt = NULL;
  if (!s_system_salt) {
    const char kStubSystemSalt[] = "stub_system_salt";
    s_system_salt = new std::vector<uint8>();
    s_system_salt->assign(kStubSystemSalt,
                          kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
  }
  return s_system_salt;
}

}  // namespace

MockDBusThreadManager::MockDBusThreadManager()
    : mock_bluetooth_adapter_client_(new MockBluetoothAdapterClient),
      mock_bluetooth_device_client_(new MockBluetoothDeviceClient),
      mock_bluetooth_input_client_(new MockBluetoothInputClient),
      mock_bluetooth_manager_client_(new MockBluetoothManagerClient),
      mock_bluetooth_node_client_(new MockBluetoothNodeClient),
      mock_bluetooth_out_of_band_client_(new MockBluetoothOutOfBandClient),
      mock_cros_disks_client_(new MockCrosDisksClient),
      mock_cryptohome_client_(new MockCryptohomeClient),
      mock_shill_device_client_(new MockShillDeviceClient),
      mock_shill_ipconfig_client_(new MockShillIPConfigClient),
      mock_shill_manager_client_(new MockShillManagerClient),
      mock_shill_profile_client_(new MockShillProfileClient),
      mock_shill_service_client_(new MockShillServiceClient),
      mock_gsm_sms_client_(new MockGsmSMSClient),
      mock_image_burner_client_(new MockImageBurnerClient),
      mock_power_manager_client_(new MockPowerManagerClient),
      mock_session_manager_client_(new MockSessionManagerClient),
      mock_system_clock_client_(new MockSystemClockClient),
      mock_update_engine_client_(new MockUpdateEngineClient),
      mock_ibus_client_(new MockIBusClient),
      mock_ibus_config_client_(new MockIBusConfigClient),
      mock_ibus_input_context_client_(new MockIBusInputContextClient),
      mock_ibus_engine_service_(new MockIBusEngineService),
      mock_ibus_engine_factory_service_(new MockIBusEngineFactoryService),
      mock_ibus_panel_service_(new MockIBusPanelService) {
  EXPECT_CALL(*this, GetBluetoothAdapterClient())
      .WillRepeatedly(Return(mock_bluetooth_adapter_client_.get()));
  EXPECT_CALL(*this, GetBluetoothDeviceClient())
      .WillRepeatedly(Return(mock_bluetooth_device_client_.get()));
  EXPECT_CALL(*this, GetBluetoothInputClient())
      .WillRepeatedly(Return(mock_bluetooth_input_client_.get()));
  EXPECT_CALL(*this, GetBluetoothManagerClient())
      .WillRepeatedly(Return(mock_bluetooth_manager_client()));
  EXPECT_CALL(*this, GetBluetoothNodeClient())
      .WillRepeatedly(Return(mock_bluetooth_node_client_.get()));
  EXPECT_CALL(*this, GetBluetoothOutOfBandClient())
      .WillRepeatedly(Return(mock_bluetooth_out_of_band_client_.get()));
  EXPECT_CALL(*this, GetCrosDisksClient())
      .WillRepeatedly(Return(mock_cros_disks_client()));
  EXPECT_CALL(*this, GetCryptohomeClient())
      .WillRepeatedly(Return(mock_cryptohome_client()));
  EXPECT_CALL(*this, GetShillDeviceClient())
      .WillRepeatedly(Return(mock_shill_device_client()));
  EXPECT_CALL(*this, GetShillIPConfigClient())
      .WillRepeatedly(Return(mock_shill_ipconfig_client()));
  EXPECT_CALL(*this, GetShillManagerClient())
      .WillRepeatedly(Return(mock_shill_manager_client()));
  EXPECT_CALL(*this, GetShillProfileClient())
      .WillRepeatedly(Return(mock_shill_profile_client()));
  EXPECT_CALL(*this, GetShillServiceClient())
      .WillRepeatedly(Return(mock_shill_service_client()));
  EXPECT_CALL(*this, GetGsmSMSClient())
      .WillRepeatedly(Return(mock_gsm_sms_client()));
  EXPECT_CALL(*this, GetImageBurnerClient())
      .WillRepeatedly(Return(mock_image_burner_client()));
  EXPECT_CALL(*this, GetPowerManagerClient())
      .WillRepeatedly(Return(mock_power_manager_client_.get()));
  EXPECT_CALL(*this, GetSessionManagerClient())
      .WillRepeatedly(Return(mock_session_manager_client_.get()));
  EXPECT_CALL(*this, GetSystemClockClient())
      .WillRepeatedly(Return(mock_system_clock_client()));
  EXPECT_CALL(*this, GetUpdateEngineClient())
      .WillRepeatedly(Return(mock_update_engine_client_.get()));
  EXPECT_CALL(*this, GetIBusClient())
      .WillRepeatedly(Return(mock_ibus_client_.get()));
  EXPECT_CALL(*this, GetIBusConfigClient())
      .WillRepeatedly(Return(mock_ibus_config_client_.get()));
  EXPECT_CALL(*this, GetIBusInputContextClient())
      .WillRepeatedly(Return(mock_ibus_input_context_client_.get()));
  EXPECT_CALL(*this, GetIBusEngineFactoryService())
      .WillRepeatedly(Return(mock_ibus_engine_factory_service_.get()));
  EXPECT_CALL(*this, GetIBusEngineService(_))
      .WillRepeatedly(Return(mock_ibus_engine_service_.get()));
  EXPECT_CALL(*this, GetIBusPanelService())
      .WillRepeatedly(Return(mock_ibus_panel_service_.get()));


  EXPECT_CALL(*this, GetSystemBus())
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(*this, GetIBusBus())
      .WillRepeatedly(ReturnNull());

  // |power_policy_controller_| calls some of these from the constructor, so
  // set these expectations before creating the controller.
  EXPECT_CALL(*mock_power_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), NotifyUserActivity())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), NotifyVideoActivity(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), SetPolicy(_))
      .Times(AnyNumber());
  power_policy_controller_.reset(
      new PowerPolicyController(this, mock_power_manager_client_.get()));
  EXPECT_CALL(*this, GetPowerPolicyController())
      .WillRepeatedly(Return(power_policy_controller_.get()));

  // These observers calls are used in ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_session_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), HasObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_update_engine_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_system_clock_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_system_clock_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_system_clock_client_.get(), HasObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_update_engine_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_adapter_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_adapter_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_device_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_device_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_input_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_input_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_node_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_node_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());

  // Called from PowerMenuButton ctor.
  EXPECT_CALL(*mock_power_manager_client_.get(), RequestStatusUpdate(_))
      .Times(AnyNumber());

  // Called from DiskMountManager::Initialize(), ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_cros_disks_client_.get(), SetUpConnections(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_cros_disks_client_.get(),
              EnumerateAutoMountableDevices(_, _))
      .Times(AnyNumber());

  // Called from BluetoothManagerImpl ctor.
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), DefaultAdapter(_))
      .Times(AnyNumber());

  // Called from AsyncMethodCaller ctor and dtor.
  EXPECT_CALL(*mock_cryptohome_client_.get(), SetAsyncCallStatusHandlers(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_cryptohome_client_.get(), ResetAsyncCallStatusHandlers())
      .Times(AnyNumber());
  // Called from various locations.
  EXPECT_CALL(*mock_cryptohome_client_.get(), GetSystemSalt(_))
      .WillRepeatedly(DoAll(SetArgumentPointee<0>(*GetMockSystemSalt()),
                            Return(true)));

  // Called from BrightnessController::GetBrightnessPercent as part of ash tray
  // initialization.
  EXPECT_CALL(*mock_power_manager_client_.get(), GetScreenBrightnessPercent(_))
      .Times(AnyNumber());

  // Called from GeolocationHandler::Init().
  EXPECT_CALL(*mock_shill_manager_client_.get(), GetProperties(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_shill_manager_client_.get(), AddPropertyChangedObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_shill_manager_client_.get(),
              RemovePropertyChangedObserver(_))
      .Times(AnyNumber());

  // For CrOS browsertests, ChromeBrowserMainPartsChromeos::PostProfileInit()
  // creates an AutomaticRebootManager which calls the following function.
  // For unittests, this function won't get called.
  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .WillRepeatedly(Return(MockUpdateEngineClient::Status()));
}

MockDBusThreadManager::~MockDBusThreadManager() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
}

void MockDBusThreadManager::AddObserver(DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void MockDBusThreadManager::RemoveObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
