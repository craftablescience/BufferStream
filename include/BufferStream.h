#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/// Only POD types are directly readable from the stream.
template<typename T>
concept BufferStreamPODType = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

/// STL container types that can hold POD type values but can't be used as buffer storage.
/// Guarantees std::begin(T), std::end(T), and T::size() are defined. T must also hold a POD type.
template<typename T>
concept BufferStreamNonContiguousContainer = BufferStreamPODType<typename T::value_type> && requires(T& t) {
	{std::begin(t)} -> std::same_as<typename T::iterator>;
	{std::end(t)} -> std::same_as<typename T::iterator>;
	{t.size()} -> std::convertible_to<std::size_t>;
};

/// STL container types that can hold POD type values and be used as buffer storage.
/// Guarantees T::data() is defined, on top of BufferStreamNonContiguousContainer.
template<typename T>
concept BufferStreamContiguousContainer = BufferStreamNonContiguousContainer<T> && requires(T& t) {
	{t.data()} -> std::same_as<typename T::value_type*>;
};

/// STL container types that can hold POD type values but can't be used as buffer storage.
/// Guarantees T::clear() and T::push_back(T::value_type) are defined, on top of BufferStreamNonContiguousContainer.
template<typename T>
concept BufferStreamNonContiguousResizableContainer = BufferStreamNonContiguousContainer<T> && requires(T& t) {
	{t.clear()} -> std::same_as<void>;
	{t.push_back(typename T::value_type{})} -> std::same_as<void>;
};

class BufferStream {
public:
	template<BufferStreamPODType T>
	BufferStream(const T* buffer, std::size_t bufferLen)
			: buffer(reinterpret_cast<const std::byte*>(buffer))
			, bufferLen(bufferLen)
			, bufferPos(0)
			, useExceptions(true) {}

	template<BufferStreamContiguousContainer T>
	explicit BufferStream(T& buffer)
			: BufferStream(buffer.data(), buffer.size()) {}

	BufferStream& setExceptionsEnabled(bool exceptions) {
		this->useExceptions = exceptions;
		return *this;
	}

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

	[[nodiscard]] std::byte peek(long offset = 1) const {
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

	template<BufferStreamPODType T, std::size_t L>
	[[nodiscard]] std::array<T, L> read_bytes() {
		const std::size_t realLen = sizeof(T) * L;
		if (this->useExceptions && this->bufferPos + realLen > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::array<T, L> out;
		std::memcpy(out.data(), this->buffer + bufferPos, realLen);
		this->bufferPos += realLen;
		return out;
	}

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		return this->read_bytes<std::byte, L>();
	}

	template<BufferStreamPODType T>
	[[nodiscard]] std::vector<T> read_bytes(std::size_t length) {
		const std::size_t realLen = sizeof(T) * length;
		if (this->useExceptions && this->bufferPos + realLen > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		std::vector<T> out;
		out.resize(length);
		std::memcpy(out.data(), this->buffer + bufferPos, realLen);
		this->bufferPos += realLen;
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

	template<BufferStreamPODType T>
	BufferStream& operator>>(T& obj) {
		return this->read(obj);
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

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator>>(T(&obj)[N]) {
		return this->read(obj);
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

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	BufferStream& operator>>(T(&obj)[M][N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& read(std::array<T, N>& obj) {
		for (int i = 0; i < N; i++) {
			obj[i] = this->read<T>();
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamNonContiguousResizableContainer T>
	BufferStream& read(T& obj, std::size_t n) {
		obj.clear();
		if (!n) {
			return *this;
		}

		// BufferStreamNonContiguousResizableContainer doesn't guarantee T::reserve(std::size_t) exists!
		if constexpr (requires([[maybe_unused]] T& t) {
			{t.reserve(1)} -> std::same_as<void>;
		}) {
			obj.reserve(n);
		}
		for (int i = 0; i < n; i++) {
			obj.push_back(this->read<T>());
		}
		return *this;
	}

	template<BufferStreamNonContiguousResizableContainer T>
	BufferStream& operator>>(T& obj) {
		return this->read(obj);
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

	BufferStream& operator>>(std::string& obj) {
		return this->read(obj);
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
