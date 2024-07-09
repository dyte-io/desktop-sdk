import asyncio
import os

import aiohttp
from dyte_sdk.transport import DyteTransport
from pipecat.frames.frames import (
    LLMMessagesFrame,
    TranscriptionFrame,
    UserStartedSpeakingFrame,
    UserStoppedSpeakingFrame
)
from pipecat.pipeline.pipeline import Pipeline
from pipecat.pipeline.runner import PipelineRunner
from pipecat.pipeline.task import PipelineTask
from pipecat.processors.aggregators.llm_response import (
    LLMAssistantResponseAggregator,
    LLMUserResponseAggregator
)
from pipecat.services.ai_services import AIService
from pipecat.services.deepgram import DeepgramSTTService, LiveOptions
from pipecat.services.elevenlabs import ElevenLabsTTSService
from pipecat.services.openai import OpenAILLMService


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
        transport = DyteTransport("preprod.dyte.io", token)

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
