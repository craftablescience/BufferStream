#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
#include <ios>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/// Only POD types are directly readable from the stream.
template<typename T>
concept BufferStreamPODType = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

/// For types that must be one byte large, on top of BufferStreamPODType.
template<typename T>
concept BufferStreamPODByteType = BufferStreamPODType<T> && sizeof(T) == 1;

/// STL container types that can hold POD type values but can't be used as buffer storage.
/// Guarantees std::begin(T), std::end(T), and T::size() are defined. T must also hold a POD type.
template<typename T>
concept BufferStreamPossiblyNonContiguousContainer = BufferStreamPODType<typename T::value_type> && requires(T& t) {
	{std::begin(t)} -> std::same_as<typename T::iterator>;
	{std::end(t)} -> std::same_as<typename T::iterator>;
	{t.size()} -> std::convertible_to<std::uint64_t>;
};

/// STL container types that can hold POD type values and be used as buffer storage.
/// Guarantees T::data() is defined and T::resize(std::uint64_t) is NOT defined, on top of BufferStreamPossiblyNonContiguousContainer.
template<typename T>
concept BufferStreamNonResizableContiguousContainer = BufferStreamPossiblyNonContiguousContainer<T> && requires(T& t) {
	{t.data()} -> std::same_as<typename T::value_type*>;
} && !requires(T& t) {
	{t.resize(1)} -> std::same_as<void>;
};

/// STL container types that can hold POD type values, be used as buffer storage, and grow/shrink.
/// Guarantees T::data() is defined and T::resize(std::uint64_t) is defined, on top of BufferStreamPossiblyNonContiguousContainer.
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

constexpr auto BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE = "Attempted to read value out of buffer bounds!";
constexpr auto BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE = "Attempted to write value out of buffer bounds!";
constexpr auto BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE = "Cannot change endianness of complex types!";

class BufferStream {
public:
	using ResizeCallback = std::function<std::byte*(BufferStream* stream, std::uint64_t newLen)>;

	template<BufferStreamPODType T>
	BufferStream(T* buffer, std::uint64_t bufferLen_, ResizeCallback resizeCallback = nullptr)
			: buffer(reinterpret_cast<std::byte*>(buffer))
			, bufferLen(sizeof(T) * bufferLen_)
			, bufferPos(0)
			, bufferResizeCallback(std::move(resizeCallback))
			, useExceptions(true)
			, bigEndian(false) {}

	template<BufferStreamPODType T, std::uint64_t N>
	explicit BufferStream(T(&buffer)[N])
			: BufferStream(buffer, sizeof(T) * N) {}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	explicit BufferStream(T(&buffer)[M][N])
			: BufferStream(buffer, sizeof(T) * M * N) {}

	template<BufferStreamNonResizableContiguousContainer T>
	explicit BufferStream(T& buffer)
			: BufferStream(buffer.data(), buffer.size() * sizeof(typename T::value_type)) {}

	template<BufferStreamResizableContiguousContainer T>
	explicit BufferStream(T& buffer, bool resizable = true)
			: BufferStream(buffer.data(), buffer.size() * sizeof(typename T::value_type), resizable ? [&buffer](BufferStream*, std::uint64_t newLen) {
				auto curSize = buffer.size();
				while (curSize * sizeof(typename T::value_type) < newLen) {
					if (!curSize) {
						curSize = 1;
					} else {
						curSize *= 2;
					}
				}
				buffer.resize(curSize);
				return reinterpret_cast<std::byte*>(buffer.data());
			} : ResizeCallback{nullptr}) {}

	[[nodiscard]] bool are_exceptions_enabled() const {
		return this->useExceptions;
	}

	BufferStream& set_exceptions_enabled(bool exceptions) {
		this->useExceptions = exceptions;
		return *this;
	}

	[[nodiscard]] bool is_big_endian() const {
		return this->bigEndian;
	}

	BufferStream& set_big_endian(bool readBigEndian) {
		this->bigEndian = readBigEndian;
		return *this;
	}

	BufferStream& seek(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		switch (offsetFrom) {
			case std::ios::beg:
				if (this->useExceptions && (std::cmp_greater(offset, this->bufferLen) || offset < 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				this->bufferPos = offset;
				break;
			case std::ios::cur:
				if (this->useExceptions && (std::cmp_greater(this->bufferPos + offset, this->bufferLen) || static_cast<int64_t>(this->bufferPos) + offset < 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				this->bufferPos += offset;
				break;
			case std::ios::end:
				if (this->useExceptions && (std::cmp_greater(offset, this->bufferLen) || offset < 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				this->bufferPos = this->bufferLen - offset;
				break;
			default:
				break;
		}
		return *this;
	}

	BufferStream& seek_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->seek(static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPODType T = std::byte>
	BufferStream& skip(std::int64_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seek(sizeof(T) * n, std::ios::cur);
	}

	template<BufferStreamPODType T = std::byte>
	BufferStream& skip_u(std::uint64_t n = 1) {
		return this->skip(static_cast<std::int64_t>(n));
	}

	[[nodiscard]] const std::byte* data() const {
		return this->buffer;
	}

	[[nodiscard]] std::byte* data() {
		return this->buffer;
	}

	[[nodiscard]] std::uint64_t tell() const {
		return this->bufferPos;
	}

	[[nodiscard]] std::uint64_t size() const {
		return this->bufferLen;
	}

	template<BufferStreamPODType T>
	BufferStream& read(T& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		std::memcpy(&obj, this->buffer + this->bufferPos, sizeof(T));
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(&obj);
					} else if constexpr (std::is_enum_v<T>) {
						swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(&obj);
					} else if constexpr (std::is_enum_v<T>) {
						swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
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
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		std::memcpy(this->buffer + this->bufferPos, &obj, sizeof(T));
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(reinterpret_cast<T*>(this->buffer + this->bufferPos));
					} else if constexpr (std::is_enum_v<T>) {
						swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(this->buffer + this->bufferPos));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						swap_endian(reinterpret_cast<T*>(this->buffer + this->bufferPos));
					} else if constexpr (std::is_enum_v<T>) {
						swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(this->buffer + this->bufferPos));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
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

	template<BufferStreamPODType T = std::byte>
	BufferStream& pad(std::uint64_t n = 1) {
		for (std::uint64_t i = 0; i < n * sizeof(T); i++) {
			this->write<std::byte>({});
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& read(T(&obj)[N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * N > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(obj, this->buffer + this->bufferPos, N);
			this->bufferPos += N;
		} else {
			for (std::uint64_t i = 0; i < N; i++) {
				this->read(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& operator>>(T(&obj)[N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& write(const T(&obj)[N]) {
		if (this->bufferPos + sizeof(T) * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * N) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(this->buffer + this->bufferPos, obj, N);
			this->bufferPos += N;
		} else {
			for (std::uint64_t i = 0; i < N; i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& operator<<(const T(&obj)[N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	BufferStream& read(T(&obj)[M][N]) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * M * N > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		for (std::uint64_t i = 0; i < M; i++) {
			for (std::uint64_t j = 0; j < N; j++) {
				this->read(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	BufferStream& operator>>(T(&obj)[M][N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	BufferStream& write(const T(&obj)[M][N]) {
		if (this->bufferPos + sizeof(T) * M * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * M * N) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		for (std::uint64_t i = 0; i < M; i++) {
			for (std::uint64_t j = 0; j < N; j++) {
				this->write(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	BufferStream& operator<<(const T(&obj)[M][N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& read(std::array<T, N>& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * N > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(obj.data(), this->buffer + this->bufferPos, N);
			this->bufferPos += N;
		} else {
			for (std::uint64_t i = 0; i < N; i++) {
				this->read(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& write(const std::array<T, N>& obj) {
		if (this->bufferPos + sizeof(T) * N > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * N) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), N);
			this->bufferPos += N;
		} else {
			for (std::uint64_t i = 0; i < N; i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	BufferStream& operator<<(const std::array<T, N>& obj) {
		return this->write(obj);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	BufferStream& read(T& obj, std::uint64_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(typename T::value_type) * n > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		obj.clear();
		if (!n) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<typename T::value_type> && BufferStreamResizableContiguousContainer<T>) {
			obj.resize(n);
			std::memcpy(obj.data(), this->buffer + this->bufferPos, n);
			this->bufferPos += n;
		} else {
			// BufferStreamPossiblyNonContiguousResizableContainer doesn't guarantee T::reserve(std::uint64_t) exists!
			if constexpr (requires([[maybe_unused]] T& t) {
				{t.reserve(1)} -> std::same_as<void>;
			}) {
				obj.reserve(n);
			}
			for (std::uint64_t i = 0; i < n; i++) {
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
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (!obj.size()) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<typename T::value_type> && (BufferStreamNonResizableContiguousContainer<T> || BufferStreamResizableContiguousContainer<T>)) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), obj.size());
			this->bufferPos += obj.size();
		} else {
			for (decltype(obj.size()) i = 0; i < obj.size(); i++) {
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
	BufferStream& read(T* obj, std::uint64_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * n > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (!n) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(obj, this->buffer + this->bufferPos, n);
			this->bufferPos += n;
		} else {
			for (std::uint64_t i = 0; i < n; i++) {
				obj[i] = this->read<T>();
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& write(const T* obj, std::uint64_t n) {
		if (this->bufferPos + sizeof(T) * n > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * n) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (!n) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<T> && (BufferStreamNonResizableContiguousContainer<T> || BufferStreamResizableContiguousContainer<T>)) {
			std::memcpy(this->buffer + this->bufferPos, obj, n);
			this->bufferPos += n;
		} else {
			for (std::uint64_t i = 0; i < n; i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& read(std::span<T>& obj) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * obj.size() > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (obj.empty()) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(obj.data(), this->buffer + this->bufferPos, obj.size());
			this->bufferPos += obj.size();
		} else {
			for (decltype(obj.size()) i = 0; i < obj.size(); i++) {
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
	BufferStream& read(std::span<T>& obj, std::uint64_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * n > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (!n) {
			return *this;
		}

		if (obj.empty()) {
			obj = std::span<T>{reinterpret_cast<T*>(this->buffer + this->bufferPos), n};
			this->bufferPos += sizeof(T) * n;
		} else {
			if constexpr (BufferStreamPODByteType<T>) {
				std::memcpy(obj.data(), this->buffer + this->bufferPos, n);
				this->bufferPos += n;
			} else {
				for (std::uint64_t i = 0; i < n; i++) {
					obj[i] = this->read<T>();
				}
			}
		}
		return *this;
	}

	template<BufferStreamPODType T>
	BufferStream& write(const std::span<T>& obj) {
		if (this->bufferPos + sizeof(T) * obj.size() > this->bufferLen && !this->resize_buffer(this->bufferPos + sizeof(T) * obj.size()) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		if (obj.empty()) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<T>) {
			std::memcpy(this->buffer + this->bufferPos, obj.data(), obj.size());
			this->bufferPos += obj.size();
		} else {
			for (decltype(obj.size()) i = 0; i < obj.size(); i++) {
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

	BufferStream& write(std::string_view obj, bool addNullTerminator = true, std::uint64_t maxSize = 0) {
		static_assert(BufferStreamPODByteType<typename std::string_view::value_type>, "String char width must be 1 byte!");

		bool bundledTerminator = !obj.empty() && obj[obj.size() - 1] == '\0';
		if (maxSize == 0) {
			// Add true,  bundled true  - one null terminator
			// Add false, bundled true  - null terminator removed
			// Add true,  bundled false - one null terminator
			// Add false, bundled false - no null terminator
			maxSize = obj.size() + addNullTerminator - bundledTerminator;
		}
		if (this->bufferPos + maxSize > this->bufferLen && !this->resize_buffer(this->bufferPos + maxSize) && this->useExceptions) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_WRITE_ERROR_MESSAGE};
		}

		for (std::uint64_t i = 0; i < maxSize; i++) {
			if (i < obj.size()) {
				this->write(obj[i]);
			} else {
				this->write('\0');
			}
		}
		return *this;
	}

	BufferStream& operator<<(std::string_view obj) {
		return this->write(obj);
	}

	BufferStream& write(const std::string& obj, bool addNullTerminator = true, std::uint64_t maxSize = 0) {
		return this->write(std::string_view{obj}, addNullTerminator, maxSize);
	}

	BufferStream& operator<<(const std::string& obj) {
		return this->write(std::string_view{obj});
	}

	BufferStream& read(std::string& obj, std::uint64_t n, bool stopOnNullTerminator = true) {
		if (this->useExceptions && this->bufferPos + sizeof(typename std::string::value_type) * n > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		obj.clear();
		if (!n) {
			return *this;
		}

		obj.reserve(n);
		for (std::uint64_t i = 0; i < n; i++) {
			char temp = this->read<char>();
			if (temp == '\0' && stopOnNullTerminator) {
				// Read the required number of characters and exit
				this->skip_u<char>(n - i - 1);
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

	template<BufferStreamPODType T, std::uint64_t N>
	[[nodiscard]] std::array<T, N> read() {
		std::array<T, N> obj{};
		this->read(obj);
		return obj;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	[[nodiscard]] T read(std::uint64_t n) {
		T obj{};
		this->read(obj, n);
		return obj;
	}

	template<BufferStreamPODType T>
	[[nodiscard]] std::span<T> read_span(std::uint64_t n) {
		if (this->useExceptions && this->bufferPos + sizeof(T) * n > this->bufferLen) {
			throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
		}

		if (!n) {
			return {};
		}

		std::span<T> out{reinterpret_cast<T*>(this->buffer + this->bufferPos), static_cast<std::span<T>::size_type>(n)};
		this->bufferPos += sizeof(T) * n;
		return out;
	}

	[[nodiscard]] std::string read_string() {
		std::string out;
		this->read(out);
		return out;
	}

	[[nodiscard]] std::string read_string(std::uint64_t n, bool stopOnNullTerminator = true) {
		std::string out;
		this->read(out, n, stopOnNullTerminator);
		return out;
	}

	template<std::uint64_t L>
	[[nodiscard]] std::array<std::byte, L> read_bytes() {
		return this->read<std::byte, L>();
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::uint64_t length) {
		std::vector<std::byte> out;
		this->read(out, length);
		return out;
	}

	[[nodiscard]] std::byte at(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) const {
		switch (offsetFrom) {
			case std::ios::beg:
				if (this->useExceptions && (std::cmp_greater(offset, this->bufferLen) || offset < 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				return this->buffer[offset];
			case std::ios::cur:
				if (this->useExceptions && (std::cmp_greater(this->bufferPos + offset, this->bufferLen) || static_cast<int64_t>(this->bufferPos) + offset < 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				return this->buffer[this->bufferPos + offset];
			case std::ios::end:
				if (this->useExceptions && (std::cmp_greater(offset, this->bufferLen) || offset <= 0)) {
					throw std::overflow_error{BUFFERSTREAM_OVERFLOW_READ_ERROR_MESSAGE};
				}
				return this->buffer[this->bufferLen - offset];
			default:
				break;
		}
		return {};
	}

	[[nodiscard]] std::byte at_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) const {
		return this->at(static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPODType T>
	[[nodiscard]] T at(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const T val = this->seek(offset, offsetFrom).read<T>();
		this->seek_u(pos);
		return val;
	}

	template<BufferStreamPODType T>
	[[nodiscard]] T at_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at(static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	[[nodiscard]] std::array<T, N> at(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::array<T, N> val = this->seek(offset, offsetFrom).read<T, N>();
		this->seek_u(pos);
		return val;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	[[nodiscard]] std::array<T, N> at_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at<T, N>(static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	[[nodiscard]] T at(std::uint64_t n, std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const T val = this->seek(offset, offsetFrom).read<T>(n);
		this->seek_u(pos);
		return val;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	[[nodiscard]] T at_u(std::uint64_t n, std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at<T>(n, static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPODType T>
	[[nodiscard]] std::span<T> at_span(std::uint64_t n, std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::span<T> val = this->seek(offset, offsetFrom).read_span<T>(n);
		this->seek_u(pos);
		return val;
	}

	template<BufferStreamPODType T>
	[[nodiscard]] std::span<T> at_span_u(std::uint64_t n, std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at_span<T>(n, static_cast<std::int64_t>(offset), offsetFrom);
	}

	[[nodiscard]] std::string at_string(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::string val = this->seek(offset, offsetFrom).read_string();
		this->seek_u(pos);
		return val;
	}

	[[nodiscard]] std::string at_string_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at_string(static_cast<std::int64_t>(offset), offsetFrom);
	}

	[[nodiscard]] std::string at_string(std::uint64_t n, bool stopOnNullTerminator, std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::string val = this->seek(offset, offsetFrom).read_string(n, stopOnNullTerminator);
		this->seek_u(pos);
		return val;
	}

	[[nodiscard]] std::string at_string_u(std::uint64_t n, bool stopOnNullTerminator, std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at_string(n, stopOnNullTerminator, static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<std::uint64_t L>
	[[nodiscard]] std::array<std::byte, L> at_bytes(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::array<std::byte, L> val = this->seek(offset, offsetFrom).read_bytes<L>();
		this->seek_u(pos);
		return val;
	}

	template<std::uint64_t L>
	[[nodiscard]] std::array<std::byte, L> at_bytes_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at_bytes<L>(static_cast<std::int64_t>(offset), offsetFrom);
	}

	[[nodiscard]] std::vector<std::byte> at_bytes(std::uint64_t length, std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		const std::uint64_t pos = this->tell();
		const std::vector<std::byte> val = this->seek(offset, offsetFrom).read_bytes(length);
		this->seek_u(pos);
		return val;
	}

	[[nodiscard]] std::vector<std::byte> at_bytes_u(std::uint64_t length, std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->at_bytes(length, static_cast<std::int64_t>(offset), offsetFrom);
	}

	[[nodiscard]] std::byte peek() const {
		return this->at(0, std::ios::cur);
	}

	template<BufferStreamPODType T>
	[[nodiscard]] T peek() {
		return this->at<T>(0, std::ios::cur);
	}

	template<BufferStreamPODType T>
	static constexpr void swap_endian(T* t) {
		union {
			T t;
			std::byte bytes[sizeof(T)];
		} source{}, dest{};
		source.t = *t;
		for (std::uint64_t k = 0; k < sizeof(T); k++) {
			dest.bytes[k] = source.bytes[sizeof(T) - k - 1];
		}
		*t = dest.t;
	}

protected:
	std::byte* buffer;
	std::uint64_t bufferLen;
	std::uint64_t bufferPos;
	ResizeCallback bufferResizeCallback;
	bool useExceptions;
	bool bigEndian;

	[[nodiscard]] bool resize_buffer(std::uint64_t newLen) {
		if (!this->bufferResizeCallback) {
			return false;
		}
		this->buffer = this->bufferResizeCallback(this, newLen);
		this->bufferLen = newLen;
		return true;
	}
};

class BufferStreamReadOnly : public BufferStream {
public:
	template<BufferStreamPODType T>
	BufferStreamReadOnly(const T* buffer, std::uint64_t bufferLen)
			: BufferStream(const_cast<T*>(buffer), bufferLen) {}

	template<BufferStreamPODType T, std::uint64_t N>
	explicit BufferStreamReadOnly(T(&buffer)[N])
			: BufferStreamReadOnly(const_cast<const T*>(buffer), sizeof(T) * N) {}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
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
