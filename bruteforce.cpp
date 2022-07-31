#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

constexpr std::array<std::array<uint32_t, 256>, 5> hashsource = []() {
	uint32_t seed = 0x00100001;
	std::array<std::array<uint32_t, 256>, 5> ret = {};

	for (unsigned i = 0; i < 256; i++) {
		for (unsigned j = 0; j < 5; j++) { // NOLINT(modernize-loop-convert)
			seed = (125 * seed + 3) % 0x2AAAAB;
			uint32_t ch = (seed & 0xFFFF);
			seed = (125 * seed + 3) % 0x2AAAAB;
			ret[j][i] = ch << 16 | (seed & 0xFFFF);
		}
	}
	return ret;
}();

uint32_t prefixHash[4];

uint32_t HashPrefix(std::string_view filename, uint8_t type)
{
	uint32_t result = 0x7FED7FED;
	uint32_t adjust = 0xEEEEEEEE;
	for (unsigned i = 0; i < filename.length(); i++) {
		int8_t origchar = *filename.substr(i, 1).data();
		result = (result + adjust) ^ hashsource[type][origchar];
		adjust += origchar + result + (adjust << 5) + 3;
	}

	prefixHash[(type - 1) * 2 + 0] = result;
	prefixHash[(type - 1) * 2 + 1] = adjust;

	return result;
}

uint32_t Hash1(std::string_view filename)
{
	uint32_t result = prefixHash[0];
	uint32_t adjust = prefixHash[1];
	for (unsigned i = 0; i < filename.length(); i++) {
		int8_t origchar = *filename.substr(i, 1).data();
		result = (result + adjust) ^ hashsource[1][origchar];
		adjust += origchar + result + (adjust << 5) + 3;
	}

	return result;
}

uint32_t Hash2(std::string_view filename)
{
	uint32_t result = prefixHash[2];
	uint32_t adjust = prefixHash[3];
	for (unsigned i = 0; i < filename.length(); i++) {
		int8_t origchar = *filename.substr(i, 1).data();
		result = (result + adjust) ^ hashsource[2][origchar];
		adjust += origchar + result + (adjust << 5) + 3;
	}

	return result;
}

/*
00D45800: File "items\wshield.cel"
00D46A00: File "File00000101.xxx"
00D46E00: File "levels\l1data\hero1.dun"
00D47000: File "levels\l1data\hero2.dun"
*/

// hash unknown, // TODd take as arg
constexpr uint32_t CRACK1 = 0xB29FC135;
constexpr uint32_t CRACK2 = 0x22575C4A;

// LEVELS\\L1DATA\\SKNGDO.DUN, 14 sec on ryzen 3700x
//constexpr uint32_t CRACK1 = 0x3CC2BEC6;
//constexpr uint32_t CRACK2 = 0xC2F426EA;

// Sorted, contiguous up to Z (90), underscore is 95.
// todo take as arg
std::array<char, 39> letters {
	'-', '.', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
	'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_'
};
std::array<char, 256> nextLetter;

void save(std::string_view result)
{
	std::ofstream myfile;
	myfile.open("cracked.txt");
	myfile << result << "\n";
	myfile.close();
}

bool nextString(char *str, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (str[i] != letters.back()) {
			memset(str, letters.front(), i);
			str[i] = nextLetter[static_cast<unsigned char>(str[i])];
			return true;
		}
	}
	return false;
}

std::stop_source stop_source;

void match(std::string_view str)
{
	if (Hash1(str) == CRACK1 && Hash2(str) == CRACK2) {
		std::cout << "Found match: " << str << std::endl;
		save(str);
		stop_source.request_stop();
	}
}

int main(int argc, char *argv[])
{
	for (size_t i = 1; i < letters.size(); ++i) {
		nextLetter[static_cast<unsigned char>(letters[i - 1])] = letters[i];
	}

	std::string prefix = "LEVELS\\L1DATA\\"; // TODO take as arg
	HashPrefix(prefix, 1);
	HashPrefix(prefix, 2);

	{
		std::vector<std::jthread> threads;
		constexpr unsigned MinLevel = 1;
		constexpr unsigned MaxLevel = 8;
		std::cout << "Trying levels from " << MinLevel << " to " << MaxLevel << "...\n";
		for (unsigned level = MinLevel; level <= MaxLevel; level++) {
			threads.emplace_back([level, stop_token = stop_source.get_token()]() {
				constexpr std::string_view Suffix = ".DUN";
				std::array<char, 16> strBuf;
				memset(strBuf.data(), letters.front(), level);
				memcpy(strBuf.data() + level, Suffix.data(), Suffix.length());
				while (!stop_token.stop_requested() && nextString(strBuf.data(), level)) {
					match(std::string_view(strBuf.data(), level + Suffix.length()));
				}
			});
		}
	}

	return 0;
}
