#include <iostream>
#include <fstream>
#include <filesystem>

#include "SoundbankWriter.h"
#include "Transcoder.h"

namespace dalia {
	void saveToRawFile(const std::vector<uint8_t>& data, const std::string& filename) {
		// Open a file in binary mode
		std::ofstream outFile(filename, std::ios::binary);

		if (outFile.is_open()) {
			// Dump the entire vector directly into the file in one go
			outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
			std::streampos size = outFile.tellp();
			std::cout << size << "thing" <<std::endl;
			outFile.close();
			std::cout << "Successfully saved raw audio to: " << filename << "\n";
		} else {
			std::cerr << "Failed to open file for writing!\n";
		}
	}
}

namespace fs = std::filesystem;

int main() {
	using namespace dalia;
	std::cout << "soundbank tool thing" << std::endl;

	std::string folderPath = "assets/";
	std::vector<std::string> fileList;

	// Loop through everything in the folder
	for (const auto& entry : fs::directory_iterator(folderPath)) {

		// Make sure it's an actual file and not another folder
		if (entry.is_regular_file()) {

			std::string ext = entry.path().extension().string();
			if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
				fileList.push_back(entry.path().string());
				std::cout << "Found audio file: " << entry.path().string() << "\n";
			}

			std::cout << "Found file: " << entry.path().string() << "\n";
		}
	}
	fs::create_directories("assets/Soundbanks");
	SoundbankWriter writer("assets/Soundbanks/Soundbank.dsb");
	Transcoder transcoder(AudioFormat::PCM_16);
	std::cout << "Soundbank header written" << std::endl;


	for (int i = 0; i < fileList.size(); i++) {
		AudioData data = transcoder.Transcode(fileList[i].c_str());

		std::filesystem::path pathObj(fileList[i]);
		data.name = pathObj.stem().string();
		std::cout << "transcode done on sound: " << data.name << std::endl;

		writer.AddSound(data.name.c_str(), data.bytes, data.format, data.channels, data.sampleRate);
		std::cout << "sound added" << std::endl;
	}
	writer.CloseSoundbank();

	//saveToRawFile(data.bytes, "assets/output.raw");

	return 0;
}
