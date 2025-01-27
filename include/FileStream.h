#pragma once

#include <filesystem>
#include <fstream>

#include "BufferStream.h"

/**
 * This class is provided for convenience, but use BufferStream if you can.
 * It has more features, like reading an object at a given location without
 * seeking, reading to a std::span, etc. This class also has no tests because
 * I was lazy.
 */
class FileStream {
public:
	enum FileStreamOptions {
		OPT_READ                  = 1 << 0,
		OPT_WRITE                 = 1 << 1,
		OPT_APPEND                = 1 << 2,
		OPT_TRUNCATE              = 1 << 3,
		OPT_CREATE_IF_NONEXISTENT = 1 << 4,
	};

	explicit FileStream(const std::string& path, int options = OPT_READ)
			: useExceptions(true)
			, bigEndian(false) {
		if ((options & OPT_CREATE_IF_NONEXISTENT) && !std::filesystem::exists(path)) {
			if (!std::filesystem::exists(std::filesystem::path{path}.parent_path())) {
				std::error_code ec;
				std::filesystem::create_directories(std::filesystem::path{path}.parent_path(), ec);
				ec.clear();
			}
			std::ofstream create(path, std::ios::trunc);
		}
		std::ios::openmode openMode = std::ios::binary;
		if (options & OPT_READ) {
			openMode |= std::ios::in;
		}
		if (options & OPT_WRITE) {
			openMode |= std::ios::out;
		}
		if (options & OPT_APPEND) {
			openMode |= std::ios::out;
			openMode |= std::ios::app;
		}
		if (options & OPT_TRUNCATE) {
			openMode |= std::ios::out;
			openMode |= std::ios::trunc;
		}
		this->file.open(path, openMode);
		this->file.unsetf(std::ios::skipws);
	}

	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(this->file);
	}

	[[nodiscard]] bool are_exceptions_enabled() const {
		return this->useExceptions;
	}

	FileStream& set_exceptions_enabled(bool exceptions) {
		this->useExceptions = exceptions;
		return *this;
	}

	[[nodiscard]] bool is_big_endian() const {
		return this->bigEndian;
	}

	FileStream& set_big_endian(bool readBigEndian) {
		this->bigEndian = readBigEndian;
		return *this;
	}

	FileStream& seek_in(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		// Match behavior in BufferStream::seek
		if (offsetFrom == std::ios::end) {
			offset *= -1;
		}
		this->file.seekg(offset, offsetFrom);
		return *this;
	}

	FileStream& seek_in_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->seek_in(static_cast<std::int64_t>(offset), offsetFrom);
	}

	FileStream& seek_out(std::int64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		// Match behavior in BufferStream::seek
		if (offsetFrom == std::ios::end) {
			offset *= -1;
		}
		this->file.seekp(offset, offsetFrom);
		return *this;
	}

	FileStream& seek_out_u(std::uint64_t offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		return this->seek_out(static_cast<std::int64_t>(offset), offsetFrom);
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skip_in(std::int64_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seek_in(sizeof(T) * n, std::ios::cur);
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skip_in_u(std::uint64_t n = 1) {
		return this->skip_in(static_cast<std::int64_t>(n));
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skip_out(std::int64_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seek_out(sizeof(T) * n, std::ios::cur);
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skip_out_u(std::uint64_t n = 1) {
		return this->skip_out(static_cast<std::int64_t>(n));
	}

	[[nodiscard]] std::uint64_t tell_in() {
		return this->file.tellg();
	}

	[[nodiscard]] std::uint64_t tell_out() {
		return this->file.tellp();
	}

	[[nodiscard]] std::byte peek() {
		return static_cast<std::byte>(this->file.peek());
	}

	template<BufferStreamPODByteType T>
	[[nodiscard]] T peek() {
		return static_cast<T>(this->peek());
	}

	template<BufferStreamPODType T>
	FileStream& read(T& obj) {
		this->file.read(reinterpret_cast<char*>(&obj), sizeof(T));
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						BufferStream::swap_endian(&obj);
					} else if constexpr (std::is_enum_v<T>) {
						BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj));
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
						BufferStream::swap_endian(&obj);
					} else if constexpr (std::is_enum_v<T>) {
						BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj));
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
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& operator>>(T& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T>
	FileStream& write(const T& obj) {
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					T objCopy = obj;
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						BufferStream::swap_endian(&objCopy);
					} else if constexpr (std::is_enum_v<T>) {
						BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&objCopy));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
					this->file.write(reinterpret_cast<const char*>(&objCopy), sizeof(T));
				} else {
					this->file.write(reinterpret_cast<const char*>(&obj), sizeof(T));
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					T objCopy = obj;
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						BufferStream::swap_endian(&objCopy);
					} else if constexpr (std::is_enum_v<T>) {
						BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&objCopy));
					} else {
						// Just don't swap the bytes...
						if (this->useExceptions) {
							throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
						}
					}
					this->file.write(reinterpret_cast<const char*>(&objCopy), sizeof(T));
				} else {
					this->file.write(reinterpret_cast<const char*>(&obj), sizeof(T));
				}
			} else {
				static_assert("Need to investigate what the proper endianness of this platform is!");
			}
		} else {
			this->file.write(reinterpret_cast<const char*>(&obj), sizeof(T));
		}
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& operator<<(const T& obj) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& read(T(&obj)[N]) {
		return this->read(&obj, N);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& operator>>(T(&obj)[N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& write(const T(&obj)[N]) {
		this->write(&obj, N);
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& operator<<(const T(&obj)[N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	FileStream& read(T(&obj)[M][N]) {
		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++) {
				this->read(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	FileStream& operator>>(T(&obj)[M][N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	FileStream& write(const T(&obj)[M][N]) {
		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++) {
				this->write(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::uint64_t M, std::uint64_t N>
	FileStream& operator<<(const T(&obj)[M][N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& read(std::array<T, N>& obj) {
		return this->read(obj.data(), obj.size());
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& write(const std::array<T, N>& obj) {
		return this->write(obj.data(), obj.size());
	}

	template<BufferStreamPODType T, std::uint64_t N>
	FileStream& operator<<(const std::array<T, N>& obj) {
		return this->write(obj);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	FileStream& read(T& obj, std::uint64_t n) {
		obj.clear();
		if (!n) {
			return *this;
		}

		if constexpr (BufferStreamPODByteType<typename T::value_type> && BufferStreamResizableContiguousContainer<T>) {
			obj.resize(n);
			this->file.read(reinterpret_cast<char*>(obj.data()), sizeof(typename T::value_type) * n);
		} else {
			// BufferStreamPossiblyNonContiguousResizableContainer doesn't guarantee T::reserve(std::uint64_t) exists!
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
	FileStream& operator>>(T& obj) {
		obj.push_back(this->read<typename T::value_type>());
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	FileStream& write(const T& obj) {
		if (obj.size() == 0) {
			return *this;
		}

		if constexpr (BufferStreamNonResizableContiguousContainer<T> || BufferStreamResizableContiguousContainer<T>) {
			this->write(obj.data(), obj.size());
		} else {
			for (int i = 0; i < obj.size(); i++) {
				this->write(obj[i]);
			}
		}
		return *this;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	FileStream& operator<<(const T& obj) {
		return this->write(obj);
	}

	template<BufferStreamPODType T>
	FileStream& read(T* obj, std::uint64_t n) {
		if (!n) {
			return *this;
		}

		this->file.read(reinterpret_cast<char*>(obj), sizeof(T) * n);
		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
						for (std::uint64_t i = 0; i < n; i++) {
							BufferStream::swap_endian(&obj[i]);
						}
					} else if constexpr (std::is_enum_v<T>) {
						for (std::uint64_t i = 0; i < n; i++) {
							BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj[i]));
						}
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
						for (std::uint64_t i = 0; i < n; i++) {
							BufferStream::swap_endian(&obj[i]);
						}
					} else if constexpr (std::is_enum_v<T>) {
						for (std::uint64_t i = 0; i < n; i++) {
							BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&obj[i]));
						}
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
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& write(const T* obj, std::uint64_t n) {
		if (!n) {
			return *this;
		}

		if constexpr (sizeof(T) > 1) {
			if constexpr (std::endian::native == std::endian::little) {
				if (this->bigEndian) {
					for (std::uint64_t i = 0; i < n; i++) {
						T objCopy = obj[i];
						if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
							BufferStream::swap_endian(&objCopy);
						} else if constexpr (std::is_enum_v<T>) {
							BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&objCopy));
						} else {
							// Just don't swap the bytes...
							if (this->useExceptions) {
								throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
							}
						}
						this->file.write(reinterpret_cast<const char*>(&objCopy), sizeof(T));
					}
				} else {
					this->file.write(reinterpret_cast<const char*>(obj), sizeof(T) * n);
				}
			} else if constexpr (std::endian::native == std::endian::big) {
				if (!this->bigEndian) {
					for (std::uint64_t i = 0; i < n; i++) {
						T objCopy = obj[i];
						if constexpr (std::is_integral_v<T> || std::floating_point<T>) {
							BufferStream::swap_endian(&objCopy);
						} else if constexpr (std::is_enum_v<T>) {
							BufferStream::swap_endian(reinterpret_cast<std::underlying_type_t<T>*>(&objCopy));
						} else {
							// Just don't swap the bytes...
							if (this->useExceptions) {
								throw std::invalid_argument{BUFFERSTREAM_BIG_ENDIAN_POD_TYPE_ERROR_MESSAGE};
							}
						}
						this->file.write(reinterpret_cast<const char*>(&objCopy), sizeof(T));
					}
				} else {
					this->file.write(reinterpret_cast<const char*>(obj), sizeof(T) * n);
				}
			} else {
				static_assert("Need to investigate what the proper endianness of this platform is!");
			}
		} else {
			this->file.write(reinterpret_cast<const char*>(obj), sizeof(T) * n);
		}
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& write(const std::span<T>& obj) {
		return this->write(obj.data(), obj.size());
	}

	template<BufferStreamPODType T>
	FileStream& operator<<(const std::span<T>& obj) {
		return this->write(obj);
	}

	FileStream& read(std::string& obj) {
		obj.clear();
		char temp = this->read<char>();
		while (temp != '\0') {
			obj += temp;
			temp = this->read<char>();
		}
		return *this;
	}

	FileStream& operator>>(std::string& obj) {
		return this->read(obj);
	}

	FileStream& write(std::string_view obj, bool addNullTerminator = true, std::uint64_t maxSize = 0) {
		static_assert(sizeof(typename std::string_view::value_type) == 1, "String char width must be 1 byte!");

		bool bundledTerminator = !obj.empty() && obj[obj.size() - 1] == '\0';
		if (maxSize == 0) {
			// Add true,  bundled true  - one null terminator
			// Add false, bundled true  - null terminator removed
			// Add true,  bundled false - one null terminator
			// Add false, bundled false - no null terminator
			maxSize = obj.size() + addNullTerminator - bundledTerminator;
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

	FileStream& operator<<(std::string_view obj) {
		return this->write(obj);
	}

	FileStream& write(const std::string& obj, bool addNullTerminator = true, std::uint64_t maxSize = 0) {
		return this->write(std::string_view{obj}, addNullTerminator, maxSize);
	}

	FileStream& operator<<(const std::string& obj) {
		return this->write(std::string_view{obj});
	}

	FileStream& read(std::string& obj, std::uint64_t n, bool stopOnNullTerminator = true) {
		obj.clear();
		if (!n) {
			return *this;
		}

		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			char temp = this->read<char>();
			if (temp == '\0' && stopOnNullTerminator) {
				// Read the required number of characters and exit
				this->skip_in_u<char>(n - i - 1);
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
	std::array<T, N> read() {
		std::array<T, N> obj{};
		this->read(obj);
		return obj;
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	T read(std::uint64_t n) {
		T obj{};
		this->read(obj, n);
		return obj;
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
		return this->read<std::array<std::byte, L>>();
	}

	[[nodiscard]] std::vector<std::byte> read_bytes(std::uint64_t length) {
		std::vector<std::byte> out;
		this->read(out, length);
		return out;
	}

	void flush() {
		this->file.flush();
	}

protected:
	std::fstream file;
	bool useExceptions;
	bool bigEndian;
};
