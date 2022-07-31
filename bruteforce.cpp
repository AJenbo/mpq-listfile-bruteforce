#include <array>
#include <fstream>
#include <iostream>
#include <string>

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

constexpr char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_-0123456789."; // todo take as arg

void save(std::string_view result)
{
	std::ofstream myfile;
	myfile.open("cracked.txt");
	myfile << result << "\n";
	myfile.close();
}

std::string generatedName;
int maxLevel = 0;

void buildString(int pos = 0)
{
	if (maxLevel == pos) {
		if (Hash1(generatedName) == CRACK1 && Hash2(generatedName) == CRACK2) {
			std::cout << "Found match: " << generatedName << std::endl;
			save(generatedName);
			exit(0);
		}
		return;
	}

	for (int i = 0; i < sizeof(letters) - 1; i++) {
		generatedName[pos] = letters[i];
		buildString(pos + 1);
	}
}

int main(int argc, char *argv[])
{
	std::string prefix = "LEVELS\\L1DATA\\"; // TODO take as arg
	HashPrefix(prefix, 1);
	HashPrefix(prefix, 2);

	for (unsigned level = 0; level < 8; level++) {
		std::cout << "Trying " << (level + 1) << " levels...\n";
		generatedName = std::string(level + 1, letters[0]);
		generatedName.append(".DUN");
		maxLevel = level + 1;

		buildString();
	}

	return 0;
}
