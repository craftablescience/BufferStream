#include <gtest/gtest.h>

#include <deque>

#include <BufferStream.h>

struct POD {
	int x;
	int y;

	[[nodiscard]] bool operator==(const POD&) const = default;
};

TEST(BufferStream, open) {
	// Pointer ctor
	{
		std::array<std::byte, 1> buffer{};
		BufferStream stream{buffer.data(), buffer.size()};
		EXPECT_EQ(stream.size(), 1);
	}

	// C array ctor
	{
		std::byte buffer[1] = {{}};
		BufferStream stream{buffer};
		EXPECT_EQ(stream.size(), 1);
	}
	{
		std::byte buffer[1][1] = {{{}}};
		BufferStream stream{buffer};
		EXPECT_EQ(stream.size(), 1);
	}

	// STL container ctor
	{
		std::array<std::byte, 1> buffer{};
		BufferStream stream{buffer};
		EXPECT_EQ(stream.size(), 1);
	}
	{
		std::vector<std::byte> buffer{{}};
		BufferStream stream{buffer};
		EXPECT_EQ(stream.size(), 1);
	}
	{
		std::string buffer = "_";
		BufferStream stream{buffer};
		EXPECT_EQ(stream.size(), 1);
	}
}

TEST(BufferStream, read_big_endian) {
	{
		std::byte x{0xAB};
		BufferStream stream{&x, sizeof(x)};

		auto y = stream.set_big_endian(true).read<std::byte>();
		EXPECT_EQ(y, std::byte{0xAB});
	}
	{
		std::uint32_t x = 0xAB'CD'EF'00;
		BufferStream stream{&x, 1};

		auto y = stream.set_big_endian(true).read<std::uint32_t>();
		EXPECT_EQ(y, 0x00'EF'CD'AB);
	}
	{
		enum class Test : uint32_t {
			TEST_ME = 1,
		};

		std::uint32_t x = 0x01'00'00'00;
		BufferStream stream{&x, 1};

		auto y = stream.set_big_endian(true).read<Test>();
		EXPECT_EQ(y, Test::TEST_ME);
	}
}

TEST(BufferStream, write_big_endian) {
	{
		std::byte x{0xAB};
		BufferStream stream{&x, sizeof(x)};

		stream.set_big_endian(true).write(std::byte{0xBC}).set_big_endian(false).seek(0);
		EXPECT_EQ(stream.read<std::byte>(), std::byte{0xBC});
	}
	{
		std::uint32_t x = 0;
		BufferStream stream{&x, 1};

		stream.set_big_endian(true).write(0xAB'CD'EF'00).set_big_endian(false).seek(0);
		EXPECT_EQ(stream.read<std::uint32_t>(), 0x00'EF'CD'AB);
	}
	{
		enum class Test : uint32_t {
			TEST_ME = 1,
		};

		std::uint32_t x = 0;
		BufferStream stream{&x, 1};

		stream.set_big_endian(true).write(Test::TEST_ME).set_big_endian(false).seek(0);
		EXPECT_EQ(static_cast<uint32_t>(stream.read<Test>()), 0x01'00'00'00);
	}
}

TEST(BufferStream, seek) {
	std::vector<unsigned char> buffer{{}};
	BufferStream stream{buffer};

	stream.seek(1, std::ios::beg);
	EXPECT_EQ(stream.tell(), 1);

	try {
		stream.seek(2, std::ios::beg);
		FAIL();
	} catch (const std::overflow_error&) {}

	stream.seek(-1, std::ios::cur);
	EXPECT_EQ(stream.tell(), 0);

	try {
		stream.seek(-1, std::ios::cur);
		FAIL();
	} catch (const std::overflow_error&) {}

	stream.seek(1, std::ios::end);
	EXPECT_EQ(stream.tell(), 0);

	try {
		stream.seek(-2, std::ios::end);
		FAIL();
	} catch (const std::overflow_error&) {}
}

TEST(BufferStream, skip) {
	std::vector<unsigned char> buffer;
	buffer.resize(8);
	BufferStream stream{buffer};

	EXPECT_EQ(stream.tell(), 0);

	stream.skip(1);
	EXPECT_EQ(stream.tell(), 1);

	stream.skip(2);
	EXPECT_EQ(stream.tell(), 3);

	stream.skip<std::int16_t>(1);
	EXPECT_EQ(stream.tell(), 3 + sizeof(std::int16_t));

	stream.skip<std::int16_t>(-1);
	EXPECT_EQ(stream.tell(), 3);
}

TEST(BufferStream, read_int_ref) {
	{
		int x = 10;
		BufferStream stream{&x, 1};

		int y = 0;
		stream.read(y);
		EXPECT_EQ(y, x);
	}
	{
		int x = 10;
		BufferStream stream{&x, 1};

		int y = 0;
		stream >> y;
		EXPECT_EQ(y, x);
	}
}

TEST(BufferStream, write_int_ref) {
	{
		int x = 10;
		BufferStream stream{&x, 1};

		int y = 0;
		EXPECT_EQ(stream.write(y).seek(0).read<int>(), y);
	}
	{
		int x = 10;
		BufferStream stream{&x, 1};

		int y = 0;
		stream << y;
		EXPECT_EQ(stream.seek(0).read<int>(), y);
	}
}

TEST(BufferStream, read_pod_ref) {
	{
		POD pod{10, 42};
		BufferStream stream{&pod, 1};

		POD read{};
		stream.read(read);
		EXPECT_EQ(read.x, pod.x);
		EXPECT_EQ(read.y, pod.y);
	}
	{
		POD pod{10, 42};
		BufferStream stream{&pod, 1};

		POD read{};
		stream >> read;
		EXPECT_EQ(read.x, pod.x);
		EXPECT_EQ(read.y, pod.y);
	}
}

TEST(BufferStream, write_pod_ref) {
	{
		POD pod{10, 42};
		BufferStream stream{&pod, 1};

		stream.write(POD{20, 84});
		auto read = stream.seek(0).read<POD>();
		EXPECT_EQ(read.x, 20);
		EXPECT_EQ(read.y, 84);
	}
	{
		POD pod{10, 42};
		BufferStream stream{&pod, 1};

		stream << POD{20, 84};
		auto read = stream.seek(0).read<POD>();
		EXPECT_EQ(read.x, 20);
		EXPECT_EQ(read.y, 84);
	}
}

TEST(BufferStream, read_c_array_ref) {
	{
		POD podArray[] = {{10, 42}, {20, 84}};
		BufferStream stream{podArray};

		POD read[] = {{0, 0}, {0, 0}};
		stream.read(read);
		EXPECT_EQ(read[0].x, 10);
		EXPECT_EQ(read[0].y, 42);
		EXPECT_EQ(read[1].x, 20);
		EXPECT_EQ(read[1].y, 84);
	}
	{
		POD podArray[2] = {{10, 42}, {20, 84}};
		BufferStream stream{podArray};

		POD read[2] = {{0, 0}, {0, 0}};
		stream >> read;
		EXPECT_EQ(read[0].x, 10);
		EXPECT_EQ(read[0].y, 42);
		EXPECT_EQ(read[1].x, 20);
		EXPECT_EQ(read[1].y, 84);
	}
	{
		POD podArray[1][2] = {{{10, 42}, {20, 84}}};
		BufferStream stream{podArray};

		POD read[1][2] = {{{10, 42}, {20, 84}}};
		stream.read(read);
		EXPECT_EQ(read[0][0].x, 10);
		EXPECT_EQ(read[0][0].y, 42);
		EXPECT_EQ(read[0][1].x, 20);
		EXPECT_EQ(read[0][1].y, 84);
	}
	{
		POD podArray[1][2] = {{{10, 42}, {20, 84}}};
		BufferStream stream{podArray};

		POD read[1][2] = {{{10, 42}, {20, 84}}};
		stream >> read;
		EXPECT_EQ(read[0][0].x, 10);
		EXPECT_EQ(read[0][0].y, 42);
		EXPECT_EQ(read[0][1].x, 20);
		EXPECT_EQ(read[0][1].y, 84);
	}
}

TEST(BufferStream, write_c_array_ref) {
	{
		POD podArray[] = {{10, 42}, {20, 84}};
		BufferStream stream{podArray};

		POD write[] = {{20, 84}, {40, 168}};
		POD read[] = {{0, 0}, {0, 0}};
		stream.write(write).seek(0).read(read);
		EXPECT_EQ(read[0].x, 20);
		EXPECT_EQ(read[0].y, 84);
		EXPECT_EQ(read[1].x, 40);
		EXPECT_EQ(read[1].y, 168);
	}
	{
		POD podArray[2] = {{10, 42}, {20, 84}};
		BufferStream stream{podArray};

		POD write[] = {{20, 84}, {40, 168}};
		POD read[] = {{0, 0}, {0, 0}};
		stream << write;
		stream.seek(0);
		stream >> read;
		EXPECT_EQ(read[0].x, 20);
		EXPECT_EQ(read[0].y, 84);
		EXPECT_EQ(read[1].x, 40);
		EXPECT_EQ(read[1].y, 168);
	}
	{
		POD podArray[1][2] = {{{10, 42}, {20, 84}}};
		BufferStream stream{podArray};

		POD write[1][2] = {{{20, 84}, {40, 168}}};
		POD read[1][2] = {{{0, 0}, {0, 0}}};
		stream.write(write).seek(0).read(read);
		EXPECT_EQ(read[0][0].x, 20);
		EXPECT_EQ(read[0][0].y, 84);
		EXPECT_EQ(read[0][1].x, 40);
		EXPECT_EQ(read[0][1].y, 168);
	}
	{
		POD podArray[1][2] = {{{10, 42}, {20, 84}}};
		BufferStream stream{podArray};

		POD write[1][2] = {{{20, 84}, {40, 168}}};
		POD read[1][2] = {{{0, 0}, {0, 0}}};
		stream << write;
		stream.seek(0);
		stream >> read;
		EXPECT_EQ(read[0][0].x, 20);
		EXPECT_EQ(read[0][0].y, 84);
		EXPECT_EQ(read[0][1].x, 40);
		EXPECT_EQ(read[0][1].y, 168);
	}
}

TEST(BufferStream, read_pointer) {
	POD podArray[] = {{10, 42}, {20, 84}};
	BufferStream stream{podArray};

	POD read[] = {{}, {}};
	stream.read(static_cast<POD*>(read), 2);
	EXPECT_EQ(read[0].x, 10);
	EXPECT_EQ(read[0].y, 42);
	EXPECT_EQ(read[1].x, 20);
	EXPECT_EQ(read[1].y, 84);
}

TEST(BufferStream, write_pointer) {
	POD podArray[] = {{}, {}};
	BufferStream stream{podArray};

	POD write[] = {{10, 42}, {20, 84}};
	stream.write(static_cast<POD*>(write), 2);
	EXPECT_EQ(podArray[0].x, 10);
	EXPECT_EQ(podArray[0].y, 42);
	EXPECT_EQ(podArray[1].x, 20);
	EXPECT_EQ(podArray[1].y, 84);
}

TEST(BufferStream, read_array_ref) {
	{
		std::array<int, 2> array{10, 42};
		BufferStream stream{array};

		std::array<int, 2> read{};
		stream.read(read);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::array<int, 2> array{10, 42};
		BufferStream stream{array};

		std::array<int, 2> read{};
		stream >> read;
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
}

TEST(BufferStream, write_array_ref) {
	{
		std::array<int, 2> array{0, 0};
		BufferStream stream{array};

		std::array<int, 2> write{10, 42};
		std::array<int, 2> read{};
		stream.write(write).seek(0).read(read);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::array<int, 2> array{0, 0};
		BufferStream stream{array};

		std::array<int, 2> write{10, 42};
		std::array<int, 2> read{};
		stream.write(write).seek(0).read(read);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
}

TEST(BufferStream, read_stl_container_ref) {
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> read;
		stream.read(read, 2);
		EXPECT_EQ(read[0], 'A');
		EXPECT_EQ(read[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> read;
		stream >> read >> read;
		EXPECT_EQ(read[0], 'A');
		EXPECT_EQ(read[1], 'B');
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> read;
		stream.read(read, 2);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> read;
		stream >> read >> read;
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::deque<int> read;
		stream.read(read, 2);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::deque<int> read;
		stream >> read >> read;
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
}

TEST(BufferStream, write_stl_container_ref) {
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write;
		stream.write(write);
		EXPECT_EQ(array[0], 'A');
		EXPECT_EQ(array[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write;
		stream << write;
		EXPECT_EQ(array[0], 'A');
		EXPECT_EQ(array[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write{'C', 'D'};
		stream.write(write);
		EXPECT_EQ(array[0], 'C');
		EXPECT_EQ(array[1], 'D');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write{'C', 'D'};
		stream << write;
		EXPECT_EQ(array[0], 'C');
		EXPECT_EQ(array[1], 'D');
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> write{20, 84};
		stream.write(write);
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> write{20, 84};
		stream << write;
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::deque<int> write{20, 84};
		stream.write(write);
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::deque<int> write{20, 84};
		stream << write;
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
}

TEST(BufferStream, read_span_ref) {
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::span<char> read;
		stream.read(read, 2);
		EXPECT_EQ(read[0], 'A');
		EXPECT_EQ(read[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> readBacking(2);
		std::span<char> read{readBacking};
		stream.read(read);
		EXPECT_EQ(read[0], 'A');
		EXPECT_EQ(read[1], 'B');
		EXPECT_EQ(readBacking[0], 'A');
		EXPECT_EQ(readBacking[1], 'B');
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::span<int> read;
		stream.read(read, 2);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> readBacking(2);
		std::span<int> read{readBacking};
		stream.read(read);
		EXPECT_EQ(read[0], 10);
		EXPECT_EQ(read[1], 42);
		EXPECT_EQ(readBacking[0], 10);
		EXPECT_EQ(readBacking[1], 42);
	}
}

TEST(BufferStream, write_span_ref) {
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::span<char> emptySpan;
		stream.write(emptySpan);
		EXPECT_EQ(array[0], 'A');
		EXPECT_EQ(array[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::span<char> emptySpan;
		stream << emptySpan;
		EXPECT_EQ(array[0], 'A');
		EXPECT_EQ(array[1], 'B');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write{'C', 'D'};
		std::span<char> writeSpan{write};
		stream.write(writeSpan);
		EXPECT_EQ(array[0], 'C');
		EXPECT_EQ(array[1], 'D');
	}
	{
		std::vector<char> array{'A', 'B'};
		BufferStream stream{array};

		std::vector<char> write{'C', 'D'};
		std::span<char> writeSpan{write};
		stream << writeSpan;
		EXPECT_EQ(array[0], 'C');
		EXPECT_EQ(array[1], 'D');
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> write{20, 84};
		std::span<int> writeSpan{write};
		stream.write(writeSpan);
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
	{
		std::vector<int> array{10, 42};
		BufferStream stream{array};

		std::vector<int> write{20, 84};
		std::span<int> writeSpan{write};
		stream << writeSpan;
		EXPECT_EQ(array[0], 20);
		EXPECT_EQ(array[1], 84);
	}
}

TEST(BufferStream, read_string_ref) {
	std::string buffer = "Hello world";
	buffer.push_back('\0');
	buffer.push_back('\0');
	buffer.push_back('\0');
	BufferStream stream{buffer};

	{
		std::string read;
		stream.read(read);
		EXPECT_STREQ(read.c_str(), "Hello world");
	}
	stream.seek(0);

	{
		std::string read;
		stream >> read;
		EXPECT_STREQ(read.c_str(), "Hello world");
	}
	stream.seek(0);

	{
		std::string read;
		stream.read(read, 5);
		EXPECT_STREQ(read.c_str(), "Hello");
	}
	stream.seek(0);

	{
		std::string read;
		stream.read(read, 13);
		EXPECT_EQ(read.size(), 11);
		EXPECT_EQ(stream.tell(), 13);
	}
	stream.seek(0);

	{
		std::string read;
		stream.read(read, 13, false);
		EXPECT_EQ(read.size(), 13);
		EXPECT_EQ(stream.tell(), 13);
	}
	stream.seek(0);
}

TEST(BufferStream, write_string_ref) {
	std::string buffer;
	BufferStream stream{buffer};

	std::string data = "Hello world";

	{
		std::string read;
		stream.write(data).seek(0).read(read);
		EXPECT_STREQ(read.c_str(), "Hello world");
	}
	stream.seek(0);
	buffer.clear();

	{
		std::string read;
		stream << data;
		stream.seek(0);
		stream >> read;
		EXPECT_STREQ(read.c_str(), "Hello world");
	}
	stream.seek(0);
	buffer.clear();

	{
		std::string read;
		stream.write(data, false).seek(0).read(read, data.size());
		EXPECT_STREQ(read.c_str(), "Hello world");
	}
	stream.seek(0);
	buffer.clear();

	{
		std::string read;
		stream.write(data, false, 5).seek(0).read(read, 5);
		EXPECT_STREQ(read.c_str(), "Hello");
	}
	stream.seek(0);
	buffer.clear();
}

TEST(BufferStream, read_int) {
	int x = 10;
	BufferStream stream{&x, 1};

	EXPECT_EQ(stream.read<decltype(x)>(), x);
}

TEST(BufferStream, read_pod) {
	POD pod{10, 42};
	BufferStream stream{&pod, 1};

	auto read = stream.read<POD>();
	EXPECT_EQ(read.x, pod.x);
	EXPECT_EQ(read.y, pod.y);
}

TEST(BufferStream, read_array) {
	std::array<int, 2> podArray{10, 42};
	BufferStream stream{podArray};

	auto read = stream.read<int, 2>();
	EXPECT_EQ(read[0], podArray[0]);
	EXPECT_EQ(read[1], podArray[1]);
}

TEST(BufferStream, read_stl_container) {
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.read<std::vector<int>>(2);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.read<std::deque<int>>(2);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
}

TEST(BufferStream, read_span) {
	{
		std::vector<char> podArray{'A', 'B'};
		BufferStream stream{podArray};

		auto read = stream.read_span<char>(2);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.read_span<int>(2);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
}

TEST(BufferStream, read_string) {
	std::string buffer = "Hello world";
	buffer.push_back('\0');
	buffer.push_back('\0');
	buffer.push_back('\0');
	BufferStream stream{buffer};

	EXPECT_STREQ(stream.read_string().c_str(), "Hello world");
	stream.seek(0);

	EXPECT_STREQ(stream.read_string(5).c_str(), "Hello");
	stream.seek(0);

	EXPECT_EQ(stream.read_string(13).size(), 11);
	EXPECT_EQ(stream.tell(), 13);
	stream.seek(0);

	EXPECT_EQ(stream.read_string(13, false).size(), 13);
	EXPECT_EQ(stream.tell(), 13);
	stream.seek(0);
}

TEST(BufferStream, read_bytes) {
	{
		int x = 10;
		BufferStream stream{&x, 1};

		std::array<std::byte, sizeof(x)> bytes = stream.read_bytes<sizeof(x)>();
		EXPECT_EQ(bytes.size(), 4);
		EXPECT_EQ(bytes.at(0), std::byte{10});
	}
	{
		int x = 10;
		BufferStream stream{&x, 1};

		std::vector<std::byte> bytes = stream.read_bytes(sizeof(x));
		EXPECT_EQ(bytes.size(), 4);
		EXPECT_EQ(bytes.at(0), std::byte{10});
	}
}

TEST(BufferStream, at_int) {
	int x = 10;
	BufferStream stream{&x, 1};

	EXPECT_EQ(stream.at<decltype(x)>(0), x);
}

TEST(BufferStream, at_pod) {
	POD pod{10, 42};
	BufferStream stream{&pod, 1};

	EXPECT_EQ(stream.at<int>(0), pod.x);
	EXPECT_EQ(stream.at<int>(sizeof(int)), pod.y);
	EXPECT_EQ(stream.at<POD>(0), pod);
}

TEST(BufferStream, at_array) {
	std::array<int, 2> podArray{10, 42};
	BufferStream stream{podArray};

	EXPECT_EQ(stream.at<int>(0), podArray[0]);
	EXPECT_EQ(stream.at<int>(sizeof(int)), podArray[1]);
	EXPECT_EQ((stream.at<int, 2>(0)), podArray);
}

TEST(BufferStream, at_stl_container) {
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.at<std::vector<int>>(2, 0);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.at<std::deque<int>>(2, 0);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
}

TEST(BufferStream, at_span) {
	{
		std::vector<char> podArray{'A', 'B'};
		BufferStream stream{podArray};

		auto read = stream.at_span<char>(2, 0);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
	{
		std::vector<int> podArray{10, 42};
		BufferStream stream{podArray};

		auto read = stream.at_span<int>(2, 0);
		EXPECT_EQ(read[0], podArray[0]);
		EXPECT_EQ(read[1], podArray[1]);
	}
}

TEST(BufferStream, at_string) {
	std::string buffer = "Hello world";
	buffer.push_back('\0');
	buffer.push_back('\0');
	buffer.push_back('\0');
	BufferStream stream{buffer};

	EXPECT_STREQ(stream.at_string(0).c_str(), "Hello world");
	EXPECT_STREQ(stream.at_string(5, false, 0).c_str(), "Hello");
	EXPECT_EQ(stream.at_string(13, true, 0).size(), 11);
	EXPECT_EQ(stream.at_string(13, false, 0).size(), 13);
}

TEST(BufferStream, at_bytes) {
	{
		int x = 10;
		BufferStream stream{&x, 1};

		std::array<std::byte, sizeof(x)> bytes = stream.at_bytes<sizeof(x)>(0);
		EXPECT_EQ(bytes.size(), 4);
		EXPECT_EQ(bytes.at(0), std::byte{10});
	}
	{
		int x = 10;
		BufferStream stream{&x, 1};

		std::vector<std::byte> bytes = stream.at_bytes(sizeof(x), 0);
		EXPECT_EQ(bytes.size(), 4);
		EXPECT_EQ(bytes.at(0), std::byte{10});
	}
}

TEST(BufferStream, peek) {
	std::string buffer = "Hello";
	BufferStream stream{buffer};

	EXPECT_EQ(stream.seek(1).peek(), std::byte{'e'});
	EXPECT_EQ(stream.seek(2).peek<char>(), 'l');
}
