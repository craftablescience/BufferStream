#include <BufferStream.h>

BufferStream::BufferStream(const std::byte* buffer, std::size_t bufferLength) {
	this->streamBuffer = buffer;
	this->streamLen = bufferLength;
	this->streamPos = 0;
}

void BufferStream::seek(std::size_t offset, std::ios::seekdir offsetFrom) {
	if (offsetFrom == std::ios::beg)
		this->streamPos = offset;
	else if (offsetFrom == std::ios::cur)
		this->streamPos += offset;
	else if (offsetFrom == std::ios::end)
		this->streamPos = this->streamLen + offset;
}

void BufferStream::skip(std::size_t offset) {
	this->seek(offset, std::ios::cur);
}

std::size_t BufferStream::tell() const {
	return this->streamPos;
}

std::byte BufferStream::peek(long offset) {
	return this->streamBuffer[this->streamPos + offset];
}

std::vector<std::byte> BufferStream::read_bytes(std::size_t length) {
	std::vector<std::byte> out;
	out.resize(length);
	for (int i = 0; i < length; i++, this->streamPos++) {
		out.push_back(this->streamBuffer[this->streamPos]);
	}
	return out;
}

std::string BufferStream::read_string() {
    std::string out;
    this->read(out);
    return out;
}

std::string BufferStream::read_string(std::size_t n, bool stopOnNullTerminator) {
    std::string out;
    this->read(out, n, stopOnNullTerminator);
    return out;
}
