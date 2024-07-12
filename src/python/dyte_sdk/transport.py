import asyncio
import time

from loguru import logger
from pipecat.frames.frames import AudioRawFrame
from pipecat.processors.frame_processor import FrameProcessor
from pipecat.transports.base_input import BaseInputTransport
from pipecat.transports.base_output import BaseOutputTransport
from pipecat.transports.base_transport import BaseTransport, TransportParams

from .prebuilts import dyte_sdk as dyte


class DyteTransportParams(TransportParams):
    audio_out_enabled: bool = True
    audio_out_sample_rate: int = 16000
    audio_out_channels: int = 1
    audio_in_enabled: bool = True
    audio_in_sample_rate: int = 16000
    audio_in_channels: int = 2


class DyteInputTransport(BaseInputTransport):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def start_listening(self, participant):
        if participant.HasDataCb():
            logger.debug(
                f"Participant already has data callback, returning: {participant.Id()}"
            )
            return

        logger.debug(f"Start listening to participant {participant.Id()}")

        def cb(
            audio_data,
            bits_per_sample,
            sample_rate,
            number_of_channels,
            number_of_frames,
            absolute_capture_time_ms,
        ):
            coro = self.push_audio_frame(
                AudioRawFrame(
                    audio=audio_data,
                    sample_rate=sample_rate,
                    num_channels=number_of_channels,
                )
            )

            asyncio.run_coroutine_threadsafe(
                coro, self.get_event_loop()
            ).result()

        participant.RegisterDataCb(cb)

    def stop_listening(self, participant):
        logger.debug(f"Stop listening to participant {participant.Id()}")
        participant.UnregisterDataCb()


class DyteOutputTransport(BaseOutputTransport):
    def __init__(self, participant, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.participant = participant
        self.prev = time.time()

    async def write_raw_audio_frames(self, frames):
        while len(frames) != 0:
            ts_diff = time.time() - self.prev
            if ts_diff < 0.01:
                await asyncio.sleep(0.01 - ts_diff)
            self.prev = time.time()

            self.participant.SendData(frames, 16, 16000, 1, 160, -1)
            frames = frames[160 * 2 :]


class DyteTransport(BaseTransport):
    def __init__(
        self,
        api_base: str,
        token: str,
        loop: asyncio.AbstractEventLoop | None = None,
    ):
        super().__init__(loop=loop)

        self._client = dyte.DyteClient()
        meeting_info = dyte.DyteMeetingInfo(
            token,
            True,
            True,
            api_base,
        )
        if not self._client.Init(meeting_info):
            raise Exception("Failed to init client")

        self._listener = dyte.DyteParticipantEventsListener(
            self.on_join,
            self.on_leave,
            self.on_audio_update,
            self._client.GetParticipantStore(),
        )
        self._client.RegisterParticipantEventsListener(self._listener)

        self._input = None
        self._output = None

        self._params = DyteTransportParams()

        self._register_event_handler("on_join")
        self._register_event_handler("on_audio_update")
        self._register_event_handler("on_leave")

        self._input = DyteInputTransport(self._params)

        self._client.JoinRoom()

    def _is_participant_self(self, participant):
        return participant.Id() == self._client.GetLocalUser().Id()

    def input(self) -> FrameProcessor:
        return self._input

    def output(self) -> FrameProcessor:
        if not self._output:
            self._output = DyteOutputTransport(
                self._client.GetLocalUser(), self._params
            )
        return self._output

    def on_join(self, participant):
        if self._is_participant_self(participant):
            return

        self._loop.create_task(
            self._call_event_handler("on_join", participant)
        )

        logger.debug(f"On Join: {participant.Id()}")

    def on_audio_update(self, enabled, participant):
        if self._is_participant_self(participant):
            return

        self._loop.create_task(
            self._call_event_handler("on_audio_update", enabled, participant)
        )

        if enabled:
            if not participant.HasAudioTrack():
                logger.debug(
                    f"No audio track for participant: {participant.Id()}"
                )
                return

            self._input.start_listening(participant)
        else:
            self._input.stop_listening(participant)

    def on_leave(self, participant):
        if self._is_participant_self(participant):
            return

        self._loop.create_task(
            self._call_event_handler("on_leave", participant)
        )

        logger.debug(f"On Leave: {participant.Id()}")

        self._input.stop_listening(participant)
