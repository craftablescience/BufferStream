#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <ios>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

/// Only POD types are directly readable from the stream.
template<typename T>
concept BufferStreamPODType = std::is_trivial_v<T> && std::is_standard_layout_v<T> && !std::is_pointer_v<T>;

/// For types that must be one byte large, on top of BufferStreamPODType.
template<typename T>
concept BufferStreamPODByteType = BufferStreamPODType<T> && sizeof(T) == 1;

/// STL container types that can hold POD type values but can't be used as buffer storage.
/// Guarantees std::begin(T), std::end(T), and T::size() are defined. T must also hold a POD type.
template<typename T>
concept BufferStreamPossiblyNonContiguousContainer = BufferStreamPODType<typename T::value_type> && requires(T& t) {
	{std::begin(t)} -> std::same_as<typename T::iterator>;
	{std::end(t)} -> std::same_as<typename T::iterator>;
	{t.size()} -> std::convertible_to<std::size_t>;
};

/// STL container types that can hold POD type values and be used as buffer storage.
/// Guarantees T::data() is defined and T::resize(std::size_t) is NOT defined, on top of BufferStreamPossiblyNonContiguousContainer.
template<typename T>
concept BufferStreamNonResizableContiguousContainer = BufferStreamPossiblyNonContiguousContainer<T> && requires(T& t) {
	{t.data()} -> std::same_as<typename T::value_type*>;
} && !requires(T& t) {
	{t.resize(1)} -> std::same_as<void>;
};

/// STL container types that can hold POD type values, be used as buffer storage, and grow/shrink.
/// Guarantees T::data() is defined and T::resize(std::size_t) is defined, on top of BufferStreamPossiblyNonContiguousContainer.
template<typename T>
concept BufferStreamResizableContiguousContainer = BufferStreamPossiblyNonContiguousContainer<T> && requires(T& t) {
	{t.data()} -> std::same_as<typename T::value_type*>;
	{t.resize(1)} -> std::same_as<void>;
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
	using ResizeCallback = std::function<std::byte*(BufferStream* stream, std::size_t newLen)>;

	template<BufferStreamPODType T>
	BufferStream(T* buffer, std::size_t bufferLen, ResizeCallback resizeCallback = nullptr)
			: buffer(reinterpret_cast<std::byte*>(buffer))
			, bufferLen(sizeof(T) * bufferLen)
			, bufferPos(0)
			, bufferResizeCallback(std::move(resizeCallback))
			, useExceptions(true)
			, bigEndian(false) {}

	template<BufferStreamPODType T, std::size_t N>
	explicit BufferStream(T(&buffer)[N])
			: BufferStream(buffer, sizeof(T) * N) {}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	explicit BufferStream(T(&buffer)[M][N])
			: BufferStream(buffer, sizeof(T) * M * N) {}

	template<BufferStreamNonResizableContiguousContainer T>
	explicit BufferStream(T& buffer)
			: BufferStream(buffer.data(), buffer.size() * sizeof(typename T::value_type)) {}

	template<BufferStreamResizableContiguousContainer T>
	explicit BufferStream(T& buffer, bool resizable = true)
			: BufferStream(buffer.data(), buffer.size() * sizeof(typename T::value_type), resizable ? [&buffer](BufferStream* stream, std::size_t newLen) {
				while (buffer.size() * sizeof(typename T::value_type) < newLen) {
					if (buffer.size() == 0) {
						buffer.resize(1);
					} else {
						buffer.resize(buffer.size() * 2);
					}
				}
				return reinterpret_cast<std::byte*>(buffer.data());
			} : ResizeCallback{nullptr}) {}

	BufferStream& set_exceptions_enabled(bool exceptions) {
		this->useExceptions = exceptions;
		return *this;
	}

	BufferStream& set_big_endian(bool readBigEndian) {
		this->bigEndian = readBigEndian;
		return *this;
	}

	BufferStream& seek(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		if (offsetFrom == std::ios::beg) {
			if (this->useExceptions && (offset > this->bufferLen || offset < 0)) {
				throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
			}
			this->bufferPos = offset;
		} else if (offsetFrom == std::ios::cur) {
			if (this->useExceptions && (this->bufferPos + offset > this->bufferLen || this->bufferPos + offset < 0)) {
				throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
			}
			this->bufferPos += offset;
		} else if (offsetFrom == std::ios::end) {
			if (this->useExceptions && (offset > this->bufferLen || offset < 0)) {
				throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
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

	[[nodiscard]] const std::byte* data() const {
		return this->buffer;
	}

	[[nodiscard]] std::byte* data() {
		return this->buffer;
	}

	[[nodiscard]] std::size_t tell() const {
		return this->bufferPos;
	}

	[[nodiscard]] std::size_t size() const {
		return this->bufferLen;
	}

	[[nodiscard]] std::byte peek(std::size_t offset = 1) const {
		if (this->useExceptions && offset >= this->bufferLen - this->bufferPos) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		return this->buffer[this->bufferPos + offset];
	}

	template<BufferStreamPODByteType T>
	[[nodiscard]] T peek(std::size_t offset = 1) const {
		return static_cast<T>(this->peek(offset));
	}

	template<BufferStreamPODType T>
	BufferStream& read(T& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		std::memcpy(&obj, this->buffer + this->bufferPos, sizeof(T));
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(&obj);
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(&obj);
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else {
				static_assert("Need to investigate what the proper endianness of this platform is!");
			}
		}
		this->bufferPos += sizeof(T);

		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& operator>>(T& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T>
	BufferStream& write(const T& obj) {
		if (this->bufferPos + sizeof(T) > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T)) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		std::memcpy(this->buffer + this->bufferPos, &obj, sizeof(T));
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(reinterpret_cast<T*>(this->buffer + this->bufferPos));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(reinterpret_cast<T*>(this->buffer + this->bufferPos));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else {
				static_assert("Need to investigate what the proper endianness of this platform is!");
			}
		}
		this->bufferPos += sizeof(T);

		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& operator<<(const T& obj) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& read(T(&obj)[N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * N > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(obj, this->buffer + this->bufferPos, N);
			this->bufferPos += N;
		} else {
			for (int i = 0; i < N; i++) {
				this->read(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator>>(T(&obj)[N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& write(const T(&obj)[N]) {
		if (this->bufferPos + sizeof(T) * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * N) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(this->buffer + this->bufferPos, obj, N);
			this->bufferPos += N;
		} else {
			for (int i = 0; i < N; i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator<<(const T(&obj)[N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	BufferStream& read(T(&obj)[M][N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * M * N > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
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

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	BufferStream& write(const T(&obj)[M][N]) {
		if (this->bufferPos + sizeof(T) * M * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * M * N) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++) {
				this->write(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	BufferStream& operator<<(const T(&obj)[M][N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& read(std::array<T, N>& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * N > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(obj.data(), this->buffer + this->bufferPos, N);
			this->bufferPos += N;
		} else {
			for (int i = 0; i < N; i++) {
				this->read(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& write(const std::array<T, N>& obj) {
		if (this->bufferPos + sizeof(T) * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * N) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), N);
			this->bufferPos += N;
		} else {
			for (int i = 0; i < N; i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	BufferStream& operator<<(const std::array<T, N>& obj) {
		return this->write(obj);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& read(T& obj, std::size_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(typename T::value_type) * n > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		obj.clear();
		if (!n) {
			return *this;
		}

		if constexpr (sizeof(typename T::value_type) == 1 && BufferStreamResizableContiguousContainer<T>) {
			obj.resize(n);
			std::memcpy(obj.data(), this->buffer + this->bufferPos, n);
			this->bufferPos += n;
		} else {
			// BufferStreamPossiblyNonContiguousResizableContainer doesn't guarantee T::reserve(std::size_t) exists!
			if constexpr (requires([[maybe_unused]] T& t) {
				{t.reserve(1)} -> std::same_as<void>;
			}) {
				obj.reserve(n);
			}
			for (int i = 0; i < n; i++) {
				obj.push_back(this->read<typename T::value_type>());
			}
		}
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& operator>>(T& obj) {
		obj.push_back(this->read<typename T::value_type>());
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& write(const T& obj) {
		if (this->bufferPos + sizeof(typename T::value_type) * obj.size() > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(typename T::value_type) * obj.size()) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (obj.size() == 0) {
			return *this;
		}

		if constexpr (sizeof(typename T::value_type) == 1 && (BufferStreamNonResizableContiguousContainer<T> || BufferStreamResizableContiguousContainer<T>)) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), obj.size());
			this->bufferPos += obj.size();
		} else {
			for (int i = 0; i < obj.size(); i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& operator<<(const T& obj) {
		return this->write(obj);
	}

	template<BufferStreamPODType T>
	BufferStream& read(std::span<T>& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * obj.size() > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (obj.empty()) {
			return *this;
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(obj.data(), this->buffer + this->bufferPos, obj.size());
			this->bufferPos += obj.size();
		} else {
			for (int i = 0; i < obj.size(); i++) {
				obj[i] = this->read<T>();
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& operator>>(std::span<T>& obj) {
		this->read(obj);
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& read(std::span<T>& obj, std::size_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * n > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (!n) {
			return *this;
		}

		if (obj.empty()) {
			obj = std::span<T>{reinterpret_cast<T*>(this->buffer + this->bufferPos), n};
			this->bufferPos += sizeof(T) * n;
		} else {
			if constexpr (sizeof(T) == 1) {
				std::memcpy(obj.data(), this->buffer + this->bufferPos, n);
				this->bufferPos += n;
			} else {
				for (int i = 0; i < n; i++) {
					obj[i] = this->read<T>();
				}
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& write(const std::span<T>& obj) {
		if (this->bufferPos + sizeof(T) * obj.size() > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * obj.size()) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (obj.empty()) {
			return *this;
		}

		if constexpr (sizeof(T) == 1) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), obj.size());
			this->bufferPos += obj.size();
		} else {
			for (int i = 0; i < obj.size(); i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& operator<<(const std::span<T>& obj) {
		return this->write(obj);
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

	BufferStream& write(const std::string& obj, bool addNullTerminator = true, std::size_t maxSize = 0) {
		auto stringByteSize = maxSize == 0 ? (sizeof(typename std::string::value_type) * obj.size() + (addNullTerminator ? sizeof(typename std::string::value_type) : 0)) : maxSize;
		if (this->bufferPos + stringByteSize > this->bufferLen && !this->resize_buffer(this->bufferPos + stringByteSize) && this->useExceptions) {
			throw std::overflow_error{OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (maxSize == 0) {
			maxSize = obj.size() + 1;
		}
		for (std::size_t i = 0; i < maxSize; i++) {
			if (i < obj.size() && !(addNullTerminator && i == maxSize - 1)) {
				this->write(obj[i]);
			} else {
				this->write('\0');
			}
		}
		return *this;
	}

	BufferStream& operator<<(const std::string& obj) {
		return this->write(obj);
	}

	BufferStream& read(std::string& obj, std::size_t n, bool stopOnNullTerminator = true) {
		if (this->useExceptions && this->bufferPos + sizeof(typename std::string::value_type) * n > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

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

	template<BufferStreamPODType T>
	[[nodiscard]] std::span<T> read_span(std::size_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * n > this->bufferLen) {
			throw std::overflow_error{OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (!n) {
			return {};
		}

		std::span<T> out{reinterpret_cast<T*>(this->buffer + this->bufferPos), n};
		this->bufferPos += sizeof(T) * n;
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

	template<std::size_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		return this->read<std::array<std::byte, L>>();
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::size_t length) {
		std::vector<std::byte> out;
		this->read(out, length);
		return out;
	}

protected:
	std::byte* buffer;
	std::size_t bufferLen;
	std::size_t bufferPos;
	ResizeCallback bufferResizeCallback;
	bool useExceptions;
	bool bigEndian;

	static inline constexpr const char* OVERFLOW_READ_ERROR_MESSAGE = "Attempted to read value out of buffer bounds!";
	static inline constexpr const char* OVERFLOW_WRITE_ERROR_MESSAGE = "Attempted to write value out of buffer bounds!";
	static inline constexpr const char* BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE = "Cannot change endianness of complex types!";

	[[nodiscard]] bool resize_buffer(std::size_t newLen) {
		if (!this->bufferResizeCallback) {
			return false;
		}
		this->buffer = this->bufferResizeCallback(this, newLen);
		this->bufferLen = newLen;
		return true;
	}

	template<BufferStreamPODType T>
	static constexpr void swap_endian(T* t) {
		union {
			T t;
			std::byte bytes[sizeof(T)];
		} source{}, dest{};
		source.t = *t;
		for (size_t k = 0; k < sizeof(T); k++) {
			dest.bytes[k] = source.bytes[sizeof(T) - k - 1];
		}
		*t = dest.t;
	};
};

class BufferStreamReadOnly : public BufferStream {
public:
	template<BufferStreamPODType T>
	BufferStreamReadOnly(const T* buffer, std::size_t bufferLen)
			: BufferStream(const_cast<T*>(buffer), bufferLen) {}

	template<BufferStreamPODType T, std::size_t N>
	explicit BufferStreamReadOnly(T(&buffer)[N])
			: BufferStreamReadOnly(const_cast<const T*>(buffer), sizeof(T) * N) {}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	explicit BufferStreamReadOnly(T(&buffer)[M][N])
			: BufferStreamReadOnly(const_cast<const T*>(buffer), sizeof(T) * M * N) {}

	template<BufferStreamNonResizableContiguousContainer T>
	explicit BufferStreamReadOnly(T& buffer)
			: BufferStreamReadOnly(const_cast<const typename T::value_type*>(buffer.data()), buffer.size() * sizeof(typename T::value_type)) {}

	template<BufferStreamResizableContiguousContainer T>
	explicit BufferStreamReadOnly(T& buffer)
			: BufferStreamReadOnly(const_cast<const typename T::value_type*>(buffer.data()), buffer.size() * sizeof(typename T::value_type)) {}

private:
	using BufferStream::write;
	using BufferStream::operator<<;
};
