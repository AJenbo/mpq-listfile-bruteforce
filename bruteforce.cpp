#include <array>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
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
// constexpr uint32_t CRACK1 = 0x3CC2BEC6;
// constexpr uint32_t CRACK2 = 0xC2F426EA;

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

void initString(char *str, size_t len, uint64_t iter)
{
	for (size_t i = 0; i < len; ++i) {
		str[i] = letters[iter % letters.size()];
		iter /= letters.size();
	}
}

void nextString(char *str, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (str[i] != letters.back()) {
			memset(str, letters.front(), i);
			str[i] = nextLetter[static_cast<unsigned char>(str[i])];
			return;
		}
	}
}

std::stop_source stop_source;
std::mutex stdout_mutex;

void match(std::string_view str)
{
	if (Hash1(str) == CRACK1 && Hash2(str) == CRACK2) {
		stop_source.request_stop();
		std::lock_guard lock { stdout_mutex };
		std::cout << "\nFound match: " << str << std::endl;
		save(str);
	}
}

std::jthread startProgressThread(
    bool display_statuses,
    const std::vector<unsigned> &progresses, const std::vector<std::string> &statuses,
    std::condition_variable &cv, std::mutex &cv_mutex, std::stop_source &progress_stop)
{
	return std::jthread([display_statuses, &cv, &cv_mutex, &statuses, &progresses, stop_token = stop_source.get_token(), progress_stop_token = progress_stop.get_token()]() {
		while (!(stop_token.stop_requested() || progress_stop_token.stop_requested())) {
			std::unique_lock lock { cv_mutex };
			cv.wait(lock);
			if (stop_token.stop_requested())
				return;
			std::lock_guard stdout_lock { stdout_mutex };
			std::cout << "\33[2K\r "; // clear line
			unsigned progressSum = 0;
			for (unsigned progress : progresses)
				progressSum += progress;
			std::cout << (progressSum / progresses.size()) << "%";
			if (display_statuses) {
				for (const std::string &status : statuses) {
					std::cout << " " << status;
				}
			}
			std::cout.flush();
		}
	});
}

int main(int argc, char *argv[])
{
	bool progress = false;
	bool display_statuses = false;
	for (int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];
		if (arg == "-p") {
			progress = true;
		} else if (arg == "-s") {
			display_statuses = true;
		}
	}

	for (size_t i = 1; i < letters.size(); ++i) {
		nextLetter[static_cast<unsigned char>(letters[i - 1])] = letters[i];
	}

	std::string prefix = "LEVELS\\L1DATA\\"; // TODO take as arg
	HashPrefix(prefix, 1);
	HashPrefix(prefix, 2);

	constexpr unsigned MinLevel = 1;
	constexpr unsigned MaxLevel = 8;
	constexpr unsigned NumChunks = 32;

	std::condition_variable progress_cv;
	std::mutex progress_cv_mutex;
	std::vector<std::string> statuses(NumChunks);
	std::vector<unsigned> progresses(NumChunks);
	for (std::string &status : statuses) {
		status.reserve(40);
	}

	uint64_t totalIters = 1;
	std::cout << "Trying levels from " << MinLevel << " to " << MaxLevel << " in " << NumChunks << " chunks...\n";
	const std::stop_token loop_stop_token = stop_source.get_token();
	for (unsigned level = MinLevel; level <= MaxLevel && !loop_stop_token.stop_requested(); level++) {
		totalIters *= letters.size();
		std::cout << "Level " << level << " with ~" << (totalIters / NumChunks) << " iterations per chunk:\n";
		{
			std::stop_source progress_stop;
			std::jthread progress_thread;
			if (progress)
				progress_thread = startProgressThread(display_statuses, progresses, statuses, progress_cv, progress_cv_mutex, progress_stop);
			{
				if (display_statuses) {
					for (std::string &status : statuses) {
						status.clear();
						status.append("Start");
					}
				}
				progress_cv.notify_one();
				std::vector<std::jthread> threads;
				for (unsigned chunk = 0; chunk < NumChunks; ++chunk) {
					std::string &status = statuses[chunk];
					unsigned &progress = progresses[chunk];
					threads.emplace_back([display_statuses, totalIters, level, chunk, &progress, &status, &progress_cv, stop_token = stop_source.get_token()]() {
						constexpr std::string_view Suffix = ".DUN";
						std::array<char, 16> strBuf;
						const size_t chunkBegin = totalIters * chunk / NumChunks;
						const size_t chunkEnd = totalIters * (chunk + 1) / NumChunks;
						const size_t chunkSize = chunkEnd - chunkBegin;

						initString(strBuf.data(), level, chunkBegin);
						memcpy(strBuf.data() + level, Suffix.data(), Suffix.length());
						size_t numIters;
						for (uint64_t i = 0; i < chunkSize && !stop_token.stop_requested(); ++i) {
							nextString(strBuf.data(), level);
							const std::string_view str { strBuf.data(), level + Suffix.length() };
							match(str);
							if (i % 1000193 == 0) {
								{
									std::lock_guard lock { stdout_mutex };
									if (display_statuses) {
										status.clear();
										status.append(strBuf.data(), level);
									}
									progress = static_cast<unsigned>(std::llround(100 * static_cast<double>(i + 1) / chunkSize));
								}
								progress_cv.notify_one();
							}
						}

						if (!stop_token.stop_requested()) {
							{
								std::lock_guard lock { stdout_mutex };
								progress = 100;
								if (display_statuses) {
									status.clear();
									status.append(std::string(level, ' '));
								}
							}
							progress_cv.notify_one();
						}
					});
				}
			}
			progress_stop.request_stop();
			progress_cv.notify_one();
		}
		if (progress && !loop_stop_token.stop_requested())
			std::cout << "\n";
	}
	stop_source.request_stop();

	return 0;
}
