#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/// Valid byte types
template<typename T>
concept BufferStreamByteType = std::same_as<T, std::byte> || std::same_as<T, unsigned char> || std::same_as<T, char>;

/// Only POD types are directly readable from the stream
template<typename T>
concept BufferStreamPODType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

class BufferStream {
public:
	template<BufferStreamByteType T>
	BufferStream(const T* buffer, std::size_t bufferLen, bool useExceptions = true)
			: buffer(reinterpret_cast<const std::byte*>(buffer))
			, bufferLen(bufferLen)
			, bufferPos(0)
			, useExceptions(useExceptions) {}

	template<BufferStreamByteType T, std::size_t N>
	explicit BufferStream(std::array<T, N>& array, bool useExceptions = true)
			: BufferStream(array.data(), array.size(), useExceptions) {}

	template<BufferStreamByteType T>
	explicit BufferStream(std::vector<T>& vector, bool useExceptions = true)
			: BufferStream(vector.data(), vector.size(), useExceptions) {}

	explicit BufferStream(std::string& string, bool useExceptions = true)
			: BufferStream(string.data(), string.size(), useExceptions) {}

	BufferStream& seek(std::size_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		if (offsetFrom == std::ios::beg) {
			if (this->useExceptions && offset > this->bufferLen) {
				throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
			}
			this->bufferPos = offset;
		} else if (offsetFrom == std::ios::cur) {
			if (this->useExceptions && this->bufferPos + offset > this->bufferLen) {
				throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
			}
			this->bufferPos += offset;
		} else if (offsetFrom == std::ios::end) {
			if (this->useExceptions && offset > this->bufferLen) {
				throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
			}
			this->bufferPos = this->bufferLen - offset;
		}
		return *this;
	}

	template<BufferStreamPODType T = std::byte>
	BufferStream& skip(std::size_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seek(sizeof(T) * n, std::ios::cur);
	}

	[[nodiscard]] std::size_t tell() const {
		return this->bufferPos;
	}

	[[nodiscard]] std::size_t size() const {
		return this->bufferLen;
	}

	[[nodiscard]] std::byte peek(long offset = 0) const {
		if (this->useExceptions && offset > this->bufferLen - this->bufferPos) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		return this->buffer[this->bufferPos + offset];
	}

	template<BufferStreamPODType T>
	[[nodiscard]] T read() {
		T obj{};
		this->read(obj);
		return obj;
	}

	template<BufferStreamByteType T, std::size_t L>
	[[nodiscard]] std::array<T, L> read_bytes() {
		if (this->useExceptions && this->bufferPos + L > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::array<T, L> out;
		for (int i = 0; i < L; i++, this->bufferPos++) {
			out[i] = this->buffer[this->bufferPos];
		}
		return out;
	}

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		return this->read_bytes<std::byte, L>();
	}

	template<BufferStreamByteType T>
	[[nodiscard]] std::vector<T> read_bytes(std::size_t length) {
		if (this->useExceptions && this->bufferPos + length > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::vector<T> out;
		out.resize(length);
		for (int i = 0; i < length; i++, this->bufferPos++) {
			out.push_back(this->buffer[this->bufferPos]);
		}
		return out;
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::size_t length) {
		return this->read_bytes<std::byte>(length);
	}

	[[nodiscard]] std::string read_string() {
		std::string out;
		this->read(out);
		return out;
	}

	[[nodiscard]] std::string read_string(std::size_t n, bool stopOnNullTerminator = true) {
		std::string out;
		this->read(out, n, stopOnNullTerminator);
		return out;
	}

	template<BufferStreamPODType T>
	BufferStream& read(T& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::memcpy(&obj, this->buffer + this->bufferPos, sizeof(T));
		this->bufferPos += sizeof(T);

		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& read(T(&obj)[N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * N > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		for (int i = 0; i < N; i++, this->bufferPos += sizeof(T)) {
			this->read(obj[i]);
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	BufferStream& read(T(&obj)[M][N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * M * N > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++, this->bufferPos += sizeof(T)) {
				this->read(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& read(std::array<T, N>& obj) {
		for (int i = 0; i < N; i++) {
			obj[i] = this->read<T>();
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& read(std::vector<T>& obj, std::size_t n) {
		obj.clear();
		if (!n) {
			return *this;
		}

		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			obj.push_back(this->read<T>());
		}
		return *this;
	}

	BufferStream& read(std::string& obj) {
		obj.clear();
		char temp = this->read<char>();
		while (temp != '\0') {
			obj += temp;
			temp = this->read<char>();
		}
		return *this;
	}

	BufferStream& read(std::string& obj, std::size_t n, bool stopOnNullTerminator = true) {
		obj.clear();
		if (!n) {
			return *this;
		}

		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			char temp = this->read<char>();
			if (temp == '\0' && stopOnNullTerminator) {
				// Read the required number of characters and exit
				this->skip<char>(n - i - 1);
				break;
			}
			obj += temp;
		}
		return *this;
	}

protected:
	const std::byte* buffer;
	std::size_t bufferLen;
	std::size_t bufferPos;
	bool useExceptions;

	static inline constexpr const char* OUT_OF_RANGE_ERROR_MESSAGE = "Out of range!";
};
