#include "releaseShared/libmobilecore_api.h"
#include <Python.h>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <string>

namespace py = pybind11;

#define DyteSdk (kSymbols->kotlin.root.io.dyte.core)
#define TypeName(ns, type) libmobilecore_kref_io_dyte_##ns##_##type

static libmobilecore_ExportedSymbols *kSymbols;

__attribute__((constructor)) void initSyms(void) {
  kSymbols = libmobilecore_symbols();
}

template <typename T> class KotlinAutoFree {
protected:
  T obj;

public:
  KotlinAutoFree(T obj_) : obj(obj_) {
    if (obj.pinned == nullptr) {
      std::cerr << __func__ << ": " << typeid(T).name()
                << ": Got null object!\n";
      std::abort();
    }

    std::cout << __func__ << ": " << typeid(T).name() << ": " << obj.pinned
              << '\n';
  }
  ~KotlinAutoFree() {
    std::cout << __func__ << ": " << typeid(T).name() << ": " << obj.pinned
              << '\n';
    kSymbols->DisposeStablePointer(obj.pinned);
  }

  operator T() const { return obj; }

  // Delete copy-constructors to prevent double-frees
  KotlinAutoFree(const KotlinAutoFree &) = delete;
  KotlinAutoFree &operator=(const KotlinAutoFree &) = delete;
};

using KDyteMeetingInfoV2 =
    KotlinAutoFree<TypeName(core_models, DyteMeetingInfoV2)>;
using KDyteMeetingBuilder = KotlinAutoFree<TypeName(core, DyteMeetingBuilder)>;
using KDyteMobileClient = KotlinAutoFree<TypeName(core, DyteMobileClient)>;
using KDyteParticipantEventsListener =
    KotlinAutoFree<TypeName(core_utils, DyteParticipantEventsListener)>;

using kDyteJoinedMeetingParticipant = TypeName(core_models,
                                               DyteJoinedMeetingParticipant);
using KDyteJoinedMeetingParticipant =
    KotlinAutoFree<kDyteJoinedMeetingParticipant>;

using kDyteAudioStreamTrack = TypeName(webrtc, AudioStreamTrack);
using KDyteAudioStreamTrack = KotlinAutoFree<kDyteAudioStreamTrack>;

using KDyteSuccessFailureCb =
    KotlinAutoFree<libmobilecore_kref_kotlin_Function0>;

class DyteMeetingInfoV2 : public KDyteMeetingInfoV2 {
public:
  DyteMeetingInfoV2(const std::string &authToken, bool enableAudio,
                    bool enableVideo, const std::string &baseUrl)
      : KDyteMeetingInfoV2(DyteSdk.models.DyteMeetingInfoV2.DyteMeetingInfoV2(
            authToken.c_str(), enableAudio, enableVideo, baseUrl.c_str())) {}
  ~DyteMeetingInfoV2() {}
};

class DyteSuccessFailureCb {
private:
  KDyteSuccessFailureCb success;
  KDyteSuccessFailureCb failure;

  std::promise<bool> promise;

  static void callback(bool success, std::promise<bool> *promise) {
    promise->set_value(success);
  }

public:
  DyteSuccessFailureCb()
      : success(KDyteSuccessFailureCb(DyteSdk.utils.createSuccessCb(
            reinterpret_cast<void *>(&DyteSuccessFailureCb::callback),
            &promise))),
        failure(KDyteSuccessFailureCb(DyteSdk.utils.createFailureCb(
            reinterpret_cast<void *>(&DyteSuccessFailureCb::callback),
            &promise))) {}
  ~DyteSuccessFailureCb() {}

  KDyteSuccessFailureCb &SuccessCb() { return success; }
  KDyteSuccessFailureCb &FailureCb() { return failure; }

  bool Get() { return promise.get_future().get(); }
};

typedef void (*CAudioCb)(const char *, int, int, size_t, size_t, int64_t,
                         void *);

using AudioCb = const std::function<void(pybind11::bytes, int, int, size_t,
                                         size_t, int64_t)>;

class DyteAudioStreamTrack : public KDyteAudioStreamTrack {
public:
  DyteAudioStreamTrack(kDyteAudioStreamTrack track)
      : KDyteAudioStreamTrack(track){};
  ~DyteAudioStreamTrack(){};

  void RegisterDataCb(CAudioCb cb, void *userp) {
    auto audioTrackExt = KotlinAutoFree<TypeName(core_media, AudioTrackExt)>(
        DyteSdk.media.mutable_(*this));

    DyteSdk.media.AudioTrackExt.registerSink(
        audioTrackExt, reinterpret_cast<void *>(cb), userp);
  }

  void UnregisterDataCb(void) {
    auto audioTrackExt = KotlinAutoFree<TypeName(core_media, AudioTrackExt)>(
        DyteSdk.media.mutable_(*this));

    DyteSdk.media.AudioTrackExt.unregisterSink(audioTrackExt);
  }

  void SendData(const char *audio_data, int bits_per_sample, int sample_rate,
                size_t number_of_channels, size_t number_of_frames,
                int64_t absolute_capture_time_ms) {
    auto audioTrackExt = KotlinAutoFree<TypeName(core_media, AudioTrackExt)>(
        DyteSdk.media.mutable_(*this));

    DyteSdk.media.AudioTrackExt.send(
        audioTrackExt, const_cast<char *>(audio_data), bits_per_sample,
        sample_rate, number_of_channels, number_of_frames,
        absolute_capture_time_ms);
  }
};

class DyteJoinedMeetingParticipant : public KDyteJoinedMeetingParticipant {
private:
  std::shared_ptr<AudioCb> audioCb;
  std::mutex audioCbMutex;

public:
  DyteJoinedMeetingParticipant(kDyteJoinedMeetingParticipant participant)
      : KDyteJoinedMeetingParticipant(participant){};
  ~DyteJoinedMeetingParticipant() {
    if (!PyGILState_Check()) {
      // GIL must be acquired to safely destroy AudioCb
      py::gil_scoped_acquire _;
      UnregisterDataCb();
    } else {
      UnregisterDataCb();
    }
  }

  static void OnAudioData(const char *audio_data, int bits_per_sample,
                          int sample_rate, size_t number_of_channels,
                          size_t number_of_frames,
                          int64_t absolute_capture_timestamp_ms, void *userp) {
    auto self = static_cast<DyteJoinedMeetingParticipant *>(userp);

    auto cb = [&] {
      std::lock_guard<std::mutex> lock(self->audioCbMutex);
      return self->audioCb;
    }();

    if (cb == nullptr) {
      return;
    }

    pybind11::gil_scoped_acquire _;

    (*cb)(
        pybind11::bytes(audio_data, number_of_channels * number_of_frames * 2),
        bits_per_sample, sample_rate, number_of_channels, number_of_frames,
        absolute_capture_timestamp_ms);
  }

  bool HasAudioTrack(void) {
    auto track =
        DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this);

    if (track.pinned != nullptr) {
      auto dropTrack = DyteAudioStreamTrack(track);
      return true;
    }

    return false;
  }

  bool HasDataCb(void) {
    std::lock_guard<std::mutex> lock(audioCbMutex);
    return audioCb != nullptr;
  }

  void RegisterDataCb(AudioCb &cb) {
    {
      std::lock_guard<std::mutex> lock(audioCbMutex);
      audioCb = std::make_shared<AudioCb>(cb);
    }

    // Must release the GIL here as well as WebRTC will internally
    // wait for any ongoing OnAudioData callback to finish
    py::gil_scoped_release _;

    auto track = DyteAudioStreamTrack(
        DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this));
    track.RegisterDataCb(&DyteJoinedMeetingParticipant::OnAudioData, this);
  }

  void UnregisterDataCb(void) {
    {
      std::lock_guard<std::mutex> lock(audioCbMutex);
      if (audioCb == nullptr) {
        return;
      }
      audioCb = nullptr;
    }

    auto track = DyteAudioStreamTrack(
        DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this));

    // WebRTC will internally wait for the OnAudioData callback to finish
    // execution which can potentially deadlock as it calls into a Python
    // function internally
    py::gil_scoped_release _;
    track.UnregisterDataCb();
  }

  void SendData(const char *audio_data, int bits_per_sample, int sample_rate,
                size_t number_of_channels, size_t number_of_frames,
                int64_t absolute_capture_time_ms) {
    auto track = DyteAudioStreamTrack(
        DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this));
    track.SendData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                   number_of_frames, absolute_capture_time_ms);
  }

  const std::string Id(void) {
    return DyteSdk.models.DyteJoinedMeetingParticipant.get_id(*this);
  }
};

using OnJoinCb =
    const std::function<void(std::shared_ptr<DyteJoinedMeetingParticipant>)>;
using OnLeaveCb =
    const std::function<void(std::shared_ptr<DyteJoinedMeetingParticipant>)>;
using OnAudioUpdateCb = const std::function<void(
    bool, std::shared_ptr<DyteJoinedMeetingParticipant>)>;

class DyteParticipantStore {
private:
  std::map<const std::string, std::shared_ptr<DyteJoinedMeetingParticipant>>
      map_;
  std::mutex mutex_;

public:
  std::shared_ptr<DyteJoinedMeetingParticipant>
  GetOrAddParticipant(kDyteJoinedMeetingParticipant participant_) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto sharedParticipant =
        std::make_shared<DyteJoinedMeetingParticipant>(participant_);
    auto it = map_.find(sharedParticipant->Id());

    if (it == map_.end()) {
      std::cerr << "No participant found for ID " << sharedParticipant->Id()
                << std::endl;
      return (map_[sharedParticipant->Id()] = sharedParticipant);
    }

    std::cerr << "Returing existing participant for ID "
              << sharedParticipant->Id() << std::endl;
    return it->second;
  }

  void RemoveParticipant(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_.erase(id);
  }
};

class DyteParticipantEventsListener : public KDyteParticipantEventsListener {
private:
  OnJoinCb OnJoin;
  OnLeaveCb OnLeave;
  OnAudioUpdateCb OnAudioUpdate;

  std::shared_ptr<DyteParticipantStore> participantStore;

  static void onJoin(kDyteJoinedMeetingParticipant participant_,
                     DyteParticipantEventsListener *self) {
    self->OnJoin(self->participantStore->GetOrAddParticipant(participant_));
  }

  static void onLeave(kDyteJoinedMeetingParticipant participant_,
                      DyteParticipantEventsListener *self) {
    auto participant =
        self->participantStore->GetOrAddParticipant(participant_);
    self->OnLeave(participant);
    self->participantStore->RemoveParticipant(participant->Id());
  }

  static void onAudioUpdate(bool enabled,
                            kDyteJoinedMeetingParticipant participant_,
                            DyteParticipantEventsListener *self) {
    self->OnAudioUpdate(
        enabled, self->participantStore->GetOrAddParticipant(participant_));
  }

public:
  DyteParticipantEventsListener(OnJoinCb onJoin, OnLeaveCb onLeave,
                                OnAudioUpdateCb onAudioUpdate,
                                std::shared_ptr<DyteParticipantStore> store)
      : KDyteParticipantEventsListener(
            DyteSdk.utils.DyteParticipantEventsListener
                .DyteParticipantEventsListener(
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onJoin),
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onLeave),
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onAudioUpdate),
                    this)),
        OnJoin(onJoin), OnLeave(onLeave), OnAudioUpdate(onAudioUpdate),
        participantStore(store) {}
  ~DyteParticipantEventsListener() {}

  // Interface
  operator libmobilecore_kref_io_dyte_core_listeners_DyteParticipantEventsListener()
      const {
    return {obj.pinned};
  }
};

class DyteMobileClient : public KDyteMobileClient {
private:
  std::shared_ptr<DyteParticipantStore> participantStore;

#if 0
  void populateParticipants() {
    auto participants =
        KotlinAutoFree<TypeName(core_models, DyteRoomParticipants)>(
            DyteSdk.DyteMobileClient.get_participants(*this));
    auto list = KotlinAutoFree<libmobilecore_kref_kotlin_collections_List>(
        DyteSdk.models.DyteRoomParticipants.get_joined(participants));

    std::vector<kDyteJoinedMeetingParticipant> vec;
    auto cb = [](void *stableRef, void *userp) {
      auto ref = static_cast<DyteMobileClient *>(userp)
          ->GetParticipantStore()
          ->GetOrAddParticipant({stableRef});
    };

    DyteSdk.utils.ListForEach(
        list,
        reinterpret_cast<void *>(static_cast<void (*)(void *, void *)>(cb)),
        this);
  }
#endif
public:
  DyteMobileClient()
      : KDyteMobileClient(DyteSdk.DyteMeetingBuilder.build(
            KDyteMeetingBuilder(DyteSdk.DyteMeetingBuilder._instance()))),
        participantStore(std::make_shared<DyteParticipantStore>()) {}
  ~DyteMobileClient() {}

  bool Init(DyteMeetingInfoV2 &info) {
    auto wrappedCb = DyteSuccessFailureCb();
    DyteSdk.DyteMobileClient.init___(*this, info, wrappedCb.SuccessCb(),
                                     wrappedCb.FailureCb());

    return wrappedCb.Get();
  }

  std::shared_ptr<DyteParticipantStore> GetParticipantStore() {
    return participantStore;
  }

  void
  RegisterParticipantEventsListener(DyteParticipantEventsListener *listener) {
    DyteSdk.DyteMobileClient.addParticipantEventsListener(*this, *listener);
  }

  std::shared_ptr<DyteJoinedMeetingParticipant> GetLocalUser() {
    auto localUser = DyteSdk.DyteMobileClient.get_localUser(*this);
    DyteSdk.models.DyteSelfParticipant.enableAudio(localUser);

    return participantStore->GetOrAddParticipant({localUser.pinned});
  }

  bool JoinRoom() {
    auto wrappedCb = DyteSuccessFailureCb();
    DyteSdk.DyteMobileClient.joinRoom_(*this, wrappedCb.SuccessCb(),
                                       wrappedCb.FailureCb());

    return wrappedCb.Get();
  }
};

PYBIND11_MODULE(mobile, m) {
  py::class_<DyteMeetingInfoV2>(m, "DyteMeetingInfo")
      .def(py::init<const std::string &, bool, bool, const std::string &>());

  py::class_<DyteJoinedMeetingParticipant,
             std::shared_ptr<DyteJoinedMeetingParticipant>>(
      m, "DyteJoinedMeetingParticipant")
      .def("HasAudioTrack", &DyteJoinedMeetingParticipant::HasAudioTrack)
      .def("HasDataCb", &DyteJoinedMeetingParticipant::HasDataCb)
      .def("RegisterDataCb", &DyteJoinedMeetingParticipant::RegisterDataCb)
      .def("UnregisterDataCb", &DyteJoinedMeetingParticipant::UnregisterDataCb)
      .def("SendData", &DyteJoinedMeetingParticipant::SendData)
      .def("Id", &DyteJoinedMeetingParticipant::Id);

  py::class_<DyteParticipantStore, std::shared_ptr<DyteParticipantStore>>(
      m, "DyteParticipantStore");

  py::class_<DyteParticipantEventsListener>(m, "DyteParticipantEventsListener")
      .def(py::init<OnJoinCb, OnLeaveCb, OnAudioUpdateCb,
                    std::shared_ptr<DyteParticipantStore>>());

  py::class_<DyteMobileClient>(m, "DyteClient")
      .def(py::init<>())
      .def("Init", &DyteMobileClient::Init,
           py::call_guard<py::gil_scoped_release>())
      .def("RegisterParticipantEventsListener",
           &DyteMobileClient::RegisterParticipantEventsListener)
      .def("GetParticipantStore", &DyteMobileClient::GetParticipantStore)
      .def("GetLocalUser", &DyteMobileClient::GetLocalUser)
      .def("JoinRoom", &DyteMobileClient::JoinRoom,
           py::call_guard<py::gil_scoped_release>());
}
