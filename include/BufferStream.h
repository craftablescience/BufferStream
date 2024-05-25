#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/// Any POD types are readable directly from the stream
template<typename T>
concept BufferStreamReadableType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

class BufferStream {
public:
	BufferStream(const std::byte* buffer, std::size_t bufferLength, bool useExceptions_ = true);

	BufferStream& seek(std::size_t offset, std::ios::seekdir offsetFrom = std::ios::beg);

	template<BufferStreamReadableType T = std::byte>
	BufferStream& skip(std::size_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seek(sizeof(T) * n, std::ios::cur);
	}

	[[nodiscard]] std::size_t tell() const;

	[[nodiscard]] std::size_t size() const;

	[[nodiscard]] std::byte peek(long offset = 0);

	template<BufferStreamReadableType T>
	[[nodiscard]] T read() {
		T obj{};
		this->read(obj);
		return obj;
	}

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		if (this->useExceptions && this->streamPos + L > this->streamLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::array<std::byte, L> out;
		for (int i = 0; i < L; i++, this->streamPos++) {
			out[i] = this->streamBuffer[this->streamPos];
		}
		return out;
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::size_t length);

	[[nodiscard]] std::string read_string();

	[[nodiscard]] std::string read_string(std::size_t n, bool stopOnNullTerminator = true);

	template<BufferStreamReadableType T>
	BufferStream& read(T& obj) {
		if (this->useExceptions && this->streamPos + sizeof(T) > this->streamLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		for (int i = 0; i < sizeof(T); i++, this->streamPos++) {
			reinterpret_cast<std::byte*>(&obj)[i] = this->streamBuffer[this->streamPos];
		}
		return *this;
	}

	template<BufferStreamReadableType T, std::size_t N>
	BufferStream& read(T(&obj)[N]) {
		if (this->useExceptions && this->streamPos + sizeof(T) * N > this->streamLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		for (int i = 0; i < sizeof(T) * N; i++, this->streamPos++) {
			reinterpret_cast<std::byte*>(&obj)[i] = this->streamBuffer[this->streamPos];
		}
		return *this;
	}

	template<BufferStreamReadableType T, std::size_t N>
	BufferStream& read(std::array<T, N>& obj) {
		for (int i = 0; i < N; i++) {
			obj[i] = this->read<T>();
		}
		return *this;
	}

	template<BufferStreamReadableType T>
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
	const std::byte* streamBuffer;
	std::size_t streamLen;
	std::size_t streamPos;
	bool useExceptions;

	static inline constexpr const char* OUT_OF_RANGE_ERROR_MESSAGE = "Out of range!";
};
