#include "releaseShared/libmobilecore_api.h"
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
  bool callbackRegistered;

public:
  DyteJoinedMeetingParticipant(kDyteJoinedMeetingParticipant participant)
      : KDyteJoinedMeetingParticipant(participant){};
  ~DyteJoinedMeetingParticipant(){};

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

  void RegisterDataCb(AudioCb &cb) {
    std::lock_guard<std::mutex> lock(audioCbMutex);

    // Only register the C callback once
    if (!callbackRegistered) {
      auto track = DyteAudioStreamTrack(
          DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this));
      track.RegisterDataCb(&DyteJoinedMeetingParticipant::OnAudioData, this);

      callbackRegistered = true;
    }

    audioCb = std::make_shared<AudioCb>(cb);
  }

  void UnregisterDataCb(void) {
    std::lock_guard<std::mutex> lock(audioCbMutex);
    audioCb = nullptr;
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

class DyteParticipantEventsListener : public KDyteParticipantEventsListener {
private:
  OnJoinCb OnJoin;
  OnLeaveCb OnLeave;
  OnAudioUpdateCb OnAudioUpdate;

  // Map pointer to the participant's ID to the participant object
  // rather than the string contents as the ID pointer will remain
  // the same across callbacks, saving us conversions between std::string
  std::map<const std::string, std::shared_ptr<DyteJoinedMeetingParticipant>>
      map_;

  std::shared_ptr<DyteJoinedMeetingParticipant>
  getOrAddParticipant(kDyteJoinedMeetingParticipant participant_) {
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

  static void onJoin(kDyteJoinedMeetingParticipant participant_,
                     DyteParticipantEventsListener *self) {
    self->OnJoin(self->getOrAddParticipant(participant_));
  }

  static void onLeave(kDyteJoinedMeetingParticipant participant_,
                      DyteParticipantEventsListener *self) {
    auto participant = self->getOrAddParticipant(participant_);
    self->OnLeave(participant);
    self->map_.erase(participant->Id());
  }

  static void onAudioUpdate(bool enabled,
                            kDyteJoinedMeetingParticipant participant_,
                            DyteParticipantEventsListener *self) {
    self->OnAudioUpdate(enabled, self->getOrAddParticipant(participant_));
  }

public:
  DyteParticipantEventsListener(OnJoinCb onJoin, OnLeaveCb onLeave,
                                OnAudioUpdateCb onAudioUpdate)
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
        OnJoin(onJoin), OnLeave(onLeave), OnAudioUpdate(onAudioUpdate) {}
  ~DyteParticipantEventsListener() {}

  // Interface
  operator libmobilecore_kref_io_dyte_core_listeners_DyteParticipantEventsListener()
      const {
    return {obj.pinned};
  }
};

class DyteMobileClient : public KDyteMobileClient {
public:
  DyteMobileClient()
      : KDyteMobileClient(DyteSdk.DyteMeetingBuilder.build(
            KDyteMeetingBuilder(DyteSdk.DyteMeetingBuilder._instance()))) {}
  ~DyteMobileClient() {}

  bool Init(DyteMeetingInfoV2 &info) {
    auto wrappedCb = DyteSuccessFailureCb();
    DyteSdk.DyteMobileClient.init___(*this, info, wrappedCb.SuccessCb(),
                                     wrappedCb.FailureCb());

    return wrappedCb.Get();
  }

  void
  RegisterParticipantEventsListener(DyteParticipantEventsListener *listener) {
    DyteSdk.DyteMobileClient.addParticipantEventsListener(*this, *listener);
  }

  std::shared_ptr<DyteJoinedMeetingParticipant> GetLocalUser() {
    auto localUser = DyteSdk.DyteMobileClient.get_localUser(*this);
    DyteSdk.models.DyteSelfParticipant.enableAudio(localUser);
    return std::make_shared<DyteJoinedMeetingParticipant>(
        kDyteJoinedMeetingParticipant{localUser.pinned});
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
      .def("RegisterDataCb", &DyteJoinedMeetingParticipant::RegisterDataCb)
      .def("UnregisterDataCb", &DyteJoinedMeetingParticipant::UnregisterDataCb)
      .def("SendData", &DyteJoinedMeetingParticipant::SendData)
      .def("Id", &DyteJoinedMeetingParticipant::Id);

  py::class_<DyteParticipantEventsListener>(m, "DyteParticipantEventsListener")
      .def(py::init<OnJoinCb, OnLeaveCb, OnAudioUpdateCb>());

  py::class_<DyteMobileClient>(m, "DyteClient")
      .def(py::init<>())
      .def("Init", &DyteMobileClient::Init,
           py::call_guard<py::gil_scoped_release>())
      .def("RegisterParticipantEventsListener",
           &DyteMobileClient::RegisterParticipantEventsListener)
      .def("GetLocalUser", &DyteMobileClient::GetLocalUser)
      .def("JoinRoom", &DyteMobileClient::JoinRoom,
           py::call_guard<py::gil_scoped_release>());
}
