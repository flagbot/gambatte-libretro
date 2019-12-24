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

#include <condition_variable>
#include <mutex>

#include "Buffer.h"
#include "easylogging++.h"

namespace gambatte {
namespace debugger {

class GdbConnection {
 public:
	GdbConnection(int sockfd);

	// NULL pointer means no message.
	// Entire buffer should be consumed before calling this again.
	util::Buffer *Process(bool &interrupted);

	std::mutex mutex;
	std::condition_variable error_condvar;
	
	bool error_flag = false;
    
    bool connection_alive = true;

	static uint8_t DecodeHexByte(char *hex);
	static uint8_t DecodeHexNybble(char hex);
	static void DecodeWithSeparator(uint64_t &out, char sep, util::Buffer &packet);
	static void Decode(uint64_t &out, util::Buffer &packet);
	static void Decode(std::vector<uint8_t> &out, util::Buffer &packet);
	static void Decode(util::Buffer &out, util::Buffer &packet);
	static char EncodeHexNybble(uint8_t n);
	static void Encode(uint64_t n, size_t size, util::Buffer &dest, bool little_endian = true);
	static void Encode(uint8_t *p, size_t size, util::Buffer &dest);
	static void Encode(std::string &string, util::Buffer &dest);
	static void Encode(std::string &&string, util::Buffer &dest);

    void SendMessage(util::Buffer &buffer, char packet_ident = '$');
	void Respond(util::Buffer &buffer);
	void RespondEmpty();
	void RespondError(int no);
	void RespondOk();
    void Notification(util::Buffer &buffer);
	void SignalError();
	void StartNoAckMode();
	
    void ReadInput();
    
    int sockfd;
 private:
    
	util::Buffer in_buffer;
	util::Buffer message_buffer;
	util::Buffer out_buffer;
	enum class State {
		WAITING_PACKET_OPEN,
		READING_PACKET_DATA,
		ESCAPE_CHARACTER,
		CHECKSUM_0,
		CHECKSUM_1
	} state = State::WAITING_PACKET_OPEN;

	uint8_t checksum = 0;
	char checksum_hex[2];
	bool ack_enabled = true; // this starts enabled
};

} // namespace debugger
} // namespace gambatte
