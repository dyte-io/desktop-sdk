#include "releaseShared/libmobilecore_api.h"
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
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
  std::optional<AudioCb> audioCb;

public:
  DyteJoinedMeetingParticipant(kDyteJoinedMeetingParticipant participant)
      : KDyteJoinedMeetingParticipant(participant){};
  ~DyteJoinedMeetingParticipant(){};

  static void OnAudioData(const char *audio_data, int bits_per_sample,
                          int sample_rate, size_t number_of_channels,
                          size_t number_of_frames,
                          int64_t absolute_capture_timestamp_ms, void *userp) {
    auto self = static_cast<DyteJoinedMeetingParticipant *>(userp);
    pybind11::gil_scoped_acquire _;

    self->audioCb.value()(
        pybind11::bytes(audio_data, number_of_channels * number_of_frames * 2),
        bits_per_sample, sample_rate, number_of_channels, number_of_frames,
        absolute_capture_timestamp_ms);
  }

  void RegisterDataCb(AudioCb &cb) {
    audioCb.emplace(cb);

    auto track = DyteAudioStreamTrack(
        DyteSdk.models.DyteJoinedMeetingParticipant.get_audioTrack(*this));
    track.RegisterDataCb(&DyteJoinedMeetingParticipant::OnAudioData, this);
  }
};

class DyteParticipantEventsListener : public KDyteParticipantEventsListener {
private:
  static void onJoin(kDyteJoinedMeetingParticipant participant_,
                     DyteParticipantEventsListener *self) {
    self->OnJoin(std::make_shared<DyteJoinedMeetingParticipant>(participant_));
  }

  static void onLeave(kDyteJoinedMeetingParticipant participant_,
                      DyteParticipantEventsListener *self) {
    self->OnLeave(std::make_shared<DyteJoinedMeetingParticipant>(participant_));
  }

  static void onAudioUpdate(bool enabled,
                            kDyteJoinedMeetingParticipant participant_,
                            DyteParticipantEventsListener *self) {
    self->OnAudioUpdate(
        enabled, std::make_shared<DyteJoinedMeetingParticipant>(participant_));
  }

public:
  DyteParticipantEventsListener()
      : KDyteParticipantEventsListener(
            DyteSdk.utils.DyteParticipantEventsListener
                .DyteParticipantEventsListener(
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onJoin),
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onLeave),
                    reinterpret_cast<void *>(
                        &DyteParticipantEventsListener::onAudioUpdate),
                    this)) {}
  ~DyteParticipantEventsListener() {}

  virtual void OnJoin(std::shared_ptr<DyteJoinedMeetingParticipant>) = 0;
  virtual void OnLeave(std::shared_ptr<DyteJoinedMeetingParticipant>) = 0;
  virtual void OnAudioUpdate(bool,
                             std::shared_ptr<DyteJoinedMeetingParticipant>) = 0;

  // Interface
  operator libmobilecore_kref_io_dyte_core_listeners_DyteParticipantEventsListener()
      const {
    return {obj.pinned};
  }
};

class PyDyteParticipantEventsListener : public DyteParticipantEventsListener {
public:
  void
  OnJoin(std::shared_ptr<DyteJoinedMeetingParticipant> participant) override {
    PYBIND11_OVERRIDE_PURE(void, DyteParticipantEventsListener, OnJoin,
                           participant);
  }

  void
  OnLeave(std::shared_ptr<DyteJoinedMeetingParticipant> participant) override {
    PYBIND11_OVERRIDE_PURE(void, DyteParticipantEventsListener, OnLeave,
                           participant);
  }

  void OnAudioUpdate(
      bool enabled,
      std::shared_ptr<DyteJoinedMeetingParticipant> participant) override {
    PYBIND11_OVERRIDE_PURE(void, DyteParticipantEventsListener, OnAudioUpdate,
                           enabled, participant);
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
      .def("RegisterDataCb", &DyteJoinedMeetingParticipant::RegisterDataCb);

  py::class_<DyteParticipantEventsListener, PyDyteParticipantEventsListener>(
      m, "DyteParticipantEventsListener")
      .def(py::init<>())
      .def("OnJoin", &DyteParticipantEventsListener::OnJoin);

  py::class_<DyteMobileClient>(m, "DyteClient")
      .def(py::init<>())
      .def("Init", &DyteMobileClient::Init,
           py::call_guard<py::gil_scoped_release>())
      .def("RegisterParticipantEventsListener",
           &DyteMobileClient::RegisterParticipantEventsListener)
      .def("JoinRoom", &DyteMobileClient::JoinRoom,
           py::call_guard<py::gil_scoped_release>());
}
