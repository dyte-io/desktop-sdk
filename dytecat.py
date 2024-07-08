import asyncio
import os
import time

import aiohttp
from pipecat.frames.frames import (
    AudioRawFrame,
    LLMMessagesFrame,
    TranscriptionFrame,
    UserStartedSpeakingFrame,
    UserStoppedSpeakingFrame,
)
from pipecat.pipeline.pipeline import Pipeline
from pipecat.pipeline.runner import PipelineRunner
from pipecat.pipeline.task import PipelineTask
from pipecat.processors.aggregators.llm_response import (
    LLMAssistantResponseAggregator,
    LLMUserResponseAggregator,
)
from pipecat.processors.frame_processor import FrameProcessor
from pipecat.services.ai_services import AIService
from pipecat.services.deepgram import DeepgramSTTService, LiveOptions
from pipecat.services.elevenlabs import ElevenLabsTTSService
from pipecat.services.openai import OpenAILLMService
from pipecat.transports.base_input import BaseInputTransport
from pipecat.transports.base_output import BaseOutputTransport
from pipecat.transports.base_transport import BaseTransport, TransportParams

import mobile


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
            return

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
        self, token: str, loop: asyncio.AbstractEventLoop | None = None
    ):
        super().__init__(loop=loop)

        self._client = mobile.DyteClient()
        meeting_info = mobile.DyteMeetingInfo(
            token, True, True, "preprod.dyte.io"
        )
        if not self._client.Init(meeting_info):
            raise Exception("Failed to init client")

        self._listener = mobile.DyteParticipantEventsListener(
            self.on_join,
            self.on_leave,
            self.on_audio_update,
        )
        self._client.RegisterParticipantEventsListener(self._listener)

        self._input = None
        self._output = None

        self._params = DyteTransportParams()

        self._register_event_handler("on_join")
        self._register_event_handler("on_audio_update")
        self._register_event_handler("on_leave")

        self._client.JoinRoom()

    def _is_participant_self(self, participant):
        return participant.Id() == self._client.GetLocalUser().Id()

    def input(self) -> FrameProcessor:
        if not self._input:
            self._input = DyteInputTransport(self._params)
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

        if participant.HasAudioTrack():
            self._input.start_listening(participant)

    def on_audio_update(self, enabled, participant):
        if not self._input:
            return

        if self._is_participant_self(participant):
            return

        self._loop.create_task(
            self._call_event_handler("on_audio_update", enabled, participant)
        )

        if enabled:
            self._input.start_listening(participant)
        else:
            self._input.stop_listening(participant)

    def on_leave(self, participant):
        if self._is_participant_self(participant):
            return

        self._loop.create_task(
            self._call_event_handler("on_leave", participant)
        )

        self._input.stop_listening(participant)


# Wrap transcript frames with {Started,Stopped}Speaking frames
class TranscriptFakeVAD(AIService):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    async def process_frame(self, frame, direction):
        await super().process_frame(frame, direction)

        if isinstance(frame, TranscriptionFrame):
            await self.push_frame(UserStartedSpeakingFrame(), direction)
            await self.push_frame(frame, direction)
            await self.push_frame(UserStoppedSpeakingFrame(), direction)
        else:
            await self.push_frame(frame, direction)


async def main():
    async with aiohttp.ClientSession() as session:
        token = os.environ["DYTE_AUTH_TOKEN"]
        transport = DyteTransport(token)

        pipeline = Pipeline(
            [
                transport.input(),
                DeepgramSTTService(
                    api_key=os.environ["DEEPGRAM_API_KEY"],
                    live_options=LiveOptions(
                        encoding="linear16",
                        language="en-US",
                        model="nova-2-conversationalai",
                        sample_rate=24000,
                        channels=2,
                        interim_results=False,
                        smart_format=True,
                    ),
                ),
                TranscriptFakeVAD(),
                LLMUserResponseAggregator(),
                OpenAILLMService(
                    api_key=os.environ["OPENAI_API_KEY"], model="gpt-4o"
                ),
                ElevenLabsTTSService(
                    aiohttp_session=session,
                    api_key=os.environ["ELEVENLABS_API_KEY"],
                    voice_id=os.environ["ELEVENLABS_VOICE_ID"],
                ),
                transport.output(),
                LLMAssistantResponseAggregator(),
            ]
        )

        runner = PipelineRunner()
        task = PipelineTask(pipeline)

        sent_intro = False

        @transport.event_handler("on_join")
        async def on_join(transport, participant):
            nonlocal sent_intro

            if sent_intro:
                return

            sent_intro = True

            messages = [
                {
                    "role": "system",
                    "content": "You are Chatbot, a friendly, helpful robot. Your goal is to demonstrate your capabilities in a succinct way. Your output will be converted to audio so don't include special characters in your answers. Respond to what the user said in a creative and helpful way, but keep your responses brief. Start by introducing yourself.",
                }
            ]

            await task.queue_frames([LLMMessagesFrame(messages)])

        await runner.run(task)


asyncio.run(main())
