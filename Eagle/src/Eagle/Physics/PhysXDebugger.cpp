#include "egpch.h"
#include "PhysXDebugger.h"
#include "PhysXInternal.h"

namespace Eagle
{
	struct PhysXDebuggerData
	{
		physx::PxPvd* Debugger = nullptr;
		physx::PxPvdTransport* Transport = nullptr;
	};

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
		if (!s_DebuggerData)
			return;

		StopDebugging();
		s_DebuggerData->Debugger->release();
		delete s_DebuggerData;
		s_DebuggerData = nullptr;
	}
	
	void PhysXDebugger::StartDebugging(const Path& filepath, bool networkDebugging)
	{
		if (!s_DebuggerData)
			return;

		StopDebugging();

		if (networkDebugging)
		{
			s_DebuggerData->Transport = physx::PxDefaultPvdSocketTransportCreate("localhost", 5425, 1000);
			s_DebuggerData->Debugger->connect(*s_DebuggerData->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
		else
		{
			s_DebuggerData->Transport = physx::PxDefaultPvdFileTransportCreate(Utils::AsString(filepath / ".pxd2").c_str());
			s_DebuggerData->Debugger->connect(*s_DebuggerData->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
	}
	
	bool PhysXDebugger::IsDebugging()
	{
		return s_DebuggerData && s_DebuggerData->Debugger->isConnected();
	}

	void PhysXDebugger::StopDebugging()
	{
		if (!s_DebuggerData)
			return;

		if (IsDebugging())
			s_DebuggerData->Debugger->disconnect();

		if (s_DebuggerData->Transport)
		{
			s_DebuggerData->Transport->release();
			s_DebuggerData->Transport = nullptr;
		}
	}
	
	physx::PxPvd* PhysXDebugger::GetDebugger()
	{
		return s_DebuggerData ? s_DebuggerData->Debugger : nullptr;
	}
}
