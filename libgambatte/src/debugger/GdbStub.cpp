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

#include "GdbStub.h"
#include "Debugger.h"
#include "cpu.h"

#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <sstream>


namespace gambatte {
namespace debugger {

#pragma mark Public Methods

GdbStub::GdbStub(Debugger* debugger, std::string address, int port) :
    debugger(debugger),
    address(address),
    port(port),
    xfer_libraries(*this, &GdbStub::XferReadLibraries),
    xfer_features(*this)
{
	AddGettableQuery(Query(*this, "Supported", &GdbStub::QueryGetSupported, false));
	AddGettableQuery(Query(*this, "C", &GdbStub::QueryGetCurrentThread, false));
	AddGettableQuery(Query(*this, "fThreadInfo", &GdbStub::QueryGetFThreadInfo, false));
	AddGettableQuery(Query(*this, "sThreadInfo", &GdbStub::QueryGetSThreadInfo, false));
	AddGettableQuery(Query(*this, "ThreadExtraInfo", &GdbStub::QueryGetThreadExtraInfo, false, ','));
    AddGettableQuery(Query(*this, "TStatu",
                           &GdbStub::QueryGetTStatus, false));
	AddGettableQuery(Query(*this, "Offsets", &GdbStub::QueryGetOffsets, false));
	AddGettableQuery(Query(*this, "Rcmd", &GdbStub::QueryGetRemoteCommand, false, ','));
	AddGettableQuery(Query(*this, "Xfer", &GdbStub::QueryXfer, false));
	AddSettableQuery(Query(*this, "StartNoAckMode", &GdbStub::QuerySetStartNoAckMode));
	AddSettableQuery(Query(*this, "ThreadEvents", &GdbStub::QuerySetThreadEvents));
	AddMultiletterHandler("Attach", &GdbStub::HandleVAttach);
	AddMultiletterHandler("Cont?", &GdbStub::HandleVContQuery);
	AddMultiletterHandler("Cont", &GdbStub::HandleVCont);
    AddMultiletterHandler("Stopped", &GdbStub::HandleVStopped);
	AddXferObject("features", xfer_features);
        
    AddFeature("PacketSize=8192");
    AddFeature("swbreak+");
//        AddFeature("hwbreak+");
}

GdbStub::~GdbStub() {
    
}

void GdbStub::Run() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG(ERROR) << "error opening socket: " << sockfd;
        return;
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        LOG(ERROR) << "setsockopt failed";
    }
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        LOG(ERROR) << "error binding socket: ";
        close(sockfd);
        return;
    }
    
    LOG(INFO) << "Listening on *:" << port << " for incoming connections";
    
    listen(sockfd, 5);

    while (true) {
        AcceptConnection(sockfd);
    }
}

void GdbStub::AcceptConnection(int sockfd)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int clientfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    
    if (clientfd < 0) {
        LOG(ERROR) << "had error accepting client connection: " << clientfd;
        return;
    }
    
    char* client_addr = inet_ntoa(cli_addr.sin_addr);
    
    LOG(INFO) << "Accepted connection from " << client_addr << ":" << cli_addr.sin_port;
            
    this->debugger->Halt(StopReason::BPReason);
    
    this->connection = new GdbConnection(clientfd);
    
    while (this->connection->connection_alive) {
        ConnectionLoop();
    }
    
    LOG(INFO) << "Connection from " << client_addr << ":" << cli_addr.sin_port << " died";
    
    this->debugger->Unhalt();
    
    delete this->connection;
    this->connection = nullptr;
}

void GdbStub::ConnectionLoop()
{
    util::Buffer *buffer;
    bool interrupted;
    while((buffer = this->connection->Process(interrupted)) != nullptr) {
        LOG(DEBUG) << "got message (" << buffer->ReadAvailable() << " bytes)";
        char ident;
        if(!buffer->Read(ident)) {
            LOG(DEBUG) << "invalid packet (zero-length?)";
            this->connection->SignalError();
            return;
        }
        LOG(DEBUG) << "got packet, ident: " << ident;
        switch(ident) {
        case '!': // extended mode
            this->connection->RespondOk();
            break;
        case '?': // stop reason
            this->HandleGetStopReason();
            break;
        case 'D': // detach
            this->HandleDetach(*buffer);
            break;
        case 'g': // read general registers
            this->HandleReadGeneralRegisters();
            break;
        case 'G': // write general registers
            this->HandleWriteGeneralRegisters(*buffer);
            break;
        case 'H': // set current thread
            this->HandleSetCurrentThread(*buffer);
            break;
        case 'm': // read memory
            this->HandleReadMemory(*buffer);
            break;
        case 'M': // write memory
            this->HandleWriteMemory(*buffer);
            break;
        case 'q': // general get query
            this->HandleGeneralGetQuery(*buffer);
            break;
        case 'Q': // general set query
            this->HandleGeneralSetQuery(*buffer);
            break;
        case 'T': // is thread alive
            this->HandleIsThreadAlive(*buffer);
            break;
        case 'v': // variable
            this->HandleMultiletterPacket(*buffer);
            break;
        case 'Z': // Insert Breakpoint
            this->HandleBreakpoint(*buffer);
            break;
        case 'z': // Delete Breakpoint
            this->HandleBreakpoint(*buffer, true);
            break;
        default:
            LOG(INFO) << "unrecognized packet: " << ident << "rest: " << buffer->GetString();
            this->connection->RespondEmpty();
            break;
        }
    }
    
    if (interrupted) {
        this->debugger->Halt(StopReason::BPReason);
    }
    
    if (this->waiting_for_stop && this->debugger->stop_reason != nullptr) {
        this->Stop();
    }
    
    this->connection->ReadInput();
}

GdbStub::Query::Query(GdbStub &stub, std::string field, void (GdbStub::*visitor)(util::Buffer&), bool should_advertise, char separator) :
	stub(stub),
	field(field),
	visitor(visitor),
	should_advertise(should_advertise),
	separator(separator) {
}

void GdbStub::NotifyHalted(StopReason *reason)
{
    if (connection != nullptr) {
        this->Stop();
    }
}

#pragma mark Handlers Initializers

void GdbStub::AddFeature(std::string feature) {
	features.push_back(feature);
}

void GdbStub::AddGettableQuery(Query query) {
	gettable_queries.emplace(query.field, query);

	if(query.should_advertise) {
		features.push_back(std::string("q") + query.field + "+");
	}
}

void GdbStub::AddSettableQuery(Query query) {
	settable_queries.emplace(query.field, query);

	if(query.should_advertise) {
		features.push_back(std::string("Q") + query.field + "+");
	}
}

void GdbStub::AddMultiletterHandler(std::string name, void (GdbStub::*handler)(util::Buffer&)) {
	multiletter_handlers.emplace(name, handler);
}

void GdbStub::AddXferObject(std::string name, XferObject &object) {
	xfer_objects.emplace(name, object);
	if(object.AdvertiseRead()) {
		features.push_back(std::string("qXfer:") + name + ":read+");
	}
	if(object.AdvertiseWrite()) {
		features.push_back(std::string("qXfer:") + name + ":write+");
	}
}

void GdbStub::Stop() {
    if (waiting_for_stop) {
        waiting_for_stop = false;
        HandleGetStopReason(); // send reason
    }
}

void GdbStub::ReadThreadId(util::Buffer &packet, int64_t &pid, int64_t &thread_id) {
	pid = 0;
	
	char ch;
	if(packet.ReadAvailable() && packet.Read()[0] == 'p') { // peek
		packet.MarkRead(1); // consume
		if(packet.ReadAvailable() && packet.Read()[0] == '-') { // all processes
			pid = -1;
			packet.MarkRead(1); // consume
		} else {
			uint64_t dec_pid;
			GdbConnection::DecodeWithSeparator(dec_pid, '.', packet);
			pid = dec_pid;
		}
	}

	if(packet.ReadAvailable() && packet.Read()[0] == '-') { // all threads
		thread_id = -1;
		packet.MarkRead(1); // consume
	} else {
		uint64_t dec_thread_id;
		GdbConnection::Decode(dec_thread_id, packet);
		thread_id = dec_thread_id;
	}
}

#pragma mark General Handlers

void GdbStub::HandleGeneralGetQuery(util::Buffer &packet) {
	std::string field;
	char ch;
	std::unordered_map<std::string, Query>::iterator i = gettable_queries.end();
	while(packet.Read(ch)) {
		if(i != gettable_queries.end() && ch == i->second.separator) {
			break;
		}
		field.push_back(ch);
		i = gettable_queries.find(field);
	}
	LOG(DEBUG) << "got get query for " << field.c_str();

	if(i != gettable_queries.end()) {
		(this->*(i->second.visitor))(packet);
	} else {
		LOG(INFO) << "unsupported query: " << field.c_str();
		connection->RespondEmpty();
	}
}

void GdbStub::HandleGeneralSetQuery(util::Buffer &packet) {
	std::string field;
	char ch;
	std::unordered_map<std::string, Query>::iterator i = settable_queries.end();
	while(packet.Read(ch)) {
		if(i != settable_queries.end() && ch == i->second.separator) {
			break;
		}
		field.push_back(ch);
		i = settable_queries.find(field);
	}
	LOG(DEBUG) << "got set query for " << field.c_str();

	if(i != settable_queries.end()) {
		(this->*(i->second.visitor))(packet);
	} else {
		LOG(INFO) << "unsupported query: " << field.c_str();
		connection->RespondEmpty();
	}
}

void GdbStub::HandleIsThreadAlive(util::Buffer &packet) {
	connection->RespondOk();
}

void GdbStub::HandleMultiletterPacket(util::Buffer &packet) {
	std::string title;
	char ch;
	while(packet.Read(ch) && ch != ';') {
		title.push_back(ch);
	}
	LOG(DEBUG) << "got v" << title.c_str();

	auto i = multiletter_handlers.find(title);
	if(i != multiletter_handlers.end()) {
		(this->*(i->second))(packet);
	} else {
		LOG(INFO) << "unsupported v: " << title.c_str();
		connection->RespondEmpty();
	}
}

void GdbStub::HandleGetStopReason() {
	util::Buffer buf;
    if (debugger->stop_reason == nullptr) {
        connection->RespondOk();
    } else {
        debugger->stop_reason->Encode(buf);
        connection->Respond(buf);
    }
}

void GdbStub::HandleDetach(util::Buffer &packet) {
	char ch;
	stop_reason = "W00";
	connection->RespondOk();
}

void GdbStub::HandleReadGeneralRegisters() {
    util::Buffer response;
    debugger->EncodeRegisters(response);
    
    LOG(INFO) << "Responding with register contents: " << response.GetString();
    
    connection->Respond(response);
}

void GdbStub::HandleWriteGeneralRegisters(util::Buffer &packet) {
    // TODO: Handle This!
}

void GdbStub::HandleSetCurrentThread(util::Buffer &packet) {
	if(packet.ReadAvailable() < 2) {
		LOG(WARNING) << "invalid thread id";
		connection->RespondError(1);
		return;
	}

	char op;
	if(!packet.Read(op)) {
		LOG(WARNING) << "invalid H packet";
		connection->RespondError(1);
		return;
	}

	int64_t pid, thread_id;
	ReadThreadId(packet, pid, thread_id);
	
	connection->RespondOk();
}

void GdbStub::HandleReadMemory(util::Buffer &packet) {
	uint64_t address, size;
	GdbConnection::DecodeWithSeparator(address, ',', packet);
	GdbConnection::Decode(size, packet);
    
    util::Buffer response;
    debugger->EncodeMemory(response, address, size);
    connection->Respond(response);
}

void GdbStub::HandleWriteMemory(util::Buffer &packet) {
	uint64_t address, size;
	GdbConnection::DecodeWithSeparator(address, ',', packet);
	GdbConnection::DecodeWithSeparator(size, ':', packet);
    
    debugger->WriteMemory(address, size, packet);
    connection->RespondOk();
}

void GdbStub::HandleBreakpoint(util::Buffer &packet, bool remove)
{
    uint64_t type, address, kind;
    GdbConnection::DecodeWithSeparator(type, ',', packet);
    GdbConnection::DecodeWithSeparator(address, ',', packet);
    GdbConnection::DecodeWithSeparator(kind, ';', packet);
    
    LOG(DEBUG) << "Handle Breakpoint type: " << type << ", address: " << address << ", kind: " << kind;
    
    if (remove) {
        debugger->RemoveBreakpoints(address);
    } else {
        debugger->AddBreakpoint(new Breakpoint(address, -1));
    }
    
    connection->RespondOk();
}

#pragma mark V Handlers

void GdbStub::HandleVAttach(util::Buffer &packet) {
	uint64_t pid = 0;
	char ch;
	while(packet.Read(ch)) {
		pid<<= 4;
		pid|= GdbConnection::DecodeHexNybble(ch);
	}
	LOG(DEBUG) << "decoded PID: " << pid;

	// TODO: Handle this!
	
	// ok
	HandleGetStopReason();
}

void GdbStub::HandleVContQuery(util::Buffer &packet) {
	util::Buffer response;
	response.Write(std::string("vCont;c;C"));
	connection->Respond(response);
}

void GdbStub::HandleVCont(util::Buffer &packet) {
	char ch;
	util::Buffer action_buffer;
	util::Buffer thread_id_buffer;
	bool reading_action = true;
	bool read_success = true;

	struct Action {
		enum class Type {
			Invalid,
			Continue,
            Step
		} type = Type::Invalid;
        int64_t pid = -1;
        int64_t thread_id = -1;
	};

	std::map<uint64_t, std::map<uint64_t, Action>> process_actions;
    
    bool unhalt = false;
	
	while(read_success) {
		read_success = packet.Read(ch);
		if(!read_success || ch == ';') {
			if(!action_buffer.Read(ch)) {
				LOG(WARNING) << "invalid vCont action: too small";
				connection->RespondError(1);
				return;
			}
			
			Action action;
			switch(ch) {
			case 'C':
				LOG(WARNING) << "vCont 'C' action not well supported";
				// fall-through
			case 'c':
				action.type = Action::Type::Continue;
				break;
            case 's':
                action.type = Action::Type::Step;
                break;
			default:
				LOG(WARNING) << "unsupported vCont action: " << ch;
			}

			if(action.type != Action::Type::Invalid) {
				if(thread_id_buffer.ReadAvailable()) {
					ReadThreadId(thread_id_buffer, action.pid, action.thread_id);
				}
				LOG(DEBUG) << "vCont " << action.pid << ", " << action.thread_id << ", action " << ch;
			}
            
            switch (action.type) {
                case Action::Type::Step:
                    debugger->step_range = std::make_tuple(debugger->cpu->correct_pc, debugger->cpu->correct_pc);
                    break;
                default:
                    break;
            }
            
            unhalt = true;
			
			reading_action = true;
			action_buffer.Clear();
			thread_id_buffer.Clear();
			continue;
		}
		if(ch == ':') {
			reading_action = false;
			continue;
		}
		if(reading_action) {
			action_buffer.Write(ch);
		} else {
			thread_id_buffer.Write(ch);
		}
	}
    
    if (unhalt) debugger->Unhalt();
    waiting_for_stop = true;
	LOG(DEBUG) << "reached end of vCont";
}

void GdbStub::HandleVStopped(util::Buffer& packet)
{
    connection->RespondOk();
}

#pragma mark Query Handlers

void GdbStub::QueryGetSupported(util::Buffer &packet) {
	util::Buffer response;

	while(packet.ReadAvailable()) {
		char ch;
		std::string feature;
		while(packet.Read(ch) && ch != ';') {
			feature.push_back(ch);
		}

		if(feature == "multiprocess+") {
			multiprocess_enabled = true;
		}
		
		LOG(DEBUG) << "gdb advertises feature: " << feature.c_str();
	}

	LOG(DEBUG) << "gdb multiprocess: " << (multiprocess_enabled ? "true" : "false");

	bool is_first = true;
	for(std::string &feature : features) {
		if(!is_first) {
			response.Write(';');
		} else {
			is_first = false;
		}
		
		response.Write(feature);
        
        LOG(DEBUG) << "advertising feature back: " << feature;
	}
    
	connection->Respond(response);
}

void GdbStub::QueryGetCurrentThread(util::Buffer &packet) {
	util::Buffer response;
	if(multiprocess_enabled) {
		response.Write('p');
		GdbConnection::Encode((uint64_t)1, 0, response);
		response.Write('.');
	}
	GdbConnection::Encode((uint64_t)1, 0, response);
	connection->Respond(response);
}

void GdbStub::QueryGetFThreadInfo(util::Buffer &packet) {
    sent_thread_info = false;
    QueryGetSThreadInfo(packet);
}

void GdbStub::QueryGetSThreadInfo(util::Buffer &packet) {
	util::Buffer response;
    if (sent_thread_info == false) {
        response.Write('m');
       if(multiprocess_enabled) {
           response.Write('p');
           GdbConnection::Encode((uint64_t)1, 0, response);
           response.Write('.');
       }
       GdbConnection::Encode(1, 0, response);
       response.Write('l'); // End of list
        sent_thread_info = true;
    }
   
	connection->Respond(response);
}

void GdbStub::QueryGetThreadExtraInfo(util::Buffer &packet) {
	int64_t pid, thread_id;
	ReadThreadId(packet, pid, thread_id);

    // TODO: Handle this!
	
	util::Buffer response;
	//GdbConnection::Encode(extra_info, response);
	connection->Respond(response);
}

void GdbStub::QueryGetOffsets(util::Buffer &packet) {
    // TODO: Handle this!

	util::Buffer response;
	response.Write("TextSeg=");
	GdbConnection::Encode((uint64_t)0, 8, response);
	connection->Respond(response);
}

void GdbStub::QueryGetRemoteCommand(util::Buffer &packet) {
	util::Buffer message;
	GdbConnection::Decode(message, packet);
	
	std::string command;
	char ch;
	while(message.Read(ch) && ch != ' ') {
		command.push_back(ch);
	}

	std::stringstream response;
	try {
		if(command == "help") {
			response << "Available commands:" << std::endl;
		} else {
			response << "Unknown command '" << command << "'" << std::endl;
		}
    } catch(std::invalid_argument &e) {
		response = std::stringstream();
		response << "Invalid argument." << std::endl;
	}
	util::Buffer response_buffer;
	GdbConnection::Encode(response.str(), response_buffer);
	connection->Respond(response_buffer);
}

void GdbStub::QueryXfer(util::Buffer &packet) {
	std::string object_name;
	std::string op;
	
	char ch;
	while(packet.Read(ch) && ch != ':') {
		object_name.push_back(ch);
	}

	auto i = xfer_objects.find(object_name);
	if(i == xfer_objects.end()) {
		connection->RespondEmpty();
		return;
	}

	while(packet.Read(ch) && ch != ':') {
		op.push_back(ch);
	}

	if(op == "read") {
		std::string annex;
		uint64_t offset;
		uint64_t length;
		while(packet.Read(ch) && ch != ':') {
			annex.push_back(ch);
		}
		GdbConnection::DecodeWithSeparator(offset, ',', packet);
		GdbConnection::Decode(length, packet);

		i->second.Read(annex, offset, length);
	} else if(op == "write") {
		std::string annex;
		uint64_t offset;
		while(packet.Read(ch) && ch != ':') {
			annex.push_back(ch);
		}
		GdbConnection::DecodeWithSeparator(offset, ':', packet);

		i->second.Write(annex, offset, packet);
	} else {
		connection->RespondEmpty();
	}
}

void GdbStub::QueryGetTStatus(util::Buffer &packet) {
    util::Buffer response;
    response.Write("T0");
    connection->Respond(response);
}

void GdbStub::QuerySetStartNoAckMode(util::Buffer &packet) {
	connection->StartNoAckMode();
	connection->RespondOk();
}

void GdbStub::QuerySetThreadEvents(util::Buffer &packet) {
	char c;
	if(!packet.Read(c)) {
		connection->RespondError(1);
		return;
	}
	if(c == '0') {
		thread_events_enabled = false;
	} else if(c == '1') {
		thread_events_enabled = true;
	} else {
		connection->RespondError(1);
		return;
	}
	connection->RespondOk();
}

#pragma mark XferObjects

bool GdbStub::XferObject::AdvertiseRead() {
	return false;
}

bool GdbStub::XferObject::AdvertiseWrite() {
	return false;
}

GdbStub::ReadOnlyStringXferObject::ReadOnlyStringXferObject(GdbStub &stub, std::string (GdbStub::*generator)()) : stub(stub), generator(generator) {
}

void GdbStub::ReadOnlyStringXferObject::Read(std::string annex, size_t offset, size_t length) {
	if(!annex.empty()) {
		stub.connection->RespondError(0);
		return;
	}

	std::string string = (stub.*generator)();
	
	util::Buffer response;
	if(offset + length >= string.size()) {
		response.Write('l');
	} else {
		response.Write('m');
	}

	response.Write((uint8_t*) string.data() + offset, std::min(string.size() - offset, length));
	stub.connection->Respond(response);
}

void GdbStub::ReadOnlyStringXferObject::Write(std::string annex, size_t offst, util::Buffer &data) {
	stub.connection->RespondError(30); // EROFS
}

bool GdbStub::ReadOnlyStringXferObject::AdvertiseRead() {
	return true;
}

std::string GdbStub::XferReadLibraries() {
    return "<library-list></library-list>";
}

GdbStub::FeaturesXferObject::FeaturesXferObject(GdbStub &stub) : stub(stub) {}

void GdbStub::FeaturesXferObject::Read(std::string annex, size_t offset, size_t length)
{
    util::Buffer response;
    
    response.Write("l<?xml version=\"1.0\"?><!DOCTYPE target SYSTEM \"gdb-target.dtd\">");
    
    if (annex == "target.xml") {
        response.Write(R"(<target><architecture>z80</architecture><xi:include href="gb-core.xml"/></target>)");
    } else if (annex == "gb-core.xml") {
        response.Write("<feature name=\"org.gnu.gdb.z80.core\">");
        size_t num = 0;
        for (Debugger::RegisterLayout layout : Debugger::Registers) {
            std::stringstream reg;
            std::string type;
            std::stringstream typen;
            switch (layout.type) {
                case Debugger::RegisterLayout::RegisterType::integer:
                    typen << "uint" << layout.bitsize;
                    type = typen.str();
                    break;
                case Debugger::RegisterLayout::RegisterType::data_ptr:
                    type = "data_ptr";
                    break;
                case Debugger::RegisterLayout::RegisterType::code_ptr:
                    type = "code_ptr";
                    break;
                default:
                    break;
            }
            reg << "<reg name=\"" << layout.name << "\" bitsize=\"" << layout.bitsize << "\" type=\"" << type << "\" regnum=\"" << num << "\" group=\"general\"/>\n";
            response.Write(reg.str().c_str());
            num++;
        }
        response.Write("</feature>");
    } else {
        LOG(ERROR) << "Feature File " << annex << " is not known";
        stub.connection->RespondError(0);
        return;
    }
    
    LOG(DEBUG) << "Sending for feature file: " << annex << ", contents: " << response.GetString();
    
    stub.connection->Respond(response);
}

void GdbStub::FeaturesXferObject::Write(std::string annex, size_t offset, util::Buffer &data)
{
    stub.connection->RespondError(30); // EROFS
}

bool GdbStub::FeaturesXferObject::AdvertiseRead()
{
    return true;
}

} // namespace debugger
} // namespace gambatte
