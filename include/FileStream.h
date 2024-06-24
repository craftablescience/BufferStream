#pragma once

#include <filesystem>
#include <fstream>

#include "BufferStream.h"

/**
 * This class is provided for convenience, but use BufferStream if you can.
 * It has more features, like peek, big-endian support, reading to a std::span, etc.
 * This class also has no tests because I was lazy.
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
			: file() {
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

	FileStream& seekIn(std::fstream::pos_type offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		this->file.seekg(offset, offsetFrom);
		return *this;
	}

	FileStream& seekOut(std::fstream::pos_type offset, std::ios::seekdir offsetFrom = std::ios::beg) {
		this->file.seekp(offset, offsetFrom);
		return *this;
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skipIn(std::size_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seekIn(sizeof(T) * n, std::ios::cur);
	}

	template<BufferStreamPODType T = std::byte>
	FileStream& skipOut(std::size_t n = 1) {
		if (!n) {
			return *this;
		}
		return this->seekOut(sizeof(T) * n, std::ios::cur);
	}

	[[nodiscard]] std::size_t tellIn() {
		return this->file.tellg();
	}

	[[nodiscard]] std::size_t tellOut() {
		return this->file.tellp();
	}

	template<BufferStreamPODType T>
	FileStream& read(T& obj) {
		this->file.read(reinterpret_cast<char*>(&obj), sizeof(T));
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& operator>>(T& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T>
	FileStream& write(const T& obj) {
		this->file.write(reinterpret_cast<const char*>(&obj), sizeof(T));
		return *this;
	}

	template<BufferStreamPODType T>
	FileStream& operator<<(const T& obj) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& read(T(&obj)[N]) {
		this->file.read(reinterpret_cast<char*>(obj), sizeof(T) * N);
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& operator>>(T(&obj)[N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& write(const T(&obj)[N]) {
		this->file.write(reinterpret_cast<const char*>(obj), sizeof(T) * N);
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& operator<<(const T(&obj)[N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	FileStream& read(T(&obj)[M][N]) {
		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++) {
				this->read(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	FileStream& operator>>(T(&obj)[M][N]) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	FileStream& write(const T(&obj)[M][N]) {
		for (int i = 0; i < M; i++) {
			for (int j = 0; j < N; j++) {
				this->write(obj[i][j]);
			}
		}
		return *this;
	}

	template<BufferStreamPODType T, std::size_t M, std::size_t N>
	FileStream& operator<<(const T(&obj)[M][N]) {
		return this->write(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& read(std::array<T, N>& obj) {
		this->file.read(reinterpret_cast<char*>(obj.data()), sizeof(T) * N);
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& operator>>(std::array<T, N>& obj) {
		return this->read(obj);
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& write(const std::array<T, N>& obj) {
		this->file.write(reinterpret_cast<const char*>(obj.data()), sizeof(T) * N);
		return *this;
	}

	template<BufferStreamPODType T, std::size_t N>
	FileStream& operator<<(const std::array<T, N>& obj) {
		return this->write(obj);
	}

	template<BufferStreamPossiblyNonContiguousResizableContainer T>
	FileStream& read(T& obj, std::size_t n) {
		obj.clear();
		if (!n) {
			return *this;
		}

		if constexpr (BufferStreamResizableContiguousContainer<T>) {
			obj.resize(n);
			this->file.read(reinterpret_cast<char*>(obj.data()), sizeof(typename T::value_type) * n);
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
			this->file.write(reinterpret_cast<const char*>(obj.data()), sizeof(T::value_type) * obj.size());
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
	FileStream& write(const std::span<T>& obj) {
		if (!obj.empty()) {
			this->file.write(reinterpret_cast<const char*>(obj.data()), sizeof(T) * obj.size());
		}
		return *this;
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

	FileStream& write(const std::string& obj, bool addNullTerminator = true, std::size_t maxSize = 0) {
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

	FileStream& operator<<(const std::string& obj) {
		return this->write(obj);
	}

	FileStream& read(std::string& obj, std::size_t n, bool stopOnNullTerminator = true) {
		obj.clear();
		if (!n) {
			return *this;
		}

		obj.reserve(n);
		for (int i = 0; i < n; i++) {
			char temp = this->read<char>();
			if (temp == '\0' && stopOnNullTerminator) {
				// Read the required number of characters and exit
				this->skipIn<char>(n - i - 1);
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
	std::fstream file;
};
