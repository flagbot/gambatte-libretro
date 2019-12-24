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

#include "Buffer.h"

#include <algorithm>
#include <cstring>

namespace gambatte {
namespace util {

Buffer::Buffer() :
	data(2048, 0) {
}

Buffer::Buffer(size_t limit) :
	data(std::min((size_t) 2048, limit), 0) {
}

Buffer::Buffer(std::vector<uint8_t> data) :
	data(data), write_head(data.size()) {
}

Buffer::~Buffer() {
}

bool Buffer::Write(const uint8_t *io, size_t size) {
	if(!EnsureSpace(size)) {
		return false;
	}
	std::copy_n(io, size, data.begin() + write_head);
	write_head+= size;
	return true;
}

bool Buffer::Write(const char *str) {
	size_t size = strlen(str);
	if(!EnsureSpace(size)) {
		return false;
	}
	std::copy_n(str, size, data.begin() + write_head);
	write_head+= size;
	return true;
}

bool Buffer::Write(std::string &string) {
	return Write((uint8_t*) string.data(), string.size());
}

std::tuple<uint8_t*, size_t> Buffer::Reserve(size_t size) {
	TryEnsureSpace(size);
	return std::make_tuple(data.data() + write_head, data.size() - write_head);
}

void Buffer::MarkWritten(size_t size) {
	write_head+= size;
}

bool Buffer::Read(uint8_t *io, size_t size) {
	if(read_head + size > write_head) {
		return false;
	}
	std::copy_n(data.begin() + read_head, size, io);
	read_head+= size;
	return true;
}

bool Buffer::Read(std::string &str, size_t len) {
	if(ReadAvailable() < len) {
		return false;
	}
	str = std::string(Read(), Read() + len);
	MarkRead(len);
	return true;
}

uint8_t *Buffer::Read() {
	return data.data() + read_head;
}

void Buffer::MarkRead(size_t size) {
	read_head+= size;
}

void Buffer::Clear() {
	read_head = 0;
	write_head = 0;
}

size_t Buffer::ReadAvailable() {
	return write_head - read_head;
}

size_t Buffer::WriteAvailableHint() {
	return data.size() - write_head;
}

bool Buffer::EnsureSpace(size_t size) {
	if(write_head + size > data.size()) {
		Compact();
	}
	if(write_head + size > data.size()) {
		data.resize(write_head + size);
	}
	return true;
}

void Buffer::TryEnsureSpace(size_t size) {
	if(write_head + size > data.size()) {
		Compact();
	}
	if(write_head + size > data.size()) {
        data.resize(write_head + size);
	}
}

void Buffer::Compact() {
	std::copy(data.begin() + read_head, data.end(), data.begin());
	write_head-= read_head;
	read_head = 0;
}

std::vector<uint8_t> Buffer::GetData() {
	return std::vector<uint8_t>(data.begin() + read_head, data.begin() + write_head);
}

std::string Buffer::GetString() {
	return std::string(data.begin() + read_head, data.begin() + write_head);
}

} // namespace util
} // namespace twili
