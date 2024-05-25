#pragma once

#include <array>
#include <bit>
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
concept BufferStreamPossiblyNonContiguousContainer = BufferStreamPODType<typename T::value_type> && requires(T& t) {
	{std::begin(t)} -> std::same_as<typename T::iterator>;
	{std::end(t)} -> std::same_as<typename T::iterator>;
	{t.size()} -> std::convertible_to<std::size_t>;
};

/// STL container types that can hold POD type values and be used as buffer storage.
/// Guarantees T::data() is defined, on top of BufferStreamNonContiguousContainer.
template<typename T>
concept BufferStreamContiguousContainer = BufferStreamPossiblyNonContiguousContainer<T> && requires(T& t) {
	{t.data()} -> std::same_as<typename T::value_type*>;
};

/// STL container types that can hold POD type values but can't be used as buffer storage.
/// Guarantees T::clear() and T::push_back(T::value_type) are defined, on top of BufferStreamNonContiguousContainer.
template<typename T>
concept BufferStreamPossiblyNonContiguousResizableContainer = BufferStreamPossiblyNonContiguousContainer<T> && requires(T& t) {
	{t.clear()} -> std::same_as<void>;
	{t.push_back(typename T::value_type{})} -> std::same_as<void>;
};

class BufferStream {
public:
	template<BufferStreamPODType T>
	BufferStream(const T* buffer, std::size_t bufferLen)
			: buffer(reinterpret_cast<const std::byte*>(buffer))
			, bufferLen(sizeof(T) * bufferLen)
			, bufferPos(0)
			, useExceptions(true)
			, bigEndian(false) {}

	template<BufferStreamPODType T, std::size_t N>
	explicit BufferStream(T(&buffer)[N])
			: BufferStream(buffer, sizeof(T) * N) {}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	explicit BufferStream(T(&buffer)[M][N])
			: BufferStream(buffer, sizeof(T) * M * N) {}

	template<BufferStreamContiguousContainer T>
	explicit BufferStream(T& buffer)
			: BufferStream(buffer.data(), sizeof(typename T::value_type) * buffer.size()) {}

	BufferStream& setExceptionsEnabled(bool exceptions) {
		this->useExceptions = exceptions;
		return *this;
	}

	BufferStream& setBigEndian(bool readBigEndian) {
		this->bigEndian = readBigEndian;
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
	BufferStream& read(T& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) > this->bufferLen) {
			throw std::out_of_range{OUT_OF_RANGE_ERROR_MESSAGE};
		}

		static constexpr auto swapEndian = [](T& t) {
			union {
				T t;
				std::byte bytes[sizeof(T)];
			} source{}, dest{};
			source.t = t;
			for (size_t k = 0; k < sizeof(T); k++) {
				dest.bytes[k] = source.bytes[sizeof(T) - k - 1];
			}
			t = dest.t;
		};

		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(&obj, this->buffer + this->bufferPos, sizeof(T));
			if (this->bigEndian) {
				swapEndian(obj);
			}
		} else if constexpr (std::endian::native == std::endian::big) {
			std::memcpy(&obj, this->buffer + this->bufferPos, sizeof(T));
			if (!this->bigEndian) {
				swapEndian(obj);
			}
		} else {
			static_assert("Need to investigate what the proper endianness of this platform is!");
		}
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

		for (int i = 0; i < N; i++) {
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
			for (int j = 0; j < N; j++) {
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
			this->read(obj[i]);
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
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
			obj.push_back(this->read<typename T::value_type>());
		}
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& operator>>(T& obj) {
		obj.push_back(this->read<typename T::value_type>());
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

	template<BufferStreamPODType T>
	[[nodiscard]] T read() {
		T obj{};
		this->read(obj);
		return obj;
	}

	template<BufferStreamPODType T, std::size_t N>
	std::array<T, N> read() {
		std::array<T, N> obj{};
		this->read(obj);
		return obj;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	T read(std::size_t n) {
		T obj{};
		this->read(obj, n);
		return obj;
	}

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		return this->read<std::array<std::byte, L>>();
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::size_t length) {
		std::vector<std::byte> out;
		this->read(out, length);
		return out;
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

protected:
	const std::byte* buffer;
	std::size_t bufferLen;
	std::size_t bufferPos;
	bool useExceptions;
	bool bigEndian;

	static inline constexpr const char* OUT_OF_RANGE_ERROR_MESSAGE = "Out of range!";
};
