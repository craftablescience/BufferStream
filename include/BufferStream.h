#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <string>
#include <type_traits>
#include <vector>

namespace detail {

template<typename T>
concept PODType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

} // namespace detail

class BufferStream {
public:
	BufferStream(const std::byte* buffer, std::size_t bufferLength);

	void seek(std::size_t offset, std::ios::seekdir offsetFrom = std::ios::beg);

	void skip(std::size_t offset);

	template<detail::PODType T, std::size_t N = 1>
	void skip() {
		this->skip(sizeof(T) * N);
	}

	[[nodiscard]] std::size_t tell() const;

	[[nodiscard]] std::byte peek(long offset = 0);

	template<detail::PODType T>
	[[nodiscard]] T read() {
		T obj{};
		this->read(obj);
		return obj;
	}

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		std::array<std::byte, L> out;
		for (int i = 0; i < L; i++, this->streamPos++) {
			out[i] = this->streamBuffer[this->streamPos];
		}
		return out;
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::size_t length);

	[[nodiscard]] std::string read_string();

	[[nodiscard]] std::string read_string(std::size_t n, bool stopOnNullTerminator = true);

	template<detail::PODType T>
	void read(T& obj) {
		for (int i = 0; i < sizeof(T); i++, this->streamPos++) {
			reinterpret_cast<std::byte*>(&obj)[i] = this->streamBuffer[this->streamPos];
		}
	}

	template<detail::PODType T, std::size_t N>
	void read(T(&obj)[N]) {
		for (int i = 0; i < sizeof(T) * N; i++, this->streamPos++) {
			reinterpret_cast<std::byte*>(&obj)[i] = this->streamBuffer[this->streamPos];
		}
	}

	template<typename T, std::size_t N>
	void read(std::array<T, N>& obj) {
		for (int i = 0; i < N; i++) {
			obj[i] = this->read<T>();
		}
	}

	template<typename T>
	void read(std::vector<T>& obj, std::size_t n) {
		obj.clear();
		if (!n) {
			return;
		}
		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			obj.push_back(this->read<T>());
		}
	}

	void read(std::string& obj) {
		obj.clear();
		char temp = this->read<char>();
		while (temp != '\0') {
			obj += temp;
			temp = this->read<char>();
		}
	}

	void read(std::string& obj, std::size_t n, bool stopOnNullTerminator = true) {
		obj.clear();
		if (!n) {
			return;
		}
		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			char temp = this->read<char>();
			if (temp == '\0' && stopOnNullTerminator) {
				break;
			}
			obj += temp;
		}
	}

protected:
	const std::byte* streamBuffer;
	std::size_t streamLen;
	std::size_t streamPos;
};
