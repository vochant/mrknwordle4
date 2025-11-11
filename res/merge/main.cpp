#include "dictReader.hpp"

#include <set>
#include <iostream>
#include <fstream>
#include "nbt_tags.h"
#include "io/stream_writer.h"

using namespace nbt;

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    auto basicValidation = [](const std::string& str) {
        if (str.length() != 5) return false;
        for (int i = 0; i < 5; i++) if (str[i] < 'a' || str[i] > 'z') return false;
        return true;
    };
    if (argc < 2) {
        std::cerr << "An output file must be specified.\n";
        return 1;
    }
    std::ofstream ofs(argv[1], std::ios::binary);
    if (!ofs) {
        std::cerr << "Could not open the output file.\n";
        return 1;
    }
    io::stream_writer writer(ofs);
    std::set<std::string> dict;
    for (int i = 2; i < argc; i += 2) {
        if (i == argc - 1) {
            std::cerr << "No file specified for the last input.\n";
            return 1;
        }
        auto reader = createDictReader(argv[i + 1], argv[i]);
        auto part = reader->read();
        for (const auto& word : part) if (basicValidation(word)) {
            dict.insert(word);
        }
    }
    tag_list list(tag_type::String);
    for (const auto& word : dict) list.push_back(tag_string(word));
    tag_compound root;
    root["words"] = std::move(list);
    writer.write_type(tag_type::Compound);
    writer.write_string("");
    root.write_payload(writer);
    ofs.close();
    return 0;
}