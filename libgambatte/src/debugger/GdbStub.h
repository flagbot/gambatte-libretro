//
// Twili - Homebrew debug monitor for the Nintendo Switch
// Copyright (C) 2018 misson20000 <xenotoad@xenotoad.net>
//
// This file is part of Twili.
//
// Twili is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Twili is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Twili.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include <unordered_map>
#include <list>
#include "easylogging++.h"
#include "GdbConnection.h"
#include "Debugger.h"

namespace gambatte {
namespace debugger {

class GdbStub {
 public:
	GdbStub(Debugger* debugger, std::string address = "0.0.0.0", int port = 55555);
	~GdbStub();
	
    /// This method is responsible for opening and closing connections to remote clients.
	void Run();
    void AcceptConnection(int sockfd);
    void ConnectionLoop();
    void NotifyHalted(StopReason* reason);
	
	class Query {
	 public:
		Query(GdbStub &stub, std::string field, void (GdbStub::*visitor)(util::Buffer&), bool should_advertise=true, char separator=':');

		const GdbStub &stub;
		const std::string field;
		void (GdbStub::*const visitor)(util::Buffer&);
		const bool should_advertise;
		const char separator;
	};

	class XferObject {
	 public:
		virtual void Read(std::string annex, size_t offset, size_t length) = 0;
		virtual void Write(std::string annex, size_t offset, util::Buffer &data) = 0;
		virtual bool AdvertiseRead();
		virtual bool AdvertiseWrite();
	};

	class ReadOnlyStringXferObject : public XferObject {
	 public:
		ReadOnlyStringXferObject(GdbStub &stub, std::string (GdbStub::*generator)());
		virtual void Read(std::string annex, size_t offset, size_t length) override;
		virtual void Write(std::string annex, size_t offset, util::Buffer &data) override;
		virtual bool AdvertiseRead() override;
	 private:
		GdbStub &stub;
		std::string (GdbStub::*const generator)();
	};
    
    class FeaturesXferObject : public XferObject {
    public:
        FeaturesXferObject(GdbStub &stub);
        virtual void Read(std::string annex, size_t offset, size_t length) override;
        virtual void Write(std::string annex, size_t offset, util::Buffer &data) override;
        virtual bool AdvertiseRead() override;
    private:
        GdbStub &stub;
    };
	
	void AddFeature(std::string feature);
	void AddGettableQuery(Query query);
	void AddSettableQuery(Query query);
	void AddMultiletterHandler(std::string name, void (GdbStub::*handler)(util::Buffer&));
	void AddXferObject(std::string name, XferObject &ob);
	
	std::string stop_reason = "T05 swbreak:";
	bool waiting_for_stop = false;
	bool has_async_wait = false;
	bool multiprocess_enabled = false;
    
    Debugger* debugger;

	void Stop();
	
 private:
	GdbConnection* connection = nullptr;
    
    std::string address;
    int port;

	std::list<std::string> features;
	std::unordered_map<std::string, Query> gettable_queries;
	std::unordered_map<std::string, Query> settable_queries;
	std::unordered_map<std::string, void (GdbStub::*)(util::Buffer&)> multiletter_handlers;
	std::unordered_map<std::string, XferObject&> xfer_objects;
    
	// utilities
	void ReadThreadId(util::Buffer &buffer, int64_t &pid, int64_t &thread_id);
	
	// packets
	void HandleGeneralGetQuery(util::Buffer &packet);
	void HandleGeneralSetQuery(util::Buffer &packet);
	void HandleIsThreadAlive(util::Buffer &packet);
	void HandleMultiletterPacket(util::Buffer &packet);
	void HandleGetStopReason();
	void HandleDetach(util::Buffer &packet);
	void HandleReadGeneralRegisters();
	void HandleWriteGeneralRegisters(util::Buffer &packet);
	void HandleSetCurrentThread(util::Buffer &packet);
	void HandleReadMemory(util::Buffer &packet);
	void HandleWriteMemory(util::Buffer &packet);
    void HandleBreakpoint(util::Buffer &packet, bool remove = false);
	
	// multiletter packets
	void HandleVAttach(util::Buffer &packet);
	void HandleVContQuery(util::Buffer &packet);
	void HandleVCont(util::Buffer &packet);
    void HandleVStopped(util::Buffer& packet);
	
	// get queries
	void QueryGetSupported(util::Buffer &packet);
	void QueryGetCurrentThread(util::Buffer &packet);
	void QueryGetFThreadInfo(util::Buffer &packet);
	void QueryGetSThreadInfo(util::Buffer &packet);
	void QueryGetThreadExtraInfo(util::Buffer &packet);
	void QueryGetOffsets(util::Buffer &packet);
	void QueryGetRemoteCommand(util::Buffer &packet);
	void QueryXfer(util::Buffer &packet);
    void QueryGetTStatus(util::Buffer &packet);
	
	// set queries
	void QuerySetStartNoAckMode(util::Buffer &packet);
	void QuerySetThreadEvents(util::Buffer &packet);

	// xfer objects
	std::string XferReadLibraries();
	ReadOnlyStringXferObject xfer_libraries;
    FeaturesXferObject xfer_features;
	
	bool thread_events_enabled = false;
    bool sent_thread_info = false;
};

} // namespace debugger
} // namespace gambatte
