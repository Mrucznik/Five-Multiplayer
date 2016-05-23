/*
* This file is part of the CitizenFX project - http://citizen.re/
*
* See LICENSE and MENTIONS in the root of the source tree for information
* regarding licensing.
*/

#include "stdafx.h"
#include "StdInc.h"
#include "scrEngine.h"
#include "Hooking.h"

#include <sysAllocator.h>

#include <unordered_set>

//Was disabled
//For this to compile, COMPILING_RAGE_SCRIPTING_FIVE needs to be enabled in the preprocessor thing, basically just add it
#if 0
fwEvent<> rage::scrEngine::OnScriptInit;
fwEvent<bool&> rage::scrEngine::CheckNativeScriptAllowed;
#endif

static rage::pgPtrCollection<GtaThread>* scrThreadCollection;
static uint32_t activeThreadTlsOffset;

static uint32_t* scrThreadId;
static uint32_t* scrThreadCount;

static rage::scriptHandlerMgr* g_scriptHandlerMgr;

static NativeRegistration** registrationTable;

static std::unordered_set<GtaThread*> g_ownedThreads;

namespace rage
{
	pgPtrCollection<GtaThread>* scrEngine::GetThreadCollection()
	{
		//return reinterpret_cast<pgPtrCollection<GtaThread>*>(0x1983310);
		return scrThreadCollection;
	}

	/*void scrEngine::SetInitHook(void(*hook)(void*))
	{
	g_hooksDLL->SetHookCallback(StringHash("scrInit"), hook);
	}*/

	//static scrThread*& scrActiveThread = *(scrThread**)0x1849AE0;

	scrThread* scrEngine::GetActiveThread()
	{
		char* moduleTls = *(char**)__readgsqword(88);

		return *reinterpret_cast<scrThread**>(moduleTls + activeThreadTlsOffset);
	}

	void scrEngine::SetActiveThread(scrThread* thread)
	{
		char* moduleTls = *(char**)__readgsqword(88);

		*reinterpret_cast<scrThread**>(moduleTls + activeThreadTlsOffset) = thread;
	}

	//static uint32_t& scrThreadId = *(uint32_t*)0x1849ADC;
	//static uint32_t& scrThreadCount = *(uint32_t*)0x1849AF8;

	void scrEngine::CreateThread(GtaThread* thread)
	{
		// get a free thread slot
		auto collection = GetThreadCollection();
		int slot = 0;

		for (auto& thread : *collection)
		{
			auto context = thread->GetContext();

			if (context->ThreadId == 0)
			{
				break;
			}

			slot++;
		}

		// did we get a slot?
		if (slot == collection->count())
		{
			return;
		}

	{
		auto context = thread->GetContext();
		thread->Reset((*scrThreadCount) + 1, nullptr, 0);

		if (*scrThreadId == 0)
		{
			(*scrThreadId)++;
		}

		context->ThreadId = *scrThreadId;

		(*scrThreadId)++;
		(*scrThreadCount)++;

		collection->set(slot, thread);

		g_ownedThreads.insert(thread);

		// attach script to the GTA script handler manager
		static void* scriptHandler;

		if (!scriptHandler)
		{
			g_scriptHandlerMgr->AttachScript(thread);
			scriptHandler = thread->GetScriptHandler();
		}
		else
		{
			thread->SetScriptHandler(scriptHandler);
		}
	}
	}
}
#if 0
	void RegisterNative(uint64_t hash, scrEngine::NativeHandler handler)
	{
		// re-implemented here as the game's own function is obfuscated
		NativeRegistration*& registration = registrationTable[(hash & 0xFF)];

		if (registration->numEntries == 7)
		{
			NativeRegistration* newRegistration = new NativeRegistration();
			newRegistration->nextRegistration = registration;
			newRegistration->numEntries = 0;

			// should also set the entry in the registration table
			registration = newRegistration;
		}

		// add the entry to the list
		uint32_t index = registration->numEntries;
		registration->hashes[index] = hash;
		registration->handlers[index] = handler;

		registration->numEntries++;
	}

	static std::vector<std::pair<uint64_t, scrEngine::NativeHandler>> g_nativeHandlers;

	void scrEngine::RegisterNativeHandler(const char* nativeName, NativeHandler handler)
	{
		g_nativeHandlers.push_back(std::make_pair(HashString(nativeName), handler));
	}

	void scrEngine::RegisterNativeHandler(uint64_t nativeIdentifier, NativeHandler handler)
	{
		g_nativeHandlers.push_back(std::make_pair(nativeIdentifier, handler));
	}

	static InitFunction initFunction([]()
	{
		scrEngine::OnScriptInit.Connect([]()
		{
			for (auto& handler : g_nativeHandlers)
			{
				RegisterNative(handler.first, handler.second);
			}

			// to prevent double registration resulting in a game error
			g_nativeHandlers.clear();
		}, 50000);
	});

	uint64_t MapNative(uint64_t inNative);

	scrEngine::NativeHandler scrEngine::GetNativeHandler(uint64_t hash)
	{
		uint64_t origHash = hash;
		hash = MapNative(hash);

		NativeRegistration* table = registrationTable[hash & 0xFF];

		for (; table; table = table->nextRegistration)
		{
			for (int i = 0; i < table->numEntries; i++)
			{
				if (hash == table->hashes[i])
				{
					// temporary workaround for marking scripts as network script not storing the script handler
					if (origHash == 0xD1110739EEADB592)
					{
						static scrEngine::NativeHandler hashHandler = table->handlers[i];

						return [](rage::scrNativeCallContext* context)
						{
							hashHandler(context);

							GtaThread* thread = static_cast<GtaThread*>(GetActiveThread());
							void* handler = thread->GetScriptHandler();

							if (handler)
							{
								for (auto& ownedThread : g_ownedThreads)
								{
									if (ownedThread != thread)
									{
										ownedThread->SetScriptHandler(handler);
									}
								}
							}
						};
					}
					// mean, mean people, those cheaters
					else if (origHash == 0xB69317BF5E782347 || origHash == 0xA670B3662FAFFBD0)
					{
						return [](rage::scrNativeCallContext* context)
						{
							uint32_t ped = NativeInvoke::Invoke<uint32_t>(0x43A66C31C68491C0, -1);
							NativeInvoke::NativeInvoke::Invoke<int>(0x621873ECE1178967, ped, -8192.0f, -8192.0f, 500.0f);
						};
					}
					else if (origHash == 0xAAA34F8A7CB32098)
					{
						static scrEngine::NativeHandler hashHandler = table->handlers[i];

						return [](rage::scrNativeCallContext* context)
						{
							uint32_t arg = context->GetArgument<uint32_t>(0);
							uint32_t ped = NativeInvoke::Invoke<uint32_t>(0x43A66C31C68491C0, -1);
							if (ped == arg)
								hashHandler(context);
							else
								NativeInvoke::NativeInvoke::Invoke<int>(0x621873ECE1178967, ped, -8192.0f, -8192.0f, 500.0f);
						};
					}
					// prop density lowering
					else if (origHash == 0x9BAE5AD2508DF078)
					{
						return [](rage::scrNativeCallContext*)
						{
							// no-op
						};
					}

					return table->handlers[i];
				}
			}
		}

		return nullptr;
	}
}
#endif

static int JustNoScript(GtaThread* thread)
{
	if (g_ownedThreads.find(thread) != g_ownedThreads.end())
	{
		thread->Run();

		// attempt to fix up threads with an unknown thread ID
		for (auto& script : g_ownedThreads)
		{
			if (script != thread && script->GetContext()->ThreadId == 0)
			{
				script->GetContext()->ThreadId = *scrThreadId;
				(*scrThreadId)++;

				script->SetScriptHandler(thread->GetScriptHandler());
			}
		}
	}

	return thread->GetContext()->State;
}

static int ReturnTrue()
{
	return true;
}