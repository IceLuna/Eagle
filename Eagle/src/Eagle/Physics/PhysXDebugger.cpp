#include "egpch.h"
#include "PhysXDebugger.h"
#include "PhysXInternal.h"

namespace Eagle
{
	struct PhysXDebuggerData
	{
		physx::PxPvd* Debugger;
		physx::PxPvdTransport* Transport;
	};

#ifdef EG_DEBUG
	static PhysXDebuggerData* s_DebuggerData = nullptr;
	
	void PhysXDebugger::Init()
	{
		EG_CORE_ASSERT(!s_DebuggerData, "Trying to init debugger twice!");
		s_DebuggerData = new PhysXDebuggerData();

		s_DebuggerData->Debugger = PxCreatePvd(PhysXInternal::GetFoundation());
		EG_CORE_ASSERT(s_DebuggerData->Debugger, "Failed to create Pvd");
	}
	
	void PhysXDebugger::Shutdown()
	{
		s_DebuggerData->Debugger->release();
		delete s_DebuggerData;
		s_DebuggerData = nullptr;
	}
	
	void PhysXDebugger::StartDebugging(const Path& filepath, bool networkDebugging)
	{
		StopDebugging();

		if (networkDebugging)
		{
			s_DebuggerData->Transport = physx::PxDefaultPvdSocketTransportCreate("localhost", 5425, 1000);
			s_DebuggerData->Debugger->connect(*s_DebuggerData->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
		else
		{
			s_DebuggerData->Transport = physx::PxDefaultPvdFileTransportCreate((filepath / ".pxd2").u8string().c_str());
			s_DebuggerData->Debugger->connect(*s_DebuggerData->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
	}
	
	bool PhysXDebugger::IsDebugging()
	{
		return s_DebuggerData->Debugger->isConnected();
	}
	
	void PhysXDebugger::StopDebugging()
	{
		if (!IsDebugging())
			return;

		s_DebuggerData->Debugger->disconnect();
		s_DebuggerData->Transport->release();
	}
	
	physx::PxPvd* PhysXDebugger::GetDebugger()
	{
		return s_DebuggerData->Debugger;
	}

#else
	void PhysXDebugger::Init() {}
	void PhysXDebugger::Shutdown() {}
	void PhysXDebugger::StartDebugging(const Path& filepath, bool networkDebugging) {}
	bool PhysXDebugger::IsDebugging() { return false; }
	void PhysXDebugger::StopDebugging() {}
	physx::PxPvd* PhysXDebugger::GetDebugger() { return nullptr; }
#endif
}
