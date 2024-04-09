#include <BufferStream.h>

BufferStream::BufferStream(const std::byte* buffer, std::size_t bufferLength, bool useExceptions_)
		: streamBuffer(buffer)
		, streamLen(bufferLength)
		, streamPos(0)
		, useExceptions(useExceptions_) {}

void BufferStream::seek(std::size_t offset, std::ios::seekdir offsetFrom) {
	if (offsetFrom == std::ios::beg) {
		if (this->useExceptions && offset > this->streamLen) {
			throw std::out_of_range{detail::OUT_OF_RANGE_ERROR_MESSAGE};
		}
		this->streamPos = offset;
	} else if (offsetFrom == std::ios::cur) {
		if (this->useExceptions && this->streamPos + offset > this->streamLen) {
			throw std::out_of_range{detail::OUT_OF_RANGE_ERROR_MESSAGE};
		}
		this->streamPos += offset;
	} else if (offsetFrom == std::ios::end) {
		if (this->useExceptions && offset > this->streamLen) {
			throw std::out_of_range{detail::OUT_OF_RANGE_ERROR_MESSAGE};
		}
		this->streamPos = this->streamLen - offset;
	}
}

std::size_t BufferStream::tell() const {
	return this->streamPos;
}

std::size_t BufferStream::size() const {
	return this->streamLen;
}

std::byte BufferStream::peek(long offset) {
	if (this->useExceptions && offset > this->streamLen - this->streamPos) {
		throw std::out_of_range{detail::OUT_OF_RANGE_ERROR_MESSAGE};
	}

	return this->streamBuffer[this->streamPos + offset];
}

std::vector<std::byte> BufferStream::read_bytes(std::size_t length) {
	if (this->useExceptions && this->streamPos + length > this->streamLen) {
		throw std::out_of_range{detail::OUT_OF_RANGE_ERROR_MESSAGE};
	}

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
